#pragma once

#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Kinematics/InverseKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// A request resolved by the kinematics facade into a base-frame flange goal, so solvers
/// stay independent of frame/tool resolution. Solvers match FK(flange) to targetFlangeInBase.
struct IKSolveContext {
    Pose targetFlangeInBase;  ///< Target flange pose expressed in the robot base frame.
    IKRequest request;        ///< Original IK request (seeds, posture constraints, tolerances) being solved.
};

/// Internal solver interface. Implementations: NumericalIKSolver (Phase 3), analytic
/// plugins (later). Solvers operate only on the canonical SerialRobotConfig.
class IKSolver {
public:
    /// Default virtual destructor, so solvers can be deleted through an IKSolver pointer.
    virtual ~IKSolver() = default;

    /// Returns this solver's identifying name (e.g. for solver selection/logging).
    virtual const char* name() const = 0;

    /// Returns the single best solution by policy (0 or 1 solution).
    virtual IKResult solve(const SerialRobotConfig& config, const IKSolveContext& context) const = 0;

    /// Returns all solutions found by this solver from the configured seeds. For numerical
    /// solvers this is found-solutions, not a mathematically exhaustive enumeration.
    virtual IKResult solveAll(const SerialRobotConfig& config, const IKSolveContext& context) const = 0;
};

} // namespace RobotKinematics
