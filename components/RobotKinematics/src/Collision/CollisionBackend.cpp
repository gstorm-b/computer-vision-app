#include <RobotKinematics/Collision/CollisionBackend.h>
#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
#include "CoalMeshCollisionBackend.h"
#endif

/// Robot kinematics, collision profile, and mesh-backend selection types used by the
/// CollisionBackends dispatch layer.
namespace RobotKinematics {

/// Translation-unit-local helpers for resolving which mesh collision backend a request
/// should use and for naming the requested-but-unavailable backend in error messages.
namespace {
/// Returns the name of the mesh backend the caller asked for (request override takes
/// priority over the profile's preference order), for use in diagnostic messages when no
/// compiled backend can serve it.
/// @return the backend's toString() name, or "none" if neither the request nor the profile
///         specify a preference
std::string requestedMeshBackendName(const MeshCollisionProfile& profile,
                                     const MeshCollisionCheckRequest& request)
{
    if (request.preferredBackend != MeshCollisionBackendKind::None) {
        return toString(request.preferredBackend);
    }
    if (!profile.backendPreference.empty()) {
        return toString(profile.backendPreference.front());
    }
    return "none";
}

/// Picks which compiled mesh backend should serve `request`: an explicit
/// `request.preferredBackend` is honored only if it is actually compiled in (Coal build
/// only), otherwise falls back to scanning the profile's backend preference order, and
/// finally defaults to Coal if compiled in or None if no mesh backend is available at all.
/// @return the backend kind to use, or MeshCollisionBackendKind::None if no compiled
///         backend can satisfy the request
MeshCollisionBackendKind selectCompiledBackend(const MeshCollisionProfile& profile,
                                               const MeshCollisionCheckRequest& request)
{
#ifndef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    (void)profile;
#endif

    if (request.preferredBackend != MeshCollisionBackendKind::None) {
#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
        if (request.preferredBackend == MeshCollisionBackendKind::Coal) {
            return MeshCollisionBackendKind::Coal;
        }
#endif
        return MeshCollisionBackendKind::None;
    }

#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    for (const MeshCollisionBackendKind backend : profile.backendPreference) {
        if (backend == MeshCollisionBackendKind::Coal) {
            return MeshCollisionBackendKind::Coal;
        }
    }
#endif

#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    return MeshCollisionBackendKind::Coal;
#else
    return MeshCollisionBackendKind::None;
#endif
}
}

/// Returns static metadata for the always-available built-in primitive (sphere/capsule)
/// self-collision backend: available, supports distance queries, but not mesh geometry or
/// contact points.
CollisionBackendInfo CollisionBackends::primitiveInfo()
{
    return CollisionBackendInfo{
        CollisionBackendKind::Primitive,
        MeshCollisionBackendKind::None,
        "primitive_sphere_capsule",
        true,
        false,
        true,
        false,
        "Built-in sphere/capsule self-collision backend",
    };
}

/// Returns static metadata for the mesh collision backend: delegates to
/// coalMeshBackendInfo() when the Coal backend is compiled in, otherwise reports an
/// unavailable "mesh_unavailable" backend with all capability flags false.
CollisionBackendInfo CollisionBackends::meshInfo()
{
#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    return coalMeshBackendInfo();
#else
    return CollisionBackendInfo{
        CollisionBackendKind::Mesh,
        MeshCollisionBackendKind::None,
        "mesh_unavailable",
        false,
        false,
        false,
        false,
        "No mesh collision backend is compiled into this build",
    };
#endif
}

/// Lists metadata for every collision backend this build knows about (primitive, then mesh),
/// regardless of whether the mesh backend is actually available.
/// @return {primitiveInfo(), meshInfo()}
std::vector<CollisionBackendInfo> CollisionBackends::availableBackends()
{
    return {primitiveInfo(), meshInfo()};
}

/// Runs the built-in primitive (sphere/capsule) self-collision check by forwarding directly
/// to CollisionChecker::check().
CollisionCheckResult CollisionBackends::checkPrimitive(const SerialRobotConfig& config,
                                                       const CollisionProfile& profile,
                                                       const CollisionCheckRequest& request)
{
    return CollisionChecker::check(config, profile, request);
}

/// Runs mesh-based self-collision checking for `config`, dispatching to the Coal backend
/// when it is both compiled in and selected by selectCompiledBackend(); otherwise returns
/// an UnsupportedSolver failure result naming the requested backend.
/// @return the mesh backend's collision result on success, or an UnsupportedSolver failure
///         result if no compiled backend can serve the request
CollisionCheckResult CollisionBackends::checkMesh(const SerialRobotConfig& config,
                                                  const MeshCollisionProfile& profile,
                                                  const MeshCollisionCheckRequest& request)
{
#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    if (selectCompiledBackend(profile, request) == MeshCollisionBackendKind::Coal) {
        return checkCoalMeshBackend(config, profile, request);
    }
#else
    (void)config;
#endif

    CollisionCheckResult result;
    result.status = KinematicsStatus::UnsupportedSolver;
    result.hasCollision = false;
    result.message =
        "requested mesh collision backend is not available in this build; requested backend preference: " +
        requestedMeshBackendName(profile, request);
    return result;
}

} // namespace RobotKinematics
