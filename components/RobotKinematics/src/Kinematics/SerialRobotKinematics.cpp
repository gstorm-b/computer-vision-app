#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>

#include <RobotKinematics/Model/FrameRegistry.h>
#include <RobotKinematics/Model/RobotModelValidator.h>
#include <RobotKinematics/Model/ToolRegistry.h>
#include <RobotKinematics/Solvers/Analytic6DofSphericalWristSolver.h>
#include <RobotKinematics/Solvers/NumericalIKSolver.h>

#include <Eigen/Geometry>

#include <utility>

namespace RobotKinematics {

namespace {
/// Resolves the pose (relative to the robot's base link) of the frame identified by `id`,
/// used as an IK request's reference frame. An empty id, the configured base link id, and the
/// literal "base" all resolve to the identity pose. Otherwise looks the frame up via
/// FrameRegistry and requires it to be parented directly to the base link.
/// @param config supplies the frame registry and the base link id
/// @param id the reference frame id from the IK request (may be empty, meaning "base")
/// @return the frame's pose in base on success; a failure (InvalidRequest if the frame is not
///         parented to the base link, otherwise the registry lookup's own status) on failure
Result<Pose> referenceFrameInBase(const SerialRobotConfig& config, const FrameId& id)
{
    if (id.empty() || id.value == config.frames.baseLinkId || id.value == "base") {
        return Result<Pose>::success(Pose::identity());
    }

    const FrameRegistry frames = FrameRegistry::fromConfig(config);
    Result<UserFrame> frame = frames.get(id);
    if (!frame.ok()) {
        return Result<Pose>::failure(frame.status, frame.message);
    }
    if (frame.value.parentLinkId != config.frames.baseLinkId) {
        return Result<Pose>::failure(KinematicsStatus::InvalidRequest,
                                     "IK reference frame must be fixed to the base link");
    }
    return Result<Pose>::success(frame.value.transform);
}

/// Resolves the tool to use for an IK request: the robot's default tool when `id` is empty,
/// otherwise the tool registered under `id`.
/// @param config supplies the tool registry
/// @param id requested tool id, or empty to use the robot's default tool
/// @return the resolved tool on success, or the registry lookup's failure status/message
Result<Tool> resolveTool(const SerialRobotConfig& config, const ToolId& id)
{
    const ToolRegistry tools = ToolRegistry::fromConfig(config);
    return id.empty() ? tools.getDefault() : tools.get(id);
}
}

/// Constructs the solver for a fixed robot configuration, taking ownership of `config` by move.
SerialRobotKinematics::SerialRobotKinematics(SerialRobotConfig config)
    : config_(std::move(config))
{
}

/// Returns the robot configuration this solver was constructed with.
const SerialRobotConfig& SerialRobotKinematics::config() const
{
    return config_;
}

/// Solves `request`, returning the single closest/best-ranked solution (delegates to run()
/// with allSolutions = false).
IKResult SerialRobotKinematics::solve(const IKRequest& request) const
{
    return run(request, false);
}

/// Solves `request` and returns every valid candidate solution found, ranked (delegates to
/// run() with allSolutions = true).
IKResult SerialRobotKinematics::solveAll(const IKRequest& request) const
{
    return run(request, true);
}

/// Shared implementation behind solve()/solveAll(): validates config_, resolves the request's
/// tool and reference frame, derives the IK target pose at the flange (reference-frame pose
/// composed with the target pose and the inverse tool flange-to-tcp offset), then solves it —
/// preferring the analytic spherical-wrist solver when it supports this robot's model and
/// finds an accepted solution, falling back to the numerical solver otherwise.
/// @param request the IK request to solve
/// @param allSolutions when true, return every valid candidate solution rather than just the
///        best-ranked one
/// @return the model/tool/reference-frame validation failure if any of those checks fail
///         first; otherwise the chosen solver's IKResult
IKResult SerialRobotKinematics::run(const IKRequest& request, bool allSolutions) const
{
    const ModelValidationResult model = RobotModelValidator::validateSerialRobotConfig(config_);
    if (!model.ok()) {
        return IKResult{model.status(), {}, model.issues.front().message};
    }

    const Result<Tool> tool = resolveTool(config_, request.tool);
    if (!tool.ok()) {
        return IKResult{tool.status, {}, tool.message};
    }

    const Result<Pose> baseFromReference = referenceFrameInBase(config_, request.referenceFrame);
    if (!baseFromReference.ok()) {
        return IKResult{baseFromReference.status, {}, baseFromReference.message};
    }

    IKSolveContext context;
    context.request = request;
    context.targetFlangeInBase = baseFromReference.value * request.targetPose * tool.value.flangeToTcp.inverse();

    // Hybrid policy: use the analytic plugin when it supports the model and returns a
    // solution; otherwise fall back to the numerical solver.
    const Analytic6DofSphericalWristSolver analytic;
    if (analytic.supportsModel(config_)) {
        const IKResult analyticResult =
            allSolutions ? analytic.solveAll(config_, context) : analytic.solve(config_, context);
        if (analyticResult.ok()) {
            return analyticResult;
        }
    }

    const NumericalIKSolver solver;
    return allSolutions ? solver.solveAll(config_, context) : solver.solve(config_, context);
}

} // namespace RobotKinematics
