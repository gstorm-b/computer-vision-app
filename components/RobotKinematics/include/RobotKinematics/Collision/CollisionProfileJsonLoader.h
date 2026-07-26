#pragma once

#include <RobotKinematics/Collision/CollisionProfile.h>
#include <RobotKinematics/Core/Result.h>

#include <string>

namespace RobotKinematics {

/// Parses CollisionProfile instances from the `robot-kinematics-collision/v1` JSON
/// schema (schema/top-level-field/unit checks, sphere/capsule shape parsing).
class CollisionProfileJsonLoader
{
public:
    /// Reads `path` as UTF-8 text and parses it via loadJson().
    /// @param path filesystem path to the collision profile JSON file
    /// @return the parsed profile on success; InvalidRequest failure if the file cannot be opened,
    /// or whatever loadJson() reports for a malformed document
    static Result<CollisionProfile> loadFile(const std::string& path);

    /// Parses `json` as a `robot-kinematics-collision/v1` document: validates it is a JSON
    /// object with only known top-level fields, the expected schema string, and m/rad units,
    /// then builds geometries (sphere/capsule), disabled pairs, sources, and string metadata.
    /// @param json raw JSON document text
    /// @return the parsed profile on success; InvalidRobotConfig failure with a descriptive
    /// message on any structural/schema/shape violation
    static Result<CollisionProfile> loadJson(const std::string& json);
};

} // namespace RobotKinematics
