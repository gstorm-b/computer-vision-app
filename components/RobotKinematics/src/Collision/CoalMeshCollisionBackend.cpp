#include "CoalMeshCollisionBackend.h"

#include <RobotKinematics/Collision/MeshCollisionProfileValidator.h>
#include <RobotKinematics/Collision/StlMeshLoader.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Model/RobotModelValidator.h>

#include <QDir>
#include <QFileInfo>

#include <coal/BV/OBBRSS.h>
#include <coal/BVH/BVH_model.h>
#include <coal/collision.h>
#include <coal/collision_data.h>
#include <coal/collision_object.h>
#include <coal/distance.h>

#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

/// Robot kinematics, collision profile, and forward-kinematics types shared across the
/// mesh collision backends.
namespace RobotKinematics {

/// Translation-unit-local helpers and caches backing the coal (HPP-FCL fork) mesh
/// collision backend: cache-key builders, coal type conversions, and the loaded-geometry
/// / built-runtime caches themselves.
namespace {
/// BVH mesh representation used by coal for all loaded triangle meshes, built with an
/// OBBRSS (oriented bounding box + rectangular swept sphere) bounding volume hierarchy.
using CoalBvh = coal::BVHModel<coal::OBBRSS>;

/// A loaded STL mesh converted into a coal BVH, keyed and cached by resolved path plus
/// format/units/scale so identical mesh assets are only parsed and built once.
struct CachedCoalGeometry {
    std::string resolvedPath;       ///< Absolute filesystem path the mesh was actually loaded from.
    TriangleMeshStatistics statistics;  ///< Vertex/face counts and other stats reported by the STL loader.
    std::shared_ptr<CoalBvh> model;  ///< Shared coal BVH model built from the mesh's vertices/faces.
};

/// One collision geometry entry within a built profile runtime: the source mesh
/// description paired with its cached coal geometry and a per-check-call coal
/// CollisionObject holding that geometry's current world transform.
struct CoalMeshRuntimeEntry {
    MeshCollisionGeometry mesh;  ///< Source mesh collision geometry description from the profile.
    std::shared_ptr<CachedCoalGeometry> geometry;  ///< Cached coal BVH backing this mesh entry.
    std::unique_ptr<coal::CollisionObject> object;  ///< Coal collision object; transform is updated per FK pose.
};

/// A fully-built, cacheable runtime for a MeshCollisionProfile: every enabled mesh loaded
/// as coal geometry and wrapped in a CollisionObject, ready to have transforms applied and
/// be checked for collisions.
struct CoalProfileRuntime {
    std::string cacheKey;  ///< Runtime cache key this instance was stored under (see profileRuntimeKey).
    std::vector<CoalMeshRuntimeEntry> meshes;  ///< Enabled mesh entries built from the profile, in profile order.
};

/// Process-lifetime cache of loaded coal BVH geometry, keyed by geometryCacheKey() so a
/// given STL file/format/units/scale combination is loaded and built only once.
/// @return mutable reference to the function-local static cache map
std::map<std::string, std::shared_ptr<CachedCoalGeometry>>& geometryCache()
{
    static std::map<std::string, std::shared_ptr<CachedCoalGeometry>> cache;
    return cache;
}

/// Process-lifetime cache of built CoalProfileRuntime instances, keyed by
/// profileRuntimeKey() so an unchanged profile is not rebuilt (and its meshes not
/// reloaded) on every collision check.
/// @return mutable reference to the function-local static cache map
std::map<std::string, std::shared_ptr<CoalProfileRuntime>>& profileCache()
{
    static std::map<std::string, std::shared_ptr<CoalProfileRuntime>> cache;
    return cache;
}

/// Builds an order-independent key for an unordered geometry-id pair, so that a disabled
/// pair {a, b} matches lookups made as either (a, b) or (b, a).
/// @return the two ids joined by a newline separator, always in the lexicographically
///         smaller-first order
std::string normalizePairKey(const std::string& a, const std::string& b)
{
    return a < b ? a + "\n" + b : b + "\n" + a;
}

/// Appends `value` to `stream` at full double precision (17 significant digits), so
/// cache keys derived from floating-point values do not collide due to truncation.
void appendDouble(std::ostringstream& stream, double value)
{
    stream << std::setprecision(17) << value;
}

/// Serializes a pose's rotation matrix and translation to a full-precision text key, used
/// to detect when a mesh's link-relative pose has changed between cache lookups.
/// @return comma-separated rotation entries (row-major) followed by translation entries
std::string poseKey(const Pose& pose)
{
    const Eigen::Isometry3d& iso = pose.isometry();
    std::ostringstream key;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            appendDouble(key, iso.linear()(row, col));
            key << ",";
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        appendDouble(key, iso.translation()(axis));
        key << ",";
    }
    return key.str();
}

/// Builds the cache key identifying a mesh's contribution to a CoalProfileRuntime: every
/// field that affects the built runtime entry (id, link, path, format, units, scale,
/// margin, enabled flag, and mesh-to-link pose) is encoded so any change forces a rebuild.
/// @return the delimited runtime key for this mesh
std::string meshRuntimeKey(const MeshCollisionGeometry& mesh)
{
    std::ostringstream key;
    key << mesh.id << "|"
        << mesh.linkId << "|"
        << mesh.path << "|"
        << static_cast<int>(mesh.format) << "|"
        << static_cast<int>(mesh.sourceUnits) << "|";
    appendDouble(key, mesh.scaleToMeters);
    key << "|";
    appendDouble(key, mesh.margin_m);
    key << "|" << (mesh.enabled ? "1" : "0") << "|" << poseKey(mesh.meshToLink);
    return key.str();
}

/// Builds the profileCache() lookup key for `profile` by combining its id/robot model
/// with every mesh's meshRuntimeKey(), so any change to any mesh invalidates the cached
/// CoalProfileRuntime and forces a rebuild.
/// @return the delimited runtime key for the whole profile
std::string profileRuntimeKey(const MeshCollisionProfile& profile)
{
    std::ostringstream key;
    key << profile.id << "|" << profile.robotModel << "|";
    for (const MeshCollisionGeometry& mesh : profile.meshes) {
        key << meshRuntimeKey(mesh) << ";";
    }
    return key.str();
}

/// Resolves a possibly-relative mesh path to an absolute filesystem path, treating a
/// relative `path` as relative to the current working directory.
/// @return the absolute path
std::string resolveMeshPath(const std::string& path)
{
    const QFileInfo fileInfo(QString::fromStdString(path));
    if (fileInfo.isAbsolute()) {
        return fileInfo.absoluteFilePath().toStdString();
    }
    return QDir::current().absoluteFilePath(fileInfo.filePath()).toStdString();
}

/// Builds the geometryCache() lookup key for a mesh's loaded coal BVH: the resolved file
/// path plus format/source-units/scale, since those are the only inputs that affect the
/// loaded geometry (mesh id, link, margin, and pose do not).
/// @return the delimited geometry cache key
std::string geometryCacheKey(const MeshCollisionGeometry& mesh, const std::string& resolvedPath)
{
    std::ostringstream key;
    key << resolvedPath << "|"
        << static_cast<int>(mesh.format) << "|"
        << static_cast<int>(mesh.sourceUnits) << "|";
    appendDouble(key, mesh.scaleToMeters);
    return key.str();
}

/// Converts an Eigen 3D vector to coal's Vec3s scalar-vector type.
coal::Vec3s toCoalVector(const Eigen::Vector3d& value)
{
    return coal::Vec3s(value.x(), value.y(), value.z());
}

/// Converts a Pose's isometry (rotation + translation) to a coal Transform3s.
coal::Transform3s toCoalTransform(const Pose& pose)
{
    const Eigen::Isometry3d& iso = pose.isometry();
    return coal::Transform3s(iso.linear(), iso.translation());
}

/// Builds a failed CollisionCheckResult carrying `status` and `message`, with
/// hasCollision forced to false.
CollisionCheckResult failure(KinematicsStatus status, const std::string& message)
{
    CollisionCheckResult result;
    result.status = status;
    result.hasCollision = false;
    result.message = message;
    return result;
}

/// Loads (or returns the already-cached) coal BVH geometry for a single mesh: resolves
/// its path, checks the geometryCache(), and on a miss loads the STL file, discarding
/// degenerate triangles, and builds the coal BVH model from its vertices/faces.
/// @param mesh the mesh collision geometry to load; only STL format is currently supported
/// @return the cached/loaded geometry, or a failure if the format is unsupported, the STL
///         file failed to load, or the coal BVH build (beginModel/addSubModel/endModel)
///         failed
Result<std::shared_ptr<CachedCoalGeometry>> loadCoalGeometry(const MeshCollisionGeometry& mesh)
{
    if (mesh.format != MeshFileFormat::Stl) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::failure(
            KinematicsStatus::InvalidRequest,
            "coal mesh backend currently supports STL meshes only");
    }

