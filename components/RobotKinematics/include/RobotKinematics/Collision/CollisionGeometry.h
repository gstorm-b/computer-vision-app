#pragma once

#include <RobotKinematics/Core/Pose.h>

#include <string>

/// Analytic (non-mesh) collision primitives, geometry attachment, and disabled-pair
/// records used by the simplified collision profile format.
namespace RobotKinematics {

/// Discriminates which analytic primitive a CollisionShape holds.
enum class CollisionShapeType {
    Sphere,
    Capsule
};

/// Analytic sphere primitive, defined solely by its radius.
struct CollisionSphere {
    double radius_m = 0.0;  ///< Sphere radius, in meters.
};

/// Analytic capsule primitive (cylinder with hemispherical caps).
struct CollisionCapsule {
    double radius_m = 0.0;  ///< Capsule radius, in meters.
    double length_m = 0.0;  ///< Capsule cylindrical segment length, in meters.
};

/// Tagged union of the analytic collision primitives: only the member matching `type`
/// is meaningful.
struct CollisionShape {
    CollisionShapeType type = CollisionShapeType::Sphere;  ///< Which primitive is active.
    CollisionSphere sphere;  ///< Valid when type == Sphere.
    CollisionCapsule capsule;  ///< Valid when type == Capsule.
};

/// A single analytic collision volume attached to a robot link.
struct CollisionGeometry {
    std::string id;  ///< Unique identifier of this geometry within its profile.
    std::string linkId;  ///< Id of the robot link this geometry is attached to.
    CollisionShape shape;  ///< The analytic primitive (sphere or capsule) for this geometry.
    Pose geometryToLink = Pose::identity();  ///< Transform from geometry frame to the link frame.
    double margin_m = 0.0;  ///< Extra clearance added around the shape, in meters.
    bool enabled = true;  ///< Whether this geometry participates in collision checking.
};

/// Records a pair of geometry ids whose mutual collision should be ignored, with a
/// human-readable justification.
struct DisabledCollisionPair {
    std::string geometryA;  ///< Id of the first geometry in the pair.
    std::string geometryB;  ///< Id of the second geometry in the pair.
    std::string reason;  ///< Why collisions between geometryA and geometryB are disabled.
};

} // namespace RobotKinematics
