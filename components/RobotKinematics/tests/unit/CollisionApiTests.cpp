#include "CollisionApiTests.h"

#include <RobotKinematics/Collision/CollisionChecker.h>
#include <RobotKinematics/Collision/CollisionGeometry.h>
#include <RobotKinematics/Collision/CollisionProfile.h>

#include <QtTest/QtTest>

using namespace RobotKinematics;

void CollisionApiTests::requestDefaultsReturnAllPairsWithZeroSafetyMargin()
{
    CollisionCheckRequest request;

    QCOMPARE(request.safetyMargin_m, 0.0);
    QVERIFY(request.returnAllPairs);
    QCOMPARE(request.joints.size(), 0);
}

void CollisionApiTests::resultOkDependsOnStatusNotCollisionFlag()
{
    CollisionCheckResult colliding;
    colliding.status = KinematicsStatus::Ok;
    colliding.hasCollision = true;

    QVERIFY(colliding.ok());
    QVERIFY(colliding.hasCollision);

    CollisionCheckResult failed;
    failed.status = KinematicsStatus::InvalidRequest;
    failed.hasCollision = false;

    QVERIFY(!failed.ok());
}

void CollisionApiTests::profileCanStoreSphereAndCapsuleGeometry()
{
    CollisionGeometry sphere;
    sphere.id = "base_body";
    sphere.linkId = "base_link";
    sphere.shape.type = CollisionShapeType::Sphere;
    sphere.shape.sphere.radius_m = 0.15;
    sphere.margin_m = 0.01;

    CollisionGeometry capsule;
    capsule.id = "link_2_body";
    capsule.linkId = "link_2";
    capsule.shape.type = CollisionShapeType::Capsule;
    capsule.shape.capsule.radius_m = 0.04;
    capsule.shape.capsule.length_m = 0.30;
    capsule.geometryToLink = Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.12, 0.0, 0.0, 0.0);

    CollisionProfile profile;
    profile.id = "virtual_6dof_test_arm_conservative_primitives";
    profile.robotModel = "Virtual6DofTestArm";
    profile.geometries = {sphere, capsule};
    profile.disabledPairs.push_back(
        DisabledCollisionPair{"base_body", "link_2_body", "adjacent_joint_contact"});
    profile.sources.push_back(SourceReference{
        "project_fixture",
        "Virtual test-arm conservative collision profile",
        "docs/planning/collision_detection_plan.md",
        {"collision_geometry"},
    });
    profile.metadata["reviewState"] = "draft";

    QCOMPARE(profile.geometries.size(), std::size_t(2));
    QCOMPARE(profile.geometries[0].shape.type, CollisionShapeType::Sphere);
    QCOMPARE(profile.geometries[0].shape.sphere.radius_m, 0.15);
    QCOMPARE(profile.geometries[1].shape.type, CollisionShapeType::Capsule);
    QCOMPARE(profile.geometries[1].shape.capsule.length_m, 0.30);
    QCOMPARE(profile.geometries[1].geometryToLink.translation_m().z(), 0.12);
    QCOMPARE(profile.disabledPairs.size(), std::size_t(1));
    QCOMPARE(profile.disabledPairs.front().reason, std::string("adjacent_joint_contact"));
    QCOMPARE(profile.sources.size(), std::size_t(1));
    QCOMPARE(profile.metadata.at("reviewState"), std::string("draft"));
}

/// Entry point invoked by TestMain to run the CollisionApiTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runCollisionApiTests(int argc, char** argv)
{
    CollisionApiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
