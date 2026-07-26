#pragma once

#include <RobotKinematics/Solvers/IKSolver.h>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Tuning parameters for NumericalIKSolver's damped-least-squares iteration: seeding,
/// convergence tolerances, the Levenberg-Marquardt-style damping schedule, and residual/cost
/// weighting. Defaults are the values used when a caller doesn't override them.
struct NumericalIKDefaults {
    int maxIterations = 200;  ///< Maximum solver iterations per seed before giving up on that seed.
    int maxSeeds = 32;        ///< Maximum number of seed joint configurations solveAll() will try.

    double positionTolerance_m = 1e-6;                         ///< Unused tolerance field; actual convergence uses IKRequest::options::maxPositionError_m.
    double orientationTolerance_rad = 1.7453292519943296e-5;   ///< Unused tolerance field; actual convergence uses IKRequest::options::maxOrientationError_rad.

    double stepTolerance = 1e-10;  ///< Iteration for a seed stops (as non-converged) once the joint-step norm falls below this.
    double costTolerance = 1e-12;  ///< Residual-cost change below which damping is increased (stalled progress).

    double initialDamping = 1e-3;         ///< Starting Levenberg-Marquardt damping factor for each seed's iteration.
    double minDamping = 1e-8;             ///< Lower bound the damping factor is clamped to after an accepted step.
    double maxDamping = 1e2;              ///< Upper bound the damping factor is clamped to after a rejected/stalled step.
    double dampingDecreaseFactor = 0.5;   ///< Multiplier applied to damping after a cost-reducing (accepted) step.
    double dampingIncreaseFactor = 10.0;  ///< Multiplier applied to damping after a rejected or stalled step.

    double maxJointStepNorm = 0.2;            ///< Maximum allowed norm of a single joint-update step; larger steps are rescaled down to this.
    double jointLimitAvoidanceWeight = 0.05;  ///< Weight of the joint-limit-margin cost term folded into each solution's score.
    double positionResidualWeight = 1.0;      ///< Weight applied to the position component of the Jacobian residual vector.
    double orientationResidualWeight = 1.0;   ///< Weight applied to the orientation component of the Jacobian residual vector.

    double duplicateJointTolerance = 1e-6;  ///< Joint-space distance below which two seeds/solutions are treated as duplicates.
};

/// Numerical inverse-kinematics solver ("adaptive_damped_least_squares"): tries a set of
/// seed joint configurations (previous/seed/posture-derived/home/midpoint/pseudo-random)
/// and refines each with a damped-least-squares Jacobian iteration until the flange pose
/// converges within the request's position/orientation tolerances, keeping deduplicated,
/// cost-sorted solutions.
class NumericalIKSolver : public IKSolver
{
public:
    /// Constructs the solver with the given tuning parameters (NumericalIKDefaults{} if omitted).
    explicit NumericalIKSolver(NumericalIKDefaults defaults = {});

    /// Returns "adaptive_damped_least_squares".
    const char* name() const override;

    /// Runs solveAll() and keeps only the single lowest-cost solution (solveAll() sorts
    /// solutions by ascending total cost, so this truncates to the first entry).
    IKResult solve(const SerialRobotConfig& config, const IKSolveContext& context) const override;
    /// Builds seed joint configurations, runs a damped-least-squares solve from each, and
    /// returns the deduplicated converged solutions sorted by ascending total cost.
    /// @return IKResult with status Ok and at least one solution on success; otherwise a
    /// failure status/message describing why no seed converged (e.g. MaxIterationsReached,
    /// NoConvergedSolution, PostureConstraintUnsatisfied) or why the request was invalid
    IKResult solveAll(const SerialRobotConfig& config, const IKSolveContext& context) const override;

private:
    NumericalIKDefaults defaults_;  ///< Tuning parameters used by solve()/solveAll() for this instance.
};

} // namespace RobotKinematics
