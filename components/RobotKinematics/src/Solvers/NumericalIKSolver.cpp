#include <RobotKinematics/Solvers/NumericalIKSolver.h>

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/JointLimitValidator.h>
#include <RobotKinematics/Model/RobotModelValidator.h>
#include <RobotKinematics/Solvers/IKSolutionRanker.h>

#include <Eigen/Cholesky>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <limits>

/// Core kinematics types and algorithms: robot models, forward/inverse kinematics,
/// posture resolution, and the solvers that implement them.
namespace RobotKinematics {

/// Internal implementation details of NumericalIKSolver: seed generation, the residual/Jacobian
/// math and damped-least-squares iteration for a single seed, and the per-solution helpers
/// (clamping, deduplication) used while assembling the final IKResult.
namespace {
/// Cartesian pose error used to drive the damped-least-squares iteration.
struct Residual {
    Eigen::Matrix<double, 6, 1> vector = Eigen::Matrix<double, 6, 1>::Zero();  ///< Weighted [position; orientation] error vector fed to the Jacobian solve.
    double positionNorm = 0.0;     ///< Unweighted position error magnitude, in meters.
    double orientationNorm = 0.0;  ///< Unweighted orientation error magnitude, in radians.
};

/// Checks that every element of `pose`'s transform matrix is finite (no NaN/Inf).
bool finitePose(const Pose& pose)
{
    return pose.isometry().matrix().allFinite();
}

/// Clamps `value` to the closed range [lower, upper].
double clamp(double value, double lower, double upper)
{
    return std::max(lower, std::min(upper, value));
}

/// Clamps each movable joint value in `q` to its configured limits (values for joints with no
/// limits, or non-revolute/prismatic joints, pass through unchanged).
/// @param config supplies joint order/type and limits, indexed in parallel with `q`
/// @param q joint vector to clamp
/// @return a copy of `q` with limited joints clamped into range
Eigen::VectorXd clampToLimits(const SerialRobotConfig& config, const Eigen::VectorXd& q)
{
    Eigen::VectorXd clamped = q;
    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }
        if (joint.limits.has_value()) {
            clamped[index] = clamp(clamped[index], joint.limits->lower, joint.limits->upper);
        }
        ++index;
    }
    return clamped;
}

/// Builds a seed joint vector from each movable joint's home position (Joint::home).
JointVector homeJoint(const SerialRobotConfig& config)
{
    Eigen::VectorXd q(ForwardKinematics::movableJointCount(config));
    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            q[index++] = joint.home;
        }
    }
    return JointVector(q);
}

/// Builds a seed joint vector from the midpoint of each limited joint's range (falling back to
/// Joint::home for joints with no configured limits).
JointVector midpointJoint(const SerialRobotConfig& config)
{
    Eigen::VectorXd q(ForwardKinematics::movableJointCount(config));
    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }
        q[index++] = joint.limits.has_value() ? 0.5 * (joint.limits->lower + joint.limits->upper) : joint.home;
    }
    return JointVector(q);
}

/// Checks whether `a` and `b` are the same joint-space point within `tolerance`: same size and
/// Euclidean distance no greater than `tolerance`.
bool sameJointVector(const JointVector& a, const JointVector& b, double tolerance)
{
    return a.size() == b.size() && (a.values() - b.values()).norm() <= tolerance;
}

/// Appends `seed` to `seeds` unless an existing entry already matches it within
/// `duplicateTolerance` (per sameJointVector()), keeping the seed list free of near-duplicates.
void appendUniqueSeed(std::vector<JointVector>& seeds, const JointVector& seed, double duplicateTolerance)
{
    for (const JointVector& existing : seeds) {
        if (sameJointVector(existing, seed, duplicateTolerance)) {
            return;
        }
    }
    seeds.push_back(seed);
}

/// Picks a seed value for one branch of a shoulder/elbow/wrist posture: the midpoint of the
/// joint's limit range restricted to the requested sign (negative half for `sign < 0`, else the
/// non-negative half), or `joint.home` if the joint has no configured limits.
/// @param joint the joint whose limits (if any) define the half-range
/// @param sign requested branch sign: negative selects the lower/negative half, non-negative the upper/positive half
double signedHalfRangeValue(const Joint& joint, int sign)
{
    if (!joint.limits.has_value()) {
        return joint.home;
    }
    if (sign < 0) {
        return 0.5 * (joint.limits->lower + std::min(0.0, joint.limits->upper));
    }
    return 0.5 * (std::max(0.0, joint.limits->lower) + joint.limits->upper);
}

