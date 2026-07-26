#include <RobotKinematics/Solvers/IKSolutionRanker.h>

#include <RobotKinematics/Posture/PostureResolver.h>

#include <algorithm>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Internal helpers used only by IKSolutionRanker::postureMatches()/score(): posture-branch
/// matching and the individual cost terms folded into IKSolution::score.
namespace {
/// Checks a single posture-branch constraint: satisfied if `requested` is unset (no
/// constraint), or if `candidate` is set and equal to `requested`.
/// @return true when the branch is unconstrained or matches; false on an unmet constraint
bool optionalBranchMatches(const std::optional<int>& candidate, const std::optional<int>& requested)
{
    return !requested.has_value() || (candidate.has_value() && *candidate == *requested);
}

/// Computes a joint-limit-avoidance cost for `joints`: for each revolute/prismatic joint that
/// has limits, adds the reciprocal of its limit range fraction remaining to the nearer bound
/// (clamped to at least 1e-6 to avoid dividing by zero), so the cost rises sharply as a joint
/// approaches either limit and is 0 for joints with no limits.
/// @param config supplies joint order/type and limits, indexed in parallel with `joints`
/// @param joints candidate joint values to evaluate
/// @return summed margin cost across all limited joints (0.0 if none have limits)
double jointLimitMarginCost(const SerialRobotConfig& config, const JointVector& joints)
{
    double cost = 0.0;
    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }
        if (joint.limits.has_value()) {
            const double range = joint.limits->upper - joint.limits->lower;
            const double margin = std::min(joints[index] - joint.limits->lower, joint.limits->upper - joints[index]);
            if (range > 0.0) {
                cost += 1.0 / std::max(margin / range, 1e-6);
            }
        }
        ++index;
    }
    return cost;
}

/// Counts posture mismatches between `candidate` and `requested`: adds 1.0 for each of
/// shoulder/elbow/wrist that fails optionalBranchMatches(), plus 1.0 for every vendor-specific
/// posture entry in `requested` that `candidate` is missing or disagrees with.
/// @return total mismatch count; 0.0 means `candidate` fully satisfies `requested`
double postureMismatchCost(const ArmPosture& candidate, const ArmPosture& requested)
{
    double cost = 0.0;
    if (!optionalBranchMatches(candidate.shoulder, requested.shoulder)) {
        cost += 1.0;
    }
    if (!optionalBranchMatches(candidate.elbow, requested.elbow)) {
        cost += 1.0;
    }
    if (!optionalBranchMatches(candidate.wrist, requested.wrist)) {
        cost += 1.0;
    }
    for (const auto& requestedVendor : requested.vendorSpecific) {
        const auto candidateVendor = candidate.vendorSpecific.find(requestedVendor.first);
        if (candidateVendor == candidate.vendorSpecific.end() || candidateVendor->second != requestedVendor.second) {
            cost += 1.0;
        }
    }
    return cost;
}
}

/// Returns whether `candidate` satisfies every branch/vendor-specific constraint set in
/// `requested` (delegates to postureMismatchCost() and checks for an exact zero).
bool IKSolutionRanker::postureMatches(const ArmPosture& candidate, const ArmPosture& requested)
{
    return postureMismatchCost(candidate, requested) == 0.0;
}

/// Builds a scored IKSolution for a candidate joint configuration: classifies `joints`' arm
/// posture via the config's posture resolver (if one is registered for it), then sums
/// position/orientation error with weighted joint-limit-avoidance, seed-distance,
/// motion-continuity, and posture-mismatch (weighted x100) costs into
/// IKSolution::score.totalCost. Seed-distance and motion-continuity costs are only computed
/// when `request` carries a seed/previous joint vector of matching size; posture-mismatch cost
/// is only computed when `request.posture` is set.
IKSolution IKSolutionRanker::score(const SerialRobotConfig& config,
                                   const IKRequest& request,
                                   JointVector joints,
                                   double positionError_m,
                                   double orientationError_rad,
                                   double jointLimitAvoidanceWeight)
{
    IKSolution solution;
    solution.joints = std::move(joints);
    solution.positionError_m = positionError_m;
    solution.orientationError_rad = orientationError_rad;

    if (std::unique_ptr<PostureResolver> resolver = PostureResolverFactory::create(config)) {
        const Result<ArmPosture> posture = resolver->classify(config, solution.joints);
        if (posture.ok()) {
            solution.posture = posture.value;
        }
    }

    solution.score.jointLimitMarginCost = jointLimitAvoidanceWeight * jointLimitMarginCost(config, solution.joints);
    if (request.seedJoint.has_value() && request.seedJoint->size() == solution.joints.size()) {
        solution.score.seedDistanceCost = (solution.joints.values() - request.seedJoint->values()).norm();
    }
    if (request.previousJoint.has_value() && request.previousJoint->size() == solution.joints.size()) {
        solution.score.motionContinuityCost = (solution.joints.values() - request.previousJoint->values()).norm();
    }
    if (request.posture.has_value()) {
        solution.score.postureMismatchCost = 100.0 * postureMismatchCost(solution.posture, *request.posture);
    }
    solution.score.totalCost = solution.positionError_m + solution.orientationError_rad
                               + solution.score.seedDistanceCost + solution.score.motionContinuityCost
                               + solution.score.jointLimitMarginCost + solution.score.postureMismatchCost;
    return solution;
}

} // namespace RobotKinematics