    const std::string resolvedPath = resolveMeshPath(mesh.path);
    const std::string cacheKey = geometryCacheKey(mesh, resolvedPath);

    auto& cache = geometryCache();
    const auto cached = cache.find(cacheKey);
    if (cached != cache.end()) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::success(cached->second);
    }

    // Real-world CAD STL exports often contain a handful of degenerate triangles.
    // Skip them silently so production loads do not fail on otherwise-valid assets;
    // Coal's BVH builder also expects non-degenerate faces.
    const Result<TriangleMesh> loaded =
        StlMeshLoader::loadFile(resolvedPath, StlMeshLoadOptions{mesh.scaleToMeters, false});
    if (!loaded.ok()) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::failure(
            loaded.status,
            "failed to load mesh '" + mesh.id + "' from '" + resolvedPath + "': " + loaded.message);
    }

    std::vector<coal::Vec3s> vertices;
    vertices.reserve(loaded.value.vertices_m.size());
    for (const Eigen::Vector3d& vertex : loaded.value.vertices_m) {
        vertices.push_back(toCoalVector(vertex));
    }

    std::vector<coal::Triangle32> triangles;
    triangles.reserve(loaded.value.faces.size());
    for (const TriangleFace& face : loaded.value.faces) {
        triangles.emplace_back(static_cast<coal::Triangle32::IndexType>(face.a),
                               static_cast<coal::Triangle32::IndexType>(face.b),
                               static_cast<coal::Triangle32::IndexType>(face.c));
    }

    auto model = std::make_shared<CoalBvh>();
    if (model->beginModel(static_cast<unsigned int>(triangles.size()),
                          static_cast<unsigned int>(vertices.size())) != 0) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::failure(
            KinematicsStatus::NumericalError,
            "coal beginModel failed for mesh '" + mesh.id + "'");
    }
    if (model->addSubModel(vertices, triangles) != 0) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::failure(
            KinematicsStatus::NumericalError,
            "coal addSubModel failed for mesh '" + mesh.id + "'");
    }
    if (model->endModel() != 0) {
        return Result<std::shared_ptr<CachedCoalGeometry>>::failure(
            KinematicsStatus::NumericalError,
            "coal endModel failed for mesh '" + mesh.id + "'");
    }

    auto cachedGeometry = std::make_shared<CachedCoalGeometry>();
    cachedGeometry->resolvedPath = resolvedPath;
    cachedGeometry->statistics = loaded.value.statistics;
    cachedGeometry->model = model;
    cache.insert({cacheKey, cachedGeometry});
    return Result<std::shared_ptr<CachedCoalGeometry>>::success(cachedGeometry);
}

