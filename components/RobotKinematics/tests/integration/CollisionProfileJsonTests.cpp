#include "CollisionProfileJsonTests.h"

#include <RobotKinematics/Collision/BuiltInCollisionProfiles.h>
#include <RobotKinematics/Collision/CollisionChecker.h>
#include <RobotKinematics/Collision/CollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/CollisionProfileValidator.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>
#include <RobotKinematics/Presets/Virtual6DofTestArm.h>

#include <QFile>
#include <QString>
#include <QtTest/QtTest>

using namespace RobotKinematics;

namespace {
/// Locates the virtual 6-DOF test arm's collision profile JSON fixture by probing a small set of
/// relative paths (accounting for different test-runner working directories); falls back to the
/// first candidate (unopened) if none exist so the caller gets a meaningful "file not found".
std::string virtualCollisionProfilePath()
{
    const char* candidates[] = {
        "../presets/virtual_6dof_test_arm_collision.json",
        "presets/virtual_6dof_test_arm_collision.json",
        "../../presets/virtual_6dof_test_arm_collision.json",
    };
    for (const char* candidate : candidates) {
        if (QFile(QString::fromUtf8(candidate)).exists()) {
            return candidate;
        }
    }
    return candidates[0];
}

/// Locates the Nachi MZ04D collision profile JSON fixture by probing a small set of relative
/// paths (accounting for different test-runner working directories); falls back to the first
/// candidate (unopened) if none exist so the caller gets a meaningful "file not found".
std::string nachiCollisionProfilePath()
{
    const char* candidates[] = {
        "../presets/Nachi/MZ04/nachi_mz04d_collision.json",
        "presets/Nachi/MZ04/nachi_mz04d_collision.json",
        "../../presets/Nachi/MZ04/nachi_mz04d_collision.json",
    };
    for (const char* candidate : candidates) {
        if (QFile(QString::fromUtf8(candidate)).exists()) {
            return candidate;
        }
    }
    return candidates[0];
}

/// Asserts (via QCOMPARE/QVERIFY) that `actual` matches `expected` field-by-field: id, robot
/// model, geometry/disabled-pair/source/metadata counts, and per-geometry id, link id, shape
/// type, sphere/capsule dimensions, geometry-to-link transform (within 1e-12 matrix norm),
/// margin, and enabled flag.
void compareProfiles(const CollisionProfile& expected, const CollisionProfile& actual)
{
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.robotModel, expected.robotModel);
    QCOMPARE(actual.geometries.size(), expected.geometries.size());
    QCOMPARE(actual.disabledPairs.size(), expected.disabledPairs.size());
    QCOMPARE(actual.sources.size(), expected.sources.size());
    QCOMPARE(actual.metadata.size(), expected.metadata.size());

    for (std::size_t i = 0; i < expected.geometries.size(); ++i) {
        const CollisionGeometry& a = actual.geometries[i];
        const CollisionGeometry& b = expected.geometries[i];
        QCOMPARE(a.id, b.id);
        QCOMPARE(a.linkId, b.linkId);
        QCOMPARE(a.shape.type, b.shape.type);
        QCOMPARE(a.shape.sphere.radius_m, b.shape.sphere.radius_m);
        QCOMPARE(a.shape.capsule.radius_m, b.shape.capsule.radius_m);
        QCOMPARE(a.shape.capsule.length_m, b.shape.capsule.length_m);
        QVERIFY((a.geometryToLink.isometry().matrix() - b.geometryToLink.isometry().matrix()).norm() <= 1e-12);
        QCOMPARE(a.margin_m, b.margin_m);
        QCOMPARE(a.enabled, b.enabled);
    }
}
}

