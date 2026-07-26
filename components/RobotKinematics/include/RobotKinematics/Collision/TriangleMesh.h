#pragma once

#include <Eigen/Core>

#include <array>
#include <cstddef>
#include <vector>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// STL encoding a mesh was parsed from (or is to be written as).
enum class StlFileFormat {
    Ascii,   ///< Human-readable "solid ... facet ... vertex ..." STL text format.
    Binary   ///< Binary STL: 80-byte header, triangle count, then 50-byte-per-triangle records.
};

/// One triangle of a TriangleMesh, given as three indices into its vertices_m array.
struct TriangleFace {
    std::size_t a = 0;  ///< Index of the triangle's first vertex.
    std::size_t b = 0;  ///< Index of the triangle's second vertex.
    std::size_t c = 0;  ///< Index of the triangle's third vertex.
};

/// Bounding-box and scale metadata computed for a loaded TriangleMesh.
struct TriangleMeshStatistics {
    std::size_t vertexCount = 0;    ///< Number of entries in the mesh's vertices_m array.
    std::size_t triangleCount = 0;  ///< Number of entries in the mesh's faces array.
    std::array<double, 3> minimumBounds_m = {0.0, 0.0, 0.0};  ///< Per-axis minimum vertex coordinate, in meters.
    std::array<double, 3> maximumBounds_m = {0.0, 0.0, 0.0};  ///< Per-axis maximum vertex coordinate, in meters.
    double scaleToMeters = 1.0;  ///< Scale factor that was applied to raw file coordinates to produce meters.
};

/// Triangle mesh loaded from an STL file: deduplicated-free vertex list (each triangle
/// owns its own three vertices), face index list, and derived statistics.
struct TriangleMesh {
    StlFileFormat sourceFormat = StlFileFormat::Ascii;  ///< STL format the mesh was parsed from.
    std::vector<Eigen::Vector3d> vertices_m;  ///< Mesh vertex positions, in meters.
    std::vector<TriangleFace> faces;  ///< Triangle index triples into vertices_m.
    TriangleMeshStatistics statistics;  ///< Bounding-box/scale statistics computed from vertices_m.
};

} // namespace RobotKinematics
