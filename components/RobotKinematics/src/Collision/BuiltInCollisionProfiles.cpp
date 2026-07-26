#include <RobotKinematics/Collision/BuiltInCollisionProfiles.h>

#include <RobotKinematics/Core/Pose.h>

/// Built-in collision profiles: conservative-primitive collision approximations
/// (spheres/capsules per link) for robot models shipped with the project, used
/// as fallbacks when no vendor-specific collision profile is available.
namespace RobotKinematics::CollisionProfiles {

namespace {
/// Builds a sphere CollisionGeometry attached to `linkId`, offset from the link
/// frame by `geometryToLink`.
/// @param id unique geometry identifier within the owning profile
/// @param linkId kinematic link the sphere is rigidly attached to
/// @param geometryToLink pose of the sphere's frame relative to the link frame
/// @param radius_m sphere radius in metres
/// @param margin_m extra clearance margin added around the shape (metres)
CollisionGeometry sphere(const std::string& id,
                         const std::string& linkId,
                         const Pose& geometryToLink,
                         double radius_m,
                         double margin_m = 0.0)
{
    CollisionGeometry geometry;
    geometry.id = id;
    geometry.linkId = linkId;
    geometry.shape.type = CollisionShapeType::Sphere;
    geometry.shape.sphere.radius_m = radius_m;
    geometry.geometryToLink = geometryToLink;
    geometry.margin_m = margin_m;
    return geometry;
}

/// Builds a capsule CollisionGeometry attached to `linkId`, offset from the link
/// frame by `geometryToLink`.
/// @param id unique geometry identifier within the owning profile
/// @param linkId kinematic link the capsule is rigidly attached to
/// @param geometryToLink pose of the capsule's frame relative to the link frame
/// @param radius_m capsule radius in metres
/// @param length_m capsule segment length in metres (excluding end caps)
/// @param margin_m extra clearance margin added around the shape (metres)
CollisionGeometry capsule(const std::string& id,
                          const std::string& linkId,
                          const Pose& geometryToLink,
                          double radius_m,
                          double length_m,
                          double margin_m = 0.0)
{
    CollisionGeometry geometry;
    geometry.id = id;
    geometry.linkId = linkId;
    geometry.shape.type = CollisionShapeType::Capsule;
    geometry.shape.capsule.radius_m = radius_m;
    geometry.shape.capsule.length_m = length_m;
    geometry.geometryToLink = geometryToLink;
    geometry.margin_m = margin_m;
    return geometry;
}

/// Builds a DisabledCollisionPair excluding geometries `a` and `b` from
/// pairwise collision checks, annotated with `reason` (e.g. adjacent-joint
/// contact that would otherwise produce false-positive collisions).
DisabledCollisionPair disabled(const std::string& a, const std::string& b, const std::string& reason)
{
    return DisabledCollisionPair{a, b, reason};
}
}

/// Builds the conservative-primitive collision profile for the
/// "Virtual6DofTestArm" project fixture: one sphere/capsule per link
/// (base through flange) sized to conservatively enclose the visual model,
/// with adjacent-joint pairs disabled and metadata marking it as an
/// unrated, project-owned approximation.
/// @return the fully populated CollisionProfile for the virtual 6-DOF test arm
CollisionProfile virtual6DofTestArm()
{
    CollisionProfile profile;
    profile.id = "virtual_6dof_test_arm_conservative_primitives";
    profile.robotModel = "Virtual6DofTestArm";
    profile.geometries = {
        sphere("base_body", "base_link", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.12, 0.0, 0.0, 0.0), 0.17, 0.01),
        sphere("shoulder_body", "link_1", Pose::fromXYZRPY_m_rad(0.08, 0.0, 0.10, 0.0, 0.0, 0.0), 0.08, 0.005),
        capsule("upper_arm_capsule", "link_2",
                Pose::fromXYZRPY_m_rad(0.175, 0.0, 0.0, 0.0, 1.5707963267948966, 0.0),
                0.07, 0.35, 0.005),
        capsule("forearm_capsule", "link_3",
                Pose::fromXYZRPY_m_rad(0.125, 0.0, 0.0, 0.0, 1.5707963267948966, 0.0),
                0.06, 0.25, 0.005),
        sphere("wrist_body", "link_4", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.09, 0.0, 0.0, 0.0), 0.08, 0.004),
        sphere("wrist_pitch_body", "link_5", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.05, 0.0, 0.0, 0.0), 0.06, 0.004),
        sphere("flange_body", "flange", Pose::identity(), 0.05, 0.003),
    };
    profile.disabledPairs = {
        disabled("base_body", "shoulder_body", "adjacent_joint_contact"),
        disabled("shoulder_body", "upper_arm_capsule", "adjacent_joint_contact"),
        disabled("upper_arm_capsule", "forearm_capsule", "adjacent_joint_contact"),
        disabled("forearm_capsule", "wrist_body", "adjacent_joint_contact"),
        disabled("wrist_body", "wrist_pitch_body", "adjacent_joint_contact"),
        disabled("wrist_pitch_body", "flange_body", "adjacent_joint_contact"),
    };
    profile.sources = {
        SourceReference{
            "project_fixture",
            "Virtual 6DOF collision approximation",
            "docs/planning/collision_detection_plan.md",
            {"collision_geometry"},
        },
    };
    profile.metadata["approximation"] = "conservative_primitives";
    profile.metadata["safety_rating"] = "not_safety_rated";
    profile.metadata["review_notes"] = "project_owned_virtual_fixture";
    return profile;
}

