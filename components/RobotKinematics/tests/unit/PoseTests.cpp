#include "PoseTests.h"

#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Units.h>

#include <QtTest/QtTest>

#include <cmath>

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Returns true if `lhs` and `rhs` differ by no more than `tolerance` (absolute difference).
bool nearlyEqual(double lhs, double rhs, double tolerance = 1e-12)
{
    return std::abs(lhs - rhs) <= tolerance;
}

/// Returns true if the Euclidean distance between `lhs` and `rhs` is within `tolerance`.
bool vectorNearlyEqual(const Eigen::Vector3d& lhs, const Eigen::Vector3d& rhs, double tolerance = 1e-12)
{
    return (lhs - rhs).norm() <= tolerance;
}
}

void PoseTests::identityHasNoTranslationOrRotation()
{
    const RobotKinematics::Pose pose = RobotKinematics::Pose::identity();

    QVERIFY(vectorNearlyEqual(pose.translation_m(), Eigen::Vector3d::Zero()));
    QVERIFY(vectorNearlyEqual(pose.isometry().linear() * Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitX()));
}

void PoseTests::fromIsometryPreservesTransform()
{
    Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
    transform.translation() = Eigen::Vector3d(1.0, 2.0, 3.0);
    transform.linear() = Eigen::AngleAxisd(kPi / 2.0, Eigen::Vector3d::UnitZ()).toRotationMatrix();

    const RobotKinematics::Pose pose = RobotKinematics::Pose::fromIsometry(transform);

    QVERIFY(vectorNearlyEqual(pose.translation_m(), Eigen::Vector3d(1.0, 2.0, 3.0)));
    QVERIFY(vectorNearlyEqual(pose.isometry().linear() * Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitY()));
}

void PoseTests::fromXYZRPY_m_radUsesFixedAxisRpyConvention()
{
    const RobotKinematics::Pose roll =
        RobotKinematics::Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, kPi / 2.0, 0.0, 0.0);
    QVERIFY(vectorNearlyEqual(roll.isometry().linear() * Eigen::Vector3d::UnitY(), Eigen::Vector3d::UnitZ()));

    const RobotKinematics::Pose combined =
        RobotKinematics::Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, kPi / 2.0, kPi / 2.0, kPi / 2.0);
    const Eigen::Matrix3d expected =
        (Eigen::AngleAxisd(kPi / 2.0, Eigen::Vector3d::UnitZ())
         * Eigen::AngleAxisd(kPi / 2.0, Eigen::Vector3d::UnitY())
         * Eigen::AngleAxisd(kPi / 2.0, Eigen::Vector3d::UnitX()))
            .toRotationMatrix();

    QVERIFY((combined.isometry().linear() - expected).norm() < 1e-12);
}

void PoseTests::fromXYZRPY_mm_degConvertsExplicitUnits()
{
    const RobotKinematics::Pose pose =
        RobotKinematics::Pose::fromXYZRPY_mm_deg(100.0, 200.0, -300.0, 0.0, 0.0, 90.0);

    QVERIFY(vectorNearlyEqual(pose.translation_m(), Eigen::Vector3d(0.1, 0.2, -0.3)));
    QVERIFY(vectorNearlyEqual(pose.isometry().linear() * Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitY()));
}

void PoseTests::composesTransforms()
{
    const RobotKinematics::Pose baseToA =
        RobotKinematics::Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, kPi / 2.0);
    const RobotKinematics::Pose aToB =
        RobotKinematics::Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    const RobotKinematics::Pose baseToB = baseToA * aToB;

    QVERIFY(vectorNearlyEqual(baseToB.translation_m(), Eigen::Vector3d(1.0, 1.0, 0.0)));
    QVERIFY((baseToB.inverse() * baseToB).isometry().matrix().isApprox(Eigen::Matrix4d::Identity(), 1e-12));
    QVERIFY(nearlyEqual(baseToB.rotationQuaternion().norm(), 1.0));
}

/// Entry point that instantiates PoseTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runPoseTests(int argc, char** argv)
{
    PoseTests tests;
    return QTest::qExec(&tests, argc, argv);
}
