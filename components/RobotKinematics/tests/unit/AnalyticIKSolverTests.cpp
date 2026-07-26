#include "AnalyticIKSolverTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>
#include <RobotKinematics/Presets/Virtual6DofTestArm.h>
#include <RobotKinematics/Solvers/Analytic6DofSphericalWristSolver.h>

#include <QtTest/QtTest>

#include <array>

using namespace RobotKinematics;

void AnalyticIKSolverTests::supportsSphericalWristModel()
{
    // Nachi MZ04D has a spherical wrist (J4/J5/J6 axes intersect at the wrist center).
    const Analytic6DofSphericalWristSolver solver;
    QVERIFY(solver.supportsModel(Presets::nachiMZ04D()));
}

void AnalyticIKSolverTests::rejectsNonSphericalWristModel()
{
    // Virtual6DofTestArm uses offset wrist axes that do not intersect at a single point.
    const Analytic6DofSphericalWristSolver solver;
    QVERIFY(!solver.supportsModel(Presets::virtual6DofTestArm()));
}

void AnalyticIKSolverTests::unsupportedModelFallsBackToNumerical()
{
    // For an unsupported morphology the facade must fall back to the numerical solver.
    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    const Analytic6DofSphericalWristSolver analytic;
    QVERIFY(!analytic.supportsModel(config));

    const SerialRobotKinematics robot(config);
    const JointVector seed = JointVector::fromRadians({0.25, -0.35, 0.45, 0.2, -0.3, 0.15});
    const Pose target = ForwardKinematics::flangePose(config, seed);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = seed;

    const IKResult result = robot.solve(request);
    QVERIFY(result.ok());
    const Pose solved = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((solved.translation_m() - target.translation_m()).norm() <= 1e-6);
}

void AnalyticIKSolverTests::analyticSolveAllBranchesReproduceTargetPose()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const Analytic6DofSphericalWristSolver solver;
    QVERIFY(solver.supportsModel(config));

    // A set of non-singular configurations spanning the workspace.
    const std::array<std::array<double, 6>, 5> samples = {{
        {{25.0, -20.0, 165.0, 10.0, 35.0, 150.0}},
        {{-40.0, 30.0, 90.0, -60.0, 60.0, -120.0}},
        {{120.0, -60.0, 200.0, 90.0, -45.0, 30.0}},
        {{-90.0, 100.0, 60.0, 150.0, 80.0, 200.0}},
        {{60.0, -30.0, 130.0, -100.0, -70.0, -200.0}},
    }};

    for (const std::array<double, 6>& s : samples) {
        const JointVector q =
            JointVector::fromDegrees({s[0], s[1], s[2], s[3], s[4], s[5]});
        const Pose target = ForwardKinematics::flangePose(config, q);

        IKSolveContext context;
        context.targetFlangeInBase = target;

        const IKResult result = solver.solveAll(config, context);
        QVERIFY(result.ok());
        QVERIFY(result.solutions.size() >= 2u); // at least a couple of discrete branches

        for (const IKSolution& solution : result.solutions) {
            const Pose fk = ForwardKinematics::flangePose(config, solution.joints);
            QVERIFY((fk.translation_m() - target.translation_m()).norm() <= 1e-6);
            QVERIFY(fk.rotationQuaternion().angularDistance(target.rotationQuaternion()) <= 1e-6);
        }
    }
}

void AnalyticIKSolverTests::analyticSolveRecoversSeededConfiguration()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const SerialRobotKinematics robot(config);

    // A real teach-pendant configuration (point 1 from the MZ04D reference).
    const JointVector seed =
        JointVector::fromDegrees({28.1579, -18.8069, 163.839, -0.710019, 35.8922, 152.731});
    const Pose target = ForwardKinematics::flangePose(config, seed);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = seed;

    const IKResult result = robot.solve(request);
    QVERIFY(result.ok());

    // The facade now routes MZ04D to the analytic solver; the returned best solution must
    // reproduce the pose to analytic precision (tighter than the numerical tolerance).
    const Pose fk = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((fk.translation_m() - target.translation_m()).norm() <= 1e-9);
    QVERIFY(fk.rotationQuaternion().angularDistance(target.rotationQuaternion()) <= 1e-9);
    QVERIFY((result.best().joints.values() - seed.values()).norm() <= 1e-6);
}

/// Entry point invoked by TestMain to run the AnalyticIKSolverTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runAnalyticIKSolverTests(int argc, char** argv)
{
    AnalyticIKSolverTests tests;
    return QTest::qExec(&tests, argc, argv);
}
