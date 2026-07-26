#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/StlMeshLoader.h>
#include <RobotKinematics/Collision/TriangleMesh.h>
#include <RobotKinematics/Core/Pose.h>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace RobotKinematics;

namespace {

/// Integer grid-cell coordinate used as the std::map key when clustering mesh vertices
/// into a uniform voxel grid in simplifyMeshVoxelGrid().
struct VoxelKey {
    int x = 0;  ///< Grid cell index along X.
    int y = 0;  ///< Grid cell index along Y.
    int z = 0;  ///< Grid cell index along Z.
    /// Lexicographic ordering on (x, y, z), required for use as a std::map key.
    bool operator<(const VoxelKey& other) const
    {
        return std::tie(x, y, z) < std::tie(other.x, other.y, other.z);
    }
};

/// Triangle-count and error metrics produced by simplifyMeshVoxelGrid() for one mesh.
struct SimplificationStats {
    std::size_t originalTriangles = 0;    ///< Triangle count of the input mesh.
    std::size_t simplifiedTriangles = 0;  ///< Triangle count remaining after decimation.
    double maxError_m = 0.0;              ///< Largest vertex-to-centroid displacement, in meters.
    double meanError_m = 0.0;             ///< Mean vertex-to-centroid displacement, in meters.
};

/// Voxel-grid vertex clustering decimation. Bounded, deterministic, and dependency-free.
/// Vertices are clustered into a uniform NxNxN grid spanning the mesh AABB; each grid
/// cell is collapsed to the centroid of its incident vertices. Triangles whose
/// remapped vertices collapse onto fewer than three distinct centroids are dropped.
///
/// `voxelCount` controls the grid resolution along the longest axis. A higher value
/// preserves more detail at the cost of less reduction.
/// @param input source mesh to simplify
/// @param voxelCount grid resolution along the longest AABB axis; must be >= 1
/// @param stats output; filled with original/simplified triangle counts and max/mean
///        vertex-to-centroid displacement error
/// @return the simplified mesh, or a default-constructed empty TriangleMesh if `input`
///         has no vertices/faces or `voxelCount` < 1
TriangleMesh simplifyMeshVoxelGrid(const TriangleMesh& input,
                                   int voxelCount,
                                   SimplificationStats& stats)
{
    stats.originalTriangles = input.faces.size();
    stats.maxError_m = 0.0;
    stats.meanError_m = 0.0;
    stats.simplifiedTriangles = 0;

    if (input.vertices_m.empty() || input.faces.empty() || voxelCount < 1) {
        return TriangleMesh{};
    }

    Eigen::Vector3d minBound = Eigen::Vector3d::Constant(std::numeric_limits<double>::max());
    Eigen::Vector3d maxBound = Eigen::Vector3d::Constant(std::numeric_limits<double>::lowest());
    for (const Eigen::Vector3d& v : input.vertices_m) {
        minBound = minBound.cwiseMin(v);
        maxBound = maxBound.cwiseMax(v);
    }
    const Eigen::Vector3d extent = maxBound - minBound;
    const double longestExtent = std::max(extent.maxCoeff(), 1e-9);
    const double cellSize = longestExtent / static_cast<double>(voxelCount);

    std::map<VoxelKey, std::vector<std::size_t>> voxelToVertices;
    for (std::size_t i = 0; i < input.vertices_m.size(); ++i) {
        const Eigen::Vector3d& v = input.vertices_m[i];
        const VoxelKey key{
            static_cast<int>(std::floor((v.x() - minBound.x()) / cellSize)),
            static_cast<int>(std::floor((v.y() - minBound.y()) / cellSize)),
            static_cast<int>(std::floor((v.z() - minBound.z()) / cellSize)),
        };
        voxelToVertices[key].push_back(i);
    }

    std::vector<Eigen::Vector3d> centroids;
    centroids.reserve(voxelToVertices.size());
    std::vector<std::size_t> remap(input.vertices_m.size(), 0);
    for (const auto& entry : voxelToVertices) {
        Eigen::Vector3d sum = Eigen::Vector3d::Zero();
        for (std::size_t idx : entry.second) {
            sum += input.vertices_m[idx];
        }
        const Eigen::Vector3d centroid = sum / static_cast<double>(entry.second.size());
        const std::size_t centroidIndex = centroids.size();
        centroids.push_back(centroid);
        for (std::size_t idx : entry.second) {
            remap[idx] = centroidIndex;
        }
    }

    double errorSum = 0.0;
    for (std::size_t i = 0; i < input.vertices_m.size(); ++i) {
        const double e = (input.vertices_m[i] - centroids[remap[i]]).norm();
        if (e > stats.maxError_m) {
            stats.maxError_m = e;
        }
        errorSum += e;
    }
    if (!input.vertices_m.empty()) {
        stats.meanError_m = errorSum / static_cast<double>(input.vertices_m.size());
    }

    TriangleMesh output;
    output.sourceFormat = StlFileFormat::Binary;
    output.vertices_m = centroids;
    output.faces.reserve(input.faces.size());

    std::map<std::tuple<std::size_t, std::size_t, std::size_t>, bool> seen;
    for (const TriangleFace& face : input.faces) {
        const std::size_t a = remap[face.a];
        const std::size_t b = remap[face.b];
        const std::size_t c = remap[face.c];
        if (a == b || b == c || a == c) {
            continue;
        }

        std::array<std::size_t, 3> sortedIndices = {a, b, c};
        std::sort(sortedIndices.begin(), sortedIndices.end());
        const auto canonical = std::make_tuple(sortedIndices[0], sortedIndices[1], sortedIndices[2]);
        if (seen.find(canonical) != seen.end()) {
            continue;
        }
        seen[canonical] = true;

        output.faces.push_back(TriangleFace{a, b, c});
    }

    Eigen::Vector3d outMin = Eigen::Vector3d::Constant(std::numeric_limits<double>::max());
    Eigen::Vector3d outMax = Eigen::Vector3d::Constant(std::numeric_limits<double>::lowest());
    for (const Eigen::Vector3d& v : output.vertices_m) {
        outMin = outMin.cwiseMin(v);
        outMax = outMax.cwiseMax(v);
    }
    output.statistics.vertexCount = output.vertices_m.size();
    output.statistics.triangleCount = output.faces.size();
    output.statistics.minimumBounds_m = {outMin.x(), outMin.y(), outMin.z()};
    output.statistics.maximumBounds_m = {outMax.x(), outMax.y(), outMax.z()};
    output.statistics.scaleToMeters = 1.0;

    stats.simplifiedTriangles = output.faces.size();
    return output;
}

/// Appends `value` to `bytes` as 4 raw little-endian bytes (memcpy of the native
/// representation; assumes a little-endian host).
void writeLeUInt32(QByteArray& bytes, std::uint32_t value)
{
    char raw[4];
    std::memcpy(raw, &value, sizeof(value));
    bytes.append(raw, static_cast<int>(sizeof(value)));
}

/// Appends `value` to `bytes` as 4 raw little-endian bytes (memcpy of the native
/// IEEE-754 representation; assumes a little-endian host).
void writeLeFloat(QByteArray& bytes, float value)
{
    char raw[4];
    std::memcpy(raw, &value, sizeof(value));
    bytes.append(raw, static_cast<int>(sizeof(value)));
}

/// Serializes `mesh` to `path` as a binary STL file: an 80-byte header (identifying this
/// tool), the triangle count, then per-triangle facet normal + 3 vertices + 2 padding
/// bytes, all little-endian, per the standard binary STL layout.
/// @param error output; set to a human-readable message if writing fails
/// @return true on success, false if the file could not be opened or the write was short
bool writeBinaryStl(const TriangleMesh& mesh, const QString& path, QString& error)
{
    QByteArray bytes(80, '\0');
    const QByteArray header = QStringLiteral("simplified-by-robotkinematics-voxel-grid").toUtf8();
    const qsizetype headerCopySize = std::min<qsizetype>(header.size(), 79);
    std::memcpy(bytes.data(), header.constData(), static_cast<std::size_t>(headerCopySize));

    writeLeUInt32(bytes, static_cast<std::uint32_t>(mesh.faces.size()));

    for (const TriangleFace& face : mesh.faces) {
        const Eigen::Vector3d& a = mesh.vertices_m[face.a];
        const Eigen::Vector3d& b = mesh.vertices_m[face.b];
        const Eigen::Vector3d& c = mesh.vertices_m[face.c];

        Eigen::Vector3d normal = (b - a).cross(c - a);
        const double normalLength = normal.norm();
        if (normalLength > 0.0) {
            normal /= normalLength;
        } else {
            normal.setZero();
        }

        writeLeFloat(bytes, static_cast<float>(normal.x()));
        writeLeFloat(bytes, static_cast<float>(normal.y()));
        writeLeFloat(bytes, static_cast<float>(normal.z()));

        const Eigen::Vector3d* points[3] = {&a, &b, &c};
        for (const Eigen::Vector3d* point : points) {
            writeLeFloat(bytes, static_cast<float>(point->x()));
            writeLeFloat(bytes, static_cast<float>(point->y()));
            writeLeFloat(bytes, static_cast<float>(point->z()));
        }
        bytes.append('\0');
        bytes.append('\0');
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("could not open output file for writing: ") + path;
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("short write while saving binary STL: ") + path;
        return false;
    }
    file.close();
    return true;
}

/// Converts a 3D vector to a `[x, y, z]` JSON array.
QJsonArray vec3ToJson(const Eigen::Vector3d& value)
{
    QJsonArray array;
    array.append(value.x());
    array.append(value.y());
    array.append(value.z());
    return array;
}

/// Converts `pose` to the profile schema's pose object: `xyz_m` translation and
/// `rpy_rad` (roll, pitch, yaw) extracted from the canonical ZYX Euler decomposition.
QJsonObject poseToJson(const Pose& pose)
{
    const Eigen::Isometry3d& iso = pose.isometry();
    const Eigen::Vector3d translation = iso.translation();
    const Eigen::Vector3d eulerZyx = iso.linear().canonicalEulerAngles(2, 1, 0);
    // Pose::fromXYZRPY_m_rad applies R = R_z(yaw) * R_y(pitch) * R_x(roll), so canonical
    // ZYX Euler returns (yaw, pitch, roll); rpy_rad in the schema is (roll, pitch, yaw).
    QJsonObject object;
    object.insert("xyz_m", vec3ToJson(translation));
    QJsonArray rpy;
    rpy.append(eulerZyx[2]);
    rpy.append(eulerZyx[1]);
    rpy.append(eulerZyx[0]);
    object.insert("rpy_rad", rpy);
    return object;
}

/// Returns the schema unit string for `units` ("m" or "mm"); falls back to "m" for an
/// unrecognized enumerator.
QString meshSourceUnitsToString(MeshSourceUnits units)
{
    switch (units) {
    case MeshSourceUnits::Meters:
        return QStringLiteral("m");
    case MeshSourceUnits::Millimeters:
        return QStringLiteral("mm");
    }
    return QStringLiteral("m");
}

/// Returns the schema format string for `format` (currently only "stl"); falls back to
/// "stl" for an unrecognized enumerator.
QString meshFormatToString(MeshFileFormat format)
{
    switch (format) {
    case MeshFileFormat::Stl:
        return QStringLiteral("stl");
    }
    return QStringLiteral("stl");
}

/// Converts an ordered list of collision-backend preferences to a JSON array of their
/// string names (via toString()).
QJsonArray backendPreferenceToJson(const std::vector<MeshCollisionBackendKind>& backends)
{
    QJsonArray array;
    for (MeshCollisionBackendKind backend : backends) {
        array.append(QString::fromStdString(toString(backend)));
    }
    return array;
}

/// Converts a SourceReference (type, title, reference, and the list of profile aspects
/// it applies to) to its JSON object representation.
QJsonObject sourceToJson(const SourceReference& source)
{
    QJsonObject object;
    object.insert("type", QString::fromStdString(source.type));
    object.insert("title", QString::fromStdString(source.title));
    object.insert("reference", QString::fromStdString(source.reference));
    QJsonArray appliesTo;
    for (const std::string& applies : source.appliesTo) {
        appliesTo.append(QString::fromStdString(applies));
    }
    object.insert("appliesTo", appliesTo);
    return object;
}

/// Loads the mesh collision profile at `inputProfilePath`, voxel-grid-simplifies every
/// referenced mesh, writes each simplified mesh as a binary STL under `meshOutputDir`,
/// pads each mesh's margin to compensate for simplification error, and writes the
/// resulting profile (with provenance recorded in `sources`/`metadata`) to
/// `outputProfilePath`.
/// @param voxelCount grid resolution along the longest AABB axis, forwarded to
///        simplifyMeshVoxelGrid()
/// @param safetyFactor pair-level margin scaling factor; per-mesh padding is
///        `0.5 * safetyFactor * max_error_m`
/// @return 0 on success; 1 if the input profile fails to load, the output directory
///         cannot be created, any referenced mesh fails to load, or any output file
///         fails to write
int simplifyProfile(const QString& inputProfilePath,
                    const QString& outputProfilePath,
                    const QString& meshOutputDir,
                    int voxelCount,
                    double safetyFactor)
{
    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(inputProfilePath.toStdString());
    if (!loaded.ok()) {
        std::cerr << "[ERROR] failed to load input profile: " << loaded.message << std::endl;
        return 1;
    }

    const MeshCollisionProfile& profile = loaded.value;
    QDir outputDir(meshOutputDir);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        std::cerr << "[ERROR] could not create mesh output dir: "
                  << meshOutputDir.toStdString() << std::endl;
        return 1;
    }

