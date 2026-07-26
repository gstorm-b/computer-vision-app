#pragma once

#include <QObject>

/// QtTest suite covering ForwardKinematics::flangePose()/toolPose() across single-joint,
/// two-joint planar, and six-joint serial-chain configurations.
class ForwardKinematicsTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies a single revolute joint whose axis passes through the flange leaves the flange
    /// position invariant under rotation, that orientation follows the joint angle, and that a
    /// tool offset swings the TCP about the joint axis.
    void singleRevoluteJointWithOffset();
    /// Verifies a 2-joint planar arm's tool-frame position matches the closed-form planar-2R
    /// solution across several (q1, q2) angle combinations.
    void twoJointPlanarArmWithTool();
    /// Verifies a 6-joint tower (all Z-axis joints stacked along Z) keeps the flange position
    /// invariant and composes orientation as Rz(sum of joint angles).
    void sixJointTowerComposition();
};
