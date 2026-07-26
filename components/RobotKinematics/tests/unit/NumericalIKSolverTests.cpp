#include "NumericalIKSolverTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Solvers/NumericalIKSolver.h>

#include <Eigen/Geometry>

#include <QtTest/QtTest>

#include <cmath>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
/// Builds a single movable (revolute or prismatic) joint with the given parent/child link
/// ids, motion axis, and [lower, upper] limits; home position defaults to 0.
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

/// Builds a synthetic 6-DOF serial fixture: an XYZ prismatic Cartesian stage (+/-1 m each
/// axis) followed by an RxRyRz revolute wrist (+/-2 rad each axis), used as a known-solvable
/// target for IK convergence tests.
SerialRobotConfig cartesianWristFixture()
{
    SerialRobotConfig config;
    config.identity.vendor = "RobotKinematics";
    config.identity.model = "CartesianWristFixture";
    config.topology = RobotTopologyType::Serial;
    config.dof = 6;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"x"}, {"y"}, {"z"}, {"rx"}, {"ry"}, {"flange"}};
    config.joints = {
        movableJoint("J1", JointType::Prismatic, "base", "x", Eigen::Vector3d::UnitX(), -1.0, 1.0),
        movableJoint("J2", JointType::Prismatic, "x", "y", Eigen::Vector3d::UnitY(), -1.0, 1.0),
        movableJoint("J3", JointType::Prismatic, "y", "z", Eigen::Vector3d::UnitZ(), -1.0, 1.0),
        movableJoint("J4", JointType::Revolute, "z", "rx", Eigen::Vector3d::UnitX(), -2.0, 2.0),
        movableJoint("J5", JointType::Revolute, "rx", "ry", Eigen::Vector3d::UnitY(), -2.0, 2.0),
        movableJoint("J6", JointType::Revolute, "ry", "flange", Eigen::Vector3d::UnitZ(), -2.0, 2.0),
    };
    return config;
}

/// Computes the angle (in radians) of the relative rotation between poses `a` and `b`,
/// used as an orientation-error metric in IK convergence checks.
double orientationError_rad(const Pose& a, const Pose& b)
{
    const Eigen::AngleAxisd angleAxis(a.isometry().linear() * b.isometry().linear().transpose());
    return std::abs(angleAxis.angle());
}
}

void NumericalIKSolverTests::convergesToKnownSixDofFixtureFromNearbySeed()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const JointVector targetJoints = JointVector::fromRadians({0.25, -0.15, 0.4, 0.2, -0.3, 0.35});
    const Pose target = ForwardKinematics::flangePose(config, targetJoints);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = JointVector::fromRadians({0.2, -0.1, 0.35, 0.15, -0.25, 0.3});

    IKSolveContext context;
    context.targetFlangeInBase = target;
    context.request = request;

    NumericalIKDefaults defaults;
    defaults.maxSeeds = 4;
    NumericalIKSolver solver(defaults);
    const IKResult result = solver.solve(config, context);

    QVERIFY(result.ok());
    const Pose solved = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((solved.translation_m() - target.translation_m()).norm() <= 1e-6);
    QVERIFY(orientationError_rad(solved, target) <= 1.7453292519943296e-5);
}

void NumericalIKSolverTests::rejectsSeedDimensionMismatch()
{
    const SerialRobotConfig config = cartesianWristFixture();
    IKRequest request;
    request.targetPose = Pose::identity();
    request.seedJoint = JointVector::fromRadians({0.0, 0.0});

    IKSolveContext context;
    context.targetFlangeInBase = Pose::identity();
    context.request = request;

    const NumericalIKSolver solver;
    const IKResult result = solver.solve(config, context);

    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::JointDimensionMismatch);
}

void NumericalIKSolverTests::reportsFailureWhenIterationBudgetIsTooSmall()
{
    const SerialRobotConfig config = cartesianWristFixture();
    const JointVector targetJoints = JointVector::fromRadians({0.6, 0.4, -0.5, 0.8, -0.7, 0.6});
    const Pose target = ForwardKinematics::flangePose(config, targetJoints);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = JointVector::fromRadians({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});

    IKSolveContext context;
    context.targetFlangeInBase = target;
    context.request = request;

    NumericalIKDefaults defaults;
    defaults.maxIterations = 1;
    defaults.maxSeeds = 1;
    NumericalIKSolver solver(defaults);
    const IKResult result = solver.solve(config, context);

    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::MaxIterationsReached);
}

/// Entry point that instantiates NumericalIKSolverTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runNumericalIKSolverTests(int argc, char** argv)
{
    NumericalIKSolverTests tests;
    return QTest::qExec(&tests, argc, argv);
}
