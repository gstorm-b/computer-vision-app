#pragma once

#include <QObject>

/// QtTest suite validating custom (non-preset) robot configuration paths: building a config via
/// SerialRobotConfigBuilder and solving IK on it, and PresetJsonLoader's rejection of unknown
/// top-level JSON fields and non-canonical units.
class CustomPresetTests : public QObject
{
    Q_OBJECT

private slots:
    /// Builds a 6-DOF Cartesian-wrist config via SerialRobotConfigBuilder, computes a target
    /// flange pose from known joint angles by forward kinematics, and verifies IK solves back to
    /// that pose from a nearby seed.
    void builderCreatesCustomSixDofConfigThatSolvesIk();
    /// Verifies PresetJsonLoader::loadJson() rejects a JSON document containing an "unexpected"
    /// top-level field with KinematicsStatus::InvalidRobotConfig.
    void jsonLoaderRejectsUnknownTopLevelField();
    /// Verifies PresetJsonLoader::loadJson() rejects non-canonical units (mm/deg instead of
    /// m/rad) with KinematicsStatus::InvalidRobotConfig.
    void jsonLoaderRejectsNonCanonicalUnits();
};