    const QFileInfo outputProfileInfo(outputProfilePath);
    const QDir outputProfileDir = outputProfileInfo.absoluteDir();

    QJsonArray meshesArray;
    for (const MeshCollisionGeometry& mesh : profile.meshes) {
        const Result<TriangleMesh> original =
            StlMeshLoader::loadFile(mesh.path, StlMeshLoadOptions{mesh.scaleToMeters, false});
        if (!original.ok()) {
            std::cerr << "[ERROR] failed to load mesh '" << mesh.id << "': "
                      << original.message << std::endl;
            return 1;
        }

        SimplificationStats stats;
        const TriangleMesh simplified = simplifyMeshVoxelGrid(original.value, voxelCount, stats);

        const QString outputMeshName =
            QString::fromStdString(mesh.id) + QStringLiteral("_voxel") +
            QString::number(voxelCount) + QStringLiteral(".stl");
        const QString outputMeshAbsolutePath = outputDir.absoluteFilePath(outputMeshName);
        QString writeError;
        if (!writeBinaryStl(simplified, outputMeshAbsolutePath, writeError)) {
            std::cerr << "[ERROR] " << writeError.toStdString() << std::endl;
            return 1;
        }

        const QString outputMeshRelativePath =
            outputProfileDir.relativeFilePath(outputMeshAbsolutePath);

        // Pad margin so the simplified mesh does not silently shrink collision coverage.
        //
        // The pair margin in CoalMeshCollisionBackend sums both meshes' margins, so each
        // mesh contributes half of the target pair threshold. We treat
        // `safety_factor=1.0` as the "typical case" where the simplified-pair threshold
        // equals the larger mesh's `max_error_m` (assuming symmetric errors). Set
        // `safety_factor=2.0` for the strict worst-case bound (pair threshold equal to
        // the sum of both meshes' errors). `safety_factor=0.0` skips padding entirely.
        const double paddedMargin = mesh.margin_m + 0.5 * safetyFactor * stats.maxError_m;

        QJsonObject meshObject;
        meshObject.insert("id", QString::fromStdString(mesh.id));
        meshObject.insert("link", QString::fromStdString(mesh.linkId));
        meshObject.insert("path", outputMeshRelativePath);
        meshObject.insert("format", meshFormatToString(mesh.format));
        // Simplified STL is emitted in meters so downstream consumers do not need to
        // re-scale or re-document units.
        meshObject.insert("sourceUnits", QStringLiteral("m"));
        meshObject.insert("scaleToMeters", 1.0);
        meshObject.insert("meshToLink", poseToJson(mesh.meshToLink));
        meshObject.insert("margin_m", paddedMargin);
        meshObject.insert("enabled", mesh.enabled);

        QJsonObject qualityObject;
        qualityObject.insert("mode", QStringLiteral("simplified"));
        qualityObject.insert("triangleCount", static_cast<qint64>(stats.simplifiedTriangles));
        qualityObject.insert("simplifiedFrom", QString::fromStdString(mesh.path));
        qualityObject.insert("maxSimplificationError_m", stats.maxError_m);
        meshObject.insert("quality", qualityObject);

        meshesArray.append(meshObject);

        std::cout << "mesh=" << mesh.id
                  << " original_tris=" << stats.originalTriangles
                  << " simplified_tris=" << stats.simplifiedTriangles
                  << " max_error_m=" << stats.maxError_m
                  << " mean_error_m=" << stats.meanError_m
                  << " padded_margin_m=" << paddedMargin
                  << std::endl;
    }

