#pragma once

#include <QObject>

/// QtTest suite covering Robot3DVisualizer's helper logic: Nachi pendant pose conversion,
/// millimeter visual-delta matrices, posture label mapping, kinematics status text, and
/// mesh-profile path derivation.
class Robot3DVisualizerLogicTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies fromNachiPendantPose()/toNachiPendantPose() round-trip a pose through the
    /// pendant's mm/deg XYZ + RZ/RY/RX display convention without loss.
    void convertsBetweenNachiPendantOrderAndPose();
    /// Verifies visualDeltaMatrixMm() expresses a pose's translation relative to a home pose
    /// in millimeters and its rotation as the relative rotation matrix.
    void buildsMillimeterVisualDeltaMatrixFromHomePose();
    /// Verifies visualDeltaMatrixMm()'s optional home-visual-correction argument recenters a
    /// tool mount/TCP marker pair onto the expected corrected offsets.
    void appliesCenteringToolHomeVisualCorrection();
    /// Verifies postureLabel() maps a signed branch value to its configured label string, and
    /// returns "Any" when the branch is unset (std::nullopt).
    void mapsPostureBranchesToConfiguredLabels();
    /// Verifies statusText() renders KinematicsStatus enum values as human-readable UI strings
    /// (e.g. "Ok", "Joint limit violation", "Posture constraint unsatisfied").
    void formatsKinematicsStatusesForUi();
    /// Verifies meshProfileSimplifiedPath() inserts a "_simplified" suffix before the
    /// extension, covering empty input, bare filenames, and extensionless absolute paths.
    void derivesSimplifiedMeshProfilePathBesideOriginal();
};
