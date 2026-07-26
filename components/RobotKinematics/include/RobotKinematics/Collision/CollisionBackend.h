#pragma once

#include <RobotKinematics/Collision/CollisionChecker.h>
#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Core/Result.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// Which family of self-collision backend a CollisionBackendInfo/check call refers to.
enum class CollisionBackendKind {
    Primitive,  ///< Built-in sphere/capsule analytic backend.
    Mesh        ///< STL-mesh-based backend (e.g. Coal), only available in builds that compile it in.
};

/// Capability/availability description for one collision backend, as returned by
/// CollisionBackends::primitiveInfo()/meshInfo().
struct CollisionBackendInfo {
    CollisionBackendKind kind = CollisionBackendKind::Primitive;            ///< Backend family this info describes.
    MeshCollisionBackendKind meshBackend = MeshCollisionBackendKind::None;  ///< Specific mesh backend implementation, if `kind` is Mesh.
    std::string backendName;          ///< Short machine-readable backend name.
    bool available = false;           ///< True if this backend is compiled into and usable in the current build.
    bool supportsMesh = false;        ///< True if the backend can check mesh (not just primitive) geometry.
    bool supportsDistance = false;    ///< True if the backend reports clearance distance for non-colliding pairs.
    bool supportsContacts = false;    ///< True if the backend reports contact point/count information.
    std::string detail;               ///< Human-readable detail, e.g. why the backend is unavailable.
};

/// Parameters for a mesh-backend self-collision check, mirroring CollisionCheckRequest but with a
/// backend preference for mesh checks.
struct MeshCollisionCheckRequest {
    JointVector joints;                 ///< Joint configuration to evaluate collisions at.
    double safetyMargin_m = 0.0;        ///< Extra clearance margin required between geometries, in metres.
    bool returnAllPairs = true;         ///< True to evaluate and return every pair; false to stop at the first collision.
    MeshCollisionBackendKind preferredBackend = MeshCollisionBackendKind::None;  ///< Backend to force, or None to use the profile's/compiled default.
};

/// Facade over the available self-collision backends: reports backend capabilities and dispatches
/// primitive/mesh collision checks to the appropriate implementation.
class CollisionBackends
{
public:
    /// @return capability info for the built-in primitive (sphere/capsule) backend; always available.
    static CollisionBackendInfo primitiveInfo();
    /// @return capability info for the compiled-in mesh backend, or an "unavailable" info if this
    /// build has no mesh collision backend compiled in
    static CollisionBackendInfo meshInfo();
    /// @return capability info for every backend (currently primitive and mesh, in that order)
    static std::vector<CollisionBackendInfo> availableBackends();

    /// Runs a self-collision check using the primitive (sphere/capsule) backend.
    /// @param config robot model the joint values are evaluated against
    /// @param profile primitive collision geometry/disabled-pair definitions to check
    /// @param request joint configuration, safety margin, and pair-reporting options
    /// @return per-pair collision/distance results, or a failure status on invalid input
    static CollisionCheckResult checkPrimitive(const SerialRobotConfig& config,
                                               const CollisionProfile& profile,
                                               const CollisionCheckRequest& request);

    /// Runs a self-collision check using the mesh backend selected by `request`/`profile`.
    /// @param config robot model the joint values are evaluated against
    /// @param profile mesh collision geometry definitions to check
    /// @param request joint configuration, safety margin, and backend preference
    /// @return per-pair collision/distance results, or KinematicsStatus::UnsupportedSolver if the
    /// requested mesh backend is not compiled into this build
    static CollisionCheckResult checkMesh(const SerialRobotConfig& config,
                                          const MeshCollisionProfile& profile,
                                          const MeshCollisionCheckRequest& request);
};

} // namespace RobotKinematics