    QJsonArray disabledPairsArray;
    for (const DisabledCollisionPair& pair : profile.disabledPairs) {
        QJsonObject object;
        object.insert("a", QString::fromStdString(pair.geometryA));
        object.insert("b", QString::fromStdString(pair.geometryB));
        object.insert("reason", QString::fromStdString(pair.reason));
        disabledPairsArray.append(object);
    }

    QJsonArray sourcesArray;
    for (const SourceReference& source : profile.sources) {
        sourcesArray.append(sourceToJson(source));
    }
    QJsonObject simplificationSource;
    simplificationSource.insert("type", QStringLiteral("derived_simplification"));
    simplificationSource.insert("title",
                                QStringLiteral("Generated by tools/mesh_simplification via "
                                               "voxel-grid vertex clustering"));
    simplificationSource.insert("reference", QFileInfo(inputProfilePath).fileName());
    QJsonArray appliesToArray;
    appliesToArray.append(QStringLiteral("mesh_collision_geometry"));
    appliesToArray.append(QStringLiteral("mesh_margin_compensation"));
    simplificationSource.insert("appliesTo", appliesToArray);
    sourcesArray.append(simplificationSource);

    QJsonObject metadataObject;
    for (const auto& entry : profile.metadata) {
        metadataObject.insert(QString::fromStdString(entry.first),
                              QString::fromStdString(entry.second));
    }
    metadataObject.insert("simplification_method", QStringLiteral("voxel_grid_vertex_clustering"));
    metadataObject.insert("simplification_voxel_count", QString::number(voxelCount));
    metadataObject.insert("simplification_safety_factor",
                          QString::number(safetyFactor, 'f', 6));

