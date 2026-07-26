#include <RobotKinematics/Collision/CollisionChecker.h>

#include <RobotKinematics/Collision/CollisionProfileValidator.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Model/RobotModelValidator.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

/// Robot kinematics, collision profile, and forward-kinematics types used by the primitive
/// (sphere/capsule) self-collision checker.
namespace RobotKinematics {

/// Translation-unit-local helpers and types backing CollisionChecker::check(): a placed
/// primitive-geometry cache entry, pair-key/clamping utilities, and the sphere/capsule
/// distance and broad-phase routines.
namespace {
/// A single collision geometry placed into the robot's base frame for one FK pose: its
/// world-space capsule segment endpoints (a sphere collapses both to the same point), center,
/// and a conservative bounding-sphere radius used for the broad-phase check.
struct PlacedGeometry {
    const CollisionGeometry* geometry = nullptr;
    Pose geometryInBase = Pose::identity();
    Eigen::Vector3d segmentStart_m = Eigen::Vector3d::Zero();
    Eigen::Vector3d segmentEnd_m = Eigen::Vector3d::Zero();
    Eigen::Vector3d center_m = Eigen::Vector3d::Zero();
    double boundingRadius_m = 0.0;
};

/// Builds an order-independent key for an unordered geometry-id pair, so a disabled pair
/// {a, b} matches lookups made as either (a, b) or (b, a).
/// @return the two ids joined by a newline separator, always in the lexicographically
///         smaller-first order
std::string normalizePairKey(const std::string& a, const std::string& b)
{
    return a < b ? a + "\n" + b : b + "\n" + a;
}

/// Clamps `value` into the [0, 1] range, used to keep segment interpolation parameters
/// within the bounds of the actual segment.
double clamp01(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

/// Computes the shortest distance from `point` to the line segment [`start`, `end`],
/// projecting the point onto the segment and clamping to its endpoints.
double distancePointToSegment(const Eigen::Vector3d& point,
                              const Eigen::Vector3d& start,
                              const Eigen::Vector3d& end)
{
    const Eigen::Vector3d segment = end - start;
    const double lengthSquared = segment.squaredNorm();
    if (lengthSquared <= 1e-18) {
        return (point - start).norm();
    }

    const double t = clamp01((point - start).dot(segment) / lengthSquared);
    return (point - (start + t * segment)).norm();
}

/// Computes the shortest distance between line segments [`p1`, `q1`] and [`p2`, `q2`] using
/// the standard closest-points-between-segments algorithm, handling the degenerate cases
/// where one or both segments collapse to a point.
double distanceSegmentToSegment(const Eigen::Vector3d& p1,
                                const Eigen::Vector3d& q1,
                                const Eigen::Vector3d& p2,
                                const Eigen::Vector3d& q2)
{
    const Eigen::Vector3d d1 = q1 - p1;
    const Eigen::Vector3d d2 = q2 - p2;
    const Eigen::Vector3d r = p1 - p2;
    const double a = d1.dot(d1);
    const double e = d2.dot(d2);
    const double f = d2.dot(r);

    double s = 0.0;
    double t = 0.0;

    if (a <= 1e-18 && e <= 1e-18) {
        return (p1 - p2).norm();
    }

    if (a <= 1e-18) {
        t = clamp01(f / e);
    } else {
        const double c = d1.dot(r);
        if (e <= 1e-18) {
            s = clamp01(-c / a);
        } else {
            const double b = d1.dot(d2);
            const double denom = a * e - b * b;

            if (std::abs(denom) > 1e-18) {
                s = clamp01((b * f - c * e) / denom);
            }

            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = clamp01(-c / a);
            } else if (t > 1.0) {
                t = 1.0;
                s = clamp01((b - c) / a);
            }
        }
    }

    const Eigen::Vector3d c1 = p1 + d1 * s;
    const Eigen::Vector3d c2 = p2 + d2 * t;
    return (c1 - c2).norm();
}

/// Places `geometry` into the robot's base frame for the given FK chain: resolves its link
/// pose, applies the geometry-to-link offset, and derives the sphere/capsule segment
/// endpoints and bounding radius used by the primitive distance and broad-phase checks.
/// @note assumes `chain.linkPosesInBase` contains `geometry.linkId` (callers check this first)
PlacedGeometry placeGeometry(const FkChain& chain, const CollisionGeometry& geometry)
{
    const Pose linkPose = chain.linkPosesInBase.at(geometry.linkId);

    PlacedGeometry placed;
    placed.geometry = &geometry;
    placed.geometryInBase = linkPose * geometry.geometryToLink;
    placed.center_m = placed.geometryInBase.translation_m();

    switch (geometry.shape.type) {
    case CollisionShapeType::Sphere:
        placed.segmentStart_m = placed.center_m;
        placed.segmentEnd_m = placed.center_m;
        placed.boundingRadius_m = geometry.shape.sphere.radius_m;
        break;
    case CollisionShapeType::Capsule: {
        const double halfLength = geometry.shape.capsule.length_m * 0.5;
        const Eigen::Vector3d axisInBase =
            placed.geometryInBase.isometry().linear() * Eigen::Vector3d::UnitZ();
        placed.segmentStart_m = placed.center_m - axisInBase * halfLength;
        placed.segmentEnd_m = placed.center_m + axisInBase * halfLength;
        placed.boundingRadius_m = geometry.shape.capsule.radius_m + halfLength;
        break;
    }
    }

    return placed;
}

/// Computes the surface-to-surface distance between two placed primitive geometries,
/// dispatching by shape-type combination (sphere-sphere, sphere-capsule, capsule-sphere, or
/// capsule-capsule) and subtracting both shapes' radii from the underlying center/segment
/// distance. A negative result means the shapes overlap.
double primitiveDistance(const PlacedGeometry& a, const PlacedGeometry& b)
{
    const CollisionGeometry& geometryA = *a.geometry;
    const CollisionGeometry& geometryB = *b.geometry;

    if (geometryA.shape.type == CollisionShapeType::Sphere &&
        geometryB.shape.type == CollisionShapeType::Sphere) {
        return (a.center_m - b.center_m).norm() -
               geometryA.shape.sphere.radius_m -
               geometryB.shape.sphere.radius_m;
    }

    if (geometryA.shape.type == CollisionShapeType::Sphere &&
        geometryB.shape.type == CollisionShapeType::Capsule) {
        return distancePointToSegment(a.center_m, b.segmentStart_m, b.segmentEnd_m) -
               geometryA.shape.sphere.radius_m -
               geometryB.shape.capsule.radius_m;
    }

    if (geometryA.shape.type == CollisionShapeType::Capsule &&
        geometryB.shape.type == CollisionShapeType::Sphere) {
        return distancePointToSegment(b.center_m, a.segmentStart_m, a.segmentEnd_m) -
               geometryA.shape.capsule.radius_m -
               geometryB.shape.sphere.radius_m;
    }

    return distanceSegmentToSegment(a.segmentStart_m, a.segmentEnd_m,
                                    b.segmentStart_m, b.segmentEnd_m) -
           geometryA.shape.capsule.radius_m -
           geometryB.shape.capsule.radius_m;
}

/// Cheap broad-phase rejection test: true when the two geometries' bounding spheres (plus
/// `extraMargin_m`) cannot possibly be closer than that margin, letting the caller skip the
/// exact primitive distance computation for clearly-separated pairs.
bool isBroadPhaseSeparated(const PlacedGeometry& a,
                           const PlacedGeometry& b,
                           double extraMargin_m)
{
    const double centerDistance = (a.center_m - b.center_m).norm();
    const double threshold = a.boundingRadius_m + b.boundingRadius_m + extraMargin_m;
    return centerDistance > threshold;
}
}

/// Runs primitive (sphere/capsule) self-collision checking for `config`: validates the robot
/// config, profile, and joint dimension, computes the FK chain for `request.joints`, places
/// every enabled geometry, then tests each non-disabled, distinct-link geometry pair for
/// collision (using a broad-phase bounding-sphere rejection before the exact primitive
/// distance) and records distance/collision results per pair.
CollisionCheckResult CollisionChecker::check(const SerialRobotConfig& config,
                                             const CollisionProfile& profile,
                                             const CollisionCheckRequest& request)
{
    const ModelValidationResult modelValidation = RobotModelValidator::validateSerialRobotConfig(config);
    if (!modelValidation.ok()) {
        return CollisionCheckResult{
            modelValidation.status(),
            false,
            {},
            modelValidation.issues.front().message,
        };
    }

    const CollisionProfileValidationResult profileValidation =
        CollisionProfileValidator::validate(config, profile);
    if (!profileValidation.ok()) {
        return CollisionCheckResult{
            profileValidation.status(),
            false,
            {},
            profileValidation.issues.front().message,
        };
    }

    if (request.joints.size() != ForwardKinematics::movableJointCount(config)) {
        return CollisionCheckResult{
            KinematicsStatus::JointDimensionMismatch,
            false,
            {},
            "joint vector dimension does not match robot dof",
        };
    }

    const FkChain chain = ForwardKinematics::computeChain(config, request.joints);

    std::vector<PlacedGeometry> placedGeometries;
    placedGeometries.reserve(profile.geometries.size());
    for (const CollisionGeometry& geometry : profile.geometries) {
        if (!geometry.enabled) {
            continue;
        }

        const auto linkIt = chain.linkPosesInBase.find(geometry.linkId);
        if (linkIt == chain.linkPosesInBase.end()) {
            return CollisionCheckResult{
                KinematicsStatus::InvalidRobotConfig,
                false,
                {},
                "collision geometry link pose not found in FK chain: " + geometry.linkId,
            };
        }

        placedGeometries.push_back(placeGeometry(chain, geometry));
    }

    std::set<std::string> disabledPairs;
    for (const DisabledCollisionPair& pair : profile.disabledPairs) {
        disabledPairs.insert(normalizePairKey(pair.geometryA, pair.geometryB));
    }

    CollisionCheckResult result;
    result.status = KinematicsStatus::Ok;

    for (std::size_t i = 0; i < placedGeometries.size(); ++i) {
        for (std::size_t j = i + 1; j < placedGeometries.size(); ++j) {
            const PlacedGeometry& a = placedGeometries[i];
            const PlacedGeometry& b = placedGeometries[j];
            const CollisionGeometry& geometryA = *a.geometry;
            const CollisionGeometry& geometryB = *b.geometry;

            if (geometryA.id == geometryB.id) {
                continue;
            }
            if (geometryA.linkId == geometryB.linkId) {
                continue;
            }
            if (disabledPairs.find(normalizePairKey(geometryA.id, geometryB.id)) != disabledPairs.end()) {
                continue;
            }

            const double pairMargin =
                geometryA.margin_m + geometryB.margin_m + request.safetyMargin_m;

            if (isBroadPhaseSeparated(a, b, pairMargin)) {
                const double distance =
                    (a.center_m - b.center_m).norm() - (a.boundingRadius_m + b.boundingRadius_m);
                result.pairs.push_back(CollisionPairResult{
                    geometryA.id,
                    geometryB.id,
                    geometryA.linkId,
                    geometryB.linkId,
                    false,
                    distance,
                    0,
                });
                continue;
            }

            const double distance = primitiveDistance(a, b);
            const bool colliding = distance <= pairMargin;

            result.pairs.push_back(CollisionPairResult{
                geometryA.id,
                geometryB.id,
                geometryA.linkId,
                geometryB.linkId,
                colliding,
                distance,
                0,
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
