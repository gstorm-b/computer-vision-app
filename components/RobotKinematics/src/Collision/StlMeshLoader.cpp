#include <RobotKinematics/Collision/StlMeshLoader.h>

#include <QByteArray>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

/// Namespace for the RobotKinematics library: robot kinematics and collision-geometry types and
/// utilities, including the STL mesh loading facilities implemented in this file.
namespace RobotKinematics {

/// Internal helpers used only by this translation unit to parse binary/ASCII STL payloads into
/// a TriangleMesh (translation-unit-local linkage).
namespace {
/// Builds a failed Result<TriangleMesh> carrying KinematicsStatus::InvalidRequest and `message`.
Result<TriangleMesh> invalid(const std::string& message)
{
    return Result<TriangleMesh>::failure(KinematicsStatus::InvalidRequest, message);
}

/// Reads a little-endian 32-bit unsigned integer out of `bytes` at byte offset `offset`.
/// @note Does not bounds-check `offset`; callers must ensure at least 4 bytes remain.
std::uint32_t readLeUInt32(const QByteArray& bytes, int offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.constData() + offset, sizeof(value));
    return value;
}

/// Reads a little-endian 32-bit IEEE-754 float out of `bytes` at byte offset `offset`.
/// @note Does not bounds-check `offset`; callers must ensure at least 4 bytes remain.
float readLeFloat(const QByteArray& bytes, int offset)
{
    float value = 0.0f;
    std::memcpy(&value, bytes.constData() + offset, sizeof(value));
    return value;
}

/// True when all three components of `vertex` are finite (neither NaN nor infinite).
bool isFinite(const Eigen::Vector3d& vertex)
{
    return std::isfinite(vertex.x()) && std::isfinite(vertex.y()) && std::isfinite(vertex.z());
}

/// True when triangle (a, b, c) is degenerate: the cross product of its two edge vectors from
/// `a` has norm at or below the 1e-12 area threshold (near-zero area / collinear vertices).
bool isDegenerateTriangle(const Eigen::Vector3d& a,
                          const Eigen::Vector3d& b,
                          const Eigen::Vector3d& c)
{
    return ((b - a).cross(c - a)).norm() <= 1e-12;
}

/// Validates triangle (a, b, c) and, if acceptable, appends its vertices and face to `mesh`.
/// Non-finite vertices are always rejected. A degenerate (near-zero-area) triangle is rejected
/// only when `options.rejectDegenerateTriangles` is set; otherwise it is silently dropped
/// without being appended, which still counts as success.
/// @param mesh mesh the triangle's vertices/face are appended to when accepted
/// @param error set to a description of the problem when this returns false
/// @return false only for non-finite vertices or a rejected degenerate triangle; true otherwise,
/// including the silently-dropped degenerate case
bool appendTriangle(TriangleMesh& mesh,
                    const Eigen::Vector3d& a,
                    const Eigen::Vector3d& b,
                    const Eigen::Vector3d& c,
                    const StlMeshLoadOptions& options,
                    std::string& error)
{
    if (!isFinite(a) || !isFinite(b) || !isFinite(c)) {
        error = "STL mesh contains non-finite vertex values";
        return false;
    }
    if (isDegenerateTriangle(a, b, c)) {
        if (options.rejectDegenerateTriangles) {
            error = "STL mesh contains a degenerate triangle";
            return false;
        }
        return true;
    }

    const std::size_t baseIndex = mesh.vertices_m.size();
    mesh.vertices_m.push_back(a);
    mesh.vertices_m.push_back(b);
    mesh.vertices_m.push_back(c);
    mesh.faces.push_back(TriangleFace{baseIndex, baseIndex + 1, baseIndex + 2});
    return true;
}

/// Computes vertex/triangle counts and the axis-aligned bounding box from `mesh.vertices_m`,
/// and writes them together with `scaleToMeters` into `mesh.statistics`.
/// @note Assumes `mesh.vertices_m` is non-empty; on an empty mesh the min/max bounds are left
/// at their numeric_limits::max()/lowest() sentinel values.
void finalizeStatistics(TriangleMesh& mesh, const double scaleToMeters)
{
    Eigen::Vector3d minimum = Eigen::Vector3d::Constant(std::numeric_limits<double>::max());
    Eigen::Vector3d maximum = Eigen::Vector3d::Constant(std::numeric_limits<double>::lowest());

    for (const Eigen::Vector3d& vertex : mesh.vertices_m) {
        minimum = minimum.cwiseMin(vertex);
        maximum = maximum.cwiseMax(vertex);
    }

    mesh.statistics.vertexCount = mesh.vertices_m.size();
    mesh.statistics.triangleCount = mesh.faces.size();
    mesh.statistics.minimumBounds_m = {minimum.x(), minimum.y(), minimum.z()};
    mesh.statistics.maximumBounds_m = {maximum.x(), maximum.y(), maximum.z()};
    mesh.statistics.scaleToMeters = scaleToMeters;
}

/// Parses `bytes` as a binary STL file: validates the 84-byte header plus fixed 50-bytes-per-
/// triangle payload against the declared triangle count, then decodes each triangle's three
/// vertices (skipping the per-triangle normal vector and the 2-byte attribute field), scaling
/// coordinates by `options.scaleToMeters`.
/// @return the parsed mesh, or an InvalidRequest failure if the payload is too small, its size
/// doesn't match the declared triangle count, a triangle fails validation (see appendTriangle),
/// or no triangles remain
Result<TriangleMesh> parseBinary(const QByteArray& bytes, const StlMeshLoadOptions& options)
{
    if (bytes.size() < 84) {
        return invalid("STL payload is too small to be a binary STL");
    }

    const std::uint32_t triangleCount = readLeUInt32(bytes, 80);
    const std::uint64_t expectedSize = 84ull + static_cast<std::uint64_t>(triangleCount) * 50ull;
    if (expectedSize != static_cast<std::uint64_t>(bytes.size())) {
        return invalid("binary STL size does not match triangle count");
    }

    TriangleMesh mesh;
    mesh.sourceFormat = StlFileFormat::Binary;
    mesh.vertices_m.reserve(static_cast<std::size_t>(triangleCount) * 3);
    mesh.faces.reserve(static_cast<std::size_t>(triangleCount));

    std::string error;
    int offset = 84;
    for (std::uint32_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
        offset += 12;

        const Eigen::Vector3d a(readLeFloat(bytes, offset) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 4) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 8) * options.scaleToMeters);
        offset += 12;
        const Eigen::Vector3d b(readLeFloat(bytes, offset) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 4) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 8) * options.scaleToMeters);
        offset += 12;
        const Eigen::Vector3d c(readLeFloat(bytes, offset) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 4) * options.scaleToMeters,
                                readLeFloat(bytes, offset + 8) * options.scaleToMeters);
        offset += 12;

        if (!appendTriangle(mesh, a, b, c, options, error)) {
            return invalid(error);
        }

        offset += 2;
    }

    if (mesh.faces.empty()) {
        return invalid("binary STL contains no triangles");
    }

    finalizeStatistics(mesh, options.scaleToMeters);
    return Result<TriangleMesh>::success(mesh);
}

