#pragma once

#include <RobotKinematics/Core/Units.h>

#include <Eigen/Core>

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Ordered joint values for a serial chain, stored in SI units: radian for revolute
/// joints and meter for prismatic joints. One entry per movable joint, in chain order.
class JointVector {
public:
    /// Constructs an empty joint vector (size 0).
    JointVector() = default;
    /// Wraps an already-built Eigen vector of SI-unit joint values directly.
    explicit JointVector(Eigen::VectorXd values) : values_(std::move(values)) {}

    /// Builds a joint vector from values already expressed in radians/meters (SI units).
    static JointVector fromRadians(const std::vector<double>& values_rad)
    {
        return JointVector(toEigen(values_rad));
    }
    /// Builds a joint vector from values already expressed in radians/meters (SI units).
    static JointVector fromRadians(std::initializer_list<double> values_rad)
    {
        return fromRadians(std::vector<double>(values_rad));
    }
    /// Convenience for angular (revolute) joints. Values are converted deg -> rad.
    static JointVector fromDegrees(const std::vector<double>& values_deg)
    {
        std::vector<double> rad(values_deg.size());
        for (std::size_t i = 0; i < values_deg.size(); ++i) {
            rad[i] = units::deg(values_deg[i]);
        }
        return fromRadians(rad);
    }
    /// Convenience for angular (revolute) joints. Values are converted deg -> rad.
    static JointVector fromDegrees(std::initializer_list<double> values_deg)
    {
        return fromDegrees(std::vector<double>(values_deg));
    }

    /// Returns the number of joint values held.
    int size() const { return static_cast<int>(values_.size()); }
    /// Returns true if the vector holds no joint values.
    bool isEmpty() const { return values_.size() == 0; }

    /// Returns the SI-unit value (radian or meter) at `index`, unchecked.
    double operator[](int index) const { return values_[index]; }
    /// Returns a mutable reference to the SI-unit value (radian or meter) at `index`, unchecked.
    double& operator[](int index) { return values_[index]; }

    /// Returns the underlying joint values as an Eigen vector, in SI units.
    const Eigen::VectorXd& values() const { return values_; }
    /// Returns a mutable reference to the underlying Eigen vector of joint values, in SI units.
    Eigen::VectorXd& values() { return values_; }

    /// Converts every value to degrees, treating each entry as an angle (rad -> deg);
    /// not unit-aware, so this is only correct for purely revolute chains.
    std::vector<double> toDegrees() const
    {
        std::vector<double> deg(static_cast<std::size_t>(values_.size()));
        for (Eigen::Index i = 0; i < values_.size(); ++i) {
            deg[static_cast<std::size_t>(i)] = units::toDeg(values_[i]);
        }
        return deg;
    }

private:
    /// Copies a std::vector<double> into a newly-sized Eigen::VectorXd.
    static Eigen::VectorXd toEigen(const std::vector<double>& v)
    {
        Eigen::VectorXd e(static_cast<Eigen::Index>(v.size()));
        for (std::size_t i = 0; i < v.size(); ++i) {
            e[static_cast<Eigen::Index>(i)] = v[i];
        }
        return e;
    }

    Eigen::VectorXd values_;  ///< Joint values in SI units (radian for revolute, meter for prismatic).
};

} // namespace RobotKinematics
