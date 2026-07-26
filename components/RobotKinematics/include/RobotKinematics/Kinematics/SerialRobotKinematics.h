#pragma once

#include <RobotKinematics/Kinematics/InverseKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

namespace RobotKinematics {

/// Solves forward/inverse kinematics for a single serial robot configuration, wrapping the
/// free-function ForwardKinematics/IK machinery in a per-robot object holding its own config.
class SerialRobotKinematics
{
public:
    /// Constructs the solver for a fixed robot configuration.
    /// @param config the serial robot config to solve against; stored by value.
    explicit SerialRobotKinematics(SerialRobotConfig config);

    /// Returns the robot configuration this solver was constructed with.
    const SerialRobotConfig& config() const;

    /// Solves `request`, returning the single closest/best-ranked solution set (see run()).
    IKResult solve(const IKRequest& request) const;

    /// Solves `request` and returns all valid candidate solutions found, ranked.
    IKResult solveAll(const IKRequest& request) const;

private:
    /// Shared implementation behind solve()/solveAll(): runs the IK solve for `request` against
    /// config_, stopping at the first accepted solution unless `allSolutions` is true.
    IKResult run(const IKRequest& request, bool allSolutions) const;

    SerialRobotConfig config_; ///< Robot configuration this instance solves kinematics for.
};

} // namespace RobotKinematics
