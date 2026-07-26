#include <RobotKinematics/Kinematics/ForwardKinematics.h>

#include <Eigen/Geometry>

/// Serial-robot kinematics library: robot model description, forward/inverse kinematics
/// solvers, posture resolution, and collision checking for articulated manipulators.
namespace RobotKinematics {

/// Counts the movable (revolute/prismatic) joints in `config` by scanning config.joints in
/// order; joints of other types (e.g. fixed) don't increment the count.
/// @return the number of revolute/prismatic joints found
int ForwardKinematics::movableJointCount(const SerialRobotConfig& config)
{
    int count = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            ++count;
        }
    }
    return count;
}

/// Walks `config.joints` in order, accumulating each joint's origin transform and, for
/// revolute/prismatic joints, the motion implied by `joints`, to build the full chain:
/// per-movable-joint axis/origin data, the base-relative pose of every named link, and the
/// resulting flange pose.
/// @param joints joint values, one per movable joint; if shorter than the number of movable
///        joints encountered so far, the missing trailing values default to 0.0
/// @return the resolved chain: per-movable-joint frame data (chain order), base-relative link
///         poses keyed by link id, and the flange pose (config.frames.flangeLinkId's recorded
///         pose if present, otherwise the final accumulated pose)
FkChain ForwardKinematics::computeChain(const SerialRobotConfig& config, const JointVector& joints)
{
    FkChain chain;

    Eigen::Isometry3d accumulated = Eigen::Isometry3d::Identity(); // base link pose in base
    if (!config.frames.baseLinkId.empty()) {
        chain.linkPosesInBase[config.frames.baseLinkId] = Pose::fromIsometry(accumulated);
    }

    int movableIndex = 0;
    for (const Joint& joint : config.joints) {
        // Joint frame in base, before applying the joint's own motion.
        const Eigen::Isometry3d jointFrame = accumulated * joint.origin.isometry();

        Eigen::Isometry3d motion = Eigen::Isometry3d::Identity();
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            const double value = (movableIndex < joints.size()) ? joints[movableIndex] : 0.0;
            const Eigen::Vector3d axisLocal = joint.axis.normalized();

            JointFrameData data;
            data.type = joint.type;
            data.axisInBase = jointFrame.linear() * axisLocal;
            data.originInBase = jointFrame.translation();
            chain.joints.push_back(data);

            if (joint.type == JointType::Revolute) {
                motion.linear() = Eigen::AngleAxisd(value, axisLocal).toRotationMatrix();
            } else {
                motion.translation() = axisLocal * value;
            }
            ++movableIndex;
        }

        accumulated = jointFrame * motion; // child link pose in base
        if (!joint.childLinkId.empty()) {
            chain.linkPosesInBase[joint.childLinkId] = Pose::fromIsometry(accumulated);
        }
    }

    chain.flangeInBase = Pose::fromIsometry(accumulated);
    if (!config.frames.flangeLinkId.empty()) {
        const auto it = chain.linkPosesInBase.find(config.frames.flangeLinkId);
        if (it != chain.linkPosesInBase.end()) {
            chain.flangeInBase = it->second;
        }
    }
    return chain;
}

/// Computes the base -> flange pose for `joints` by building the full chain via computeChain()
/// and returning just its flange pose.
Pose ForwardKinematics::flangePose(const SerialRobotConfig& config, const JointVector& joints)
{
    return computeChain(config, joints).flangeInBase;
}

/// Computes the base -> tool TCP pose by composing the flange pose (flangePose()) with the
/// active tool's flange -> TCP transform.
Pose ForwardKinematics::toolPose(const SerialRobotConfig& config, const JointVector& joints,
                                 const Pose& flangeToTcp)
{
    return flangePose(config, joints) * flangeToTcp;
}

/// Looks up `frame.parentLinkId` among `chain.linkPosesInBase` and composes that link's
/// base-relative pose with `frame.transform` to resolve the user frame's base-relative pose.
/// @return success with the resolved pose, or a KinematicsStatus::FrameNotFound failure if the
///         parent link isn't recorded in `chain`
Result<Pose> ForwardKinematics::userFrameInBase(const FkChain& chain, const UserFrame& frame)
{
    const auto it = chain.linkPosesInBase.find(frame.parentLinkId);
    if (it == chain.linkPosesInBase.end()) {
        return Result<Pose>::failure(KinematicsStatus::FrameNotFound,
                                     "user frame parent link not found: " + frame.parentLinkId);
    }
    return Result<Pose>::success(it->second * frame.transform);
}

} // namespace RobotKinematics
