#pragma once

#include <QObject>

/// QtTest suite covering NumericalIKSolver convergence and failure handling against a
/// synthetic 6-DOF Cartesian-wrist fixture (3 prismatic + 3 revolute joints).
class NumericalIKSolverTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies the solver converges from a seed close to a known solution to within
    /// 1e-6 m translation and ~1e-5 rad orientation error of the target pose.
    void convergesToKnownSixDofFixtureFromNearbySeed();
    /// Verifies a seed joint vector of the wrong dimension is rejected with
    /// IKStatus::JointDimensionMismatch instead of being solved.
    void rejectsSeedDimensionMismatch();
    /// Verifies that limiting maxIterations/maxSeeds to 1 makes the solver report
    /// IKStatus::MaxIterationsReached rather than silently returning a poor solution.
    void reportsFailureWhenIterationBudgetIsTooSmall();
};
