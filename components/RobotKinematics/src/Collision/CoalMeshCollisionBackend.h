#pragma once

#include <RobotKinematics/Collision/CollisionBackend.h>

/// Robot kinematics, collision profile, and forward-kinematics types shared across the
/// mesh collision backends.
namespace RobotKinematics {

/// Returns static capability metadata for the coal (HPP-FCL fork) mesh collision backend:
/// its kind, name "coal", and the fact that it is compiled in and supports mesh, distance,
/// and contact reporting.
CollisionBackendInfo coalMeshBackendInfo();

/// Runs mesh-vs-mesh self-collision checking for `config` using the coal backend: validates
/// the robot config, profile, and joint dimension, loads/caches each enabled mesh as a coal
/// BVH geometry, poses it via forward kinematics, and checks every non-disabled, distinct-link
/// pair for distance/collision using coal's distance and collision queries.
/// @param config the robot model whose links carry the mesh collision geometry
/// @param profile mesh collision profile (per-link STL meshes, margins, disabled pairs)
/// @param request joint state, safety margin, and pair-reporting options for the check
/// @return per-pair distances/collision flags on success, or a failure result with a status
///         and message if validation, mesh loading, or the coal query fails
CollisionCheckResult checkCoalMeshBackend(const SerialRobotConfig& config,
                                          const MeshCollisionProfile& profile,
                                          const MeshCollisionCheckRequest& request);

} // namespace RobotKinematics
