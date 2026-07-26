#pragma once

#include <QObject>

/// Minimal QtTest suite that verifies the RobotKinematics test harness itself (build, link,
/// and QtTest execution) is functioning.
class SmokeTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies the QtTest harness runs and that RobotKinematics::libraryAnchor() links
    /// correctly and returns 0.
    void qtTestHarnessRuns();
};
