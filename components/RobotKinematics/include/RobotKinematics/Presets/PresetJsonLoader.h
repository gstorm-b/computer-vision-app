#pragma once

#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>

namespace RobotKinematics {

/// Loads and parses a SerialRobotConfig from the project's JSON preset schema
/// ("robot-kinematics-preset/v1", units fixed to meters/radians), validating the
/// result with RobotModelValidator before returning it.
class PresetJsonLoader
{
public:
    /// Reads the file at `path` and parses it as a preset via loadJson().
    /// @return failure with KinematicsStatus::InvalidRequest if the file cannot be opened,
    /// otherwise the result of loadJson() on its contents.
    static Result<SerialRobotConfig> loadFile(const std::string& path);
    /// Parses `json` as a preset document: checks it is valid JSON, restricted to known
    /// top-level fields, on schema "robot-kinematics-preset/v1" with m/rad units, then
    /// builds a SerialRobotConfig from its identity/links/joints/frames/tools/posture/
    /// solver/sources/metadata sections and runs RobotModelValidator on it.
    /// @return failure with KinematicsStatus::InvalidRobotConfig describing the first
    /// schema or validation problem found, otherwise success with the parsed config.
    static Result<SerialRobotConfig> loadJson(const std::string& json);
};

} // namespace RobotKinematics
