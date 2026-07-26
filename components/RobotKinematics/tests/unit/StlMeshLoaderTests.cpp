#include "StlMeshLoaderTests.h"

#include <RobotKinematics/Collision/StlMeshLoader.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace RobotKinematics;

namespace {
/// Absolute tolerance used when comparing loaded mesh statistics against expected values.
constexpr double kTolerance = 1e-6;

/// Plain double-precision 3D point used to build test triangle data (independent of Eigen).
struct Vec3
{
    double x;
    double y;
    double z;
};

/// Three vertices of a test triangle, in the winding order used to build STL fixtures.
struct Triangle
{
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

/// Builds the 12 triangles of a closed 200x100x100 mm axis-aligned cuboid (millimeter units,
/// consistent winding), used as the shared fixture geometry for all loader tests.
std::vector<Triangle> cuboidTrianglesMm()
{
    const Vec3 p000{0.0, 0.0, 0.0};
    const Vec3 p100{200.0, 0.0, 0.0};
    const Vec3 p010{0.0, 100.0, 0.0};
    const Vec3 p110{200.0, 100.0, 0.0};
    const Vec3 p001{0.0, 0.0, 100.0};
    const Vec3 p101{200.0, 0.0, 100.0};
    const Vec3 p011{0.0, 100.0, 100.0};
    const Vec3 p111{200.0, 100.0, 100.0};

    return {
        {p000, p110, p100}, {p000, p010, p110},
        {p001, p101, p111}, {p001, p111, p011},
        {p000, p100, p101}, {p000, p101, p001},
        {p010, p011, p111}, {p010, p111, p110},
        {p000, p001, p011}, {p000, p011, p010},
        {p100, p110, p111}, {p100, p111, p101},
    };
}

/// Serializes cuboidTrianglesMm() as an ASCII STL file (millimeter coordinates, zeroed facet
/// normals) matching the "solid ... endsolid" text format.
QByteArray asciiStlBytes()
{
    QByteArray bytes;
    bytes += "solid cuboid\n";
    for (const Triangle& triangle : cuboidTrianglesMm()) {
        bytes += "  facet normal 0 0 0\n";
        bytes += "    outer loop\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.a.x, 'f', 6) + " " +
                 QByteArray::number(triangle.a.y, 'f', 6) + " " +
                 QByteArray::number(triangle.a.z, 'f', 6) + "\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.b.x, 'f', 6) + " " +
                 QByteArray::number(triangle.b.y, 'f', 6) + " " +
                 QByteArray::number(triangle.b.z, 'f', 6) + "\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.c.x, 'f', 6) + " " +
                 QByteArray::number(triangle.c.y, 'f', 6) + " " +
                 QByteArray::number(triangle.c.z, 'f', 6) + "\n";
        bytes += "    endloop\n";
        bytes += "  endfacet\n";
    }
    bytes += "endsolid cuboid\n";
    return bytes;
}

/// Appends `value` to `bytes` as 4 raw little-endian bytes (host-endianness memcpy; test fixtures
/// only run on little-endian targets).
void appendLeUInt32(QByteArray& bytes, std::uint32_t value)
{
    char raw[4];
    std::memcpy(raw, &value, sizeof(value));
    bytes.append(raw, static_cast<int>(sizeof(value)));
}

/// Appends `value` to `bytes` as 4 raw little-endian bytes (host-endianness memcpy; test fixtures
/// only run on little-endian targets).
void appendLeFloat(QByteArray& bytes, float value)
{
    char raw[4];
    std::memcpy(raw, &value, sizeof(value));
    bytes.append(raw, static_cast<int>(sizeof(value)));
}

/// Serializes cuboidTrianglesMm() as a binary STL file per the standard 80-byte header + u32
/// triangle count + (12 floats + 2-byte attribute) per-triangle layout, converting each
/// millimeter vertex coordinate to meters (multiplying by 0.001) so the fixture is already in
/// meters, unlike asciiStlBytes().
QByteArray binaryStlBytesMeters()
{
    const std::vector<Triangle> triangles = cuboidTrianglesMm();

    QByteArray bytes(80, '\0');
    const QByteArray header("cuboid-binary");
    std::copy(header.begin(), header.end(), bytes.begin());
    appendLeUInt32(bytes, static_cast<std::uint32_t>(triangles.size()));

    for (const Triangle& triangle : triangles) {
        appendLeFloat(bytes, 0.0f);
        appendLeFloat(bytes, 0.0f);
        appendLeFloat(bytes, 0.0f);

        const Vec3 vertices[] = {triangle.a, triangle.b, triangle.c};
        for (const Vec3& vertex : vertices) {
            appendLeFloat(bytes, static_cast<float>(vertex.x * 0.001));
            appendLeFloat(bytes, static_cast<float>(vertex.y * 0.001));
            appendLeFloat(bytes, static_cast<float>(vertex.z * 0.001));
        }

        bytes.append('\0');
        bytes.append('\0');
    }

    return bytes;
}

/// Builds an ASCII STL with a single collinear triangle (0,0,0 / 1,1,1 / 2,2,2), i.e. zero area,
/// used to exercise degenerate-triangle rejection/filtering.
QByteArray degenerateAsciiStlBytes()
{
    return QByteArray(
        "solid degenerate\n"
        "facet normal 0 0 0\n"
        "outer loop\n"
        "vertex 0 0 0\n"
        "vertex 1 1 1\n"
        "vertex 2 2 2\n"
        "endloop\n"
        "endfacet\n"
        "endsolid degenerate\n");
}

/// Writes `bytes` to `fileName` under the system temp directory, removing any pre-existing file
/// first.
/// @return the full path on success, or an empty string if the file could not be opened or the
///         write was short.
QString writeTempFile(const QString& fileName, const QByteArray& bytes)
{
    const QString path = QDir::temp().filePath(fileName);
    QFile::remove(path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    if (file.write(bytes) != bytes.size()) {
        return QString();
    }
    file.close();
    return path;
}
}

/// @see StlMeshLoaderTests::loadsAsciiStlAndNormalizesMillimetersToMeters
void StlMeshLoaderTests::loadsAsciiStlAndNormalizesMillimetersToMeters()
{
    const QString path = writeTempFile(QStringLiteral("rk_ascii_mesh_loader.stl"), asciiStlBytes());
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> result =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{0.001, true});

