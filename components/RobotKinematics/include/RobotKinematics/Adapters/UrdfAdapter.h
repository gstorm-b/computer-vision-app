#pragma once

#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>

namespace RobotKinematics {

/// Converts between RobotKinematics' SerialRobotConfig model and a minimal URDF XML
/// representation (links, joints, origins, axes, and limits only; tools/posture/solver metadata
/// is not representable in URDF and is dropped/noted as a comment).
class UrdfAdapter
{
public:
    /// Serializes `config` to a minimal URDF `<robot>` document: one `<link>` per link, one
    /// `<joint>` per joint with its parent/child, origin (RPY derived from the joint's Pose),
    /// axis, and limits.
    /// @param config the serial robot model to export
    /// @return the generated URDF XML on success, or a failure if `config` fails validation
    static Result<std::string> exportSerialRobot(const SerialRobotConfig& config);
    /// Parses `urdf` into a SerialRobotConfig: reads every link/joint, then walks the parent-child
    /// chain from `baseLinkId` to `flangeLinkId` to build the serial joint sequence (revolute/
    /// prismatic joints without an explicit `<limit>` default to +/-pi).
    /// @param urdf the URDF XML document to parse
    /// @param baseLinkId link id to start the serial chain from
    /// @param flangeLinkId link id to end the serial chain at
    /// @return the parsed and validated SerialRobotConfig, or a failure on XML parse error, an
    /// incomplete chain between the given links, or model validation failure
    static Result<SerialRobotConfig> importSerialRobot(const std::string& urdf,
                                                       const std::string& baseLinkId,
                                                       const std::string& flangeLinkId);
};

} // namespace RobotKinematics
