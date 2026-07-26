#include "NachiMeshCollisionTests.h"

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/MeshCollisionProfileValidator.h>
#include <RobotKinematics/Collision/StlMeshLoader.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtTest/QtTest>

#include <Eigen/Core>

#include <array>
#include <map>
#include <string>

using namespace RobotKinematics;

namespace {

/// Builds a list of candidate repository-root directories to search: the current working
/// directory, the running executable's directory, and up to 8 levels of that directory's
/// ancestors (deduplicated), to tolerate different test-runner working directories.
QStringList candidateRoots()
{
    QStringList roots;
    roots << QDir::currentPath();

    const QString appDirPath = QCoreApplication::applicationDirPath();
    if (!appDirPath.isEmpty()) {
        roots << appDirPath;
        QDir walker(appDirPath);
        for (int depth = 0; depth < 8; ++depth) {
            if (!walker.cdUp()) {
                break;
            }
            roots << walker.absolutePath();
        }
    }

    roots.removeDuplicates();
    return roots;
}

/// Searches candidateRoots() for the directory containing the Nachi mesh-collision profile
/// JSON fixture.
/// @return the absolute path of the first matching root, or an empty string if none contain it
QString findRepoRoot()
{
    for (const QString& root : candidateRoots()) {
        const QDir directory(root);
        if (directory.exists(QStringLiteral("presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json"))) {
            return directory.absolutePath();
        }
    }
    return QString();
}

/// Resolves the absolute path to presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json via
/// findRepoRoot().
/// @return the absolute profile path, or an empty string if the repo root could not be located
QString nachiMeshProfilePath()
{
    const QString root = findRepoRoot();
    if (root.isEmpty()) {
        return QString();
    }
    return QDir(root).absoluteFilePath(QStringLiteral("presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json"));
}

/// A mesh id paired with the base-frame pose it is expected to occupy at the robot's home
/// joint state.
struct ExpectedMeshAtHome {
    std::string id;
    Pose expectedInBase;
};

/// Returns the 8 Nachi mesh ids with their expected base-frame poses at the home joint state.
/// These match Robot3DVizualize::visualHomeCorrectionForPartKey for the corresponding mesh ids.
std::array<ExpectedMeshAtHome, 8> expectedMeshHomePoses()
{
    return {{
        {"base_mesh",
         Pose::fromXYZRPY_mm_deg(0.0, 0.0, 0.0, 90.0, 0.0, 0.0)},
        {"j1_mesh",
         Pose::fromXYZRPY_mm_deg(0.0, 0.0, 340.0, 90.0, 0.0, 0.0)},
        {"j2_mesh",
         Pose::fromXYZRPY_mm_deg(0.0, 0.0, 340.0, 90.0, 0.0, 0.0)},
        {"j3_mesh",
         Pose::fromXYZRPY_mm_deg(260.0, 0.0, 340.0, 90.0, 0.0, 0.0)},
        {"j4_mesh",
         Pose::fromXYZRPY_mm_deg(285.0, 0.0, 259.0, 90.0, 0.0, 0.0)},
        {"j5_mesh",
         Pose::fromXYZRPY_mm_deg(285.0, 0.0, 60.0, 90.0, 0.0, 0.0)},
        {"j6_mesh",
         Pose::fromXYZRPY_mm_deg(285.0, 0.0, -12.0, 90.0, 0.0, 0.0)},
        {"centering_tool_mesh",
         Pose::fromXYZRPY_mm_deg(285.0, 0.0, -12.0, 0.0, 0.0, 180.0)},
    }};
}

/// Returns the all-zero joint vector representing the robot's home configuration.
JointVector homeJointVector()
{
    return JointVector::fromDegrees({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
}

/// Returns a joint configuration (J2 = -85 deg, J3 = 175 deg) used as a self-collision smoke
/// pose for the mesh backend.
JointVector aggressiveFoldedJointDegrees()
{
    // J2 folds the upper arm back over the shoulder column and J3 curls the forearm
    // back toward the shoulder cluster. This pose is used as a smoke pose so we can
    // assert that the mesh backend runs end-to-end on the real Nachi STL meshes; the
    // exact collision boolean is recorded for future regression tightening rather than
    // asserted, because verified problem poses must come from user observation.
    return JointVector::fromDegrees({0.0, -85.0, 175.0, 0.0, 0.0, 0.0});
}

} // namespace

/// Checks the on-disk Nachi mesh-collision profile loads with the expected id, robot model,
/// mesh/disabled-pair counts, and Coal-first backend preference, and validates cleanly against
/// Presets::nachiMZ04D().
void NachiMeshCollisionTests::profileLoadsAndValidatesAgainstNachiPreset()
{
    const QString path = nachiMeshProfilePath();
    QVERIFY2(!path.isEmpty(), "could not locate presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json relative to test working directory");

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(path.toStdString());

    QVERIFY2(loaded.ok(), loaded.message.c_str());
    QCOMPARE(loaded.value.id, std::string("nachi_mz04d_stl_mesh_collision"));
    QCOMPARE(loaded.value.robotModel, std::string("MZ04D"));
    QCOMPARE(loaded.value.meshes.size(), std::size_t(8));
    QCOMPARE(loaded.value.disabledPairs.size(), std::size_t(6));
    QVERIFY(!loaded.value.backendPreference.empty());
    QCOMPARE(static_cast<int>(loaded.value.backendPreference.front()),
             static_cast<int>(MeshCollisionBackendKind::Coal));

    const MeshCollisionProfileValidationResult validation =
        MeshCollisionProfileValidator::validate(Presets::nachiMZ04D(), loaded.value);

    QVERIFY2(validation.ok(),
             validation.issues.empty() ? "" : validation.issues.front().message.c_str());
}

/// Checks every mesh in the profile has an STL file on disk that loads successfully at
/// millimeter scale (scaleToMeters == 0.001), has at least one triangle, and has a bounding-box
/// extent under 1 m on each axis.
void NachiMeshCollisionTests::everyStlMeshLoadsWithMillimeterScale()
{
    const QString path = nachiMeshProfilePath();
    QVERIFY(!path.isEmpty());

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    for (const MeshCollisionGeometry& mesh : loaded.value.meshes) {
        QVERIFY2(QFileInfo(QString::fromStdString(mesh.path)).exists(),
                 (mesh.id + " STL missing: " + mesh.path).c_str());

        // Production paths skip degenerate triangles silently; mirror that here so
        // real CAD-exported STLs (which routinely contain a handful) still load.
        const Result<TriangleMesh> triangleMesh =
            StlMeshLoader::loadFile(mesh.path, StlMeshLoadOptions{mesh.scaleToMeters, false});
        QVERIFY2(triangleMesh.ok(),
                 (mesh.id + " STL load failed: " + triangleMesh.message).c_str());

        QVERIFY(triangleMesh.value.statistics.triangleCount > 0);
        QCOMPARE(triangleMesh.value.statistics.scaleToMeters, 0.001);

        // Sanity: max half-extent should be < 1 meter for an industrial-arm STL part in mm.
        const auto& minBounds = triangleMesh.value.statistics.minimumBounds_m;
        const auto& maxBounds = triangleMesh.value.statistics.maximumBounds_m;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            QVERIFY2(maxBounds[axis] - minBounds[axis] <= 1.0,
                     (mesh.id + " STL extent exceeds 1 m on axis " + std::to_string(axis)).c_str());
        }
    }
}

/// Checks that composing each mesh's link pose (from FK at the home joint state) with its
/// meshToLink transform reproduces the fixed reference poses in expectedMeshHomePoses() to
/// within 1e-9.
void NachiMeshCollisionTests::meshToLinkTransformsReproduceVisualizerHomePlacement()
{
    const QString path = nachiMeshProfilePath();
    QVERIFY(!path.isEmpty());

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    const SerialRobotConfig config = Presets::nachiMZ04D();
    const FkChain chain = ForwardKinematics::computeChain(config, homeJointVector());

    std::map<std::string, Pose> meshById;
    for (const MeshCollisionGeometry& mesh : loaded.value.meshes) {
        const auto linkIt = chain.linkPosesInBase.find(mesh.linkId);
        QVERIFY2(linkIt != chain.linkPosesInBase.end(),
                 (mesh.id + " links to missing link: " + mesh.linkId).c_str());

        meshById.emplace(mesh.id, linkIt->second * mesh.meshToLink);
    }

    for (const ExpectedMeshAtHome& expected : expectedMeshHomePoses()) {
        const auto it = meshById.find(expected.id);
        QVERIFY2(it != meshById.end(), (expected.id + " not present in profile").c_str());

        const Eigen::Matrix4d delta = it->second.isometry().matrix() - expected.expectedInBase.isometry().matrix();
        // 1e-9 m and ~1e-9 rad — well below the documented visualizer placement granularity (mm/deg).
        QVERIFY2(delta.norm() <= 1e-9,
                 (expected.id + " mesh home pose deviates from visualizer placement").c_str());
    }
}

/// Skipped unless ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND is defined. Runs the mesh collision
/// backend at the home joint state and checks it reports no self-collision.
void NachiMeshCollisionTests::meshBackendDetectsHomePoseHasNoCollisionWhenCompiled()
{
#ifndef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    QSKIP("Coal mesh backend is not compiled in this build");
#else
    const QString path = nachiMeshProfilePath();
    QVERIFY(!path.isEmpty());

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    MeshCollisionCheckRequest request;
    request.joints = homeJointVector();
    request.returnAllPairs = true;

    const CollisionCheckResult result =
        CollisionBackends::checkMesh(Presets::nachiMZ04D(), loaded.value, request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QVERIFY2(!result.hasCollision,
             "Nachi mesh profile at home reported self-collision; mesh-to-link transforms or disabled pairs are wrong.");
#endif
}

/// Skipped unless ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND is defined. Runs the mesh collision
/// backend at aggressiveFoldedJointDegrees() (zero safety margin) and checks it reports a
/// self-collision involving the base mesh and the folded J2 or J3 mesh.
void NachiMeshCollisionTests::meshBackendDetectsKnownSelfCollisionPoseWhenCompiled()
{
#ifndef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    QSKIP("Coal mesh backend is not compiled in this build");
#else
    const QString path = nachiMeshProfilePath();
    QVERIFY(!path.isEmpty());

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    MeshCollisionCheckRequest request;
    request.joints = aggressiveFoldedJointDegrees();
    request.returnAllPairs = true;
    request.safetyMargin_m = 0.0;

    const CollisionCheckResult result =
        CollisionBackends::checkMesh(Presets::nachiMZ04D(), loaded.value, request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QVERIFY2(result.hasCollision,
             "Folded Nachi mesh fixture should report self-collision; this guards the "
             "known mesh-profile/backend regression case.");
    QVERIFY(!result.pairs.empty());

    bool foundBaseJ2Collision = false;
    bool foundBaseJ3Collision = false;
    for (const CollisionPairResult& pair : result.pairs) {
        const bool baseJ2 =
            (pair.geometryA == "base_mesh" && pair.geometryB == "j2_mesh") ||
            (pair.geometryA == "j2_mesh" && pair.geometryB == "base_mesh");
        const bool baseJ3 =
            (pair.geometryA == "base_mesh" && pair.geometryB == "j3_mesh") ||
            (pair.geometryA == "j3_mesh" && pair.geometryB == "base_mesh");
        foundBaseJ2Collision = foundBaseJ2Collision || (baseJ2 && pair.colliding);
        foundBaseJ3Collision = foundBaseJ3Collision || (baseJ3 && pair.colliding);
    }
    QVERIFY2(foundBaseJ2Collision || foundBaseJ3Collision,
             "Expected folded pose to collide between the base mesh and the folded arm meshes.");
#endif
}

/// QtTest entry point for NachiMeshCollisionTests: constructs the suite and runs it via
/// QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runNachiMeshCollisionTests(int argc, char** argv)
{
    NachiMeshCollisionTests tests;
    return QTest::qExec(&tests, argc, argv);
}
