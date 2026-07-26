#pragma once

#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

/// Structural/consistency validation for SerialRobotConfig instances loaded from presets
/// or produced by SerialRobotConfigBuilder.
namespace RobotKinematics {

/// A single validation failure: which status code it maps to, which config field it
/// applies to (dotted/indexed path, e.g. "joints[2].limits"), and a human-readable message.
struct ModelValidationIssue {
    KinematicsStatus status = KinematicsStatus::InvalidRobotConfig;  ///< Status code this issue should surface as.
    std::string field;    ///< Dotted/indexed path to the offending config field.
    std::string message;  ///< Human-readable description of the problem.
};

/// Aggregated result of validating a SerialRobotConfig: empty `issues` means the config is valid.
struct ModelValidationResult {
    std::vector<ModelValidationIssue> issues;  ///< All issues found, in the order they were detected; empty when valid.

    /// True when no issues were recorded.
    bool ok() const;
    /// Status of the first recorded issue, or KinematicsStatus::Ok when `issues` is empty.
    KinematicsStatus status() const;
};

/// Validates SerialRobotConfig instances for structural consistency (unique/referenced
/// link and joint ids, contiguous serial chain, joint axis/limit sanity, tool references).
class RobotModelValidator {
public:
    /// Runs all structural checks against `config` (topology, dof vs. movable joint count,
    /// link/joint/frame/tool id uniqueness and references, chain contiguity, joint axis and
    /// limit validity) and collects every failure found rather than stopping at the first.
    /// @return a result whose `issues` is empty iff `config` passes all checks.
    static ModelValidationResult validateSerialRobotConfig(const SerialRobotConfig& config);
};

} // namespace RobotKinematics
