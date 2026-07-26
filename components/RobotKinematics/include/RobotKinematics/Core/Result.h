#pragma once

#include <string>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Structured outcome code carried by Result<T> instead of throwing an exception.
enum class KinematicsStatus {
    Ok,                              ///< Operation succeeded.
    InvalidRobotConfig,              ///< The robot model/configuration failed validation.
    InvalidRequest,                  ///< The request's own parameters are invalid (e.g. bad path, negative margin).
    FrameNotFound,                   ///< A referenced FrameId does not exist in the robot/chain.
    ToolNotFound,                    ///< A referenced ToolId does not exist in the robot model.
    JointDimensionMismatch,          ///< A JointVector's size does not match the robot's degree-of-freedom count.
    JointLimitViolation,             ///< A joint value falls outside its configured limits.
    TargetUnreachable,               ///< No IK solution exists for the requested target pose.
    Singularity,                     ///< The robot is at or too near a kinematic singularity to solve/move safely.
    MaxIterationsReached,            ///< An iterative solver stopped after exhausting its iteration budget.
    NoConvergedSolution,             ///< An iterative solver ran but did not converge to a valid solution.
    PostureConstraintUnsatisfied,    ///< The candidate solution violates a requested posture constraint/label.
    UnsupportedSolver,               ///< The requested solver/backend is not supported for this configuration.
    NumericalError                  ///< A numerical computation failed (e.g. non-finite result, backend library error).
};

/// Lightweight status + value wrapper for lookups and operations that can fail
/// with a structured KinematicsStatus instead of throwing.
template <typename T>
struct Result {
    KinematicsStatus status = KinematicsStatus::Ok;  ///< Outcome of the operation; Ok on success.
    T value{};                                       ///< Result payload; default-constructed/unused on failure.
    std::string message;                             ///< Human-readable detail, set on failure (may be empty on success).

    /// Returns true if status is KinematicsStatus::Ok.
    bool ok() const { return status == KinematicsStatus::Ok; }

    /// Builds a successful result wrapping `v`.
    static Result success(T v) { return Result{KinematicsStatus::Ok, std::move(v), {}}; }
    /// Builds a failed result with the given status and an optional detail message.
    /// @param s the failure status (must not be Ok, by convention)
    /// @param msg human-readable detail describing the failure
    static Result failure(KinematicsStatus s, std::string msg = {})
    {
        return Result{s, T{}, std::move(msg)};
    }
};

} // namespace RobotKinematics
