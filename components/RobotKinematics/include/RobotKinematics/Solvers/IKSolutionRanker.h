#pragma once

#include <RobotKinematics/Kinematics/InverseKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Stateless helper for judging and scoring candidate IK solutions against a request's
/// posture constraints and quality criteria. All members are static.
class IKSolutionRanker
{
public:
    /// Checks whether `candidate` (the posture actually achieved by a solution) satisfies
    /// every branch/vendor-specific constraint set in `requested` (unset fields in
    /// `requested` are treated as unconstrained and always match).
    /// @return true if there is no posture mismatch at all (mismatch cost is zero)
    static bool postureMatches(const ArmPosture& candidate, const ArmPosture& requested);

    /// Builds a scored IKSolution for a candidate joint configuration: classifies its arm
    /// posture via the model's posture resolver, then combines position/orientation error
    /// with joint-limit-avoidance, seed-distance, motion-continuity, and posture-mismatch
    /// costs into IKSolution::score.totalCost.
    /// @param config robot model the joints were solved against (used to resolve posture and joint limits)
    /// @param request original IK request; supplies seedJoint/previousJoint/posture used in the cost terms
    /// @param joints candidate joint solution to score (moved into the returned IKSolution)
    /// @param positionError_m position error of the candidate solution, in meters
    /// @param orientationError_rad orientation error of the candidate solution, in radians
    /// @param jointLimitAvoidanceWeight weight applied to the joint-limit-margin cost term
    /// @return the populated IKSolution, including resolved posture and computed score
    static IKSolution score(const SerialRobotConfig& config,
                            const IKRequest& request,
                            JointVector joints,
                            double positionError_m,
                            double orientationError_rad,
                            double jointLimitAvoidanceWeight);
};

} // namespace RobotKinematics
