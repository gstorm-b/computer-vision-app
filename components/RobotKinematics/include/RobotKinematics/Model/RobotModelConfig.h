#pragma once

#include <RobotKinematics/Core/Pose.h>

#include <Eigen/Core>

#include <map>
#include <optional>
#include <string>
#include <vector>

/// Plain data model for describing a robot's kinematic structure (links, joints, frames,
/// tools, posture/solver metadata) as loaded from a preset or built via SerialRobotConfigBuilder.
namespace RobotKinematics {

/// Kinematic chain topology of a robot; only Serial is currently supported by the
/// validator and solvers, the others are reserved for future robot families.
enum class RobotTopologyType {
    Serial,
    Scara,
    Parallel,
    Custom
};

/// Mechanical type of a joint: Revolute (rotates about `axis`), Prismatic (slides along
/// `axis`), or Fixed (rigid, no motion, not counted in dof).
enum class JointType {
    Revolute,
    Prismatic,
    Fixed
};

/// Descriptive identity of a robot model (not used for kinematics computation).
struct RobotIdentity {
    std::string vendor;    ///< Manufacturer name.
    std::string model;     ///< Model designation.
    std::string name;      ///< Human-readable display name.
    std::string revision;  ///< Revision/version string of this robot definition.
};

/// A rigid body in the kinematic chain, identified only by id (geometry lives elsewhere,
/// e.g. in collision profiles).
struct Link {
    std::string id;  ///< Unique link identifier referenced by joints/frames.
};

/// Motion limits for a single (movable) joint.
struct JointLimits {
    double lower = 0.0;                    ///< Lower position limit, in the joint's native unit (rad or m).
    double upper = 0.0;                    ///< Upper position limit, in the joint's native unit (rad or m).
    std::optional<double> velocity;        ///< Optional maximum velocity magnitude; unset means unconstrained.
    std::optional<double> acceleration;    ///< Optional maximum acceleration magnitude; unset means unconstrained.
};

/// A single joint connecting a parent link to a child link.
struct Joint {
    std::string id;                        ///< Unique joint identifier.
    JointType type = JointType::Revolute;  ///< Revolute, Prismatic, or Fixed.
    std::string parentLinkId;              ///< Id of the link this joint is attached to (upstream).
    std::string childLinkId;               ///< Id of the link this joint moves (downstream).
    Pose origin = Pose::identity();        ///< Joint frame origin relative to the parent link.
    Eigen::Vector3d axis = Eigen::Vector3d::UnitZ();  ///< Rotation/translation axis, expected unit-length for Revolute/Prismatic joints.
    std::optional<JointLimits> limits;     ///< Position/velocity/acceleration limits; required for movable joints.
    double home = 0.0;                     ///< Home/reference position, must lie within `limits` when set.
    std::vector<std::string> aliases;      ///< Alternate names (e.g. vendor axis labels) accepted for this joint.
};

/// A user-defined coordinate frame rigidly attached to a link.
struct UserFrame {
    std::string id;             ///< Unique user frame identifier.
    std::string parentLinkId;   ///< Link this frame is rigidly attached to.
    Pose transform = Pose::identity();  ///< Frame pose relative to the parent link.
};

/// Identifies the base and flange links that bound the kinematic chain, plus any
/// additional user-defined frames.
struct FrameConfig {
    std::string baseLinkId;              ///< Link id treated as the fixed base of the chain.
    std::string flangeLinkId;            ///< Link id treated as the end/flange of the chain.
    std::vector<UserFrame> userFrames;   ///< Extra named frames attached to links in the chain.
};

/// A tool (end effector / TCP) that can be mounted on the flange.
struct Tool {
    std::string id;    ///< Unique tool identifier.
    std::string name;  ///< Human-readable tool name.
    Pose flangeToTcp = Pose::identity();  ///< Transform from the flange frame to the tool's TCP.
};

/// Names used to describe the negative and positive branch of one posture axis
/// (e.g. shoulder: "lefty"/"righty").
struct PostureLabelAxis {
    std::string negative;  ///< Label for the -1 branch of this axis.
    std::string positive;  ///< Label for the +1 branch of this axis.
};

/// Configuration for posture classification: which resolver to use and the
/// human-readable labels for each posture axis it produces.
struct PostureMetadata {
    std::string resolver;  ///< Name of the PostureResolver implementation to use (see PostureResolverFactory).
    std::map<std::string, PostureLabelAxis> labels;  ///< Label pairs keyed by axis name (e.g. "shoulder", "elbow", "wrist").
};

/// Configuration for IK solving: which solver to use by default and its numeric tuning parameters.
struct SolverMetadata {
    std::string defaultSolver;  ///< Name of the default IK solver to use for this robot.
    std::map<std::string, double> numericParameters;  ///< Solver-specific numeric parameters (e.g. tolerances, iteration limits) keyed by name.
};

/// Reference to external documentation/data that a preset's dimensions or limits were derived from.
struct SourceReference {
    std::string type;       ///< Kind of source (e.g. datasheet, manual, project fixture).
    std::string title;      ///< Human-readable title of the source.
    std::string reference;  ///< Path or citation identifying the source.
    std::vector<std::string> appliesTo;  ///< Which parts of the config this source backs (e.g. "dimensions", "joint_limits").
};

/// Complete configuration for a serial robot arm: identity, kinematic chain (links/joints),
/// frames, tools, and the metadata used to select posture/solver behavior.
struct SerialRobotConfig {
    RobotIdentity identity;                        ///< Descriptive identity of the robot model.
    RobotTopologyType topology = RobotTopologyType::Serial;  ///< Kinematic topology; must be Serial for RobotModelValidator/serial kinematics.
    int dof = 0;                                   ///< Degrees of freedom; must match the number of movable joints.
    std::vector<Link> links;                       ///< All links in the chain, referenced by id from joints/frames.
    std::vector<Joint> joints;                     ///< Ordered joints forming the serial chain from base to flange.
    FrameConfig frames;                            ///< Base/flange link ids and any additional user frames.
    std::vector<Tool> tools;                       ///< Tools available for mounting on the flange.
    std::string defaultToolId;                     ///< Id of the tool used when none is explicitly selected; must refer to an entry in `tools` when set.
    PostureMetadata posture;                       ///< Posture resolver name and axis labels.
    SolverMetadata solver;                         ///< Default IK solver name and numeric parameters.
    std::vector<SourceReference> sources;          ///< Documentation/data sources backing this config's values.
    std::map<std::string, std::string> metadata;   ///< Free-form string metadata (e.g. associated collision profile path).
};

} // namespace RobotKinematics