/// Builds the conservative-primitive collision profile for the Nachi MZ04D
/// robot, estimated from the vendor's visual CAD assets: one sphere/capsule
/// per link (base through flange) with adjacent-joint pairs disabled and
/// metadata marking it as an unrated, CAD-based estimate.
/// @return the fully populated CollisionProfile for the Nachi MZ04D
CollisionProfile nachiMZ04D()
{
    CollisionProfile profile;
    profile.id = "nachi_mz04d_conservative_primitives";
    profile.robotModel = "MZ04D";
    profile.geometries = {
        sphere("base_body", "base_link", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.17, 0.0, 0.0, 0.0), 0.10, 0.01),
        capsule("shoulder_column", "link_1",
                Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.17, 0.0, 0.0, 0.0),
                0.08, 0.34, 0.005),
        capsule("upper_arm_capsule", "link_2",
                Pose::fromXYZRPY_m_rad(0.16, 0.0, 0.0, 0.0, 1.5707963267948966, 0.0),
                0.045, 0.20, 0.005),
        sphere("elbow_body", "link_3", Pose::fromXYZRPY_m_rad(0.025, -0.12, 0.0, 0.0, 0.0, 0.0), 0.08, 0.005),
        sphere("forearm_body", "link_4", Pose::fromXYZRPY_m_rad(0.0, 0.0, -0.03, 0.0, 0.0, 0.0), 0.03, 0.004),
        sphere("wrist_pitch_body", "link_5", Pose::identity(), 0.05, 0.004),
        sphere("flange_body", "flange", Pose::identity(), 0.025, 0.003),
    };
    profile.disabledPairs = {
        disabled("base_body", "shoulder_column", "adjacent_joint_contact"),
        disabled("shoulder_column", "upper_arm_capsule", "adjacent_joint_contact"),
        disabled("upper_arm_capsule", "elbow_body", "adjacent_joint_contact"),
        disabled("elbow_body", "forearm_body", "adjacent_joint_contact"),
        disabled("forearm_body", "wrist_pitch_body", "adjacent_joint_contact"),
        disabled("wrist_pitch_body", "flange_body", "adjacent_joint_contact"),
    };
    profile.sources = {
        SourceReference{
            "project_estimate",
            "Conservative primitive approximation from Nachi visual CAD assets",
            "presets/Nachi/MZ04",
            {"collision_geometry"},
        },
    };
    profile.metadata["approximation"] = "conservative_primitives";
    profile.metadata["safety_rating"] = "not_safety_rated";
    profile.metadata["review_notes"] = "visual_cad_based_estimate";
    return profile;
}

} // namespace RobotKinematics::CollisionProfiles
