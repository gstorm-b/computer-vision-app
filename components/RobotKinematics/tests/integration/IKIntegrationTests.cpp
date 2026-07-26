#include "IKIntegrationTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>

#include <Eigen/Geometry>

#include <QtTest/QtTest>

#include <cmath>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
/// Builds a single movable Joint (Prismatic or Revolute) connecting `parent` to `child`, with
/// the given `axis`, symmetric position/velocity-unset limits [`lower`, `upper`], and a zero
/// home position.
Joint movableJoint(const std::string& id,
                   JointType type,
                   const std::string& parent,
                   const std::string& child,
                   const Eigen::Vector3d& axis,
                   double lower,
                   double upper)
{
    Joint joint;
    joint.id = id;
    joint.type = type;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.axis = axis;
    joint.limits = JointLimits{lower, upper, std::nullopt, std::nullopt};
    joint.home = 0.0;
    return joint;
}

/// Builds a custom 6-DOF Cartesian-then-wrist robot config (3 prismatic + 3 revolute joints) with
/// a "vision" user frame offset from the base, a shoulder/elbow/wrist posture resolver and
/// labels, and two tools ("default" identity, "probe" offset), for use as an IK integration
/// fixture.
SerialRobotConfig cartesianWristFixture()
{
    SerialRobotConfig config;
    config.identity.vendor = "RobotKinematics";
    config.identity.model = "CartesianWristFixture";
    config.topology = RobotTopologyType::Serial;
    config.dof = 6;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.frames.userFrames.push_back(UserFrame{
        "vision",
        "base",
        Pose::fromXYZRPY_m_rad(0.1, -0.2, 0.05, 0.0, 0.0, 0.25),
    });
    config.posture.resolver = "serial_6dof_shoulder_elbow_wrist";
    config.posture.labels["shoulder"] = PostureLabelAxis{"lefty", "righty"};
    config.posture.labels["elbow"] = PostureLabelAxis{"below", "above"};
    config.posture.labels["wrist"] = PostureLabelAxis{"non-flip", "flip"};
    config.links = {{"base"}, {"x"}, {"y"}, {"z"}, {"rx"}, {"ry"}, {"flange"}};
    config.joints = {
        movableJoint("J1", JointType::Prismatic, "base", "x", Eigen::Vector3d::UnitX(), -1.0, 1.0),
        movableJoint("J2", JointType::Prismatic, "x", "y", Eigen::Vector3d::UnitY(), -1.0, 1.0),
        movableJoint("J3", JointType::Prismatic, "y", "z", Eigen::Vector3d::UnitZ(), -1.0, 1.0),
        movableJoint("J4", JointType::Revolute, "z", "rx", Eigen::Vector3d::UnitX(), -2.0, 2.0),
        movableJoint("J5", JointType::Revolute, "rx", "ry", Eigen::Vector3d::UnitY(), -2.0, 2.0),
        movableJoint("J6", JointType::Revolute, "ry", "flange", Eigen::Vector3d::UnitZ(), -2.0, 2.0),
    };
    config.tools = {
        Tool{"default", "Default Tool", Pose::identity()},
        Tool{"probe", "Probe", Pose::fromXYZRPY_m_rad(0.03, 0.0, 0.02, 0.0, 0.0, 0.0)},
    };
    config.defaultToolId = "default";
    return config;
}

/// Angular difference (radians, always non-negative) between the rotational parts of poses `a`
/// and `b`, computed as the angle of the relative rotation `a * b^-1`.
double orientationError_rad(const Pose& a, const Pose& b)
{
    const Eigen::AngleAxisd angleAxis(a.isometry().linear() * b.isometry().linear().transpose());
    return std::abs(angleAxis.angle());
}
}

