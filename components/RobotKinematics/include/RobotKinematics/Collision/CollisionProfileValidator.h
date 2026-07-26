#pragma once

#include <RobotKinematics/Collision/CollisionProfile.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// A single semantic validation problem found in a CollisionProfile, identifying the
/// offending field and a human-readable message.
struct CollisionProfileValidationIssue {
    KinematicsStatus status = KinematicsStatus::InvalidRobotConfig;  ///< Status code classifying the issue.
    std::string field;  ///< Dotted/indexed path of the offending field, e.g. "geometries[0].id".
    std::string message;  ///< Human-readable description of the violation.
};

/// Aggregated outcome of CollisionProfileValidator::validate(): empty `issues` means the
/// profile is valid.
struct CollisionProfileValidationResult {
    std::vector<CollisionProfileValidationIssue> issues;  ///< All issues found, in detection order.

    /// Returns true when no validation issues were recorded.
    bool ok() const;

    /// Returns Ok when there are no issues, otherwise the status of the first recorded issue.
    KinematicsStatus status() const;
};

/// Validates a CollisionProfile against a SerialRobotConfig: checks required ids are
/// non-empty and unique, geometry linkIds refer to declared robot links, margins/shape
/// dimensions are within valid ranges, and disabledPairs reference existing geometry ids.
class CollisionProfileValidator
{
public:
    /// Validates `profile` against `config`, collecting every semantic issue found rather
    /// than stopping at the first one.
    /// @note Same-link geometry is allowed in the profile, but the runtime checker may skip
    /// same-link pairs unless a later scope explicitly enables them.
    /// Adjacent contacts should be modeled via explicit disabledPairs with a reason such as
    /// adjacent_joint_contact so the runtime filter is deterministic and reviewable.
    /// @param config the robot's link/joint configuration the geometries must attach to
    /// @param profile the collision profile to validate
    /// @return a result whose ok() is true iff no issues were found
    static CollisionProfileValidationResult validate(const SerialRobotConfig& config,
                                                     const CollisionProfile& profile);
};

} // namespace RobotKinematics
