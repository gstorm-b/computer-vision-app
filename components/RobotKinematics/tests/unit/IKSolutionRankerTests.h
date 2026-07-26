#pragma once

#include <QObject>

/// QtTest suite covering IKSolutionRanker's posture-matching and posture-mismatch scoring
/// logic.
class IKSolutionRankerTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies IKSolutionRanker::postureMatches() returns true when candidate and requested
    /// posture fields agree, and false once a field (elbow) is set to a conflicting value.
    void detectsPostureMatchAndMismatch();
    /// Verifies IKSolutionRanker::score() adds a positive postureMismatchCost when the
    /// requested shoulder posture differs from the solution's posture, and that this cost is
    /// folded into totalCost.
    void addsPostureMismatchCostWhenPreferenceIsSoft();
};
