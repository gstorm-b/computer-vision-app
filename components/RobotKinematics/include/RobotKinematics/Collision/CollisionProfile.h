#pragma once

#include <RobotKinematics/Collision/CollisionGeometry.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <map>
#include <string>
#include <vector>

namespace RobotKinematics {

/// Full analytic collision profile for a robot model: its collision geometries, the
/// pairs excluded from checking, and the supporting metadata/sources loaded from the
/// `robot-kinematics-collision/v1` JSON schema (see CollisionProfileJsonLoader).
struct CollisionProfile {
    std::string id;  ///< Unique identifier of this collision profile.
    std::string robotModel;  ///< Robot model this profile applies to.
    std::vector<CollisionGeometry> geometries;  ///< Analytic collision volumes for the robot's links.
    std::vector<DisabledCollisionPair> disabledPairs;  ///< Geometry-id pairs excluded from collision checks.
    std::vector<SourceReference> sources;  ///< Documentation/reference sources backing this profile's data.
    std::map<std::string, std::string> metadata;  ///< Free-form string metadata carried through from the JSON.
};

} // namespace RobotKinematics
