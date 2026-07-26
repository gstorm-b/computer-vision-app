#pragma once

#include <QObject>

/// QtTest suite covering the plain IK data types (IKResult::ok()/best(), IKRequest defaults)
/// and the IKSolver interface's ability to report a structured failure.
class IKApiTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies IKResult::ok() requires both an Ok status and at least one solution: an
    /// Ok-status result with no solutions is not ok(), while one with a pushed solution is.
    void resultOkRequiresSolution();
    /// Verifies a default-constructed IKRequest has an empty referenceFrame/tool (implying
    /// base frame and default tool), returnClosestToSeed set, requirePosture unset, and the
    /// documented default maxSolutions/maxPositionError_m/maxOrientationError_rad values.
    void requestDefaultsUseBaseFrameAndDefaultTool();
    /// Verifies a custom IKSolver (RejectingSolver) can report UnsupportedSolver status with a
    /// message via solve(), and that name() and result.ok()/status/message all reflect it.
    void solverInterfaceCanReturnStructuredFailure();
};
