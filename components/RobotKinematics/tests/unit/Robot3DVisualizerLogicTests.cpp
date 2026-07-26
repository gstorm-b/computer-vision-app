#include "Robot3DVisualizerLogicTests.h"

#include "../../examples/Robot3DVizualize/Robot3DVisualizerLogic.h"

#include <RobotKinematics/Core/Pose.h>

#include <QtTest/QtTest>

#include <cmath>
#include <optional>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Returns true if `lhs` and `rhs` differ by no more than `tolerance` (default 1e-9).
bool nearlyEqual(double lhs, double rhs, double tolerance = 1e-9)
{
    return std::abs(lhs - rhs) <= tolerance;
}
}

void Robot3DVisualizerLogicTests::convertsBetweenNachiPendantOrderAndPose()
{
    const Pose pose =
        Robot3DVisualizer::fromNachiPendantPose(339.15, 182.108, 571.95, 29.5244, 3.54526, 19.2647);

    const Robot3DVisualizer::PendantPoseDisplay display = Robot3DVisualizer::toNachiPendantPose(pose);

    QVERIFY(nearlyEqual(display.x_mm, 339.15));
    QVERIFY(nearlyEqual(display.y_mm, 182.108));
    QVERIFY(nearlyEqual(display.z_mm, 571.95));
    QVERIFY(nearlyEqual(display.rz_deg, 29.5244));
    QVERIFY(nearlyEqual(display.ry_deg, 3.54526));
    QVERIFY(nearlyEqual(display.rx_deg, 19.2647));
}

void Robot3DVisualizerLogicTests::buildsMillimeterVisualDeltaMatrixFromHomePose()
{
    const Pose home = Pose::identity();
    const Pose current = Pose::fromXYZRPY_m_rad(0.1, -0.02, 0.03, 0.0, 0.0, kPi / 2.0);

    const Eigen::Matrix4d visualDelta = Robot3DVisualizer::visualDeltaMatrixMm(current, home);

    QVERIFY(nearlyEqual(visualDelta(0, 3), 100.0));
    QVERIFY(nearlyEqual(visualDelta(1, 3), -20.0));
    QVERIFY(nearlyEqual(visualDelta(2, 3), 30.0));
    QVERIFY(nearlyEqual(visualDelta(0, 0), 0.0));
    QVERIFY(nearlyEqual(visualDelta(0, 1), -1.0));
    QVERIFY(nearlyEqual(visualDelta(1, 0), 1.0));
    QVERIFY(nearlyEqual(visualDelta(1, 1), 0.0));
}

void Robot3DVisualizerLogicTests::appliesCenteringToolHomeVisualCorrection()
{
    const Pose rawToolMountInCad =
        Pose::fromXYZRPY_mm_deg(-352.0, 625.0, 0.0, 0.0, 0.0, 0.0);
    const Pose rawToolTcpMarkerInCad =
        Pose::fromXYZRPY_mm_deg(-464.0, 625.0, 45.0, 0.0, 0.0, 0.0);
    const Pose homeVisualCorrection =
        Pose::fromXYZRPY_mm_deg(0.0, 0.0, 0.0, 0.0, 90.0, 0.0) *
        Pose::fromXYZRPY_mm_deg(352.0, -625.0, 0.0, 0.0, 0.0, 0.0);

    const Eigen::Matrix4d corrected =
        Robot3DVisualizer::visualDeltaMatrixMm(Pose::identity(), Pose::identity(), homeVisualCorrection);

    const Eigen::Vector4d mount =
        corrected * Eigen::Vector4d(-352.0, 625.0, 0.0, 1.0);
    const Eigen::Vector4d tcpMarker =
        corrected * Eigen::Vector4d(-464.0, 625.0, 45.0, 1.0);

    QVERIFY(nearlyEqual(mount.x(), 0.0));
    QVERIFY(nearlyEqual(mount.y(), 0.0));
    QVERIFY(nearlyEqual(mount.z(), 0.0));

    QVERIFY(nearlyEqual(tcpMarker.x(), 45.0));
    QVERIFY(nearlyEqual(tcpMarker.y(), 0.0));
    QVERIFY(nearlyEqual(tcpMarker.z(), 112.0));
}

void Robot3DVisualizerLogicTests::mapsPostureBranchesToConfiguredLabels()
{
    PostureMetadata posture;
    posture.labels["shoulder"] = PostureLabelAxis{"righty", "lefty"};
    posture.labels["elbow"] = PostureLabelAxis{"above", "below"};
    posture.labels["wrist"] = PostureLabelAxis{"flip", "non-flip"};

    QCOMPARE(Robot3DVisualizer::postureLabel(posture, "shoulder", -1), QString("righty"));
    QCOMPARE(Robot3DVisualizer::postureLabel(posture, "elbow", 1), QString("below"));
    QCOMPARE(Robot3DVisualizer::postureLabel(posture, "wrist", std::nullopt), QString("Any"));
}

void Robot3DVisualizerLogicTests::formatsKinematicsStatusesForUi()
{
    QCOMPARE(Robot3DVisualizer::statusText(KinematicsStatus::Ok), QString("Ok"));
    QCOMPARE(Robot3DVisualizer::statusText(KinematicsStatus::JointLimitViolation),
             QString("Joint limit violation"));
    QCOMPARE(Robot3DVisualizer::statusText(KinematicsStatus::PostureConstraintUnsatisfied),
             QString("Posture constraint unsatisfied"));
}

void Robot3DVisualizerLogicTests::derivesSimplifiedMeshProfilePathBesideOriginal()
{
    QCOMPARE(Robot3DVisualizer::meshProfileSimplifiedPath(QString()), QString());

    QCOMPARE(Robot3DVisualizer::meshProfileSimplifiedPath(
                 QStringLiteral("presets/Nachi/MZ04/nachi_mz04d_mesh_collision.json")),
             QString(QStringLiteral("presets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json")));

    // Bare filenames inherit QFileInfo::dir() == ".", so the result is "./<name>_simplified.<ext>".
    QCOMPARE(Robot3DVisualizer::meshProfileSimplifiedPath(QStringLiteral("foo_mesh.json")),
             QString(QStringLiteral("./foo_mesh_simplified.json")));

    QCOMPARE(Robot3DVisualizer::meshProfileSimplifiedPath(QStringLiteral("/abs/dir/foo")),
             QString(QStringLiteral("/abs/dir/foo_simplified")));
}

/// Entry point that instantiates Robot3DVisualizerLogicTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runRobot3DVisualizerLogicTests(int argc, char** argv)
{
    Robot3DVisualizerLogicTests tests;
    return QTest::qExec(&tests, argc, argv);
}
