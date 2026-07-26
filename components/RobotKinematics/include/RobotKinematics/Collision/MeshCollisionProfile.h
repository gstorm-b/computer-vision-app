#pragma once

#include <RobotKinematics/Collision/CollisionGeometry.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

/// Mesh-based collision primitives, geometry attachment, and profile format used for
/// triangle-mesh collision checking (loaded via MeshCollisionProfileJsonLoader).
namespace RobotKinematics {

/// Identifies which third-party collision backend a mesh collision profile prefers,
/// or a debug-only visualization mode.
enum class MeshCollisionBackendKind {
    None,
    Coal,
    Fcl,
    VtkDebug
};

/// Supported mesh file formats for collision geometry (currently STL only).
enum class MeshFileFormat {
    Stl
};

/// Units the mesh file's vertex coordinates are stored in, before scaleToMeters is applied.
enum class MeshSourceUnits {
    Meters,
    Millimeters
};

/// Level of processing applied to a mesh before it is used for collision checking.
enum class MeshQualityMode {
    Original,
    Simplified,
    Convex
};

/// Converts `backendKind` to its lowercase JSON string form (e.g. "vtk_debug"); returns
/// "none" for any unrecognized enumerator value.
std::string toString(MeshCollisionBackendKind backendKind);

/// Parses a MeshCollisionBackendKind from its lowercase JSON string form.
/// @param value one of "none", "coal", "fcl", "vtk_debug"
/// @return the matching backend kind, or an InvalidRobotConfig failure if `value` is unrecognized
Result<MeshCollisionBackendKind> meshCollisionBackendKindFromString(const std::string& value);

/// Describes the processing level of a collision mesh and, when simplified, the
/// provenance/error bound of that simplification.
struct MeshCollisionQuality {
    MeshQualityMode mode = MeshQualityMode::Original;  ///< Processing level applied to the mesh.
    std::optional<std::size_t> triangleCount;  ///< Triangle count of the mesh, when known.
    std::optional<std::string> simplifiedFrom;  ///< Id/path of the source mesh this was simplified from.
    std::optional<double> maxSimplificationError_m;  ///< Upper bound on simplification deviation, in meters.
};

/// A single triangle-mesh collision volume attached to a robot link.
struct MeshCollisionGeometry {
    std::string id;  ///< Unique identifier of this mesh geometry within its profile.
    std::string linkId;  ///< Id of the robot link this mesh is attached to.
    std::string path;  ///< Filesystem path to the mesh file (made absolute by loadFile()).
    MeshFileFormat format = MeshFileFormat::Stl;  ///< File format of the mesh at `path`.
    MeshSourceUnits sourceUnits = MeshSourceUnits::Meters;  ///< Units of the mesh file's raw vertex coordinates.
    double scaleToMeters = 1.0;  ///< Multiplier applied to raw vertex coordinates to convert them to meters.
    Pose meshToLink = Pose::identity();  ///< Transform from mesh frame to the link frame.
    double margin_m = 0.0;  ///< Extra clearance added around the mesh, in meters.
    bool enabled = true;  ///< Whether this mesh participates in collision checking.
    MeshCollisionQuality quality;  ///< Processing/simplification metadata for this mesh.
};

/// Full mesh-based collision profile for a robot model: its mesh geometries, preferred
/// collision backends, excluded pairs, and supporting metadata/sources loaded from the
/// `robot-kinematics-collision-mesh/v1` JSON schema (see MeshCollisionProfileJsonLoader).
struct MeshCollisionProfile {
    std::string id;  ///< Unique identifier of this mesh collision profile.
    std::string robotModel;  ///< Robot model this profile applies to.
    std::vector<MeshCollisionBackendKind> backendPreference;  ///< Ordered list of acceptable collision backends.
    std::vector<MeshCollisionGeometry> meshes;  ///< Mesh collision volumes for the robot's links.
    std::vector<DisabledCollisionPair> disabledPairs;  ///< Mesh-id pairs excluded from collision checks.
    std::vector<SourceReference> sources;  ///< Documentation/reference sources backing this profile's data.
    std::map<std::string, std::string> metadata;  ///< Free-form string metadata carried through from the JSON.
};

} // namespace RobotKinematics
