#include "MeshCollisionProfileJsonTests.h"

#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/MeshCollisionProfileValidator.h>
#include <RobotKinematics/Presets/Virtual6DofTestArm.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace RobotKinematics;

namespace {
/// Returns a well-formed mesh-collision profile JSON document (single mesh on link_2, coal/
/// fcl/vtk_debug backend preference) used as the happy-path fixture across several tests.
std::string validMeshProfileJson()
{
    return R"json(
{
  "schema": "robot-kinematics-collision-mesh/v1",
  "profile": {
    "id": "virtual_mesh_profile",
    "robotModel": "Virtual6DofTestArm",
    "units": { "length": "m", "angle": "rad" },
    "backendPreference": ["coal", "fcl", "vtk_debug"]
  },
  "meshes": [
    {
      "id": "upper_arm_mesh",
      "link": "link_2",
      "path": "MZ04-01_j2.stl",
      "format": "stl",
      "sourceUnits": "mm",
      "scaleToMeters": 0.001,
      "meshToLink": {
        "xyz_m": [0.0, 0.0, 0.0],
        "rpy_rad": [0.0, 0.0, 0.0]
      },
      "margin_m": 0.002,
      "enabled": true,
      "quality": {
        "mode": "original",
        "triangleCount": 1200,
        "simplifiedFrom": null,
        "maxSimplificationError_m": null
      }
    }
  ],
  "disabledPairs": [],
  "sources": [
    {
      "type": "visual_cad_stl",
      "title": "Draft mesh source",
      "reference": "presets/Nachi/MZ04",
      "appliesTo": ["mesh_collision_geometry"]
    }
  ],
  "metadata": {
    "reviewState": "draft"
  }
}
)json";
}
}

/// Checks that an unrecognized top-level field and a mesh missing scaleToMeters both fail
/// to load with KinematicsStatus::InvalidRobotConfig.
void MeshCollisionProfileJsonTests::loaderRejectsUnknownTopLevelFieldsAndMissingScale()
{
    const Result<MeshCollisionProfile> unknownField =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":[]},"meshes":[],"disabledPairs":[],"sources":[],"metadata":{},"unexpected":true})");

    const Result<MeshCollisionProfile> missingScale =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":[]},"meshes":[{"id":"m","link":"link_1","path":"mesh.stl","format":"stl","sourceUnits":"mm","meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[],"sources":[],"metadata":{}})");

    QVERIFY(!unknownField.ok());
    QVERIFY(!missingScale.ok());
    QCOMPARE(unknownField.status, KinematicsStatus::InvalidRobotConfig);
    QCOMPARE(missingScale.status, KinematicsStatus::InvalidRobotConfig);
}

/// Checks that loading validMeshProfileJson() yields the expected profile id, robot model,
/// ordered backend preference, mesh link id/scale/quality mode, and metadata entry.
void MeshCollisionProfileJsonTests::loaderParsesBackendPreferenceAndMetadata()
{
    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadJson(validMeshProfileJson());

    QVERIFY2(loaded.ok(), loaded.message.c_str());
    QCOMPARE(loaded.value.id, std::string("virtual_mesh_profile"));
    QCOMPARE(loaded.value.robotModel, std::string("Virtual6DofTestArm"));
    QCOMPARE(loaded.value.backendPreference.size(), std::size_t(3));
    QCOMPARE(static_cast<int>(loaded.value.backendPreference[0]), static_cast<int>(MeshCollisionBackendKind::Coal));
    QCOMPARE(static_cast<int>(loaded.value.backendPreference[1]), static_cast<int>(MeshCollisionBackendKind::Fcl));
    QCOMPARE(static_cast<int>(loaded.value.backendPreference[2]), static_cast<int>(MeshCollisionBackendKind::VtkDebug));
    QCOMPARE(loaded.value.meshes.size(), std::size_t(1));
    QCOMPARE(loaded.value.meshes[0].linkId, std::string("link_2"));
    QCOMPARE(loaded.value.meshes[0].scaleToMeters, 0.001);
    QCOMPARE(static_cast<int>(loaded.value.meshes[0].quality.mode), static_cast<int>(MeshQualityMode::Original));
    QCOMPARE(loaded.value.metadata.at("reviewState"), std::string("draft"));
}

/// Writes a profile JSON with a relative mesh path to a temp directory, loads it via
/// loadFile(), and checks the mesh's path is resolved to an absolute path anchored at that
/// directory.
void MeshCollisionProfileJsonTests::loaderResolvesRelativeMeshPathsFromProfileFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString profilePath = QDir(tempDir.path()).filePath(QStringLiteral("mesh_profile.json"));
    QFile profileFile(profilePath);
    QVERIFY(profileFile.open(QIODevice::WriteOnly));
    const QByteArray json =
        R"json(
{
  "schema": "robot-kinematics-collision-mesh/v1",
  "profile": {
    "id": "relative_path_profile",
    "robotModel": "Virtual6DofTestArm",
    "units": { "length": "m", "angle": "rad" },
    "backendPreference": ["coal"]
  },
  "meshes": [
    {
      "id": "link_mesh",
      "link": "link_2",
      "path": "meshes/link.stl",
      "format": "stl",
      "sourceUnits": "m",
      "scaleToMeters": 1.0,
      "meshToLink": { "xyz_m": [0, 0, 0], "rpy_rad": [0, 0, 0] },
      "margin_m": 0.0,
      "enabled": true,
      "quality": {
        "mode": "original",
        "triangleCount": 1,
        "simplifiedFrom": null,
        "maxSimplificationError_m": null
      }
    }
  ],
  "disabledPairs": [],
  "sources": [],
  "metadata": {}
}
)json";
    QCOMPARE(profileFile.write(json), static_cast<qint64>(json.size()));
    profileFile.close();

    const Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadFile(profilePath.toStdString());

    QVERIFY2(loaded.ok(), loaded.message.c_str());
    QCOMPARE(loaded.value.meshes.size(), std::size_t(1));

    const QString resolvedPath = QString::fromStdString(loaded.value.meshes.front().path);
    QVERIFY(QFileInfo(resolvedPath).isAbsolute());
    QCOMPARE(QDir::cleanPath(resolvedPath),
             QDir::cleanPath(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("meshes/link.stl"))));
}

