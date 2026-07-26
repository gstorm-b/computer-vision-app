#pragma once

#include <RobotKinematics/Core/Ids.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Posture/ArmPosture.h>

#include <optional>
#include <string>
#include <vector>

namespace RobotKinematics {

/// IK and validation share the canonical KinematicsStatus contract.
using IKStatus = KinematicsStatus;

/// Tuning knobs and convergence tolerances for an IK solve.
struct IKOptions {
    bool returnClosestToSeed = true;  ///< When true, rank/return the solution closest to seedJoint first.
    bool requirePosture = false;      ///< When true, reject solutions that don't match the requested posture.
    double maxPositionError_m = 1e-6; ///< Maximum acceptable TCP position error, in meters.
    /// Maximum acceptable TCP orientation error, in radians. Reconciled to the spec's
    /// NumericalIKDefaults / accuracy criteria (0.001 deg). The IKOptions example in the
    /// spec showed 1e-6 rad; the authoritative default is below.
    double maxOrientationError_rad = 1.7453292519943296e-5;
    int maxSolutions = 16; ///< Upper bound on the number of solutions returned by solveAll.
};

/// A single inverse-kinematics query: target pose plus solver hints and constraints.
struct IKRequest {
    Pose targetPose;                          ///< Target TCP pose, expressed in referenceFrame.
    std::optional<JointVector> seedJoint;     ///< Preferred starting joints.
    std::optional<JointVector> previousJoint; ///< Last commanded joints (motion continuity).
    std::optional<ArmPosture> posture;        ///< Requested/required posture branch.
    FrameId referenceFrame;                   ///< Reference frame for targetPose; empty => base frame.
    ToolId tool;                              ///< Tool whose TCP is being targeted; empty => default tool.
    IKOptions options;                        ///< Tuning knobs and convergence tolerances.
};

/// Cost breakdown used to rank multiple IK solutions against each other.
struct IKSolutionScore {
    double totalCost = 0.0;             ///< Sum of the weighted cost terms below.
    double seedDistanceCost = 0.0;      ///< Cost from joint-space distance to IKRequest::seedJoint.
    double jointLimitMarginCost = 0.0;  ///< Cost from proximity to joint limits.
    double postureMismatchCost = 0.0;   ///< Cost from deviating from the requested posture.
    double motionContinuityCost = 0.0;  ///< Cost from joint-space distance to IKRequest::previousJoint.
};

/// A single candidate joint solution for an IK request, with its resulting errors and rank score.
struct IKSolution {
    JointVector joints;               ///< Candidate joint values.
    ArmPosture posture;                ///< Posture branch of this candidate.
    double positionError_m = 0.0;      ///< Resulting TCP position error, in meters.
    double orientationError_rad = 0.0; ///< Resulting TCP orientation error, in radians.
    IKSolutionScore score;             ///< Ranking cost breakdown for this candidate.
};

/// Outcome of an IK solve: status plus zero or more ranked candidate solutions.
struct IKResult {
    IKStatus status = IKStatus::NoConvergedSolution; ///< Overall solve status.
    std::vector<IKSolution> solutions;               ///< Candidate solutions, best-ranked first.
    std::string message;                             ///< Human-readable detail, mainly for failures.

    /// True when at least one valid solution is available.
    bool ok() const { return status == IKStatus::Ok && !solutions.empty(); }

    /// Returns the best-ranked (first) solution.
    /// @note Precondition: !solutions.empty(). Callers should check ok() first.
    const IKSolution& best() const { return solutions.front(); }
};

} // namespace RobotKinematics