    QJsonObject profileObject;
    profileObject.insert("id", QString::fromStdString(profile.id) + QStringLiteral("_simplified"));
    profileObject.insert("robotModel", QString::fromStdString(profile.robotModel));
    QJsonObject unitsObject;
    unitsObject.insert("length", QStringLiteral("m"));
    unitsObject.insert("angle", QStringLiteral("rad"));
    profileObject.insert("units", unitsObject);
    profileObject.insert("backendPreference", backendPreferenceToJson(profile.backendPreference));

    QJsonObject root;
    root.insert("schema", QStringLiteral("robot-kinematics-collision-mesh/v1"));
    root.insert("profile", profileObject);
    root.insert("meshes", meshesArray);
    root.insert("disabledPairs", disabledPairsArray);
    root.insert("sources", sourcesArray);
    root.insert("metadata", metadataObject);

    const QByteArray serialized = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QFile outputFile(outputProfilePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "[ERROR] could not write output profile: "
                  << outputProfilePath.toStdString() << std::endl;
        return 1;
    }
    if (outputFile.write(serialized) != serialized.size()) {
        std::cerr << "[ERROR] short write on output profile" << std::endl;
        return 1;
    }

    std::cout << "[OK] wrote simplified profile to "
              << outputProfilePath.toStdString() << std::endl;
    return 0;
}

} // namespace