/// If `branch` is set, overwrites `q[movableIndex]` with signedHalfRangeValue() for the joint at
/// that movable-joint position (a no-op if `branch` is unset or `movableIndex` is out of range).
/// @param q joint vector to modify in place
/// @param config used to find the Joint corresponding to `movableIndex` (in movable-joint order)
/// @param movableIndex index, among movable joints only, of the joint to seed
/// @param branch requested branch sign for that joint, or nullopt to leave `q` unchanged
void applyPostureBranchSeedValue(Eigen::VectorXd& q,
                                 const SerialRobotConfig& config,
                                 int movableIndex,
                                 const std::optional<int>& branch)
{
    if (!branch.has_value() || movableIndex < 0 || movableIndex >= q.size()) {
        return;
    }

    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }
        if (index == movableIndex) {
            q[index] = signedHalfRangeValue(joint, *branch);
            return;
        }
        ++index;
    }
}

/// Derives a seed joint vector from a requested posture, for the one resolver this seeding
/// strategy understands ("serial_6dof_shoulder_elbow_wrist" with at least 6 movable joints):
/// starts from midpointJoint() and overwrites the shoulder/elbow/wrist joints (movable indices
/// 0, 2, 4) with signedHalfRangeValue() per posture.shoulder/elbow/wrist.
/// @return the posture-derived seed, or nullopt if `config`'s posture resolver/DOF don't match
std::optional<JointVector> postureSeed(const SerialRobotConfig& config, const ArmPosture& posture)
{
    if (config.posture.resolver != "serial_6dof_shoulder_elbow_wrist"
        || ForwardKinematics::movableJointCount(config) < 6) {
        return std::nullopt;
    }

    Eigen::VectorXd q = midpointJoint(config).values();
    applyPostureBranchSeedValue(q, config, 0, posture.shoulder);
    applyPostureBranchSeedValue(q, config, 2, posture.elbow);
    applyPostureBranchSeedValue(q, config, 4, posture.wrist);
    return JointVector(q);
}

/// Assembles the ordered list of seed joint configurations solveFromSeed() will be tried from,
/// in priority order: request.previousJoint, request.seedJoint, a posture-derived seed
/// (postureSeed(), if applicable), the home configuration, the midpoint configuration, and
/// finally pseudo-random seeds (generated via a fractional-part-of-golden-ratio-multiple
/// sequence over each joint's range, or [-1, 1] for unlimited joints) until either
/// `defaults.maxSeeds` unique seeds are collected or 4x that many candidates have been tried.
/// Seeds are deduplicated via appendUniqueSeed()/defaults.duplicateJointTolerance throughout.
/// @return the seed list, containing at most `defaults.maxSeeds` entries
std::vector<JointVector> buildSeeds(const SerialRobotConfig& config,
                                    const IKRequest& request,
                                    const NumericalIKDefaults& defaults)
{
    std::vector<JointVector> seeds;
    if (request.previousJoint.has_value()) {
        appendUniqueSeed(seeds, *request.previousJoint, defaults.duplicateJointTolerance);
    }
    if (request.seedJoint.has_value()) {
        appendUniqueSeed(seeds, *request.seedJoint, defaults.duplicateJointTolerance);
    }
    if (request.posture.has_value()) {
        const std::optional<JointVector> seed = postureSeed(config, *request.posture);
        if (seed.has_value()) {
            appendUniqueSeed(seeds, *seed, defaults.duplicateJointTolerance);
        }
    }

    appendUniqueSeed(seeds, homeJoint(config), defaults.duplicateJointTolerance);
    appendUniqueSeed(seeds, midpointJoint(config), defaults.duplicateJointTolerance);

    const int dof = ForwardKinematics::movableJointCount(config);
    for (int s = 0; static_cast<int>(seeds.size()) < defaults.maxSeeds && s < defaults.maxSeeds * 4; ++s) {
        Eigen::VectorXd q(dof);
        int index = 0;
        for (const Joint& joint : config.joints) {
            if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
                continue;
            }
            const double lower = joint.limits.has_value() ? joint.limits->lower : -1.0;
            const double upper = joint.limits.has_value() ? joint.limits->upper : 1.0;
            const double fraction = std::fmod((s + 1) * (index + 1) * 0.6180339887498948, 1.0);
            q[index] = lower + fraction * (upper - lower);
            ++index;
        }
        appendUniqueSeed(seeds, JointVector(q), defaults.duplicateJointTolerance);
    }
    return seeds;
}

