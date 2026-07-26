#pragma once

#include <QObject>

/// Integration test suite for the Virtual6DofTestArm preset: validates the built-in C++
/// fallback config, cross-checks it against the on-disk JSON preset, and exercises a
/// full forward/inverse kinematics round trip on the model.
class Virtual6DofTestArmTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies the C++ fallback preset passes RobotModelValidator and carries the
    /// expected DOF, joint/tool counts, user frames, posture resolver, and sources.
    void fallbackPresetIsValidAndHasRequiredMetadata();
    /// Loads the JSON preset from disk and compares every solver-facing field against
    /// the C++ fallback via compareSolverFacingConfig().
    void jsonPresetMatchesCppFallbackForSolverFacingFields();
    /// Runs forward kinematics on a sample joint configuration to get a target pose,
    /// then verifies IK seeded at that same joint configuration recovers it within tolerance.
    void presetRunsFkAndSeededIkRoundTrip();
};