/// CLI entry point: parses `--input-profile`, `--output-profile`, `--mesh-output-dir`,
/// `--voxel-count` (default 40), and `--safety-factor` (default 1.0), validates them,
/// and dispatches to simplifyProfile().
/// @return 0 on success; 1 on missing/invalid arguments or if simplifyProfile() fails
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "RobotKinematics mesh simplification tool. Reads a mesh collision profile, "
        "generates voxel-grid decimated STLs and a new mesh profile that records the "
        "simplification source and margin compensation.");
    parser.addHelpOption();

    const QCommandLineOption inputProfileOption(
        QStringList{"input-profile"},
        "Path to the input mesh collision profile JSON.",
        "path");
    const QCommandLineOption outputProfileOption(
        QStringList{"output-profile"},
        "Path to write the generated simplified mesh profile JSON.",
        "path");
    const QCommandLineOption meshOutputDirOption(
        QStringList{"mesh-output-dir"},
        "Directory to write simplified binary STL meshes.",
        "path");
    const QCommandLineOption voxelCountOption(
        QStringList{"voxel-count"},
        "Voxel grid resolution along the longest mesh axis (default 40).",
        "count",
        "40");
    const QCommandLineOption safetyFactorOption(
        QStringList{"safety-factor"},
        "Pair-level margin scaling for simplification error. Per-mesh padding is "
        "(0.5 * safety_factor * max_error_m), so the runtime backend sees a pair "
        "margin of approximately (safety_factor * max_error_m) for symmetric meshes. "
        "Use 0.0 for no padding, 1.0 (default) for typical pair margin equal to the "
        "larger mesh error, or 2.0 for the strict worst-case sum-of-errors bound.",
        "value",
        "1.0");

    parser.addOption(inputProfileOption);
    parser.addOption(outputProfileOption);
    parser.addOption(meshOutputDirOption);
    parser.addOption(voxelCountOption);
    parser.addOption(safetyFactorOption);
    parser.process(app);

    if (!parser.isSet(inputProfileOption) || !parser.isSet(outputProfileOption) ||
        !parser.isSet(meshOutputDirOption)) {
        std::cerr << "[ERROR] input-profile, output-profile, and mesh-output-dir are required."
                  << std::endl;
        parser.showHelp(1);
        return 1;
    }

    bool voxelOk = false;
    const int voxelCount = parser.value(voxelCountOption).toInt(&voxelOk);
    if (!voxelOk || voxelCount < 1) {
        std::cerr << "[ERROR] voxel-count must be a positive integer." << std::endl;
        return 1;
    }

    bool safetyOk = false;
    const double safetyFactor = parser.value(safetyFactorOption).toDouble(&safetyOk);
    if (!safetyOk || !(safetyFactor >= 0.0)) {
        std::cerr << "[ERROR] safety-factor must be a non-negative number." << std::endl;
        return 1;
    }

    return simplifyProfile(parser.value(inputProfileOption),
                           parser.value(outputProfileOption),
                           parser.value(meshOutputDirOption),
                           voxelCount,
                           safetyFactor);
}
