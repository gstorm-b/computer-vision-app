#pragma once

#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Core/Result.h>

#include <string>

namespace RobotKinematics {

/// Parses MeshCollisionProfile instances from the `robot-kinematics-collision-mesh/v1`
/// JSON schema (schema/unit checks, per-mesh numeric/format/quality validation,
/// backend-preference parsing).
class MeshCollisionProfileJsonLoader
{
public:
    /// Reads `path` as UTF-8 text, parses it via loadJson(), then resolves every relative
    /// mesh `path` field against the directory containing `path` so all mesh paths in the
    /// returned profile are absolute.
    /// @param path filesystem path to the mesh collision profile JSON file
    /// @return the parsed profile (with absolute mesh paths) on success; InvalidRequest failure
    /// if the file cannot be opened, or whatever loadJson() reports for a malformed document
    static Result<MeshCollisionProfile> loadFile(const std::string& path);

    /// Parses `json` as a `robot-kinematics-collision-mesh/v1` document: validates schema,
    /// top-level fields, and m/rad units, then builds each mesh's transform/scale/format/
    /// source-units/quality fields (rejecting non-finite numbers and malformed quality data),
    /// the backend preference list, disabled pairs (which must reference existing mesh ids),
    /// sources, and string metadata. Mesh paths are left as given in the JSON (not resolved).
    /// @param json raw JSON document text
    /// @return the parsed profile on success; InvalidRobotConfig failure with a descriptive
    /// message on any structural/schema/numeric violation
    static Result<MeshCollisionProfile> loadJson(const std::string& json);
};

} // namespace RobotKinematics
