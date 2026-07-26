#include "RobotModelValidatorTests.h"

#include <RobotKinematics/Model/RobotModelValidator.h>

#include <QtTest/QtTest>

namespace {
/// Builds a fully-populated, well-formed serial 6-DOF revolute robot config (identity,
/// frames, links, joints with limits/home, a user frame, and a default tool) used as the
/// valid-config baseline that each test perturbs to trigger a specific validation issue.
RobotKinematics::SerialRobotConfig validSixDofConfig()
{
    RobotKinematics::SerialRobotConfig config;
    config.identity.vendor = "RobotKinematics";
    config.identity.model = "Virtual6DofTestArm";
    config.identity.name = "Virtual 6DOF Test Arm";
    config.identity.revision = "1.0.0";
    config.topology = RobotKinematics::RobotTopologyType::Serial;
    config.dof = 6;
    config.frames.baseLinkId = "base_link";
    config.frames.flangeLinkId = "flange";

    config.links.push_back({"base_link"});
    for (int i = 1; i <= 5; ++i) {
        config.links.push_back({"link_" + std::to_string(i)});
    }
    config.links.push_back({"flange"});

    for (int i = 0; i < 6; ++i) {
        RobotKinematics::Joint joint;
        joint.id = "J" + std::to_string(i + 1);
        joint.type = RobotKinematics::JointType::Revolute;
        joint.parentLinkId = (i == 0) ? "base_link" : "link_" + std::to_string(i);
        joint.childLinkId = (i == 5) ? "flange" : "link_" + std::to_string(i + 1);
        joint.axis = Eigen::Vector3d::UnitZ();
        joint.limits = RobotKinematics::JointLimits{-3.0, 3.0, 5.0, 10.0};
        joint.home = 0.0;
        config.joints.push_back(joint);
    }

    config.frames.userFrames.push_back({"vision_frame", "base_link", RobotKinematics::Pose::identity()});
    config.tools.push_back({"default", "Default Tool", RobotKinematics::Pose::identity()});
    config.defaultToolId = "default";
    return config;
}

/// Returns true if `result.issues` contains at least one issue whose `field` member equals
/// `field`.
bool hasIssueForField(const RobotKinematics::ModelValidationResult& result, const std::string& field)
{
    for (const RobotKinematics::ModelValidationIssue& issue : result.issues) {
        if (issue.field == field) {
            return true;
        }
    }
    return false;
}
}

void RobotModelValidatorTests::acceptsValidSerialRobotConfig()
{
    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(validSixDofConfig());

    QVERIFY(result.ok());
    QCOMPARE(result.status(), RobotKinematics::KinematicsStatus::Ok);
}

void RobotModelValidatorTests::reportsMissingBaseAndFlange()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.frames.baseLinkId.clear();
    config.frames.flangeLinkId.clear();

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(!result.ok());
    QVERIFY(hasIssueForField(result, "frames.base"));
    QVERIFY(hasIssueForField(result, "frames.flange"));
}

void RobotModelValidatorTests::reportsInvalidJointAxis()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.joints[2].axis = Eigen::Vector3d::Zero();

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(!result.ok());
    QVERIFY(hasIssueForField(result, "joints[2].axis"));
}

void RobotModelValidatorTests::reportsInvalidLimitsAndHome()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.joints[1].limits = RobotKinematics::JointLimits{2.0, -2.0, -1.0, 0.0};
    config.joints[1].home = 4.0;

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(!result.ok());
    QVERIFY(hasIssueForField(result, "joints[1].limits"));
    QVERIFY(hasIssueForField(result, "joints[1].limits.velocity"));
    QVERIFY(hasIssueForField(result, "joints[1].limits.acceleration"));
}

void RobotModelValidatorTests::reportsMalformedSerialChain()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.joints[3].parentLinkId = "base_link";

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(!result.ok());
    QVERIFY(hasIssueForField(result, "joints[3].parent"));
}

void RobotModelValidatorTests::reportsMissingDefaultTool()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.defaultToolId = "missing";

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(!result.ok());
    QCOMPARE(result.issues.back().status, RobotKinematics::KinematicsStatus::ToolNotFound);
    QVERIFY(hasIssueForField(result, "defaultTool"));
}

void RobotModelValidatorTests::acceptsFixedJointBeyondConfiguredDof()
{
    RobotKinematics::SerialRobotConfig config = validSixDofConfig();
    config.links.push_back({"tcp_mount"});
    config.frames.flangeLinkId = "tcp_mount";
    config.joints.back().childLinkId = "flange";

    RobotKinematics::Joint fixed;
    fixed.id = "flange_fixed";
    fixed.type = RobotKinematics::JointType::Fixed;
    fixed.parentLinkId = "flange";
    fixed.childLinkId = "tcp_mount";
    fixed.origin = RobotKinematics::Pose::fromXYZRPY_m_rad(0.1, 0.0, 0.0, 0.0, 0.0, 0.0);
    config.joints.push_back(fixed);

    const RobotKinematics::ModelValidationResult result =
        RobotKinematics::RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(result.ok());
}

/// Entry point that instantiates RobotModelValidatorTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runRobotModelValidatorTests(int argc, char** argv)
{
    RobotModelValidatorTests tests;
    return QTest::qExec(&tests, argc, argv);
}
