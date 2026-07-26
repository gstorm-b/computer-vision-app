#pragma once

#include <Eigen/Geometry>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Rigid-body transform (translation + rotation) between two frames, backed by an
/// Eigen::Isometry3d. Lengths are in meters and angles in radians except where a
/// constructor's name says otherwise (e.g. fromXYZRPY_mm_deg).
class Pose {
public:
    /// Constructs the identity pose (zero translation, no rotation).
    Pose();

    /// Returns the identity pose (zero translation, no rotation).
    static Pose identity();
    /// Wraps an existing Eigen isometry transform as a Pose.
    static Pose fromIsometry(const Eigen::Isometry3d& transform);

    /// Builds a pose from translation (meters) and fixed-axis roll/pitch/yaw (radians).
    /// RPY convention: fixed-axis roll(X), pitch(Y), yaw(Z), applied as Rz * Ry * Rx.
    static Pose fromXYZRPY_m_rad(double x_m, double y_m, double z_m,
                                 double roll_rad, double pitch_rad, double yaw_rad);

    /// Builds a pose from translation (millimeters) and fixed-axis roll/pitch/yaw
    /// (degrees), converting both to SI units before delegating to fromXYZRPY_m_rad.
    static Pose fromXYZRPY_mm_deg(double x_mm, double y_mm, double z_mm,
                                  double roll_deg, double pitch_deg, double yaw_deg);

    /// Returns the underlying Eigen isometry transform.
    const Eigen::Isometry3d& isometry() const;
    /// Returns the translation component of the pose, in meters.
    Eigen::Vector3d translation_m() const;
    /// Returns the rotation component of the pose as a unit quaternion.
    Eigen::Quaterniond rotationQuaternion() const;

    /// Returns the inverse of this pose (the transform that undoes it).
    Pose inverse() const;
    /// Composes this pose with `other` (this * other), i.e. `other` expressed in this
    /// pose's frame, then transformed into the parent frame.
    Pose operator*(const Pose& other) const;

private:
    /// Constructs a pose directly from a precomputed isometry transform.
    explicit Pose(const Eigen::Isometry3d& transform);

    Eigen::Isometry3d transform_;  ///< Rigid-body translation + rotation this pose represents.
};

} // namespace RobotKinematics