void IKIntegrationTests::solveUsesDefaultToolAndReturnsBestSolution()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const SerialRobotKinematics robot(config);
    const JointVector targetJoints = JointVector::fromRadians({0.22, -0.12, 0.34, 0.15, -0.2, 0.3});
    const Pose target = ForwardKinematics::flangePose(config, targetJoints);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = JointVector::fromRadians({0.2, -0.1, 0.3, 0.1, -0.15, 0.25});

    const IKResult result = robot.solve(request);

    QVERIFY(result.ok());
    QCOMPARE(result.solutions.size(), std::size_t(1));
    const Pose solved = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((solved.translation_m() - target.translation_m()).norm() <= 1e-6);
    QVERIFY(orientationError_rad(solved, target) <= 1.7453292519943296e-5);
}

void IKIntegrationTests::solveAllReturnsFoundSolutions()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const SerialRobotKinematics robot(config);
    const JointVector targetJoints = JointVector::fromRadians({0.1, 0.2, 0.25, -0.2, 0.25, -0.3});

    IKRequest request;
    request.targetPose = ForwardKinematics::flangePose(config, targetJoints);
    request.seedJoint = JointVector::fromRadians({0.12, 0.18, 0.22, -0.18, 0.22, -0.25});
    request.options.maxSolutions = 4;

    const IKResult result = robot.solveAll(request);

    QVERIFY(result.ok());
    QVERIFY(!result.solutions.empty());
}

void IKIntegrationTests::missingToolAndFrameReturnStructuredStatus()
{
    const SerialRobotKinematics robot(cartesianWristFixture());

    IKRequest missingTool;
    missingTool.tool = ToolId{"missing"};
    IKResult result = robot.solve(missingTool);
    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::ToolNotFound);

    IKRequest missingFrame;
    missingFrame.referenceFrame = FrameId{"missing"};
    result = robot.solve(missingFrame);
    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::FrameNotFound);
}

void IKIntegrationTests::solveResolvesBaseFixedUserFrameAndTool()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const SerialRobotKinematics robot(config);
    const JointVector targetJoints = JointVector::fromRadians({0.18, -0.08, 0.28, 0.12, -0.18, 0.22});
    const Pose flangeInBase = ForwardKinematics::flangePose(config, targetJoints);
    const Pose tcpInBase = flangeInBase * config.tools[1].flangeToTcp;
    const Pose baseFromVision = config.frames.userFrames.front().transform;

    IKRequest request;
    request.targetPose = baseFromVision.inverse() * tcpInBase;
    request.referenceFrame = FrameId{"vision"};
    request.tool = ToolId{"probe"};
    request.seedJoint = JointVector::fromRadians({0.16, -0.06, 0.26, 0.1, -0.16, 0.2});

    const IKResult result = robot.solve(request);

    QVERIFY(result.ok());
    const Pose solvedTcp = ForwardKinematics::flangePose(config, result.best().joints) * config.tools[1].flangeToTcp;
    QVERIFY((solvedTcp.translation_m() - tcpInBase.translation_m()).norm() <= 1e-6);
    QVERIFY(orientationError_rad(solvedTcp, tcpInBase) <= 1.7453292519943296e-5);
}

void IKIntegrationTests::requirePostureRejectsMismatchedSolution()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const SerialRobotKinematics robot(config);
    const JointVector targetJoints = JointVector::fromRadians({-0.2, 0.1, 0.25, 0.1, -0.2, 0.15});

    IKRequest request;
    request.targetPose = ForwardKinematics::flangePose(config, targetJoints);
    request.seedJoint = targetJoints;
    request.posture = ArmPosture{};
    request.posture->shoulder = 1;
    request.options.requirePosture = true;
    request.options.maxSolutions = 1;

    const IKResult result = robot.solve(request);

    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::PostureConstraintUnsatisfied);
}

/// QtTest entry point for IKIntegrationTests: constructs the suite and runs it via QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runIKIntegrationTests(int argc, char** argv)
{
    IKIntegrationTests tests;
    return QTest::qExec(&tests, argc, argv);
}