/// Checks that a non-numeric meshToLink translation component, a string scaleToMeters, and a
/// string margin_m each independently fail to load with InvalidRobotConfig.
void MeshCollisionProfileJsonTests::loaderRejectsNonNumericMeshTransformsAndOptions()
{
    const Result<MeshCollisionProfile> badTransform =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":["coal"]},"meshes":[{"id":"m","link":"link_1","path":"mesh.stl","format":"stl","sourceUnits":"m","scaleToMeters":1.0,"meshToLink":{"xyz_m":[0,"bad",0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[],"sources":[],"metadata":{}})");

    const Result<MeshCollisionProfile> badScale =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":["coal"]},"meshes":[{"id":"m","link":"link_1","path":"mesh.stl","format":"stl","sourceUnits":"m","scaleToMeters":"1.0","meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[],"sources":[],"metadata":{}})");

    const Result<MeshCollisionProfile> badMargin =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":["coal"]},"meshes":[{"id":"m","link":"link_1","path":"mesh.stl","format":"stl","sourceUnits":"m","scaleToMeters":1.0,"meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"margin_m":"bad","quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[],"sources":[],"metadata":{}})");

    QVERIFY(!badTransform.ok());
    QVERIFY(!badScale.ok());
    QVERIFY(!badMargin.ok());
    QCOMPARE(badTransform.status, KinematicsStatus::InvalidRobotConfig);
    QCOMPARE(badScale.status, KinematicsStatus::InvalidRobotConfig);
    QCOMPARE(badMargin.status, KinematicsStatus::InvalidRobotConfig);
}

/// Loads a valid profile, then mutates its single mesh to reference a non-existent link and
/// to have inconsistent "simplified" quality metadata (no simplifiedFrom, negative error
/// bound), and checks that MeshCollisionProfileValidator::validate() reports at least three
/// issues with InvalidRobotConfig.
void MeshCollisionProfileJsonTests::validatorRejectsInvalidLinkAndBrokenSimplifiedMetadata()
{
    Result<MeshCollisionProfile> loaded =
        MeshCollisionProfileJsonLoader::loadJson(validMeshProfileJson());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    loaded.value.meshes[0].linkId = "missing_link";
    loaded.value.meshes[0].quality.mode = MeshQualityMode::Simplified;
    loaded.value.meshes[0].quality.simplifiedFrom.reset();
    loaded.value.meshes[0].quality.maxSimplificationError_m = -0.001;

    const MeshCollisionProfileValidationResult validation =
        MeshCollisionProfileValidator::validate(Presets::virtual6DofTestArm(), loaded.value);

    QVERIFY(!validation.ok());
    QCOMPARE(validation.status(), KinematicsStatus::InvalidRobotConfig);
    QVERIFY(validation.issues.size() >= std::size_t(3));
}

/// Checks that a profile with two meshes sharing the same id, and separately a profile whose
/// disabledPairs entry names a mesh id that does not exist, both fail to load with
/// InvalidRobotConfig.
void MeshCollisionProfileJsonTests::loaderRejectsDuplicateMeshIdsAndInvalidDisabledPairs()
{
    const Result<MeshCollisionProfile> duplicateIds =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":[]},"meshes":[{"id":"dup","link":"link_1","path":"a.stl","format":"stl","sourceUnits":"m","scaleToMeters":1.0,"meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}},{"id":"dup","link":"link_2","path":"b.stl","format":"stl","sourceUnits":"m","scaleToMeters":1.0,"meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[],"sources":[],"metadata":{}})");

    const Result<MeshCollisionProfile> invalidPair =
        MeshCollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision-mesh/v1","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"},"backendPreference":[]},"meshes":[{"id":"a","link":"link_1","path":"a.stl","format":"stl","sourceUnits":"m","scaleToMeters":1.0,"meshToLink":{"xyz_m":[0,0,0],"rpy_rad":[0,0,0]},"quality":{"mode":"original","triangleCount":1,"simplifiedFrom":null,"maxSimplificationError_m":null}}],"disabledPairs":[{"a":"a","b":"missing","reason":"adjacent_joint_contact"}],"sources":[],"metadata":{}})");

    QVERIFY(!duplicateIds.ok());
    QVERIFY(!invalidPair.ok());
    QCOMPARE(duplicateIds.status, KinematicsStatus::InvalidRobotConfig);
    QCOMPARE(invalidPair.status, KinematicsStatus::InvalidRobotConfig);
}

/// Entry point that constructs MeshCollisionProfileJsonTests and runs it under QTest::qExec,
/// forwarding argc/argv (e.g. for -maxwarnings or slot-filter arguments).
/// @return process-style exit code from QTest::qExec (0 on success)
int runMeshCollisionProfileJsonTests(int argc, char** argv)
{
    MeshCollisionProfileJsonTests tests;
    return QTest::qExec(&tests, argc, argv);
}
