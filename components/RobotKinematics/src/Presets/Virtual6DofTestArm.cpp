#include <RobotKinematics/Presets/Virtual6DofTestArm.h>

#include <RobotKinematics/Core/Pose.h>

#include <cmath>
#include <optional>

/// Built-in preset robot configurations shipped with RobotKinematics (test fixtures and
/// reference arms usable without an external JSON preset file).
namespace RobotKinematics::Presets {

/// Translation-unit-local helpers for building the virtual 6-DOF test arm preset.
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;  ///< Pi, used for the default +/-pi revolute joint range.

/// Builds a Revolute Joint with a full +/-pi range, no velocity/acceleration limits, a
/// zero home position, and an alias derived from `id` (e.g. "J1" -> "JT1").
/// @param id joint id (expected in the form "J<n>"); also used to derive the JT-alias
/// @param parent parent link id
/// @param child child link id
/// @param origin joint origin pose relative to the parent link
/// @param axis joint rotation axis, expressed in the joint's local frame
Joint revolute(const std::string& id,
               const std::string& parent,
               const std::string& child,
               const Pose& origin,
               const Eigen::Vector3d& axis)
{
    Joint joint;
    joint.id = id;
    joint.type = JointType::Revolute;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.origin = origin;
    joint.axis = axis;
    joint.limits = JointLimits{-kPi, kPi, std::nullopt, std::nullopt};
    joint.home = 0.0;
    joint.aliases = {"JT" + id.substr(1)};
    return joint;
}
}

/// Builds the "Virtual6DofTestArm" preset: a fixture 6-DOF serial revolute arm (base_link
/// through flange, joints J1-J6 built via revolute()) with a vision user frame, default
/// and probe tools, shoulder/elbow/wrist posture labels, an adaptive damped
/// least-squares solver configuration, a project_fixture source reference, and
/// collision-profile/purpose metadata.
/// @return the fully populated SerialRobotConfig for this test fixture
SerialRobotConfig virtual6DofTestArm()
{
    SerialRobotConfig config;
    config.identity = RobotIdentity{"RobotKinematics", "Virtual6DofTestArm", "Virtual 6DOF Test Arm", "1.0.0"};
    config.topology = RobotTopologyType::Serial;
    config.dof = 6;
    config.links = {{"base_link"}, {"link_1"}, {"link_2"}, {"link_3"}, {"link_4"}, {"link_5"}, {"flange"}};
    config.frames.baseLinkId = "base_link";
    config.frames.flangeLinkId = "flange";
    config.frames.userFrames.push_back(UserFrame{
        "vision_frame",
        "base_link",
        Pose::fromXYZRPY_m_rad(0.2, -0.1, 0.15, 0.0, 0.0, 0.35),
    });
    config.joints = {
        revolute("J1", "base_link", "link_1", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.25, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitZ()),
        revolute("J2", "link_1", "link_2", Pose::fromXYZRPY_m_rad(0.12, 0.0, 0.10, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitY()),
        revolute("J3", "link_2", "link_3", Pose::fromXYZRPY_m_rad(0.35, 0.0, 0.0, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitY()),
        revolute("J4", "link_3", "link_4", Pose::fromXYZRPY_m_rad(0.25, 0.0, 0.0, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitX()),
        revolute("J5", "link_4", "link_5", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.18, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitY()),
        revolute("J6", "link_5", "flange", Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.10, 0.0, 0.0, 0.0),
                 Eigen::Vector3d::UnitX()),
    };
    config.tools = {
        Tool{"default", "Default Tool", Pose::identity()},
        Tool{"probe", "Probe Tool", Pose::fromXYZRPY_m_rad(0.04, 0.0, 0.08, 0.0, 0.0, 0.0)},
    };
    config.defaultToolId = "default";
    config.posture.resolver = "serial_6dof_shoulder_elbow_wrist";
    config.posture.labels["shoulder"] = PostureLabelAxis{"lefty", "righty"};
    config.posture.labels["elbow"] = PostureLabelAxis{"below", "above"};
    config.posture.labels["wrist"] = PostureLabelAxis{"non-flip", "flip"};
    config.solver.defaultSolver = "adaptive_damped_least_squares";
    config.solver.numericParameters["maxIterations"] = 200.0;
    config.solver.numericParameters["maxSeeds"] = 32.0;
    config.solver.numericParameters["positionTolerance_m"] = 0.000001;
    config.solver.numericParameters["orientationTolerance_rad"] = 0.000017453292519943296;
    config.sources.push_back(SourceReference{
        "project_fixture",
        "RobotKinematics virtual preset definition",
        "docs/robot_kinematics_spec.md",
        {"dimensions", "joint_limits", "posture"},
    });
    config.metadata["collisionProfile"] = "presets/virtual_6dof_test_arm_collision.json";
    config.metadata["purpose"] = "base_milestone_fixture";
    return config;
}

} // namespace RobotKinematics::Presets
