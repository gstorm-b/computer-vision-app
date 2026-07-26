#pragma once

#include <QObject>

/// QtTest suite validating the Nachi MZ04D preset (Presets::nachiMZ04D()) and its JSON
/// equivalent against real teach-pendant measurements: config validity, JSON/C++ parity,
/// forward-kinematics accuracy (with and without a tool offset), posture classification, and
/// an end-to-end FK/IK round trip.
class NachiMZ04DTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies the C++ fallback preset passes RobotModelValidator and has the expected
    /// vendor/model identity, DOF, joint/tool counts, user frames, posture resolver name, and
    /// non-empty sources list.
    void fallbackPresetIsValidAndHasRequiredMetadata();
    /// Loads the on-disk JSON preset and compares it field-by-field against the C++ fallback
    /// (identity, DOF, frame ids, default tool, posture resolver, and per-joint id/parent/
    /// child/axis/origin/limits) to guard against the two definitions drifting apart.
    void jsonPresetMatchesCppFallbackForSolverFacingFields();
    /// Runs ForwardKinematics::flangePose() over 21 recorded teach-pendant poses and checks
    /// the computed position/orientation stay within the documented DH-fit tolerances
    /// (<= 0.04 mm, <= 0.012 deg).
    void forwardKinematicsMatchesTeachPendantPoses();
    /// Runs ForwardKinematics::toolPose() with a fixed (44.2, 0, 139.0) mm tool offset over 4
    /// measured arm-config poses and checks the result stays within tolerance (<= 0.05 mm,
    /// <= 0.007 deg).
    void forwardKinematicsWithToolMatchesMeasuredPoses();
    /// Checks that PostureResolver::classify() reproduces the Nachi manual's shoulder/elbow/
    /// wrist labels for 6 joint configurations (4 from measured data, 2 synthetic to cover the
    /// elbow-above and wrist-flip branches), and that fromLabels() maps labels back to the
    /// expected branch signs.
    void postureClassificationMatchesNachiManualRules();
    /// Computes a target pose via FK from a known-good seed joint vector, solves IK for that
    /// target using the same seed, and checks the solved joints reproduce the target position
    /// to within 1e-6 m.
    void presetRunsFkAndSeededIkRoundTrip();
};
