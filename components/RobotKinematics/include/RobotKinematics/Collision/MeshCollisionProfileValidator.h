#pragma once

#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// A single semantic validation problem found in a MeshCollisionProfile, identifying the
/// offending field and a human-readable message.
struct MeshCollisionProfileValidationIssue {
    KinematicsStatus status = KinematicsStatus::InvalidRobotConfig;  ///< Status code classifying the issue.
    std::string field;  ///< Dotted/indexed path of the offending field, e.g. "meshes[0].path".
    std::string message;  ///< Human-readable description of the violation.
};

/// Aggregated outcome of MeshCollisionProfileValidator::validate(): empty `issues` means
/// the profile is valid.
struct MeshCollisionProfileValidationResult {
    std::vector<MeshCollisionProfileValidationIssue> issues;  ///< All issues found, in detection order.

    /// Returns true when no validation issues were recorded.
    bool ok() const;

    /// Returns Ok when there are no issues, otherwise the status of the first recorded issue.
    KinematicsStatus status() const;
};

/// Validates a MeshCollisionProfile against a SerialRobotConfig: checks required ids are
/// non-empty and unique, mesh linkIds refer to declared robot links, path/scale/margin
/// values are valid, quality fields (triangleCount, maxSimplificationError_m,
/// simplifiedFrom) are internally consistent, and disabledPairs reference existing mesh ids.
class MeshCollisionProfileValidator
{
public:
    /// Validates `profile` against `config`, collecting every semantic issue found rather
    /// than stopping at the first one.
    /// @param config the robot's link/joint configuration the meshes must attach to
    /// @param profile the mesh collision profile to validate
    /// @return a result whose ok() is true iff no issues were found
    static MeshCollisionProfileValidationResult validate(const SerialRobotConfig& config,
                                                         const MeshCollisionProfile& profile);
};

} // namespace RobotKinematics
