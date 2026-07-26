#include "CollisionCheckerTests.h"

#include <RobotKinematics/Collision/CollisionChecker.h>

#include <Eigen/Core>

#include <QtTest/QtTest>

#include <optional>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Builds a single-DOF revolute joint rotating about the local Z axis, spanning
/// +/-pi, with zero home position.
Joint revoluteZ(const std::string& id,
                const std::string& parent,
                const std::string& child,
                const Pose& origin)
{
    Joint joint;
    joint.id = id;
    joint.type = JointType::Revolute;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.axis = Eigen::Vector3d::UnitZ();
    joint.origin = origin;
    joint.limits = JointLimits{-kPi, kPi, std::nullopt, std::nullopt};
    joint.home = 0.0;
    return joint;
}

/// Builds a minimal 3-DOF serial robot (three Z-axis revolute joints, each link 1m
/// apart along X) used as a lightweight fixture for collision-checker tests.
SerialRobotConfig lineRobot()
{
    SerialRobotConfig config;
    config.identity = RobotIdentity{"RobotKinematics", "CollisionLineRobot", "Collision Line Robot", "1.0.0"};
    config.topology = RobotTopologyType::Serial;
    config.dof = 3;
    config.links = {{"base_link"}, {"link_1"}, {"link_2"}, {"flange"}};
    config.joints = {
        revoluteZ("J1", "base_link", "link_1", Pose::identity()),
        revoluteZ("J2", "link_1", "link_2", Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0)),
        revoluteZ("J3", "link_2", "flange", Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0)),
    };
    config.frames.baseLinkId = "base_link";
    config.frames.flangeLinkId = "flange";
    config.tools = {Tool{"default", "Default Tool", Pose::identity()}};
    config.defaultToolId = "default";
    return config;
}

/// Builds a CollisionProfile with three geometries placed along lineRobot() (a base
/// sphere, an X-offset link_1 sphere, and a flange capsule) used to test pair ordering
/// and distance computation.
CollisionProfile placementProfile()
{
    CollisionProfile profile;
    profile.id = "placement_profile";
    profile.robotModel = "CollisionLineRobot";

    CollisionGeometry base;
    base.id = "base_sphere";
    base.linkId = "base_link";
    base.shape.type = CollisionShapeType::Sphere;
    base.shape.sphere.radius_m = 0.2;

    CollisionGeometry middle;
    middle.id = "link_1_sphere";
    middle.linkId = "link_1";
    middle.geometryToLink = Pose::fromXYZRPY_m_rad(0.55, 0.0, 0.0, 0.0, 0.0, 0.0);
    middle.shape.type = CollisionShapeType::Sphere;
    middle.shape.sphere.radius_m = 0.1;

    CollisionGeometry end;
    end.id = "flange_capsule";
    end.linkId = "flange";
    end.shape.type = CollisionShapeType::Capsule;
    end.shape.capsule.radius_m = 0.1;
    end.shape.capsule.length_m = 0.4;

    profile.geometries = {base, middle, end};
    return profile;
}

/// Returns the all-zero joint vector representing lineRobot()'s home/rest configuration.
JointVector homeJoints()
{
    return JointVector::fromRadians({0.0, 0.0, 0.0});
}
}

void CollisionCheckerTests::transformsGeometryIntoBaseAndKeepsDeterministicPairOrder()
{
    CollisionCheckRequest request;
    request.joints = homeJoints();

    const CollisionCheckResult result = CollisionChecker::check(lineRobot(), placementProfile(), request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QVERIFY(!result.hasCollision);
    QCOMPARE(result.pairs.size(), std::size_t(3));

    QCOMPARE(result.pairs[0].geometryA, std::string("base_sphere"));
    QCOMPARE(result.pairs[0].geometryB, std::string("link_1_sphere"));
    QCOMPARE(result.pairs[1].geometryA, std::string("base_sphere"));
    QCOMPARE(result.pairs[1].geometryB, std::string("flange_capsule"));
    QCOMPARE(result.pairs[2].geometryA, std::string("link_1_sphere"));
    QCOMPARE(result.pairs[2].geometryB, std::string("flange_capsule"));

    QCOMPARE(result.pairs[0].linkA, std::string("base_link"));
    QCOMPARE(result.pairs[0].linkB, std::string("link_1"));
    QVERIFY(qAbs(result.pairs[0].distance_m - 0.25) < 1e-12);
}

void CollisionCheckerTests::skipsDisabledGeometryAndDisabledPairs()
{
    CollisionProfile profile = placementProfile();
    profile.geometries[2].enabled = false;
    profile.disabledPairs = {
        DisabledCollisionPair{"base_sphere", "link_1_sphere", "adjacent_joint_contact"},
    };

    CollisionCheckRequest request;
    request.joints = homeJoints();

    const CollisionCheckResult result = CollisionChecker::check(lineRobot(), profile, request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QCOMPARE(result.pairs.size(), std::size_t(0));
}

void CollisionCheckerTests::invalidJointDimensionReturnsStructuredFailure()
{
    CollisionCheckRequest request;
    request.joints = JointVector::fromRadians({0.0, 0.0});

    const CollisionCheckResult result = CollisionChecker::check(lineRobot(), placementProfile(), request);

    QCOMPARE(result.status, KinematicsStatus::JointDimensionMismatch);
    QVERIFY(!result.ok());
    QVERIFY(QString::fromStdString(result.message).contains("joint"));
}

void CollisionCheckerTests::stopsAfterFirstCollisionWhenReturnAllPairsIsFalse()
{
    CollisionProfile profile = placementProfile();
    profile.geometries[1].shape.sphere.radius_m = 0.4;

    CollisionCheckRequest request;
    request.joints = homeJoints();
    request.returnAllPairs = false;

    const CollisionCheckResult result = CollisionChecker::check(lineRobot(), profile, request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QVERIFY(result.hasCollision);
    QCOMPARE(result.pairs.size(), std::size_t(1));
    QVERIFY(result.pairs.front().colliding);
    QCOMPARE(result.pairs.front().geometryA, std::string("base_sphere"));
    QCOMPARE(result.pairs.front().geometryB, std::string("link_1_sphere"));
}

/// Entry point invoked by TestMain to run the CollisionCheckerTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runCollisionCheckerTests(int argc, char** argv)
{
    CollisionCheckerTests tests;
    return QTest::qExec(&tests, argc, argv);
}