/// Loads (or returns the already-cached) CoalProfileRuntime for a profile: builds a coal
/// CollisionObject for every enabled mesh (skipping disabled ones), loading each mesh's
/// geometry via loadCoalGeometry(), and caches the result under profileRuntimeKey().
/// @return the cached/built runtime, or a failure propagated from the first mesh that
///         fails to load
Result<std::shared_ptr<CoalProfileRuntime>> loadProfileRuntime(const MeshCollisionProfile& profile)
{
    const std::string cacheKey = profileRuntimeKey(profile);

    auto& cache = profileCache();
    const auto cached = cache.find(cacheKey);
    if (cached != cache.end()) {
        return Result<std::shared_ptr<CoalProfileRuntime>>::success(cached->second);
    }

    auto runtime = std::make_shared<CoalProfileRuntime>();
    runtime->cacheKey = cacheKey;
    runtime->meshes.reserve(profile.meshes.size());

    for (const MeshCollisionGeometry& mesh : profile.meshes) {
        if (!mesh.enabled) {
            continue;
        }

        const Result<std::shared_ptr<CachedCoalGeometry>> geometry = loadCoalGeometry(mesh);
        if (!geometry.ok()) {
            return Result<std::shared_ptr<CoalProfileRuntime>>::failure(geometry.status, geometry.message);
        }

        CoalMeshRuntimeEntry entry;
        entry.mesh = mesh;
        entry.geometry = geometry.value;
        entry.object = std::make_unique<coal::CollisionObject>(entry.geometry->model);
        runtime->meshes.push_back(std::move(entry));
    }

    cache.insert({cacheKey, runtime});
    return Result<std::shared_ptr<CoalProfileRuntime>>::success(runtime);
}

} // namespace

/// Describes the coal mesh collision backend's identity and availability, reporting it
/// as compiled-in and available since this backend is always built.
/// @return backend info for CollisionBackendKind::Mesh / MeshCollisionBackendKind::Coal
CollisionBackendInfo coalMeshBackendInfo()
{
    return CollisionBackendInfo{
        CollisionBackendKind::Mesh,
        MeshCollisionBackendKind::Coal,
        "coal",
        true,
        true,
        true,
        true,
        "Coal mesh collision backend compiled and available",
    };
}