    QVERIFY2(result.ok(), result.message.c_str());
    QCOMPARE(static_cast<int>(result.value.sourceFormat), static_cast<int>(StlFileFormat::Ascii));
    QCOMPARE(static_cast<qulonglong>(result.value.statistics.triangleCount), qulonglong(12));
    QCOMPARE(static_cast<qulonglong>(result.value.statistics.vertexCount), qulonglong(36));
    QVERIFY(std::abs(result.value.statistics.minimumBounds_m[0] - 0.0) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[0] - 0.2) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[1] - 0.1) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[2] - 0.1) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.scaleToMeters - 0.001) <= kTolerance);
}

void StlMeshLoaderTests::loadsBinaryStlAndPreservesMeterScale()
{
    const QString path = writeTempFile(QStringLiteral("rk_binary_mesh_loader.stl"), binaryStlBytesMeters());
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> result =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, true});

    QVERIFY2(result.ok(), result.message.c_str());
    QCOMPARE(static_cast<int>(result.value.sourceFormat), static_cast<int>(StlFileFormat::Binary));
    QCOMPARE(static_cast<qulonglong>(result.value.statistics.triangleCount), qulonglong(12));
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[0] - 0.2) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[1] - 0.1) <= kTolerance);
    QVERIFY(std::abs(result.value.statistics.maximumBounds_m[2] - 0.1) <= kTolerance);
}

void StlMeshLoaderTests::rejectsDegenerateTriangles()
{
    const QString path = writeTempFile(QStringLiteral("rk_degenerate_mesh_loader.stl"), degenerateAsciiStlBytes());
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> result =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, true});

    QVERIFY(!result.ok());
    QCOMPARE(result.status, KinematicsStatus::InvalidRequest);
    QVERIFY(result.message.find("degenerate triangle") != std::string::npos);
}

void StlMeshLoaderTests::filtersDegenerateTrianglesWhenSkipModeIsRequested()
{
    QByteArray bytes;
    bytes += "solid mixed\n";
    bytes += "  facet normal 0 0 0\n";
    bytes += "    outer loop\n";
    bytes += "      vertex 0 0 0\n";
    bytes += "      vertex 1 1 1\n";
    bytes += "      vertex 2 2 2\n";
    bytes += "    endloop\n";
    bytes += "  endfacet\n";
    bytes += "  facet normal 0 0 1\n";
    bytes += "    outer loop\n";
    bytes += "      vertex 0 0 0\n";
    bytes += "      vertex 1 0 0\n";
    bytes += "      vertex 0 1 0\n";
    bytes += "    endloop\n";
    bytes += "  endfacet\n";
    bytes += "endsolid mixed\n";

    const QString path = writeTempFile(QStringLiteral("rk_mixed_degenerate_mesh_loader.stl"), bytes);
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> rejected =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, true});
    QVERIFY(!rejected.ok());

    const Result<TriangleMesh> skipped =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, false});

    QVERIFY2(skipped.ok(), skipped.message.c_str());
    QCOMPARE(static_cast<qulonglong>(skipped.value.statistics.triangleCount), qulonglong(1));
    QCOMPARE(static_cast<qulonglong>(skipped.value.statistics.vertexCount), qulonglong(3));
}

void StlMeshLoaderTests::rejectsAllDegenerateAsciiStlWhenSkipModeLeavesNoTriangles()
{
    const QString path =
        writeTempFile(QStringLiteral("rk_all_degenerate_mesh_loader.stl"), degenerateAsciiStlBytes());
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> result =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, false});

    QVERIFY(!result.ok());
    QCOMPARE(result.status, KinematicsStatus::InvalidRequest);
    QVERIFY(result.message.find("no triangles") != std::string::npos);
}

void StlMeshLoaderTests::rejectsInvalidPayloadAndNonPositiveScale()
{
    const QString path = writeTempFile(QStringLiteral("rk_invalid_mesh_loader.stl"), QByteArray("not an stl"));
    QVERIFY(!path.isEmpty());

    const Result<TriangleMesh> invalidPayload =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{1.0, true});
    const Result<TriangleMesh> invalidScale =
        StlMeshLoader::loadFile(path.toStdString(), StlMeshLoadOptions{0.0, true});

    QVERIFY(!invalidPayload.ok());
    QVERIFY(!invalidScale.ok());
    QCOMPARE(invalidPayload.status, KinematicsStatus::InvalidRequest);
    QCOMPARE(invalidScale.status, KinematicsStatus::InvalidRequest);
}

/// Entry point invoked by TestMain to run the StlMeshLoaderTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runStlMeshLoaderTests(int argc, char** argv)
{
    StlMeshLoaderTests tests;
    return QTest::qExec(&tests, argc, argv);
}
