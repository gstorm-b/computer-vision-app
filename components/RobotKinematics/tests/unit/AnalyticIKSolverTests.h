#pragma once

#include <QObject>

/// Unit test suite for Analytic6DofSphericalWristSolver: checks model-support detection,
/// the numerical-solver fallback for unsupported morphologies, and that analytic solves
/// reproduce target poses (across all discrete branches and for a seeded configuration).
class AnalyticIKSolverTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies the solver reports support for the Nachi MZ04D preset, whose J4/J5/J6
    /// axes intersect at a single wrist center.
    void supportsSphericalWristModel();
    /// Verifies the solver reports no support for Virtual6DofTestArm, whose wrist axes
    /// are offset and do not intersect at a single point.
    void rejectsNonSphericalWristModel();
    /// Verifies that solving IK for an unsupported (non-spherical-wrist) model via the
    /// SerialRobotKinematics facade falls back to the numerical solver and still reaches
    /// the target pose within tolerance.
    void unsupportedModelFallsBackToNumerical();
    /// Runs solveAll() on the Nachi MZ04D preset across five non-singular joint samples
    /// and verifies every returned discrete branch reproduces the target pose's
    /// translation and rotation within tolerance.
    void analyticSolveAllBranchesReproduceTargetPose();
    /// Verifies that solving IK seeded at a real teach-pendant joint configuration
    /// recovers that same configuration (and the exact target pose) to analytic
    /// precision, tighter than the numerical solver's tolerance.
    void analyticSolveRecoversSeededConfiguration();
};
