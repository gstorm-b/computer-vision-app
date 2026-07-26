#include "IKApiTests.h"

#include <RobotKinematics/Kinematics/InverseKinematics.h>
#include <RobotKinematics/Solvers/IKSolver.h>

#include <QtTest/QtTest>

using namespace RobotKinematics;

namespace {
/// Test-double IKSolver that always reports UnsupportedSolver, used to verify callers can
/// observe a structured solver failure instead of a solution.
class RejectingSolver : public IKSolver
{
public:
    /// Returns the fixed solver name "rejecting".
    const char* name() const override { return "rejecting"; }

    /// Always fails with IKStatus::UnsupportedSolver and message "unsupported"; ignores its
    /// arguments.
    IKResult solve(const SerialRobotConfig&, const IKSolveContext&) const override
    {
        return IKResult{IKStatus::UnsupportedSolver, {}, "unsupported"};
    }

    /// Delegates to solve(), so it fails the same way as the single-solution variant.
    IKResult solveAll(const SerialRobotConfig& config, const IKSolveContext& context) const override
    {
        return solve(config, context);
    }
};
}

void IKApiTests::resultOkRequiresSolution()
{
    IKResult emptyOk;
    emptyOk.status = IKStatus::Ok;
    QVERIFY(!emptyOk.ok());

    IKSolution solution;
    solution.joints = JointVector::fromRadians({0.0});
    IKResult result;
    result.status = IKStatus::Ok;
    result.solutions.push_back(solution);

    QVERIFY(result.ok());
    QCOMPARE(result.best().joints.size(), 1);
}

void IKApiTests::requestDefaultsUseBaseFrameAndDefaultTool()
{
    IKRequest request;

    QVERIFY(request.referenceFrame.empty());
    QVERIFY(request.tool.empty());
    QVERIFY(request.options.returnClosestToSeed);
    QVERIFY(!request.options.requirePosture);
    QCOMPARE(request.options.maxSolutions, 16);
    QCOMPARE(request.options.maxPositionError_m, 1e-6);
    QCOMPARE(request.options.maxOrientationError_rad, 1.7453292519943296e-5);
}

void IKApiTests::solverInterfaceCanReturnStructuredFailure()
{
    RejectingSolver solver;
    SerialRobotConfig config;
    IKSolveContext context;

    const IKResult result = solver.solve(config, context);

    QCOMPARE(std::string(solver.name()), std::string("rejecting"));
    QVERIFY(!result.ok());
    QCOMPARE(result.status, IKStatus::UnsupportedSolver);
    QCOMPARE(result.message, std::string("unsupported"));
}

/// Entry point that instantiates IKApiTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runIKApiTests(int argc, char** argv)
{
    IKApiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