/// Computes the Cartesian pose error driving one damped-least-squares iteration: position error
/// as target-minus-current translation, and orientation error as the axis-angle rotation vector
/// of the rotation that takes current to target (zeroed if the angle is negligibly small,
/// avoiding a degenerate axis). The 6-vector residual applies
/// defaults.positionResidualWeight/orientationResidualWeight to the respective halves.
/// @return the weighted residual vector plus the unweighted position/orientation error norms
Residual computeResidual(const Pose& current, const Pose& target, const NumericalIKDefaults& defaults)
{
    Residual residual;
    const Eigen::Vector3d position = target.translation_m() - current.translation_m();
    const Eigen::Matrix3d rotationError = target.isometry().linear() * current.isometry().linear().transpose();
    const Eigen::AngleAxisd angleAxis(rotationError);
    Eigen::Vector3d orientation = Eigen::Vector3d::Zero();
    if (std::abs(angleAxis.angle()) > 1e-14) {
        orientation = angleAxis.axis() * angleAxis.angle();
    }

    residual.positionNorm = position.norm();
    residual.orientationNorm = std::abs(angleAxis.angle());
    residual.vector.template segment<3>(0) = defaults.positionResidualWeight * position;
    residual.vector.template segment<3>(3) = defaults.orientationResidualWeight * orientation;
    return residual;
}

/// Builds the 6xN geometric Jacobian (linear velocity rows 0-2, angular velocity rows 3-5) of
/// the tool point `currentTool` with respect to each joint in `chain`: for a revolute joint,
/// column = [axis x (tool - jointOrigin); axis]; for a prismatic joint, column = [axis; 0]; for
/// any other joint type, the column is zero.
/// @return the assembled Jacobian, with one column per entry in chain.joints
Eigen::MatrixXd geometricJacobian(const FkChain& chain, const Pose& currentTool)
{
    Eigen::MatrixXd jacobian(6, static_cast<Eigen::Index>(chain.joints.size()));
    const Eigen::Vector3d end = currentTool.translation_m();

    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(chain.joints.size()); ++i) {
        const JointFrameData& joint = chain.joints[static_cast<std::size_t>(i)];
        if (joint.type == JointType::Revolute) {
            jacobian.block<3, 1>(0, i) = joint.axisInBase.cross(end - joint.originInBase);
            jacobian.block<3, 1>(3, i) = joint.axisInBase;
        } else if (joint.type == JointType::Prismatic) {
            jacobian.block<3, 1>(0, i) = joint.axisInBase;
            jacobian.block<3, 1>(3, i).setZero();
        } else {
            jacobian.block<6, 1>(0, i).setZero();
        }
    }
    return jacobian;
}

/// Thin wrapper that forwards to IKSolutionRanker::score() with `defaults.jointLimitAvoidanceWeight`
/// as the joint-limit-avoidance weight.
IKSolution scoreSolution(const SerialRobotConfig& config,
                         const IKRequest& request,
                         JointVector joints,
                         double positionError,
                         double orientationError,
                         const NumericalIKDefaults& defaults)
{
    return IKSolutionRanker::score(config, request, std::move(joints), positionError, orientationError,
                                   defaults.jointLimitAvoidanceWeight);
}

/// Outcome of running the damped-least-squares iteration from a single seed.
struct SolveAttempt {
    bool converged = false;                                                    ///< True if the iteration reached the target's position/orientation tolerances (and satisfied posture, if required).
    IKSolution solution;                                                       ///< The scored solution, populated only when converged is true.
    KinematicsStatus failureStatus = KinematicsStatus::MaxIterationsReached;   ///< Reason for non-convergence, meaningful only when converged is false.
};

