#include <RobotKinematics/Kinematics/JointLimitValidator.h>

namespace RobotKinematics {

namespace {
constexpr double kLimitTolerance = 1e-9;  ///< Slack added to a joint's [lower, upper] bounds when checking for limit violations, to absorb floating-point rounding.

/// Counts the movable (Revolute or Prismatic) joints in `config`; this is the expected
/// dimension of a joint vector passed to validate() for this robot.
int movableJointCount(const SerialRobotConfig& config)
{
    int count = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            ++count;
        }
    }
    return count;
}
}

/// Validates `joints` against `config`: first checks that the joint count matches the number
/// of movable (Revolute/Prismatic) joints, then checks each movable joint's value against its
/// configured [lower, upper] limit (widened by kLimitTolerance on both sides). All out-of-range
/// joints are collected into the result rather than stopping at the first violation.
/// @param config robot configuration supplying the expected dimension and per-joint limits
/// @param joints candidate joint vector to validate, in the same order as config's movable joints
/// @return status Ok if the vector matches config's dimension and all limits are satisfied;
///         JointDimensionMismatch on a size mismatch; JointLimitViolation (with every
///         offending joint listed in violations) otherwise
JointLimitCheck JointLimitValidator::validate(const SerialRobotConfig& config, const JointVector& joints)
{
    JointLimitCheck result;

    if (joints.size() != movableJointCount(config)) {
        result.status = KinematicsStatus::JointDimensionMismatch;
        return result;
    }

    int movableIndex = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }

        const double value = joints[movableIndex];
        if (joint.limits.has_value()) {
            const JointLimits& limits = *joint.limits;
            if (value < limits.lower - kLimitTolerance || value > limits.upper + kLimitTolerance) {
                JointLimitViolation violation;
                violation.jointIndex = movableIndex;
                violation.jointId = joint.id;
                violation.value = value;
                violation.lower = limits.lower;
                violation.upper = limits.upper;
                result.violations.push_back(violation);
            }
        }
        ++movableIndex;
    }

    if (!result.violations.empty()) {
        result.status = KinematicsStatus::JointLimitViolation;
    }
    return result;
}

} // namespace RobotKinematics
