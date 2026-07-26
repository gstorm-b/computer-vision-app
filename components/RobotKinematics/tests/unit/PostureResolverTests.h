#pragma once

#include <QObject>

/// QtTest suite covering Serial6DofSignPostureResolver's mapping between configured posture
/// labels and generic +/-1 branch codes, and its joint-sign-based posture classification for
/// a serial 6-DOF arm.
class PostureResolverTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies fromLabels() maps configured shoulder/elbow/wrist label strings (e.g.
    /// "lefty"/"above"/"flip") to the correct generic +/-1 branch values.
    void mapsConfiguredLabelsToGenericBranches();
    /// Verifies classify() derives shoulder/elbow/wrist branch signs from a joint vector's
    /// sign pattern using the default serial_6dof_shoulder_elbow_wrist resolver.
    void classifiesSerialSixDofSignBranches();
};