/// Runs the adaptive damped-least-squares (Levenberg-Marquardt-style) iteration from `seed`
/// toward `context.targetFlangeInBase`: each iteration computes the pose residual and geometric
/// Jacobian, solves a damped normal-equations step, clamps it to `defaults.maxJointStepNorm`,
/// and accepts it only if it reduces the residual cost (otherwise the damping factor is
/// increased and the step discarded); damping is decreased after accepted steps and increased
/// after rejected or stalled ones. Returns as soon as the residual is within the request's
/// position/orientation tolerances (checking the posture constraint if
/// `context.request.options.requirePosture` and `context.request.posture` are set), or fails
/// early on a non-finite step, a step norm at/below `defaults.stepTolerance`, or exhausting
/// `defaults.maxIterations`.
/// @param config robot model to solve against
/// @param context target pose, request, and solve options
/// @param seed starting joint configuration (clamped to joint limits before iterating)
/// @param defaults tuning parameters (iteration cap, tolerances, damping schedule, weights)
/// @return a converged SolveAttempt with its scored solution, or a failed one with the
///         KinematicsStatus explaining why (NumericalError, NoConvergedSolution,
///         PostureConstraintUnsatisfied, InvalidRobotConfig, or MaxIterationsReached)
SolveAttempt solveFromSeed(const SerialRobotConfig& config,
                           const IKSolveContext& context,
                           const JointVector& seed,
                           const NumericalIKDefaults& defaults)
{
    const int dof = ForwardKinematics::movableJointCount(config);
    Eigen::VectorXd q = clampToLimits(config, seed.values());
    double damping = defaults.initialDamping;
    double previousCost = std::numeric_limits<double>::infinity();

    for (int iteration = 0; iteration < defaults.maxIterations; ++iteration) {
        const JointVector currentJoints(q);
        const FkChain chain = ForwardKinematics::computeChain(config, currentJoints);
        const Pose current = chain.flangeInBase;
        const Residual residual = computeResidual(current, context.targetFlangeInBase, defaults);

        if (residual.positionNorm <= context.request.options.maxPositionError_m
            && residual.orientationNorm <= context.request.options.maxOrientationError_rad) {
            IKSolution solution = scoreSolution(config, context.request, currentJoints,
                                                residual.positionNorm, residual.orientationNorm, defaults);
            if (context.request.options.requirePosture && context.request.posture.has_value()
                && !IKSolutionRanker::postureMatches(solution.posture, *context.request.posture)) {
                return SolveAttempt{false, {}, KinematicsStatus::PostureConstraintUnsatisfied};
            }

            SolveAttempt attempt;
            attempt.converged = true;
            attempt.solution = std::move(solution);
            return attempt;
        }

        const double cost = residual.vector.squaredNorm();
        if (std::abs(previousCost - cost) <= defaults.costTolerance) {
            damping = std::min(defaults.maxDamping, damping * defaults.dampingIncreaseFactor);
        }
        previousCost = cost;

        const Eigen::MatrixXd jacobian = geometricJacobian(chain, current);
        const Eigen::Matrix<double, 6, 6> normal =
            jacobian * jacobian.transpose()
            + Eigen::Matrix<double, 6, 6>::Identity() * damping * damping;
        Eigen::VectorXd step = jacobian.transpose() * normal.ldlt().solve(residual.vector);
        if (!step.allFinite()) {
            return SolveAttempt{false, {}, KinematicsStatus::NumericalError};
        }
        const double stepNorm = step.norm();
        if (stepNorm <= defaults.stepTolerance) {
            return SolveAttempt{false, {}, KinematicsStatus::NoConvergedSolution};
        }
        if (stepNorm > defaults.maxJointStepNorm) {
            step *= defaults.maxJointStepNorm / stepNorm;
        }

        const Eigen::VectorXd candidate = clampToLimits(config, q + step);
        const Pose candidatePose = ForwardKinematics::flangePose(config, JointVector(candidate));
        const Residual candidateResidual = computeResidual(candidatePose, context.targetFlangeInBase, defaults);
        if (candidateResidual.vector.squaredNorm() < cost) {
            q = candidate;
            damping = std::max(defaults.minDamping, damping * defaults.dampingDecreaseFactor);
        } else {
            damping = std::min(defaults.maxDamping, damping * defaults.dampingIncreaseFactor);
        }
    }

    if (dof == 0) {
        return SolveAttempt{false, {}, KinematicsStatus::InvalidRobotConfig};
    }
    return SolveAttempt{false, {}, KinematicsStatus::MaxIterationsReached};
}

