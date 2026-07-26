#include <RobotKinematics/Presets/NachiMZ04D.h>

#include <RobotKinematics/Core/Pose.h>

#include <cmath>
#include <optional>

/// Built-in robot presets shipped with RobotKinematics (e.g. the Nachi MZ04D config below).
namespace RobotKinematics::Presets {

/// Internal helpers used only while assembling the Nachi MZ04D preset: angle-conversion
/// constants and a factory for the six identical-shape revolute joints.
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;  ///< Pi to double precision, for radian conversions.
constexpr double kHalfPi = kPi / 2.0;  ///< Pi/2, used as the recurring +/-90 deg DH twist angle below.

/// Converts an angle from degrees to radians using #kPi.
constexpr double deg(double d) { return d * kPi / 180.0; }

/// Builds one revolute joint with a Z-axis rotation axis, zero home position, and a "JT<n>"
/// alias derived from the numeric suffix of `id` (e.g. id "J1" -> alias "JT1").
/// @param id joint identifier (expected form "J<n>")
/// @param parent parent link id
/// @param child child link id
/// @param origin joint origin pose relative to the parent link
/// @param limits joint position/velocity/effort limits
/// @return the constructed Joint
Joint revolute(const std::string& id,
               const std::string& parent,
               const std::string& child,
               const Pose& origin,
               const JointLimits& limits)
{
    Joint joint;
    joint.id = id;
    joint.type = JointType::Revolute;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.origin = origin;
    joint.axis = Eigen::Vector3d::UnitZ();
    joint.limits = limits;
    joint.home = 0.0;
    joint.aliases = {"JT" + id.substr(1)};
    return joint;
}
} // namespace

/// Builds the SerialRobotConfig preset for the Nachi MZ04D 6-DOF serial arm: link/joint
/// topology and DH-derived origins, joint position limits from the teach pendant, tool
/// definitions, shoulder/elbow/wrist posture labeling, default IK solver parameters, and
/// source/metadata references back to docs/preset_references/nachi-mz04d.md.
/// @return the fully populated Nachi MZ04D robot configuration
SerialRobotConfig nachiMZ04D()
{
    SerialRobotConfig config;
    config.identity = RobotIdentity{"Nachi", "MZ04D", "Nachi MZ04D", "1.0.0"};
    config.topology = RobotTopologyType::Serial;
    config.dof = 6;
    config.links = {{"base_link"}, {"link_1"}, {"link_2"}, {"link_3"}, {"link_4"}, {"link_5"}, {"flange"}};
    config.frames.baseLinkId = "base_link";
    config.frames.flangeLinkId = "flange";
    config.frames.userFrames.push_back(UserFrame{
        "robot_frame",
        "base_link",
        Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
    });
    config.frames.userFrames.push_back(UserFrame{
        "ceiling_frame",
        "base_link",
        Pose::fromXYZRPY_mm_deg(0.0, 0.0, 0.0, 180.0, 0.0, 0.0),
    });

    // Canonical link transforms derived from the reverse-engineered standard-DH table
    // (theta=0 for every joint; lengths in meters). See NachiMZ04D.h for the DH table.
    // Joint position limits are from the teach pendant (docs/preset_references/nachi-mz04d.md).
    config.joints = {
        revolute("J1", "base_link", "link_1", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.340, 0.0, 0.0, 0.0),
                 JointLimits{deg(-170.0), deg(170.0), std::nullopt, std::nullopt}),
        revolute("J2", "link_1", "link_2", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, kHalfPi, 0.0, 0.0),
                 JointLimits{deg(-55.0), deg(180.0), std::nullopt, std::nullopt}),
        revolute("J3", "link_2", "link_3", Pose::fromXYZRPY_m_rad(0.260, 0.0, 0.0, 0.0, 0.0, 0.0),
                 JointLimits{deg(-70.0), deg(190.0), std::nullopt, std::nullopt}),
        revolute("J4", "link_3", "link_4", Pose::fromXYZRPY_m_rad(0.025, -0.280, 0.0, kHalfPi, 0.0, 0.0),
                 JointLimits{deg(-190.0), deg(190.0), std::nullopt, std::nullopt}),
        revolute("J5", "link_4", "link_5", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, -kHalfPi, 0.0, 0.0),
                 JointLimits{deg(-120.0), deg(120.0), std::nullopt, std::nullopt}),
        revolute("J6", "link_5", "flange", Pose::fromXYZRPY_m_rad(0.0, -0.072, 0.0, kHalfPi, 0.0, 0.0),
                 JointLimits{deg(-360.0), deg(360.0), std::nullopt, std::nullopt}),
    };

    config.tools = {
        Tool{"default", "Default Tool", Pose::identity()},
        Tool{"pointer", "Pointer Tool", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)},
    };
    config.defaultToolId = "default";

    // Posture mapping per the Nachi official manual (docs/preset_references/nachi-mz04d.md).
    // The generic resolver classifies branches by sign: shoulder=sign(J1), elbow=sign(J3),
    // wrist=sign(J5), with branchSign(v) = (v < 0 ? -1 : +1). The label axes below map that
    // sign to the Nachi names:
    //   shoulder: J1 < 0 -> righty (-1), J1 > 0 -> lefty (+1)
    //   wrist:    J5 < 0 -> flip   (-1), J5 > 0 -> non-flip (+1)
    //   elbow:    J3 < 0 -> above  (-1), J3 > 0 -> below (+1)
    // The elbow below-side (J3 > 0) is confirmed by the Arm-config ground-truth measurements
    // in docs/preset_references/nachi-mz04d.md; the above-side (J3 < 0) is inferred by symmetry.
    config.posture.resolver = "serial_6dof_shoulder_elbow_wrist";
    config.posture.labels["shoulder"] = PostureLabelAxis{"righty", "lefty"};
    config.posture.labels["elbow"] = PostureLabelAxis{"above", "below"};
    config.posture.labels["wrist"] = PostureLabelAxis{"flip", "non-flip"};

    config.solver.defaultSolver = "adaptive_damped_least_squares";
    config.solver.numericParameters["maxIterations"] = 200.0;
    config.solver.numericParameters["maxSeeds"] = 32.0;
    config.solver.numericParameters["positionTolerance_m"] = 0.000001;
    config.solver.numericParameters["orientationTolerance_rad"] = 0.000017453292519943296;

    config.sources.push_back(SourceReference{
        "field_measurement",
        "Nachi MZ04D teach-pendant joint/flange pose and joint-limit measurements (tool offset 0)",
        "docs/preset_references/nachi-mz04d.md",
        {"dimensions", "joint_limits"},
    });
    config.metadata["collisionProfile"] = "presets/Nachi/MZ04/nachi_mz04d_collision.json";
    config.metadata["meshCollisionProfile"] = "presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json";
    config.metadata["kinematics_source"] = "reverse_engineered_from_teach_pendant_poses";
    config.metadata["joint_limits_status"] = "from_teach_pendant";
    config.metadata["posture_status"] = "from_nachi_manual";
    return config;
}

} // namespace RobotKinematics::Presets
