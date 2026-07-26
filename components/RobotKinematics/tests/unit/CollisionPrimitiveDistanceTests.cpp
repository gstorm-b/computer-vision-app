#include "CollisionPrimitiveDistanceTests.h"

#include <RobotKinematics/Collision/CollisionChecker.h>

#include <Eigen/Core>

#include <QtTest/QtTest>

#include <optional>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Builds a Z-axis revolute joint with the given id, parent/child link ids, and origin pose;
/// limits are set to +/-pi with no velocity/effort caps.
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

/// Builds a fixture 3-DOF serial robot (base_link -> link_1 -> link_2 -> flange, each hop
/// 1m along X) with three Z-axis revolute joints, used as the shared kinematic model for the
/// collision-distance fixtures in this suite.
SerialRobotConfig threeStageRobot()
{
    SerialRobotConfig config;
    config.identity = RobotIdentity{"RobotKinematics", "CollisionDistanceFixture", "Collision Distance Fixture", "1.0.0"};
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

/// Returns the zero joint vector for the 3-DOF threeStageRobot() fixture.
JointVector zeros()
{
    return JointVector::fromRadians({0.0, 0.0, 0.0});
}
}

void CollisionPrimitiveDistanceTests::sphereSphereDistancesCoverSeparatedTouchingAndOverlap()
{
    CollisionProfile separated;
    separated.id = "sphere_sphere";
    separated.robotModel = "CollisionDistanceFixture";
    separated.geometries = {
        CollisionGeometry{"sphere_a", "base_link",
                          CollisionShape{CollisionShapeType::Sphere, CollisionSphere{0.2}, CollisionCapsule{}},
                          Pose::identity(), 0.0, true},
        CollisionGeometry{"sphere_b", "link_1",
                          CollisionShape{CollisionShapeType::Sphere, CollisionSphere{0.2}, CollisionCapsule{}},
                          Pose::fromXYZRPY_m_rad(0.7, 0.0, 0.0, 0.0, 0.0, 0.0), 0.0, true},
    };

    CollisionCheckRequest request;
    request.joints = zeros();

    const CollisionCheckResult separatedResult = CollisionChecker::check(threeStageRobot(), separated, request);
    QVERIFY2(qAbs(separatedResult.pairs.front().distance_m - 0.3) < 1e-12,
             qPrintable(QStringLiteral("sphere-sphere separated distance was %1")
                            .arg(separatedResult.pairs.front().distance_m, 0, 'g', 16)));
    QVERIFY(!separatedResult.pairs.front().colliding);

    separated.geometries[1].geometryToLink = Pose::fromXYZRPY_m_rad(0.4, 0.0, 0.0, 0.0, 0.0, 0.0);
    const CollisionCheckResult touchingResult = CollisionChecker::check(threeStageRobot(), separated, request);
    QVERIFY(qAbs(touchingResult.pairs.front().distance_m) < 1e-12);
    QVERIFY(touchingResult.pairs.front().colliding);

    separated.geometries[1].geometryToLink = Pose::fromXYZRPY_m_rad(0.2, 0.0, 0.0, 0.0, 0.0, 0.0);
    const CollisionCheckResult overlapResult = CollisionChecker::check(threeStageRobot(), separated, request);
    QVERIFY(overlapResult.pairs.front().distance_m < 0.0);
    QVERIFY(overlapResult.pairs.front().colliding);
}

void CollisionPrimitiveDistanceTests::sphereCapsuleDistanceReportsGapAndCollision()
{
    CollisionProfile profile;
    profile.id = "sphere_capsule";
    profile.robotModel = "CollisionDistanceFixture";

    CollisionGeometry sphere;
    sphere.id = "sphere";
    sphere.linkId = "base_link";
    sphere.shape.type = CollisionShapeType::Sphere;
    sphere.shape.sphere.radius_m = 0.1;

    CollisionGeometry capsule;
    capsule.id = "capsule";
    capsule.linkId = "link_1";
    capsule.geometryToLink = Pose::fromXYZRPY_m_rad(0.6, 0.0, 0.0, 0.0, 1.5707963267948966, 0.0);
    capsule.shape.type = CollisionShapeType::Capsule;
    capsule.shape.capsule.radius_m = 0.1;
    capsule.shape.capsule.length_m = 0.4;

    profile.geometries = {sphere, capsule};

    CollisionCheckRequest request;
    request.joints = zeros();

    const CollisionCheckResult gapResult = CollisionChecker::check(threeStageRobot(), profile, request);
    QVERIFY2(qAbs(gapResult.pairs.front().distance_m - 0.2) < 1e-12,
             qPrintable(QStringLiteral("sphere-capsule gap distance was %1")
                            .arg(gapResult.pairs.front().distance_m, 0, 'g', 16)));
    QVERIFY(!gapResult.pairs.front().colliding);

    sphere.geometryToLink = Pose::fromXYZRPY_m_rad(0.45, 0.0, 0.0, 0.0, 0.0, 0.0);
    profile.geometries[0] = sphere;
    const CollisionCheckResult collisionResult = CollisionChecker::check(threeStageRobot(), profile, request);
    QVERIFY(collisionResult.pairs.front().distance_m < 0.0);
    QVERIFY(collisionResult.pairs.front().colliding);
}

void CollisionPrimitiveDistanceTests::capsuleCapsuleDistanceAndSafetyMarginAreApplied()
{
    CollisionProfile profile;
    profile.id = "capsule_capsule";
    profile.robotModel = "CollisionDistanceFixture";

    CollisionGeometry capsuleA;
    capsuleA.id = "capsule_a";
    capsuleA.linkId = "base_link";
    capsuleA.geometryToLink = Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, 0.0, 1.5707963267948966, 0.0);
    capsuleA.shape.type = CollisionShapeType::Capsule;
    capsuleA.shape.capsule.radius_m = 0.1;
    capsuleA.shape.capsule.length_m = 0.4;

    CollisionGeometry capsuleB;
    capsuleB.id = "capsule_b";
    capsuleB.linkId = "link_2";
    capsuleB.geometryToLink = Pose::fromXYZRPY_m_rad(-0.6, 0.25, 0.0, 0.0, 1.5707963267948966, 0.0);
    capsuleB.shape.type = CollisionShapeType::Capsule;
    capsuleB.shape.capsule.radius_m = 0.1;
    capsuleB.shape.capsule.length_m = 0.4;

    profile.geometries = {capsuleA, capsuleB};

    CollisionCheckRequest request;
    request.joints = zeros();

    const CollisionCheckResult gapResult = CollisionChecker::check(threeStageRobot(), profile, request);
    QVERIFY2(qAbs(gapResult.pairs.front().distance_m - 0.05) < 1e-12,
             qPrintable(QStringLiteral("capsule-capsule gap distance was %1")
                            .arg(gapResult.pairs.front().distance_m, 0, 'g', 16)));
    QVERIFY(!gapResult.pairs.front().colliding);

    request.safetyMargin_m = 0.06;
    const CollisionCheckResult marginResult = CollisionChecker::check(threeStageRobot(), profile, request);
    QVERIFY(marginResult.pairs.front().colliding);
    QVERIFY(marginResult.hasCollision);
}

/// Entry point that instantiates CollisionPrimitiveDistanceTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runCollisionPrimitiveDistanceTests(int argc, char** argv)
{
    CollisionPrimitiveDistanceTests tests;
    return QTest::qExec(&tests, argc, argv);
}
