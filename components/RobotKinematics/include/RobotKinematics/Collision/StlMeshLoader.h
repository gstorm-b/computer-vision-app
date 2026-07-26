#pragma once

#include <RobotKinematics/Collision/TriangleMesh.h>
#include <RobotKinematics/Core/Result.h>

#include <string>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Options controlling how StlMeshLoader::loadFile interprets a raw STL file.
struct StlMeshLoadOptions {
    double scaleToMeters = 1.0;      ///< Multiplier applied to every parsed vertex coordinate to convert it to meters.
    bool rejectDegenerateTriangles = true;  ///< When true, a zero-area triangle (near-zero cross product) makes the whole load fail instead of being silently dropped.
};

/// Loads triangle meshes from STL files, auto-detecting binary vs. ASCII format.
class StlMeshLoader
{
public:
    /// Loads `path` as an STL mesh: first tries to parse it as binary STL, and if that
    /// fails, falls back to parsing it as ASCII STL. Validates header/size framing (binary)
    /// or vertex-line grouping (ASCII), and rejects non-finite vertices.
    /// @param path filesystem path to the .stl file to read
    /// @param options scale factor and degenerate-triangle handling applied while parsing
    /// @return success with the loaded TriangleMesh, or failure with InvalidRequest and a
    /// descriptive message (e.g. bad scale, unreadable file, malformed/empty mesh)
    static Result<TriangleMesh> loadFile(const std::string& path,
                                         const StlMeshLoadOptions& options = {});
};

} // namespace RobotKinematics
