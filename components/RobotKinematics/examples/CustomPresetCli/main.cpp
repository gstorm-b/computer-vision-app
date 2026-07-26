#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Units.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/SerialRobotConfigBuilder.h>

#include <QCoreApplication>

#include <Eigen/Core>

#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {

/// Builds a single movable (prismatic or revolute) Joint with the given id, parent/child
/// links, axis, and symmetric-style [lower, upper] position limits (velocity/acceleration
/// limits left unset via std::nullopt).
Joint movableJoint(const std::string& id,
                   JointType type,
                   const std::string& parent,
                   const std::string& child,
                   const Eigen::Vector3d& axis,
                   double lower,
                   double upper)
{
    Joint joint;
    joint.id = id;
    joint.type = type;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.axis = axis;
    joint.limits = JointLimits{lower, upper, std::nullopt, std::nullopt};
    return joint;
}

/// Builds a custom 6-DOF "Cartesian wrist" robot preset (3 prismatic axes X/Y/Z followed by
/// 3 revolute wrist axes) via SerialRobotConfigBuilder, with a default identity tool and the
/// numerical_adaptive_dls solver.
/// @return the validated SerialRobotConfig, or a Result carrying a validation-failure message
Result<SerialRobotConfig> createCartesianWristPreset()
{
    SerialRobotConfigBuilder builder;
    builder.identity(RobotIdentity{"ExampleVendor", "Cartesian6", "Example Cartesian Wrist", "1.0.0"})
        .baseAndFlange("base", "flange")
        .addLink("base")
        .addLink("x")
        .addLink("y")
        .addLink("z")
        .addLink("rx")
        .addLink("ry")
        .addLink("flange")
        .addJoint(movableJoint("J1", JointType::Prismatic, "base", "x", Eigen::Vector3d::UnitX(), -1.0, 1.0))
        .addJoint(movableJoint("J2", JointType::Prismatic, "x", "y", Eigen::Vector3d::UnitY(), -1.0, 1.0))
        .addJoint(movableJoint("J3", JointType::Prismatic, "y", "z", Eigen::Vector3d::UnitZ(), -1.0, 1.0))
        .addJoint(movableJoint("J4", JointType::Revolute, "z", "rx", Eigen::Vector3d::UnitX(), -2.0, 2.0))
        .addJoint(movableJoint("J5", JointType::Revolute, "rx", "ry", Eigen::Vector3d::UnitY(), -2.0, 2.0))
        .addJoint(movableJoint("J6", JointType::Revolute, "ry", "flange", Eigen::Vector3d::UnitZ(), -2.0, 2.0))
        .addTool(Tool{"default", "Default Tool", Pose::identity()})
        .defaultTool("default")
        .solver(SolverMetadata{"numerical_adaptive_dls", {}});

    return builder.build();
}

/// Assembles a 6-element JointVector for the Cartesian wrist preset: the first three
/// components are passed through in meters, the last three are converted from degrees to
/// radians via units::deg().
JointVector mixedJointVector(double x_m,
                             double y_m,
                             double z_m,
                             double rx_deg,
                             double ry_deg,
                             double rz_deg)
{
    Eigen::VectorXd values(6);
    values << x_m, y_m, z_m, units::deg(rx_deg), units::deg(ry_deg), units::deg(rz_deg);
    return JointVector(values);
}

/// Prints the mixed-unit joint vector to stdout: x/y/z in meters, rx/ry/rz converted back to
/// degrees via units::toDeg().
void printMixedJoints(const JointVector& joints)
{
    std::cout << std::fixed << std::setprecision(6)
              << "  x=" << joints[0] << " m, "
              << "y=" << joints[1] << " m, "
              << "z=" << joints[2] << " m, "
              << "rx=" << units::toDeg(joints[3]) << " deg, "
              << "ry=" << units::toDeg(joints[4]) << " deg, "
              << "rz=" << units::toDeg(joints[5]) << " deg\n";
}

/// Prints the pose's translation to stdout, converted from meters to millimeters.
void printPose(const Pose& pose)
{
    const Eigen::Vector3d t = pose.translation_m();
    std::cout << std::fixed << std::setprecision(6)
              << "  translation: "
              << units::toMm(t.x()) << ", "
              << units::toMm(t.y()) << ", "
              << units::toMm(t.z()) << " mm\n";
}

/// End-to-end demo: builds the custom Cartesian-wrist preset, runs forward kinematics on a
/// sample joint target, then solves inverse kinematics back from that target pose and prints
/// the resulting joints and position/orientation error.
/// @return 0 on success, 1 if preset validation or IK solving fails
int runCustomPresetExample()
{
    const Result<SerialRobotConfig> built = createCartesianWristPreset();
    if (!built.ok()) {
        std::cerr << "Preset validation failed: " << built.message << "\n";
        return 1;
    }

    const SerialRobotConfig& config = built.value;
    const SerialRobotKinematics robot(config);

    const JointVector targetJoints = mixedJointVector(0.20, -0.15, 0.30, 12.0, -14.0, 20.0);
    const JointVector seedJoints = mixedJointVector(0.18, -0.13, 0.28, 10.0, -12.0, 18.0);
    const Pose targetPose = ForwardKinematics::flangePose(config, targetJoints);

    std::cout << "Created preset: " << config.identity.vendor << " "
              << config.identity.model << ", dof=" << config.dof << "\n";
    std::cout << "\nInput joints:\n";
    printMixedJoints(targetJoints);

    std::cout << "\nForward kinematics, base -> flange:\n";
    printPose(targetPose);

    IKRequest request;
    request.targetPose = targetPose;
    request.seedJoint = seedJoints;

    const IKResult solved = robot.solve(request);
    if (!solved.ok()) {
        std::cerr << "\nIK failed: " << solved.message << "\n";
        return 1;
    }

    std::cout << "\nInverse kinematics, best solution:\n";
    printMixedJoints(solved.best().joints);
    std::cout << "  position error: " << solved.best().positionError_m << " m\n";
    std::cout << "  orientation error: " << solved.best().orientationError_rad << " rad\n";

    return 0;
}

} // namespace

/// Entry point for the CustomPresetCli example: creates a minimal QCoreApplication (needed
/// for Qt types used by the library) and runs runCustomPresetExample().
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    return runCustomPresetExample();
}
