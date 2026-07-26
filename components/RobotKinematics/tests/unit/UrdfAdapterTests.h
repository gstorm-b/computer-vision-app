#pragma once

#include <QObject>

/// QtTest suite for UrdfAdapter: verifies that exporting a SerialRobotConfig produces the
/// expected URDF structure, and that exporting then re-importing a config preserves its
/// forward-kinematics behavior.
class UrdfAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    /// Exports a planar 2R config (with a tool and posture resolver set) via
    /// UrdfAdapter::exportSerialRobot and checks the resulting URDF text contains the robot
    /// name, base link, first revolute joint, and a warning about metadata not represented
    /// in URDF.
    void exportIncludesLinksJointsAndMetadataWarning();
    /// Exports a planar 2R config to URDF, re-imports it via UrdfAdapter::importSerialRobot,
    /// and verifies the imported config's flange pose (via ForwardKinematics) matches the
    /// original's at a sample joint configuration to within 1e-9.
    void importExportedSerialChainAndPreserveFk();
};
