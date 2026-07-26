#include <RobotKinematics/Collision/MeshCollisionProfile.h>

namespace RobotKinematics {

/// Returns the JSON/config string identifier for `backendKind` ("none", "coal", "fcl", or
/// "vtk_debug"); falls back to "none" for any unrecognized enumerator value.
std::string toString(const MeshCollisionBackendKind backendKind)
{
    switch (backendKind) {
    case MeshCollisionBackendKind::None:
        return "none";
    case MeshCollisionBackendKind::Coal:
        return "coal";
    case MeshCollisionBackendKind::Fcl:
        return "fcl";
    case MeshCollisionBackendKind::VtkDebug:
        return "vtk_debug";
    }

    return "none";
}

/// Parses a mesh collision backend preference string ("none", "coal", "fcl", or "vtk_debug")
/// into its MeshCollisionBackendKind enumerator.
Result<MeshCollisionBackendKind> meshCollisionBackendKindFromString(const std::string& value)
{
    if (value == "none") {
        return Result<MeshCollisionBackendKind>::success(MeshCollisionBackendKind::None);
    }
    if (value == "coal") {
        return Result<MeshCollisionBackendKind>::success(MeshCollisionBackendKind::Coal);
    }
    if (value == "fcl") {
        return Result<MeshCollisionBackendKind>::success(MeshCollisionBackendKind::Fcl);
    }
    if (value == "vtk_debug") {
        return Result<MeshCollisionBackendKind>::success(MeshCollisionBackendKind::VtkDebug);
    }

    return Result<MeshCollisionBackendKind>::failure(
        KinematicsStatus::InvalidRobotConfig,
        "unsupported mesh collision backend preference: " + value);
}

} // namespace RobotKinematics
