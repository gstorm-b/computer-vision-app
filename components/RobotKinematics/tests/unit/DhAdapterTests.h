#pragma once

#include <QObject>

/// QtTest suite covering DhAdapter::fromStandardDh() conversion of standard DH parameters into
/// a SerialRobotConfig usable by ForwardKinematics.
class DhAdapterTests : public QObject
{
    Q_OBJECT

private slots:
    /// Builds a 2-joint planar robot from standard DH parameters (link lengths only) and
    /// verifies its forward-kinematics flange position matches the closed-form planar-2R
    /// solution for several joint angles.
    void standardDhPlanarTwoJointMatchesExpectedFk();
};
