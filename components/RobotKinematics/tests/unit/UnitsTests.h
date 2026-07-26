#pragma once

#include <QObject>

/// QtTest suite for RobotKinematics::units: covers millimeter/meter and degree/radian
/// conversion helpers in both directions.
class UnitsTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies units::mm() divides a millimeter value by 1000 to yield meters (0, 1000, and
    /// a negative value).
    void convertsMillimetersToMeters();
    /// Verifies units::toMm() multiplies a meter value by 1000 to yield millimeters (0, 1, and
    /// a negative value).
    void convertsMetersToMillimeters();
    /// Verifies units::deg() converts a degree value to radians (180 degrees to pi, -90 degrees
    /// to -pi/2), within a 1e-12 tolerance.
    void convertsDegreesToRadians();
    /// Verifies units::toDeg() converts a radian value to degrees (pi to 180, -pi/2 to -90),
    /// within a 1e-12 tolerance.
    void convertsRadiansToDegrees();
};
