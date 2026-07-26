#pragma once

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <Eigen/Core>

#include <map>
#include <string>
#include <vector>

namespace RobotKinematics {

/// Per-movable-joint geometric data expressed in the base frame. Used both for FK
/// output and for Jacobian construction by IK solvers.
struct JointFrameData {
    Eigen::Vector3d axisInBase = Eigen::Vector3d::UnitZ();  ///< Unit joint axis in base frame.
    Eigen::Vector3d originInBase = Eigen::Vector3d::Zero(); ///< Point on the joint axis in base frame.
    JointType type = JointType::Revolute; ///< Joint kind (revolute/prismatic/fixed) this frame belongs to.
};

/// Result of solving the full kinematic chain for a joint vector.
struct FkChain {
    std::vector<JointFrameData> joints;          ///< One entry per movable joint, in chain order.
    std::map<std::string, Pose> linkPosesInBase; ///< Base-relative pose of every link in the chain.
    Pose flangeInBase = Pose::identity();        ///< base -> flange transform.
};

/// Forward kinematics for serial robot configs. All transforms are in SI units and use Pose.
class ForwardKinematics {
public:
    /// Counts the movable (revolute/prismatic) joints in `config`; fixed joints are excluded.
    static int movableJointCount(const SerialRobotConfig& config);

    /// Solves the chain: link poses, per-joint axis/origin data, and the flange pose.
    /// @param joints joint values; must satisfy joints.size() == movableJointCount(config)
    ///        (callers should validate first, e.g. via JointLimitValidator).
    /// @return the resolved chain (per-joint frame data, per-link poses, and flange pose)
    static FkChain computeChain(const SerialRobotConfig& config, const JointVector& joints);

    /// Computes the base -> flange pose for `joints`.
    static Pose flangePose(const SerialRobotConfig& config, const JointVector& joints);

    /// Computes the base -> tool TCP pose, given the active tool's flange->TCP transform.
    static Pose toolPose(const SerialRobotConfig& config, const JointVector& joints,
                         const Pose& flangeToTcp);

    /// Resolves a user frame (defined relative to a parent link) to its base-relative pose.
    /// @param chain a previously computed FkChain holding the link poses
    /// @param frame the user frame to resolve, referencing one of chain's link poses
    /// @return the resolved base-relative pose, or a failure Result if the parent link is unknown
    static Result<Pose> userFrameInBase(const FkChain& chain, const UserFrame& frame);
};

} // namespace RobotKinematics
