#include <RobotKinematics/Posture/PostureResolver.h>

namespace RobotKinematics {

namespace {
/// Maps a raw joint value to a branch sign: -1 if strictly negative, +1 otherwise (zero
/// included). Used to derive shoulder/elbow/wrist branch classification from joint angles.
int branchSign(double value)
{
    return value < 0.0 ? -1 : 1;
}

/// Resolves one posture axis (e.g. "shoulder") to a branch sign by matching a caller-supplied
/// label against the axis's configured negative/positive label pair.
/// @param metadata posture metadata holding the configured label pairs per axis
/// @param labels caller-supplied axis-name -> label-string map
/// @param axis name of the axis to resolve (e.g. "shoulder", "elbow", "wrist")
/// @param target set to -1 or +1 when a match is found; left unmodified if `axis` is absent
/// from `labels`
/// @return true if `axis` is absent from `labels` (nothing to resolve) or the requested label
/// matched the configured negative/positive label; false if `axis` has no configured labels in
/// `metadata`, or the requested label matches neither
bool applyLabel(const PostureMetadata& metadata,
                const std::map<std::string, std::string>& labels,
                const std::string& axis,
                std::optional<int>& target)
{
    const auto requested = labels.find(axis);
    if (requested == labels.end()) {
        return true;
    }
    const auto configured = metadata.labels.find(axis);
    if (configured == metadata.labels.end()) {
        return false;
    }
    if (requested->second == configured->second.negative) {
        target = -1;
        return true;
    }
    if (requested->second == configured->second.positive) {
        target = 1;
        return true;
    }
    return false;
}
}

/// Classifies a serial 6-DOF arm's posture directly from joint values, using the sign of
/// joints[0]/[2]/[4] as the shoulder/elbow/wrist branch respectively (via branchSign()).
/// @param joints joint vector; must contain at least 6 elements
/// @return ArmPosture with shoulder/elbow/wrist set on success; failure with
/// KinematicsStatus::JointDimensionMismatch if fewer than 6 joints are supplied
Result<ArmPosture> Serial6DofSignPostureResolver::classify(const SerialRobotConfig&,
                                                           const JointVector& joints) const
{
    if (joints.size() < 6) {
        return Result<ArmPosture>::failure(KinematicsStatus::JointDimensionMismatch,
                                           "serial 6dof posture classification requires at least 6 joints");
    }

    ArmPosture posture;
    posture.shoulder = branchSign(joints[0]);
    posture.elbow = branchSign(joints[2]);
    posture.wrist = branchSign(joints[4]);
    return Result<ArmPosture>::success(posture);
}

/// Builds an ArmPosture from caller-supplied axis labels (shoulder/elbow/wrist) by matching
/// each against the robot's configured posture label pairs (via applyLabel()).
/// @param metadata configured negative/positive label pairs per axis
/// @param labels caller-supplied axis-name -> label-string map
/// @return ArmPosture with the resolved branch signs on success; failure with
/// KinematicsStatus::PostureConstraintUnsatisfied if any requested label is unconfigured or
/// does not match either the negative or positive label for its axis
Result<ArmPosture> Serial6DofSignPostureResolver::fromLabels(
    const PostureMetadata& metadata,
    const std::map<std::string, std::string>& labels) const
{
    ArmPosture posture;
    if (!applyLabel(metadata, labels, "shoulder", posture.shoulder)
        || !applyLabel(metadata, labels, "elbow", posture.elbow)
        || !applyLabel(metadata, labels, "wrist", posture.wrist)) {
        return Result<ArmPosture>::failure(KinematicsStatus::PostureConstraintUnsatisfied,
                                           "posture label is not configured");
    }
    return Result<ArmPosture>::success(posture);
}

/// Creates the PostureResolver implementation named by `config.posture.resolver`.
/// @param config robot configuration; only the posture.resolver name is consulted
/// @return a new Serial6DofSignPostureResolver for "serial_6dof_shoulder_elbow_wrist";
/// nullptr for any other/unrecognized resolver name
std::unique_ptr<PostureResolver> PostureResolverFactory::create(const SerialRobotConfig& config)
{
    if (config.posture.resolver == "serial_6dof_shoulder_elbow_wrist") {
        return std::make_unique<Serial6DofSignPostureResolver>();
    }
    return nullptr;
}

} // namespace RobotKinematics
