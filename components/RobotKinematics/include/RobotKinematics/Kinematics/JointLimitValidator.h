#pragma once

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// A single joint whose commanded value falls outside its configured position limits.
struct JointLimitViolation {
    int jointIndex = -1;    ///< Index of the offending joint within the movable-joint vector.
    std::string jointId;    ///< Id of the offending joint, as configured in SerialRobotConfig.
    double value = 0.0;     ///< Commanded value, in SI units (rad for revolute, m for prismatic).
    double lower = 0.0;     ///< Configured lower limit, in the same SI units as value.
    double upper = 0.0;     ///< Configured upper limit, in the same SI units as value.
};

/// Result of validating a joint vector against a robot's dimension and position limits.
struct JointLimitCheck {
    KinematicsStatus status = KinematicsStatus::Ok;     ///< Ok, JointDimensionMismatch, or JointLimitViolation.
    std::vector<JointLimitViolation> violations;         ///< All out-of-range joints found (empty when ok()).

    /// True when the joint vector matched the expected dimension and no limits were violated.
    bool ok() const { return status == KinematicsStatus::Ok; }
};

/// Validates a joint vector's dimension and per-joint position limits against a config.
class JointLimitValidator {
public:
    /// Checks that `joints` has the expected dimension for `config` and that every value lies
    /// within its configured [lower, upper] position limit.
    /// @return JointLimitCheck::status is JointDimensionMismatch on a size mismatch, or
    ///         JointLimitViolation (with the offending joints listed) when any limit is exceeded.
    static JointLimitCheck validate(const SerialRobotConfig& config, const JointVector& joints);
};

} // namespace RobotKinematics