void CollisionProfileJsonTests::loaderRejectsInvalidSchemaAndUnits()
{
    const Result<CollisionProfile> schemaResult =
        CollisionProfileJsonLoader::loadJson(
            R"({"schema":"wrong","profile":{"id":"p","robotModel":"r","units":{"length":"m","angle":"rad"}},"geometries":[],"disabledPairs":[],"sources":[],"metadata":{}})");
    const Result<CollisionProfile> unitsResult =
        CollisionProfileJsonLoader::loadJson(
            R"({"schema":"robot-kinematics-collision/v1","profile":{"id":"p","robotModel":"r","units":{"length":"mm","angle":"deg"}},"geometries":[],"disabledPairs":[],"sources":[],"metadata":{}})");

    QVERIFY(!schemaResult.ok());
    QVERIFY(!unitsResult.ok());
    QCOMPARE(schemaResult.status, KinematicsStatus::InvalidRobotConfig);
    QCOMPARE(unitsResult.status, KinematicsStatus::InvalidRobotConfig);
}

void CollisionProfileJsonTests::virtualProfileJsonMatchesCppFallbackAndRepresentativeStates()
{
    const Result<CollisionProfile> loaded =
        CollisionProfileJsonLoader::loadFile(virtualCollisionProfilePath());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    const CollisionProfile expected = CollisionProfiles::virtual6DofTestArm();
    compareProfiles(expected, loaded.value);

    const CollisionProfileValidationResult validation =
        CollisionProfileValidator::validate(config, loaded.value);
    QVERIFY(validation.ok());

    CollisionCheckRequest safeRequest;
    safeRequest.joints = JointVector::fromRadians({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    const CollisionCheckResult safeResult = CollisionChecker::check(config, loaded.value, safeRequest);
    QCOMPARE(safeResult.status, KinematicsStatus::Ok);
    QVERIFY2(!safeResult.hasCollision,
             qPrintable(QStringLiteral("virtual safe state unexpectedly collided with %1 pair(s)")
                            .arg(safeResult.pairs.size())));

    CollisionCheckRequest collisionRequest;
    collisionRequest.joints = JointVector::fromRadians({0.0, 3.141592653589793, 0.0, 0.0, 0.0, 0.0});
    const CollisionCheckResult collisionResult = CollisionChecker::check(config, loaded.value, collisionRequest);
    QCOMPARE(collisionResult.status, KinematicsStatus::Ok);
    QVERIFY2(collisionResult.hasCollision,
             qPrintable(QStringLiteral("virtual collision state returned %1 pairs but no collision")
                            .arg(collisionResult.pairs.size())));
}

void CollisionProfileJsonTests::nachiProfileJsonMatchesCppFallbackAndRepresentativeStates()
{
    const Result<CollisionProfile> loaded =
        CollisionProfileJsonLoader::loadFile(nachiCollisionProfilePath());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    const SerialRobotConfig config = Presets::nachiMZ04D();
    const CollisionProfile expected = CollisionProfiles::nachiMZ04D();
    compareProfiles(expected, loaded.value);

    const CollisionProfileValidationResult validation =
        CollisionProfileValidator::validate(config, loaded.value);
    QVERIFY(validation.ok());

    CollisionCheckRequest safeRequest;
    safeRequest.joints = JointVector::fromDegrees({28.1579, -18.8069, 163.839, -0.710019, 35.8922, 152.731});
    const CollisionCheckResult safeResult = CollisionChecker::check(config, loaded.value, safeRequest);
    QCOMPARE(safeResult.status, KinematicsStatus::Ok);
    QVERIFY2(!safeResult.hasCollision,
             qPrintable(QStringLiteral("nachi safe state unexpectedly collided with %1 pair(s)")
                            .arg(safeResult.pairs.size())));

    CollisionCheckRequest collisionRequest;
    collisionRequest.joints = JointVector::fromDegrees({0.0, 135.0, -135.0, 0.0, 0.0, 0.0});
    const CollisionCheckResult collisionResult = CollisionChecker::check(config, loaded.value, collisionRequest);
    QCOMPARE(collisionResult.status, KinematicsStatus::Ok);
    QVERIFY2(collisionResult.hasCollision,
             qPrintable(QStringLiteral("nachi collision state returned %1 pairs but no collision")
                            .arg(collisionResult.pairs.size())));
}

/// QtTest entry point for CollisionProfileJsonTests: constructs the suite and runs it via
/// QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runCollisionProfileJsonTests(int argc, char** argv)
{
    CollisionProfileJsonTests tests;
    return QTest::qExec(&tests, argc, argv);
}
