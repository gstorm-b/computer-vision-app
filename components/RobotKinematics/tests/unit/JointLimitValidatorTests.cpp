#include "JointLimitValidatorTests.h"

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Kinematics/JointLimitValidator.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Builds a minimal 3-DOF serial revolute chain (base-link1-link2-flange), each joint
/// limited to [-pi, pi] about its local Z axis, for use as a JointLimitValidator fixture.
SerialRobotConfig threeJointConfig()
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.dof = 3;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"link1"}, {"link2"}, {"flange"}};

    const char* parents[] = {"base", "link1", "link2"};
    const char* children[] = {"link1", "link2", "flange"};
    for (int i = 0; i < 3; ++i) {
        Joint joint;
        joint.id = "J" + std::to_string(i + 1);
        joint.type = JointType::Revolute;
        joint.parentLinkId = parents[i];
        joint.childLinkId = children[i];
        joint.axis = Eigen::Vector3d::UnitZ();
        joint.limits = JointLimits{-kPi, kPi, std::nullopt, std::nullopt};
        config.joints.push_back(joint);
    }
    return config;
}
}

void JointLimitValidatorTests::jointVectorConvertsRadiansAndDegrees()
{
    const JointVector r = JointVector::fromRadians({0.0, kPi / 2.0});
    QCOMPARE(r.size(), 2);
    QVERIFY(std::abs(r[1] - kPi / 2.0) < 1e-12);

    const JointVector d = JointVector::fromDegrees({0.0, 90.0, -45.0});
    QCOMPARE(d.size(), 3);
    QVERIFY(std::abs(d[1] - kPi / 2.0) < 1e-12);
    QVERIFY(std::abs(d[2] + kPi / 4.0) < 1e-12);
}

void JointLimitValidatorTests::acceptsJointsInsideLimits()
{
    const JointLimitCheck check =
        JointLimitValidator::validate(threeJointConfig(), JointVector::fromRadians({0.0, 1.0, -1.0}));

    QVERIFY(check.ok());
    QCOMPARE(check.status, KinematicsStatus::Ok);
    QVERIFY(check.violations.empty());
}

void JointLimitValidatorTests::rejectsJointBelowMinimum()
{
    const JointLimitCheck check =
        JointLimitValidator::validate(threeJointConfig(), JointVector::fromRadians({0.0, -4.0, 0.0}));

    QCOMPARE(check.status, KinematicsStatus::JointLimitViolation);
    QCOMPARE(check.violations.size(), static_cast<std::size_t>(1));
    QCOMPARE(check.violations.front().jointIndex, 1);
    QCOMPARE(check.violations.front().jointId, std::string("J2"));
}

void JointLimitValidatorTests::rejectsJointAboveMaximum()
{
    const JointLimitCheck check =
        JointLimitValidator::validate(threeJointConfig(), JointVector::fromRadians({4.0, 0.0, 0.0}));

    QCOMPARE(check.status, KinematicsStatus::JointLimitViolation);
    QCOMPARE(check.violations.front().jointIndex, 0);
}

void JointLimitValidatorTests::rejectsWrongDimension()
{
    const JointLimitCheck check =
        JointLimitValidator::validate(threeJointConfig(), JointVector::fromRadians({0.0, 0.0}));

    QCOMPARE(check.status, KinematicsStatus::JointDimensionMismatch);
}

/// Entry point that instantiates JointLimitValidatorTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runJointLimitValidatorTests(int argc, char** argv)
{
    JointLimitValidatorTests tests;
    return QTest::qExec(&tests, argc, argv);
}