/// Parses `bytes` as an ASCII STL file: decodes the bytes as UTF-8, then scans every trimmed
/// line starting with "vertex " for three whitespace-separated numeric coordinates (scaled by
/// `options.scaleToMeters`); every 3 consecutive vertices form one triangle. Other STL keywords
/// (facet/normal/loop/endloop/endfacet) are ignored rather than validated.
/// @return the parsed mesh, or an InvalidRequest failure if a vertex line is malformed, the
/// vertex count is not a multiple of 3, a triangle fails validation (see appendTriangle), or no
/// triangles remain after filtering degenerate faces
Result<TriangleMesh> parseAscii(const QByteArray& bytes, const StlMeshLoadOptions& options)
{
    const QString text = QString::fromUtf8(bytes);
    const QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    std::vector<Eigen::Vector3d> vertices;
    vertices.reserve(static_cast<std::size_t>(lines.size()));

    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(QStringLiteral("vertex "))) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() != 4) {
            return invalid("ASCII STL vertex line must contain three numeric coordinates");
        }

        bool okX = false;
        bool okY = false;
        bool okZ = false;
        const double x = parts[1].toDouble(&okX);
        const double y = parts[2].toDouble(&okY);
        const double z = parts[3].toDouble(&okZ);
        if (!okX || !okY || !okZ) {
            return invalid("ASCII STL contains a non-numeric vertex coordinate");
        }

        vertices.emplace_back(x * options.scaleToMeters,
                              y * options.scaleToMeters,
                              z * options.scaleToMeters);
    }

    if (vertices.empty() || (vertices.size() % 3) != 0) {
        return invalid("ASCII STL does not contain a whole number of triangles");
    }

    TriangleMesh mesh;
    mesh.sourceFormat = StlFileFormat::Ascii;
    mesh.vertices_m.reserve(vertices.size());
    mesh.faces.reserve(vertices.size() / 3);

    std::string error;
    for (std::size_t i = 0; i < vertices.size(); i += 3) {
        if (!appendTriangle(mesh, vertices[i], vertices[i + 1], vertices[i + 2], options, error)) {
            return invalid(error);
        }
    }

    if (mesh.faces.empty()) {
        return invalid("ASCII STL contains no triangles after filtering degenerate faces");
    }

    finalizeStatistics(mesh, options.scaleToMeters);
    return Result<TriangleMesh>::success(mesh);
}
}

/// Loads an STL mesh from the file at `path`, auto-detecting the format: always attempts binary
/// STL parsing first, falling back to ASCII STL parsing only if the binary parse fails. Fails
/// with InvalidRequest if `options.scaleToMeters` is not positive, the file cannot be opened for
/// reading, or both binary and ASCII parsing fail (the ASCII parser's failure result is returned
/// in that case).
Result<TriangleMesh> StlMeshLoader::loadFile(const std::string& path,
                                             const StlMeshLoadOptions& options)
{
    if (options.scaleToMeters <= 0.0) {
        return invalid("STL mesh scaleToMeters must be positive");
    }

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return invalid("STL mesh file could not be opened");
    }

    const QByteArray bytes = file.readAll();

    Result<TriangleMesh> result = parseBinary(bytes, options);
    if (result.ok()) {
        return result;
    }

    const Result<TriangleMesh> asciiResult = parseAscii(bytes, options);
    if (asciiResult.ok()) {
        return asciiResult;
    }

    return asciiResult;
}

} // namespace RobotKinematics
