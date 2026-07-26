#pragma once

#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

namespace RobotKinematics {

/// Fluent builder for assembling a SerialRobotConfig piece by piece; `build()` runs
/// RobotModelValidator before returning so callers cannot obtain an invalid config.
class SerialRobotConfigBuilder
{
public:
    /// Constructs an empty builder pre-set to RobotTopologyType::Serial.
    SerialRobotConfigBuilder();

    /// Sets the robot's descriptive identity (vendor/model/name/revision).
    SerialRobotConfigBuilder& identity(RobotIdentity value);
    /// Sets the base and flange link ids that bound the kinematic chain.
    SerialRobotConfigBuilder& baseAndFlange(std::string baseLinkId, std::string flangeLinkId);
    /// Appends a link with the given id to the chain.
    SerialRobotConfigBuilder& addLink(std::string id);
    /// Appends a joint to the chain and updates `dof` to the new joint count.
    SerialRobotConfigBuilder& addJoint(Joint joint);
    /// Appends a user-defined frame attached to one of the chain's links.
    SerialRobotConfigBuilder& addUserFrame(UserFrame frame);
    /// Appends a mountable tool definition.
    SerialRobotConfigBuilder& addTool(Tool tool);
    /// Sets the id of the tool used by default when none is explicitly selected.
    SerialRobotConfigBuilder& defaultTool(std::string id);
    /// Sets the posture resolver name and axis labels.
    SerialRobotConfigBuilder& posture(PostureMetadata metadata);
    /// Sets the default IK solver name and its numeric parameters.
    SerialRobotConfigBuilder& solver(SolverMetadata metadata);
    /// Appends a documentation/data source reference.
    SerialRobotConfigBuilder& addSource(SourceReference source);

    /// Runs RobotModelValidator::validateSerialRobotConfig against the accumulated config.
    /// @return success with the built config, or failure with the first validation issue's
    /// status/message.
    Result<SerialRobotConfig> build() const;

private:
    SerialRobotConfig config_;  ///< Configuration accumulated so far by the fluent setters.
};

} // namespace RobotKinematics