/// Checks whether `candidate`'s joint configuration matches (within `tolerance`, via
/// sameJointVector()) any solution already in `solutions`.
bool duplicateSolution(const std::vector<IKSolution>& solutions, const IKSolution& candidate, double tolerance)
{
    for (const IKSolution& solution : solutions) {
        if (sameJointVector(solution.joints, candidate.joints, tolerance)) {
            return true;
        }
    }
    return false;
}
}

/// Constructs the solver, storing `defaults` (or NumericalIKDefaults{} if omitted) as the
/// tuning parameters used by every subsequent solve()/solveAll() call on this instance.
NumericalIKSolver::NumericalIKSolver(NumericalIKDefaults defaults)
    : defaults_(defaults)
{
}

/// Returns this solver's registered name, "adaptive_damped_least_squares".
const char* NumericalIKSolver::name() const
{
    return "adaptive_damped_least_squares";
}

/// Runs solveAll() and keeps only its single lowest-cost solution (solveAll() already returns
/// solutions sorted by ascending total cost, so this just truncates to the first entry).
IKResult NumericalIKSolver::solve(const SerialRobotConfig& config, const IKSolveContext& context) const
{
    IKResult result = solveAll(config, context);
    if (result.solutions.size() > 1) {
        result.solutions.resize(1);
    }
    return result;
}

/// Validates `config` and `context.targetFlangeInBase`/seed dimensions, then tries a
/// damped-least-squares solve (solveFromSeed()) from each seed produced by buildSeeds(),
/// collecting deduplicated (duplicateSolution()) converged solutions until
/// `context.request.options.maxSolutions` is reached (if positive). On success the collected
/// solutions are sorted by ascending IKSolution::score.totalCost and returned with
/// IKStatus::Ok; if no seed converges, returns the last seed's failure status as-is (IKStatus
/// is an alias of KinematicsStatus) with an explanatory message.
/// @return IKResult with status Ok and at least one solution on success; otherwise a failure
///         status (InvalidRequest/JointDimensionMismatch from validation, or the last attempt's
///         failure status) and message
IKResult NumericalIKSolver::solveAll(const SerialRobotConfig& config, const IKSolveContext& context) const
{
    const ModelValidationResult model = RobotModelValidator::validateSerialRobotConfig(config);
    if (!model.ok()) {
        return IKResult{model.status(), {}, model.issues.front().message};
    }
    if (!finitePose(context.targetFlangeInBase)) {
        return IKResult{IKStatus::InvalidRequest, {}, "target pose contains non-finite values"};
    }

    const int dof = ForwardKinematics::movableJointCount(config);
    if ((context.request.seedJoint.has_value() && context.request.seedJoint->size() != dof)
        || (context.request.previousJoint.has_value() && context.request.previousJoint->size() != dof)) {
        return IKResult{IKStatus::JointDimensionMismatch, {}, "seed or previous joint dimension mismatch"};
    }

    IKResult result;
    result.status = IKStatus::NoConvergedSolution;
    KinematicsStatus lastFailure = KinematicsStatus::NoConvergedSolution;

    const std::vector<JointVector> seeds = buildSeeds(config, context.request, defaults_);
    for (const JointVector& seed : seeds) {
        const JointLimitCheck seedLimits = JointLimitValidator::validate(config, seed);
        if (seedLimits.status == KinematicsStatus::JointDimensionMismatch) {
            return IKResult{IKStatus::JointDimensionMismatch, {}, "generated seed dimension mismatch"};
        }

        const SolveAttempt attempt = solveFromSeed(config, context, seed, defaults_);
        if (attempt.converged) {
            if (!duplicateSolution(result.solutions, attempt.solution, defaults_.duplicateJointTolerance)) {
                result.solutions.push_back(attempt.solution);
            }
            if (context.request.options.maxSolutions > 0
                && static_cast<int>(result.solutions.size()) >= context.request.options.maxSolutions) {
                break;
            }
        } else {
            lastFailure = attempt.failureStatus;
        }
    }

    if (!result.solutions.empty()) {
        std::sort(result.solutions.begin(), result.solutions.end(), [](const IKSolution& a, const IKSolution& b) {
            return a.score.totalCost < b.score.totalCost;
        });
        result.status = IKStatus::Ok;
        return result;
    }

    result.status = lastFailure;
    result.message = "no converged numerical IK solution";
    return result;
}

} // namespace RobotKinematics
