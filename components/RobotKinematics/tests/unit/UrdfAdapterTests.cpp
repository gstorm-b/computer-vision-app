#include "UrdfAdapterTests.h"

#include <RobotKinematics/Adapters/DhAdapter.h>
#include <RobotKinematics/Adapters/UrdfAdapter.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>

#include <QtTest/QtTest>

#include <optional>

using namespace RobotKinematics;

namespace {
/// Builds a minimal planar 2-revolute-joint (2R) SerialRobotConfig via DhAdapter::fromStandardDh,
/// with link lengths 0.5 m and 0.3 m and +-3.14 rad joint limits, used as the shared fixture for
/// the export/import round-trip tests.
SerialRobotConfig planarConfig()
{
    StandardDhParameter j1;
    j1.jointId = "J1";
    j1.a_m = 0.5;
    j1.limits = JointLimits{-3.14, 3.14, std::nullopt, std::nullopt};
    StandardDhParameter j2;
    j2.jointId = "J2";
    j2.a_m = 0.3;
    j2.limits = JointLimits{-3.14, 3.14, std::nullopt, std::nullopt};
    Result<SerialRobotConfig> config =
        DhAdapter::fromStandardDh(RobotIdentity{"RobotKinematics", "Planar2R", "Planar 2R", "1.0"}, {j1, j2});
    return config.value;
}
}

void UrdfAdapterTests::exportIncludesLinksJointsAndMetadataWarning()
{
    SerialRobotConfig config = planarConfig();
    config.tools.push_back(Tool{"default", "Default Tool", Pose::identity()});
    config.defaultToolId = "default";
    config.posture.resolver = "serial_6dof_shoulder_elbow_wrist";

    const Result<std::string> exported = UrdfAdapter::exportSerialRobot(config);

    QVERIFY(exported.ok());
    QVERIFY(exported.value.find("<robot name=\"Planar2R\">") != std::string::npos);
    QVERIFY(exported.value.find("<link name=\"base_link\"/>") != std::string::npos);
    QVERIFY(exported.value.find("<joint name=\"J1\" type=\"revolute\">") != std::string::npos);
    QVERIFY(exported.value.find("metadata not represented in URDF") != std::string::npos);
}

void UrdfAdapterTests::importExportedSerialChainAndPreserveFk()
{
    const SerialRobotConfig original = planarConfig();
    const Result<std::string> exported = UrdfAdapter::exportSerialRobot(original);
    QVERIFY(exported.ok());

    const Result<SerialRobotConfig> imported =
        UrdfAdapter::importSerialRobot(exported.value, original.frames.baseLinkId, original.frames.flangeLinkId);

    QVERIFY(imported.ok());
    const JointVector joints = JointVector::fromRadians({0.3, -0.2});
    const Pose originalPose = ForwardKinematics::flangePose(original, joints);
    const Pose importedPose = ForwardKinematics::flangePose(imported.value, joints);
    QVERIFY((originalPose.isometry().matrix() - importedPose.isometry().matrix()).norm() <= 1e-9);
}

/// Entry point invoked by TestMain to run the UrdfAdapterTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runUrdfAdapterTests(int argc, char** argv)
{
    UrdfAdapterTests tests;
    return QTest::qExec(&tests, argc, argv);
}
