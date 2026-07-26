#pragma once

#include <QObject>

/// QtTest suite covering ForwardKinematics tool/user-frame composition and JointLimitValidator's
/// dimension check, using a single-joint fixture with a user frame offset from the base link.
class FrameToolFkTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies ForwardKinematics::toolPose() composes the flange pose with a tool offset along
    /// Z, comparing against flangePose() at the same joint state.
    void toolPoseComposesFlangeAndTcp();
    /// Verifies ForwardKinematics::userFrameInBase() places the user frame correctly in the base
    /// frame, and that expressing the flange pose in that user frame yields the expected relative
    /// translation.
    void userFrameRelativePose();
    /// Verifies JointLimitValidator::validate() reports KinematicsStatus::JointDimensionMismatch
    /// when given a joint vector with more elements than the robot's DOF.
    void jointLimitValidatorRejectsWrongDimension();
};
