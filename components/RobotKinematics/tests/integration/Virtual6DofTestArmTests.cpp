#include "Virtual6DofTestArmTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/RobotModelValidator.h>
#include <RobotKinematics/Presets/PresetJsonLoader.h>
#include <RobotKinematics/Presets/Virtual6DofTestArm.h>

#include <QFile>
#include <QString>
#include <QtTest/QtTest>

#include <cmath>
#include <string>

using namespace RobotKinematics;

namespace {
/// Locates the on-disk `virtual_6dof_test_arm.json` preset by probing a handful of
/// candidate relative paths (to tolerate different CTest/build working directories);
/// falls back to the first candidate if none exist.
/// @return the first candidate path found to exist, or the first candidate as a fallback
std::string virtualPresetPath()
{
    const char* candidates[] = {
        "../presets/virtual_6dof_test_arm.json",
        "presets/virtual_6dof_test_arm.json",
        "../../presets/virtual_6dof_test_arm.json",
    };
    for (const char* candidate : candidates) {
        QFile file(QString::fromUtf8(candidate));
        if (file.exists()) {
            return candidate;
        }
    }
    return candidates[0];
}

/// Compares two poses by the Frobenius norm of the difference between their
/// homogeneous transform matrices.
/// @return true when the poses match within `tolerance`
bool poseNear(const Pose& a, const Pose& b, double tolerance = 1e-12)
{
    return (a.isometry().matrix() - b.isometry().matrix()).norm() <= tolerance;
}

/// Asserts (via QCOMPARE/QVERIFY) that two SerialRobotConfig instances agree on every
/// field the IK/FK solvers consume: identity model, DOF, link/joint/tool counts, frame
/// ids, default tool, posture resolver, source list, and per-joint/per-tool geometry.
void compareSolverFacingConfig(const SerialRobotConfig& a, const SerialRobotConfig& b)
{
    QCOMPARE(a.identity.model, b.identity.model);
    QCOMPARE(a.dof, b.dof);
    QCOMPARE(a.links.size(), b.links.size());
    QCOMPARE(a.joints.size(), b.joints.size());
    QCOMPARE(a.frames.baseLinkId, b.frames.baseLinkId);
    QCOMPARE(a.frames.flangeLinkId, b.frames.flangeLinkId);
    QCOMPARE(a.defaultToolId, b.defaultToolId);
    QCOMPARE(a.posture.resolver, b.posture.resolver);
    QCOMPARE(a.sources.size(), b.sources.size());

    for (std::size_t i = 0; i < a.joints.size(); ++i) {
        QCOMPARE(a.joints[i].id, b.joints[i].id);
        QCOMPARE(a.joints[i].parentLinkId, b.joints[i].parentLinkId);
        QCOMPARE(a.joints[i].childLinkId, b.joints[i].childLinkId);
        QVERIFY((a.joints[i].axis - b.joints[i].axis).norm() <= 1e-12);
        QVERIFY(poseNear(a.joints[i].origin, b.joints[i].origin));
        QVERIFY(a.joints[i].limits.has_value());
        QVERIFY(b.joints[i].limits.has_value());
        QCOMPARE(a.joints[i].limits->lower, b.joints[i].limits->lower);
        QCOMPARE(a.joints[i].limits->upper, b.joints[i].limits->upper);
    }

    QCOMPARE(a.tools.size(), b.tools.size());
    for (std::size_t i = 0; i < a.tools.size(); ++i) {
        QCOMPARE(a.tools[i].id, b.tools[i].id);
        QVERIFY(poseNear(a.tools[i].flangeToTcp, b.tools[i].flangeToTcp));
    }
}
}

void Virtual6DofTestArmTests::fallbackPresetIsValidAndHasRequiredMetadata()
{
    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    const ModelValidationResult validation = RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY(validation.ok());
    QCOMPARE(config.dof, 6);
    QCOMPARE(config.joints.size(), std::size_t(6));
    QCOMPARE(config.tools.size(), std::size_t(2));
    QVERIFY(!config.frames.userFrames.empty());
    QCOMPARE(config.posture.resolver, std::string("serial_6dof_shoulder_elbow_wrist"));
    QVERIFY(!config.sources.empty());
}

void Virtual6DofTestArmTests::jsonPresetMatchesCppFallbackForSolverFacingFields()
{
    const Result<SerialRobotConfig> loaded = PresetJsonLoader::loadFile(virtualPresetPath());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    compareSolverFacingConfig(Presets::virtual6DofTestArm(), loaded.value);
}

void Virtual6DofTestArmTests::presetRunsFkAndSeededIkRoundTrip()
{
    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    const SerialRobotKinematics robot(config);
    const JointVector targetJoints = JointVector::fromRadians({0.25, -0.35, 0.45, 0.2, -0.3, 0.15});
    const Pose target = ForwardKinematics::flangePose(config, targetJoints);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = targetJoints;

    const IKResult result = robot.solve(request);

    QVERIFY(result.ok());
    const Pose solved = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((solved.translation_m() - target.translation_m()).norm() <= 1e-6);
}

/// Entry point invoked by TestMain to run the Virtual6DofTestArmTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runVirtual6DofTestArmTests(int argc, char** argv)
{
    Virtual6DofTestArmTests tests;
    return QTest::qExec(&tests, argc, argv);
}
