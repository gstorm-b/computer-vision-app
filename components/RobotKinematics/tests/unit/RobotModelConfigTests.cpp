#include "RobotModelConfigTests.h"

#include <RobotKinematics/Model/RobotModelConfig.h>

#include <QtTest/QtTest>

namespace {
/// Returns true if the Euclidean distance between `lhs` and `rhs` is within `tolerance`.
bool vectorNearlyEqual(const Eigen::Vector3d& lhs, const Eigen::Vector3d& rhs, double tolerance = 1e-12)
{
    return (lhs - rhs).norm() <= tolerance;
}
}

void RobotModelConfigTests::representsSerialSixDofRobotConfig()
{
    RobotKinematics::SerialRobotConfig config;
    config.identity.vendor = "RobotKinematics";
    config.identity.model = "Virtual6DofTestArm";
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
        joint.limits = RobotKinematics::JointLimits{-3.14, 3.14, std::nullopt, std::nullopt};
        config.joints.push_back(joint);
    }

    config.tools.push_back({"default", "Default Tool", RobotKinematics::Pose::identity()});
    config.defaultToolId = "default";

    QCOMPARE(config.joints.size(), static_cast<std::size_t>(6));
    QCOMPARE(config.links.front().id, std::string("base_link"));
    QCOMPARE(config.links.back().id, std::string("flange"));
    QVERIFY(vectorNearlyEqual(config.tools.front().flangeToTcp.translation_m(), Eigen::Vector3d::Zero()));
}

void RobotModelConfigTests::supportsOptionalVelocityAndAccelerationLimits()
{
    RobotKinematics::JointLimits limits;
    limits.lower = -1.0;
    limits.upper = 1.0;
    limits.velocity = 2.0;
    limits.acceleration = 10.0;

    QVERIFY(limits.velocity.has_value());
    QVERIFY(limits.acceleration.has_value());
    QCOMPARE(*limits.velocity, 2.0);
    QCOMPARE(*limits.acceleration, 10.0);
}

/// Entry point that instantiates RobotModelConfigTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runRobotModelConfigTests(int argc, char** argv)
{
    RobotModelConfigTests tests;
    return QTest::qExec(&tests, argc, argv);
}
