#pragma once

#include <RobotKinematics/Solvers/IKSolver.h>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Base interface for closed-form (analytic) IK solvers that only handle robot models
/// matching a specific kinematic morphology (e.g. a 6R articulated arm with spherical
/// wrist). Adds a model-applicability check on top of the generic IKSolver interface.
class AnalyticIKSolver : public IKSolver
{
public:
    /// Checks whether this solver's closed-form derivation applies to `config` (e.g. joint
    /// count, joint types, and axis-intersection geometry match the solver's assumptions).
    /// @return true if solve()/solveAll() can be used with this model
    virtual bool supportsModel(const SerialRobotConfig& config) const = 0;
};

} // namespace RobotKinematics
