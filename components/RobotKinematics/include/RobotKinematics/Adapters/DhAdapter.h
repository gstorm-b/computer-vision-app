#pragma once

#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// Motion type of one standard-DH row: whether the joint's row varies theta (Revolute) or d
/// (Prismatic).
enum class DhJointType {
    Revolute,
    Prismatic
};

/// One row of a standard (non-modified) Denavit-Hartenberg parameter table describing a single
/// joint's kinematics.
struct StandardDhParameter {
    std::string jointId;              ///< Joint identifier; auto-generated as "J<n>" if left empty.
    double a_m = 0.0;                 ///< Link length (translation along the common normal), in metres.
    double alpha_rad = 0.0;           ///< Link twist (rotation about the common normal), in radians.
    double d_m = 0.0;                 ///< Link offset (translation along the joint axis); fixed for revolute joints.
    double thetaOffset_rad = 0.0;     ///< Joint angle offset (rotation about the joint axis); fixed for prismatic joints.
    DhJointType type = DhJointType::Revolute;  ///< Whether theta (Revolute) or d (Prismatic) is the joint's free variable.
    JointLimits limits;                ///< Motion limits applied to the joint.
    double home = 0.0;                ///< Home position/angle for the joint.
};

/// Converts standard-DH parameter tables into RobotKinematics' internal SerialRobotConfig model.
class DhAdapter
{
public:
    /// Builds a serial-chain SerialRobotConfig from `rows`: each row becomes a movable joint
    /// followed by a fixed joint encoding the DH link transform, chained from "base_link" to
    /// "flange", then validated via RobotModelValidator.
    /// @param identity robot identity metadata to attach to the resulting config
    /// @param rows the standard-DH parameter table, one row per joint, in kinematic order
    /// @return the built and validated SerialRobotConfig, or a failure if `rows` is empty or the
    /// resulting model fails validation
    static Result<SerialRobotConfig> fromStandardDh(const RobotIdentity& identity,
                                                    const std::vector<StandardDhParameter>& rows);
};

} // namespace RobotKinematics
