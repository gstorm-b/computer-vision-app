#pragma once

/// Unit-conversion helpers between the library's SI internals (meters, radians) and
/// the millimeter/degree units commonly used at API boundaries.
namespace RobotKinematics::units {

/// Converts a length from millimeters to meters.
double mm(double value_mm);
/// Converts an angle from degrees to radians.
double deg(double value_deg);

/// Converts a length from meters to millimeters.
double toMm(double value_m);
/// Converts an angle from radians to degrees.
double toDeg(double value_rad);

} // namespace RobotKinematics::units
