#include "CustomPresetTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/SerialRobotConfigBuilder.h>
#include <RobotKinematics/Presets/PresetJsonLoader.h>

#include <Eigen/Geometry>

#include <QtTest/QtTest>

#include <optional>

using namespace RobotKinematics;

namespace {
/// Builds a single movable Joint (Prismatic or Revolute) connecting `parent` to `child`, with
/// the given `axis` and symmetric position/velocity-unset limits [`lower`, `upper`].
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

/// Builds a custom 3-prismatic + 3-revolute (Cartesian-then-wrist) 6-DOF robot config via
/// SerialRobotConfigBuilder, with a default identity tool, for use as a non-preset IK fixture.
SerialRobotConfigBuilder cartesianBuilder()
{
    SerialRobotConfigBuilder builder;
    builder.identity(RobotIdentity{"RobotKinematics", "CustomCartesian", "Custom Cartesian Wrist", "1.0.0"})
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
        .defaultTool("default");
    return builder;
}
}

void CustomPresetTests::builderCreatesCustomSixDofConfigThatSolvesIk()
{
    const Result<SerialRobotConfig> built = cartesianBuilder().build();
    QVERIFY(built.ok());

    const JointVector targetJoints = JointVector::fromRadians({0.2, -0.15, 0.3, 0.2, -0.25, 0.35});
    IKRequest request;
    request.targetPose = ForwardKinematics::flangePose(built.value, targetJoints);
    request.seedJoint = JointVector::fromRadians({0.18, -0.13, 0.28, 0.18, -0.23, 0.32});

    const SerialRobotKinematics robot(built.value);
    const IKResult result = robot.solve(request);
    QVERIFY(result.ok());
}

void CustomPresetTests::jsonLoaderRejectsUnknownTopLevelField()
{
    const std::string json =
        R"({"schema":"robot-kinematics-preset/v1","units":{"length":"m","angle":"rad"},"unexpected":true})";
    const Result<SerialRobotConfig> loaded = PresetJsonLoader::loadJson(json);
    QVERIFY(!loaded.ok());
    QCOMPARE(loaded.status, KinematicsStatus::InvalidRobotConfig);
}

void CustomPresetTests::jsonLoaderRejectsNonCanonicalUnits()
{
    const std::string json =
        R"({"schema":"robot-kinematics-preset/v1","units":{"length":"mm","angle":"deg"},"metadata":{}})";
    const Result<SerialRobotConfig> loaded = PresetJsonLoader::loadJson(json);
    QVERIFY(!loaded.ok());
    QCOMPARE(loaded.status, KinematicsStatus::InvalidRobotConfig);
}

/// QtTest entry point for CustomPresetTests: constructs the suite and runs it via QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runCustomPresetTests(int argc, char** argv)
{
    CustomPresetTests tests;
    return QTest::qExec(&tests, argc, argv);
}
