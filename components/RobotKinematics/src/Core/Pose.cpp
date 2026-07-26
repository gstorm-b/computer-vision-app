#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Units.h>

namespace RobotKinematics {

/// Default-constructs the identity pose (zero translation, no rotation).
Pose::Pose()
    : transform_(Eigen::Isometry3d::Identity())
{
}

/// Wraps an already-built Eigen isometry as a Pose, taking its value as-is.
Pose::Pose(const Eigen::Isometry3d& transform)
    : transform_(transform)
{
}

/// Returns the identity pose (zero translation, no rotation).
Pose Pose::identity()
{
    return Pose(Eigen::Isometry3d::Identity());
}

/// Wraps an arbitrary rigid transform as a Pose.
Pose Pose::fromIsometry(const Eigen::Isometry3d& transform)
{
    return Pose(transform);
}

/// Builds a pose from a translation (meters) and fixed-axis roll/pitch/yaw (radians).
/// @note RPY convention: fixed-axis roll(X), pitch(Y), yaw(Z), composed as Rz * Ry * Rx.
Pose Pose::fromXYZRPY_m_rad(double x_m, double y_m, double z_m,
                            double roll_rad, double pitch_rad, double yaw_rad)
{
    Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
    transform.translation() = Eigen::Vector3d(x_m, y_m, z_m);
    transform.linear() =
        (Eigen::AngleAxisd(yaw_rad, Eigen::Vector3d::UnitZ())
         * Eigen::AngleAxisd(pitch_rad, Eigen::Vector3d::UnitY())
         * Eigen::AngleAxisd(roll_rad, Eigen::Vector3d::UnitX()))
            .toRotationMatrix();
    return Pose(transform);
}

/// Builds a pose from a translation (millimeters) and fixed-axis roll/pitch/yaw (degrees),
/// converting each value to meters/radians before delegating to fromXYZRPY_m_rad.
Pose Pose::fromXYZRPY_mm_deg(double x_mm, double y_mm, double z_mm,
                             double roll_deg, double pitch_deg, double yaw_deg)
{
    return fromXYZRPY_m_rad(units::mm(x_mm),
                            units::mm(y_mm),
                            units::mm(z_mm),
                            units::deg(roll_deg),
                            units::deg(pitch_deg),
                            units::deg(yaw_deg));
}

/// Returns the underlying rigid transform (translation + rotation) by const reference.
const Eigen::Isometry3d& Pose::isometry() const
{
    return transform_;
}

/// Returns the pose's translation component, in meters.
Eigen::Vector3d Pose::translation_m() const
{
    return transform_.translation();
}

/// Returns the pose's rotation component as a unit quaternion.
Eigen::Quaterniond Pose::rotationQuaternion() const
{
    return Eigen::Quaterniond(transform_.rotation());
}

/// Returns the inverse of this pose (the transform that undoes it).
Pose Pose::inverse() const
{
    return Pose(transform_.inverse());
}

/// Composes this pose with `other`, chaining the rigid transforms (this * other).
Pose Pose::operator*(const Pose& other) const
{
    return Pose(transform_ * other.transform_);
}

} // namespace RobotKinematics
