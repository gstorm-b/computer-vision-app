#include "FrameToolFkTests.h"

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/JointLimitValidator.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// True when `a` and `b` differ by no more than `tol` in Euclidean norm.
bool vecNear(const Eigen::Vector3d& a, const Eigen::Vector3d& b, double tol = 1e-12)
{
    return (a - b).norm() <= tol;
}

/// Single revolute-Z joint whose flange sits 0.4 m along base X, plus a user frame at
/// (1,0,0) relative to the base link and a default tool.
SerialRobotConfig oneJointWithUserFrame()
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.dof = 1;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"flange"}};

    Joint joint;
    joint.id = "J1";
    joint.type = JointType::Revolute;
    joint.parentLinkId = "base";
    joint.childLinkId = "flange";
    joint.axis = Eigen::Vector3d::UnitZ();
    joint.origin = Pose::fromXYZRPY_m_rad(0.4, 0.0, 0.0, 0.0, 0.0, 0.0);
    joint.limits = JointLimits{-kPi, kPi, std::nullopt, std::nullopt};
    config.joints = {joint};

    UserFrame vision;
    vision.id = "vision";
    vision.parentLinkId = "base";
    vision.transform = Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    config.frames.userFrames = {vision};

    config.tools.push_back({"default", "Default Tool", Pose::identity()});
    config.defaultToolId = "default";
    return config;
}
}

void FrameToolFkTests::toolPoseComposesFlangeAndTcp()
{
    const SerialRobotConfig config = oneJointWithUserFrame();
    const Pose tool = Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.2, 0.0, 0.0, 0.0);
    const JointVector joints = JointVector::fromRadians({0.0});

    const Pose flange = ForwardKinematics::flangePose(config, joints);
    const Pose tcp = ForwardKinematics::toolPose(config, joints, tool);

    QVERIFY(vecNear(flange.translation_m(), Eigen::Vector3d(0.4, 0.0, 0.0)));
    QVERIFY(vecNear(tcp.translation_m(), Eigen::Vector3d(0.4, 0.0, 0.2)));
}

void FrameToolFkTests::userFrameRelativePose()
{
    const SerialRobotConfig config = oneJointWithUserFrame();
    const JointVector joints = JointVector::fromRadians({0.0});
    const FkChain chain = ForwardKinematics::computeChain(config, joints);

    const Result<Pose> frameInBase =
        ForwardKinematics::userFrameInBase(chain, config.frames.userFrames.front());
    QVERIFY(frameInBase.ok());
    QVERIFY(vecNear(frameInBase.value.translation_m(), Eigen::Vector3d(1.0, 0.0, 0.0)));

    // base->flange is (0.4,0,0); expressed in the user frame at (1,0,0) that is (-0.6,0,0).
    const Pose flangeInUserFrame = frameInBase.value.inverse() * chain.flangeInBase;
    QVERIFY(vecNear(flangeInUserFrame.translation_m(), Eigen::Vector3d(-0.6, 0.0, 0.0)));
}

void FrameToolFkTests::jointLimitValidatorRejectsWrongDimension()
{
    const SerialRobotConfig config = oneJointWithUserFrame();
    const JointLimitCheck check =
        JointLimitValidator::validate(config, JointVector::fromRadians({0.0, 0.0}));
    QCOMPARE(check.status, KinematicsStatus::JointDimensionMismatch);
}

/// QtTest entry point for FrameToolFkTests: constructs the suite and runs it via QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runFrameToolFkTests(int argc, char** argv)
{
    FrameToolFkTests tests;
    return QTest::qExec(&tests, argc, argv);
}
