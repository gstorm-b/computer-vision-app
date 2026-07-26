#include <RobotKinematics/Model/SerialRobotConfigBuilder.h>

#include <RobotKinematics/Model/RobotModelValidator.h>

#include <utility>

namespace RobotKinematics {

/// Constructs an empty builder pre-set to RobotTopologyType::Serial.
SerialRobotConfigBuilder::SerialRobotConfigBuilder()
{
    config_.topology = RobotTopologyType::Serial;
}

/// Sets the robot's descriptive identity (vendor/model/name/revision).
SerialRobotConfigBuilder& SerialRobotConfigBuilder::identity(RobotIdentity value)
{
    config_.identity = std::move(value);
    return *this;
}

/// Sets the base and flange link ids that bound the kinematic chain.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::baseAndFlange(std::string baseLinkId, std::string flangeLinkId)
{
    config_.frames.baseLinkId = std::move(baseLinkId);
    config_.frames.flangeLinkId = std::move(flangeLinkId);
    return *this;
}

/// Appends a link with the given id to the chain.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::addLink(std::string id)
{
    config_.links.push_back(Link{std::move(id)});
    return *this;
}

/// Appends a joint to the chain and updates `dof` to the new total joint count.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::addJoint(Joint joint)
{
    config_.joints.push_back(std::move(joint));
    config_.dof = static_cast<int>(config_.joints.size());
    return *this;
}

/// Appends a user-defined frame attached to one of the chain's links.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::addUserFrame(UserFrame frame)
{
    config_.frames.userFrames.push_back(std::move(frame));
    return *this;
}

/// Appends a mountable tool definition.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::addTool(Tool tool)
{
    config_.tools.push_back(std::move(tool));
    return *this;
}

/// Sets the id of the tool used by default when none is explicitly selected.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::defaultTool(std::string id)
{
    config_.defaultToolId = std::move(id);
    return *this;
}

/// Sets the posture resolver name and axis labels.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::posture(PostureMetadata metadata)
{
    config_.posture = std::move(metadata);
    return *this;
}

/// Sets the default IK solver name and its numeric parameters.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::solver(SolverMetadata metadata)
{
    config_.solver = std::move(metadata);
    return *this;
}

/// Appends a documentation/data source reference.
SerialRobotConfigBuilder& SerialRobotConfigBuilder::addSource(SourceReference source)
{
    config_.sources.push_back(std::move(source));
    return *this;
}

/// Runs RobotModelValidator::validateSerialRobotConfig against the accumulated config.
/// @return success with the built config, or failure with the first validation issue's
/// status/message.
Result<SerialRobotConfig> SerialRobotConfigBuilder::build() const
{
    const ModelValidationResult validation = RobotModelValidator::validateSerialRobotConfig(config_);
    if (!validation.ok()) {
        return Result<SerialRobotConfig>::failure(validation.status(), validation.issues.front().message);
    }
    return Result<SerialRobotConfig>::success(config_);
}

} // namespace RobotKinematics
