#pragma once

#include <QObject>

/// QtTest suite covering RobotKinematics::SerialRobotConfig's ability to represent a serial
/// 6-DOF robot and JointLimits' optional velocity/acceleration fields.
class RobotModelConfigTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies a SerialRobotConfig populated with 6 revolute joints, 7 links, and a default
    /// tool exposes the expected joint count, link ordering, and identity-pose default tool.
    void representsSerialSixDofRobotConfig();
    /// Verifies JointLimits::velocity/acceleration are std::optional fields that report
    /// has_value() and hold the assigned values once set.
    void supportsOptionalVelocityAndAccelerationLimits();
};
