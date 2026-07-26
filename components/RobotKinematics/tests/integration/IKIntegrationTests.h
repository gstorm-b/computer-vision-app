#pragma once

#include <QObject>

/// QtTest suite covering SerialRobotKinematics::solve()/solveAll() on a custom Cartesian-wrist
/// fixture: default-tool solving, multi-solution retrieval, tool/frame-not-found errors, resolving
/// a non-default tool via a base-fixed user frame, and posture-constraint rejection.
class IKIntegrationTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies solve() with the default tool converges to the single expected solution within
    /// 1e-6 m position and ~1e-5 rad orientation tolerance of a known target pose.
    void solveUsesDefaultToolAndReturnsBestSolution();
    /// Verifies solveAll() with maxSolutions=4 returns a non-empty solution set for a reachable
    /// target pose.
    void solveAllReturnsFoundSolutions();
    /// Verifies solve() reports IKStatus::ToolNotFound for an unknown ToolId and
    /// IKStatus::FrameNotFound for an unknown reference FrameId.
    void missingToolAndFrameReturnStructuredStatus();
    /// Verifies solve() resolves a target expressed in a non-base user frame ("vision") for a
    /// non-default tool ("probe"), converging to the expected TCP pose within tolerance.
    void solveResolvesBaseFixedUserFrameAndTool();
    /// Verifies solve() with options.requirePosture set rejects a solution whose posture does not
    /// match the requested ArmPosture, returning IKStatus::PostureConstraintUnsatisfied.
    void requirePostureRejectsMismatchedSolution();
};
