#pragma once

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>
#include <RobotKinematics/Posture/ArmPosture.h>

#include <map>
#include <memory>
#include <string>

namespace RobotKinematics {

/// Abstract strategy for classifying an ArmPosture from a joint solution, and for
/// converting posture axis labels (e.g. "lefty"/"righty") to/from ArmPosture branch signs.
class PostureResolver
{
public:
    virtual ~PostureResolver() = default;

    /// Classifies the posture branch (shoulder/elbow/wrist signs) implied by `joints` for
    /// the given robot config.
    /// @return failure when `joints` is not compatible with this resolver (e.g. wrong dof).
    virtual Result<ArmPosture> classify(const SerialRobotConfig& config, const JointVector& joints) const = 0;
    /// Converts requested axis labels (e.g. {"shoulder": "righty"}) to an ArmPosture using
    /// the label definitions in `metadata`.
    /// @return failure when a requested label is not configured in `metadata`.
    virtual Result<ArmPosture> fromLabels(const PostureMetadata& metadata,
                                          const std::map<std::string, std::string>& labels) const = 0;
};

/// PostureResolver for 6-DOF serial arms that classifies shoulder/elbow/wrist branch by
/// the sign of joints 1, 3, and 5 (0-indexed).
class Serial6DofSignPostureResolver : public PostureResolver
{
public:
    /// Requires at least 6 joints; sets shoulder/elbow/wrist to the sign of joints[0],
    /// joints[2], and joints[4] respectively.
    /// @return failure with KinematicsStatus::JointDimensionMismatch if fewer than 6 joints are given.
    Result<ArmPosture> classify(const SerialRobotConfig& config, const JointVector& joints) const override;
    /// Maps each requested shoulder/elbow/wrist label to its configured branch sign.
    /// @return failure with KinematicsStatus::PostureConstraintUnsatisfied if a requested
    /// label does not match either the configured negative or positive label for its axis.
    Result<ArmPosture> fromLabels(const PostureMetadata& metadata,
                                  const std::map<std::string, std::string>& labels) const override;
};

/// Selects and instantiates the PostureResolver implementation named by a robot config's
/// posture metadata.
class PostureResolverFactory
{
public:
    /// Creates the resolver matching `config.posture.resolver`.
    /// @return a Serial6DofSignPostureResolver when the resolver name is
    /// "serial_6dof_shoulder_elbow_wrist", otherwise nullptr.
    static std::unique_ptr<PostureResolver> create(const SerialRobotConfig& config);
};

} // namespace RobotKinematics
