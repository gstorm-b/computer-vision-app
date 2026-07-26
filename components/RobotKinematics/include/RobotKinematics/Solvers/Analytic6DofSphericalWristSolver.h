#pragma once

#include <RobotKinematics/Solvers/AnalyticIKSolver.h>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Closed-form analytic IK solver for a standard articulated 6R arm with a spherical
/// wrist (axes 4/5/6 intersecting at a common point): decouples the wrist-center position
/// (solved via shoulder/elbow geometry, i.e. law-of-cosines on the upper-arm/forearm
/// triangle) from the wrist orientation (solved via a ZYZ Euler decomposition), enumerating
/// up to 8 shoulder/elbow/wrist branch combinations per target pose.
class Analytic6DofSphericalWristSolver : public AnalyticIKSolver
{
public:
    /// Returns "analytic_6dof_spherical_wrist".
    const char* name() const override;
    /// Checks that `config` is a serial 6-DOF, all-revolute robot whose home-configuration
    /// axes match the articulated-arm/spherical-wrist morphology this solver assumes
    /// (axes 1/2 intersecting and perpendicular, axis 2 parallel to axis 3, axes 4/5/6
    /// intersecting at a common wrist center, and a decomposable wrist-reduction rotation).
    bool supportsModel(const SerialRobotConfig& config) const override;

    /// Runs solveAll() and keeps the single solution closest (in joint space) to the
    /// request's seed or previous joints if either is supplied; otherwise keeps the first
    /// enumerated branch.
    IKResult solve(const SerialRobotConfig& config, const IKSolveContext& context) const override;
    /// Solves the wrist-center position from the target pose and the arm geometry, then
    /// enumerates all valid shoulder/elbow branch and wrist Euler-angle combinations,
    /// discarding branches that are unreachable, out of joint limits, fail the FK residual
    /// check, or duplicate an already-found solution.
    /// @return IKResult with all found solutions (posture branch signs set from q1/q3/q5)
    /// on success; UnsupportedSolver if the model geometry doesn't match, Singularity if
    /// the wrist center lies on the J1 axis, or TargetUnreachable if no branch yields an
    /// in-limit, FK-consistent solution
    IKResult solveAll(const SerialRobotConfig& config, const IKSolveContext& context) const override;
};

} // namespace RobotKinematics
