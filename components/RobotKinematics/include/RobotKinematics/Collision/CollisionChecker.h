#pragma once

#include <RobotKinematics/Collision/CollisionProfile.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <cstddef>
#include <string>
#include <vector>

namespace RobotKinematics {

/// Parameters for a primitive-backend self-collision check.
struct CollisionCheckRequest {
    JointVector joints;             ///< Joint configuration to evaluate collisions at.
    double safetyMargin_m = 0.0;    ///< Extra clearance margin required between geometries, in metres.
    bool returnAllPairs = true;     ///< True to evaluate and return every pair; false to stop at the first collision.
};

/// Collision/clearance outcome for one candidate pair of collision geometries.
struct CollisionPairResult {
    std::string geometryA;          ///< Id of the first geometry in the pair.
    std::string geometryB;          ///< Id of the second geometry in the pair.
    std::string linkA;              ///< Kinematic link geometryA is attached to.
    std::string linkB;              ///< Kinematic link geometryB is attached to.
    bool colliding = false;         ///< True if the pair's clearance is at or below the requested safety margin.
    double distance_m = 0.0;        ///< Signed clearance distance between the shape surfaces, in metres (negative if overlapping).
    std::size_t contactCount = 0;   ///< Number of contact points reported, if the backend supports it (0 for the primitive backend).
};

/// Overall result of a self-collision check across every evaluated geometry pair.
struct CollisionCheckResult {
    KinematicsStatus status = KinematicsStatus::Ok;  ///< Ok on a completed check; another status on invalid input.
    bool hasCollision = false;      ///< True if any evaluated pair is colliding.
    std::vector<CollisionPairResult> pairs;  ///< Per-pair results; empty or partial if `returnAllPairs` was false and a collision was found.
    std::string message;            ///< Human-readable detail, populated on failure.

    /// @return true when `status` is KinematicsStatus::Ok
    bool ok() const { return status == KinematicsStatus::Ok; }
};

/// Analytic self-collision checker for the built-in sphere/capsule primitive shapes: computes
/// pairwise clearance distances via closest-point-between-segments math (with a bounding-sphere
/// broad phase), skipping same-link and explicitly disabled pairs.
class CollisionChecker
{
public:
    /// Validates `config`/`profile`/`request`, computes the FK chain, places every enabled
    /// collision geometry in the base frame, and evaluates pairwise clearance/collision for every
    /// non-disabled, non-same-link pair.
    /// @param config robot model the joint values are evaluated against
    /// @param profile primitive collision geometry/disabled-pair definitions to check
    /// @param request joint configuration, safety margin, and pair-reporting options
    /// @return per-pair collision/distance results, or a failure status if the model/profile fail
    /// validation or the joint vector's dimension does not match the robot's dof
    static CollisionCheckResult check(const SerialRobotConfig& config,
                                      const CollisionProfile& profile,
                                      const CollisionCheckRequest& request);
};

} // namespace RobotKinematics