/// Runs a full mesh collision check using the coal backend: validates the robot config,
/// mesh profile, and joint vector; loads/reuses the cached CoalProfileRuntime; poses every
/// mesh's collision object at its FK-computed link transform; then checks every eligible
/// mesh pair (excluding same-id, same-link, and explicitly disabled pairs) for collision
/// within the combined per-pair safety margin, using coal::distance and coal::collide and
/// preferring the contact penetration depth as the reported distance when available.
CollisionCheckResult checkCoalMeshBackend(const SerialRobotConfig& config,
                                          const MeshCollisionProfile& profile,
                                          const MeshCollisionCheckRequest& request)
{
    const ModelValidationResult modelValidation = RobotModelValidator::validateSerialRobotConfig(config);
    if (!modelValidation.ok()) {
        return failure(modelValidation.status(), modelValidation.issues.front().message);
    }

    const MeshCollisionProfileValidationResult profileValidation =
        MeshCollisionProfileValidator::validate(config, profile);
    if (!profileValidation.ok()) {
        return failure(profileValidation.status(), profileValidation.issues.front().message);
    }

    if (request.joints.size() != ForwardKinematics::movableJointCount(config)) {
        return failure(KinematicsStatus::JointDimensionMismatch,
                       "joint vector dimension does not match robot dof");
    }

    const Result<std::shared_ptr<CoalProfileRuntime>> runtime = loadProfileRuntime(profile);
    if (!runtime.ok()) {
        return failure(runtime.status, runtime.message);
    }

    const FkChain chain = ForwardKinematics::computeChain(config, request.joints);
    for (CoalMeshRuntimeEntry& mesh : runtime.value->meshes) {
        const auto linkIt = chain.linkPosesInBase.find(mesh.mesh.linkId);
        if (linkIt == chain.linkPosesInBase.end()) {
            return failure(KinematicsStatus::InvalidRobotConfig,
                           "mesh collision link pose not found in FK chain: " + mesh.mesh.linkId);
        }

        const Pose meshInBase = linkIt->second * mesh.mesh.meshToLink;
        mesh.object->setTransform(toCoalTransform(meshInBase));
        mesh.object->computeAABB();
    }

    std::set<std::string> disabledPairs;
    for (const DisabledCollisionPair& pair : profile.disabledPairs) {
        disabledPairs.insert(normalizePairKey(pair.geometryA, pair.geometryB));
    }

    CollisionCheckResult result;
    result.status = KinematicsStatus::Ok;

    for (std::size_t i = 0; i < runtime.value->meshes.size(); ++i) {
        for (std::size_t j = i + 1; j < runtime.value->meshes.size(); ++j) {
            const CoalMeshRuntimeEntry& a = runtime.value->meshes[i];
            const CoalMeshRuntimeEntry& b = runtime.value->meshes[j];

            if (a.mesh.id == b.mesh.id) {
                continue;
            }
            if (a.mesh.linkId == b.mesh.linkId) {
                continue;
            }
            if (disabledPairs.find(normalizePairKey(a.mesh.id, b.mesh.id)) != disabledPairs.end()) {
                continue;
            }

            coal::DistanceRequest distanceRequest(false, true);
            coal::DistanceResult distanceResult;
            coal::distance(a.object.get(), b.object.get(), distanceRequest, distanceResult);
            double distance = static_cast<double>(distanceResult.min_distance);

            const double pairMargin = a.mesh.margin_m + b.mesh.margin_m + request.safetyMargin_m;
            coal::CollisionRequest collisionRequest;
            collisionRequest.num_max_contacts = request.returnAllPairs ? std::size_t(16) : std::size_t(1);
            collisionRequest.enable_contact = true;
            collisionRequest.security_margin = pairMargin;
            collisionRequest.distance_upper_bound = (std::numeric_limits<coal::Scalar>::max)();

            coal::CollisionResult collisionResult;
            coal::collide(a.object.get(), b.object.get(), collisionRequest, collisionResult);
            const std::size_t contactCount = collisionResult.numContacts();
            const bool colliding = contactCount > 0 || distance <= pairMargin;

            if (colliding) {
                for (std::size_t contactIndex = 0; contactIndex < contactCount; ++contactIndex) {
                    const double contactDistance =
                        static_cast<double>(collisionResult.getContact(contactIndex).penetration_depth);
                    if (std::isfinite(contactDistance) && (contactIndex == 0 || contactDistance < distance)) {
                        distance = contactDistance;
                    }
                }
            }

            if (!std::isfinite(distance)) {
                return failure(KinematicsStatus::NumericalError,
                               "coal returned a non-finite mesh distance for pair '" +
                                   a.mesh.id + "'/'" + b.mesh.id + "'");
            }

            result.pairs.push_back(CollisionPairResult{
                a.mesh.id,
                b.mesh.id,
                a.mesh.linkId,
                b.mesh.linkId,
                colliding,
                distance,
                contactCount,
            });

            if (colliding) {
                result.hasCollision = true;
                if (!request.returnAllPairs) {
                    return result;
                }
            }
        }
    }

    return result;
}

} // namespace RobotKinematics
