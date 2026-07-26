#include <RobotKinematics/Core/Units.h>

#include <cmath>

/// Unit-conversion helpers used throughout RobotKinematics to move between the library's
/// canonical internal units (meters, radians) and the millimeter/degree units common in
/// authoring/UI-facing data.
namespace RobotKinematics::units {

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;  ///< Pi, used to derive the deg/rad conversion factors below.
constexpr double kMetersPerMillimeter = 0.001;                  ///< Meters per millimeter.
constexpr double kDegreesPerRadian = 180.0 / kPi;                ///< Degrees per radian.
constexpr double kRadiansPerDegree = kPi / 180.0;                ///< Radians per degree.
}

/// Converts a length from millimeters to the library's canonical unit, meters.
double mm(double value_mm)
{
    return value_mm * kMetersPerMillimeter;
}

/// Converts an angle from degrees to the library's canonical unit, radians.
double deg(double value_deg)
{
    return value_deg * kRadiansPerDegree;
}

/// Converts a length from meters to millimeters.
double toMm(double value_m)
{
    return value_m / kMetersPerMillimeter;
}

/// Converts an angle from radians to degrees.
double toDeg(double value_rad)
{
    return value_rad * kDegreesPerRadian;
}

} // namespace RobotKinematics::units
