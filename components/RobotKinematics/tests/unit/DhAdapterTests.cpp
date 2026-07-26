#include "DhAdapterTests.h"

#include <RobotKinematics/Adapters/DhAdapter.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>

#include <QtTest/QtTest>

#include <cmath>
#include <optional>

using namespace RobotKinematics;

void DhAdapterTests::standardDhPlanarTwoJointMatchesExpectedFk()
{
    const double l1 = 0.5;
    const double l2 = 0.3;
    const double q1 = 0.4;
    const double q2 = -0.25;

    StandardDhParameter j1;
    j1.jointId = "J1";
    j1.a_m = l1;
    j1.limits = JointLimits{-3.14, 3.14, std::nullopt, std::nullopt};
    StandardDhParameter j2;
    j2.jointId = "J2";
    j2.a_m = l2;
    j2.limits = JointLimits{-3.14, 3.14, std::nullopt, std::nullopt};

    const Result<SerialRobotConfig> config =
        DhAdapter::fromStandardDh(RobotIdentity{"RobotKinematics", "Planar2R", "Planar 2R", "1.0"}, {j1, j2});

    QVERIFY(config.ok());
    const Pose flange = ForwardKinematics::flangePose(config.value, JointVector::fromRadians({q1, q2}));
    const Eigen::Vector3d expected(l1 * std::cos(q1) + l2 * std::cos(q1 + q2),
                                   l1 * std::sin(q1) + l2 * std::sin(q1 + q2),
                                   0.0);
    QVERIFY((flange.translation_m() - expected).norm() <= 1e-12);
}

/// Entry point that instantiates DhAdapterTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runDhAdapterTests(int argc, char** argv)
{
    DhAdapterTests tests;
    return QTest::qExec(&tests, argc, argv);
}
