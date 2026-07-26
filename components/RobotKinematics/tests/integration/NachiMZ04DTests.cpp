#include "NachiMZ04DTests.h"

#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/RobotModelValidator.h>
#include <RobotKinematics/Posture/PostureResolver.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>
#include <RobotKinematics/Presets/PresetJsonLoader.h>

#include <QFile>
#include <QString>
#include <QtTest/QtTest>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
/// Conversion factor from radians to degrees, used to express angular tolerances/errors in
/// degrees for readable test failure messages.
constexpr double kRadToDeg = 180.0 / 3.141592653589793238462643383279502884;
}

namespace {

/// One teach-pendant reference measurement (tool offset 0, robot base frame).
/// Joints are in degrees. The pose orientation is reported by the Nachi pendant in
/// Z-first order: (yaw about Z, pitch about Y, roll about X), all degrees.
struct TeachPoint {
    std::array<double, 6> jointsDeg;
    double x_mm, y_mm, z_mm;
    double yawZ_deg, pitchY_deg, rollX_deg;
};

/// 21 recorded teach-pendant reference points (joints in degrees, pose in mm/degrees,
/// Z-first orientation order) used as forward-kinematics ground truth.
/// @note source: docs/preset_references/nachi-mz04d.md
const std::array<TeachPoint, 21> kTeachPoints = {{
    {{28.1579, -18.8069, 163.839, -0.710019, 35.8922, 152.731}, 339.15, 182.108, 571.95, 0.301758, -1.00829, 0.060508},
    {{46.3996, -18.8029, 163.841, -0.710019, 35.8898, 152.727}, 265.078, 279.092, 571.996, 18.5415, -1.01493, 0.0655774},
    {{-2.49169, -18.8029, 163.843, -0.710019, 35.8898, 152.727}, 384.555, -16.1959, 572.015, -30.3416, -1.01655, 0.0663919},
    {{28.1557, -13.0437, 163.841, -0.710019, 35.8898, 152.727}, 316.881, 170.172, 609.464, 0.129697, -6.10352, 2.76823},
    {{28.1557, -30.531, 163.843, -0.710019, 35.8898, 152.727}, 373.609, 200.531, 488.92, -0.10146, 9.3413, -5.45622},
    {{28.1557, -18.8049, 180.206, -0.710019, 35.8898, 152.727}, 255.728, 137.446, 598.296, -0.835413, -15.4284, 7.91414},
    {{28.1557, -18.8029, 147.018, -0.710019, 35.8898, 152.727}, 414.47, 222.397, 518.315, -0.604066, 13.8207, -7.93948},
    {{28.1557, -18.8049, 163.841, 34.0763, 35.8898, 152.727}, 355.761, 163.565, 567.837, 29.5244, 3.54526, 19.2647},
    {{28.1557, -18.8029, 163.841, -35.5607, 35.8898, 152.727}, 333.466, 206.294, 567.493, -26.5584, -14.1285, -14.8798},
    {{28.1557, -18.8049, 163.841, -0.714615, 69.0539, 152.727}, 304.422, 163.866, 559.622, -4.14049, -30.099, 16.8315},
    {{28.1557, -18.7989, 163.841, -0.714615, 7.70156, 152.721}, 369.405, 197.824, 564.041, -2.70127, 23.8103, -13.6682},
    {{28.1557, -18.8029, 163.841, -0.710019, 35.8898, 200.78}, 339.126, 182.077, 571.996, 48.3522, -0.72722, -0.711033},
    {{28.1557, -18.8029, 163.841, -0.712317, 35.8898, 120.206}, 339.117, 182.074, 572.001, -32.2176, -0.821677, 0.600015},
    {{28.2282, -18.9278, 164.21, 0.00919119, 34.718, 151.759}, 339.088, 182.008, 572.065, -0.00376925, -0.00358024, 0.00786421},
    {{31.122, -16.8986, 160.187, 21.2041, 48.5086, 133.088}, 339.088, 181.928, 572.142, -0.0238987, -0.00493311, 18.4547},
    {{29.6938, -20.1833, 169.043, 25.1747, 18.7945, 127.285}, 339.057, 182.004, 572.09, -0.00233472, 16.0384, 0.0113963},
    {{28.2282, -18.9238, 164.212, 0.0137868, 34.718, 219.293}, 339.062, 182.009, 572.097, 67.5266, -0.0106346, -0.000248291},
    {{32.1613, -18.6659, 165.74, 40.5814, 34.3617, 128.805}, 339.082, 181.968, 572.085, 17.0802, 11.0767, 19.3541},
    {{23.2932, -15.8386, 159.494, -35.6112, 53.2523, 185.217}, 339.119, 182.571, 571.895, 6.5187, -18.922, -23.4265},
    {{-0.00219726, 0.00430813, 179.996, 0.00459559, 0.0046875, -0.000121055}, 234.998, -0.00901241, 692.029, -179.995, 0.00212387, -2.86984e-08},
    {{0, 90.002, -0.00217014, 0.00459559, -0.00234375, 6.05273e-05}, 351.991, 4.50987e-07, 624.996, -61.782, -89.9947, -118.218},
}};

/// Locates the Nachi MZ04D JSON preset by checking a few relative paths (accounting for
/// differing test working directories); falls back to the first candidate if none exist.
/// @return relative path to nachi_mz04d.json that exists on disk, or the first candidate
///     as a fallback
std::string nachiPresetPath()
{
    const char* candidates[] = {
        "../presets/Nachi/MZ04/nachi_mz04d.json",
        "presets/Nachi/MZ04/nachi_mz04d.json",
        "../../presets/Nachi/MZ04/nachi_mz04d.json",
    };
    for (const char* candidate : candidates) {
        if (QFile(QString::fromUtf8(candidate)).exists()) {
            return candidate;
        }
    }
    return candidates[0];
}

/// Compares two poses by the Frobenius norm of the difference between their 4x4 isometry
/// matrices.
/// @param tolerance maximum allowed matrix-difference norm
/// @return true if the poses match within tolerance
bool poseNear(const Pose& a, const Pose& b, double tolerance = 1e-12)
{
    return (a.isometry().matrix() - b.isometry().matrix()).norm() <= tolerance;
}

/// Converts a recorded TeachPoint into a Pose, reordering the pendant's Z-first
/// (yaw, pitch, roll) tuple into the (roll, pitch, yaw) order expected by
/// Pose::fromXYZRPY_mm_deg.
Pose expectedPose(const TeachPoint& p)
{
    // Pose::fromXYZRPY_mm_deg takes (roll about X, pitch about Y, yaw about Z); map the
    // pendant's Z-first tuple accordingly.
    return Pose::fromXYZRPY_mm_deg(p.x_mm, p.y_mm, p.z_mm, p.rollX_deg, p.pitchY_deg, p.yawZ_deg);
}
} // namespace

/// Checks the C++ fallback preset passes RobotModelValidator and has the expected vendor/model
/// identity, DOF, joint/tool counts, non-empty user frames, posture resolver name, and non-empty
/// sources list.
void NachiMZ04DTests::fallbackPresetIsValidAndHasRequiredMetadata()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const ModelValidationResult validation = RobotModelValidator::validateSerialRobotConfig(config);

    QVERIFY2(validation.ok(), validation.issues.empty() ? "" : validation.issues.front().message.c_str());
    QCOMPARE(config.identity.vendor, std::string("Nachi"));
    QCOMPARE(config.identity.model, std::string("MZ04D"));
    QCOMPARE(config.dof, 6);
    QCOMPARE(config.joints.size(), std::size_t(6));
    QCOMPARE(config.tools.size(), std::size_t(2));
    QVERIFY(!config.frames.userFrames.empty());
    QCOMPARE(config.posture.resolver, std::string("serial_6dof_shoulder_elbow_wrist"));
    QVERIFY(!config.sources.empty());
}

/// Loads the on-disk JSON preset and checks it matches the C++ fallback for identity, DOF,
/// frame ids, default tool, posture resolver, and per-joint id/parent/child/axis/origin/limits.
void NachiMZ04DTests::jsonPresetMatchesCppFallbackForSolverFacingFields()
{
    const Result<SerialRobotConfig> loaded = PresetJsonLoader::loadFile(nachiPresetPath());
    QVERIFY2(loaded.ok(), loaded.message.c_str());

    const SerialRobotConfig fallback = Presets::nachiMZ04D();
    const SerialRobotConfig& json = loaded.value;

    QCOMPARE(json.identity.model, fallback.identity.model);
    QCOMPARE(json.dof, fallback.dof);
    QCOMPARE(json.joints.size(), fallback.joints.size());
    QCOMPARE(json.frames.baseLinkId, fallback.frames.baseLinkId);
    QCOMPARE(json.frames.flangeLinkId, fallback.frames.flangeLinkId);
    QCOMPARE(json.defaultToolId, fallback.defaultToolId);
    QCOMPARE(json.posture.resolver, fallback.posture.resolver);

    for (std::size_t i = 0; i < fallback.joints.size(); ++i) {
        QCOMPARE(json.joints[i].id, fallback.joints[i].id);
        QCOMPARE(json.joints[i].parentLinkId, fallback.joints[i].parentLinkId);
        QCOMPARE(json.joints[i].childLinkId, fallback.joints[i].childLinkId);
        QVERIFY((json.joints[i].axis - fallback.joints[i].axis).norm() <= 1e-12);
        QVERIFY(poseNear(json.joints[i].origin, fallback.joints[i].origin));
        QVERIFY(json.joints[i].limits.has_value());
        QVERIFY(fallback.joints[i].limits.has_value());
        QVERIFY(std::abs(json.joints[i].limits->lower - fallback.joints[i].limits->lower) <= 1e-9);
        QVERIFY(std::abs(json.joints[i].limits->upper - fallback.joints[i].limits->upper) <= 1e-9);
    }
}

/// Runs ForwardKinematics::flangePose() over all 21 recorded teach-pendant poses and checks the
/// position/orientation error stays within the DH-fit tolerances (<= 0.04 mm, <= 0.012 deg).
void NachiMZ04DTests::forwardKinematicsMatchesTeachPendantPoses()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();

    // The DH table was reverse-engineered from these measurements. Keep these tolerances
    // close to the documented fit (<= 0.035 mm / <= 0.010 deg) so regressions do not hide
    // behind broad acceptance thresholds.
    const double positionTol_m = 4.0e-5;              // 0.04 mm
    const double orientationTol_rad = 0.012 / kRadToDeg; // 0.012 deg

    for (std::size_t i = 0; i < kTeachPoints.size(); ++i) {
        const TeachPoint& p = kTeachPoints[i];
        const JointVector joints = JointVector::fromDegrees(
            {p.jointsDeg[0], p.jointsDeg[1], p.jointsDeg[2], p.jointsDeg[3], p.jointsDeg[4], p.jointsDeg[5]});

        const Pose actual = ForwardKinematics::flangePose(config, joints);
        const Pose expected = expectedPose(p);

        const double posErr = (actual.translation_m() - expected.translation_m()).norm();
        const double oriErr = actual.rotationQuaternion().angularDistance(expected.rotationQuaternion());

        QVERIFY2(posErr <= positionTol_m,
                 qPrintable(QString("point %1 position error %2 mm").arg(i + 1).arg(posErr * 1000.0)));
        QVERIFY2(oriErr <= orientationTol_rad,
                 qPrintable(QString("point %1 orientation error %2 deg").arg(i + 1).arg(oriErr * kRadToDeg)));
    }
}

/// Runs ForwardKinematics::toolPose() with a fixed (44.2, 0, 139.0) mm tool offset over 4
/// measured arm-config poses and checks the position/orientation error stays within tolerance
/// (<= 0.05 mm, <= 0.007 deg).
void NachiMZ04DTests::forwardKinematicsWithToolMatchesMeasuredPoses()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();

    // Arm-config ground-truth set, measured with a tool offset of (44.2, 0, 139.0) mm and
    // zero tool rotation (docs/preset_references/nachi-mz04d.md). TCP orientation is reported
    // Z-first like the flange poses.
    const Pose flangeToTcp = Pose::fromXYZRPY_mm_deg(44.2, 0.0, 139.0, 0.0, 0.0, 0.0);

    struct ToolPoint {
        std::array<double, 6> jointsDeg;
        double x_mm, y_mm, z_mm, yawZ_deg, pitchY_deg, rollX_deg;
    };
    const std::array<ToolPoint, 4> points = {{
        {{25.5519, -22.283, 174.08, -0.767464, 29.1492, 155.426}, 357.026, 151.287, 711.747, 0.302115, -1.01447, 0.066286},
        {{23.124, -18.8593, 164.056, -0.569853, 35.8219, 157.649}, 394.634, 151.222, 711.924, 0.307335, -1.0669, 0.0869993},
        {{-26.3452, -22.0674, 173.422, 1.03631, 29.5313, 205.746}, 356.991, -156.748, 711.81, 0.302113, -1.01847, 0.0591564},
        {{-23.4909, -17.9565, 161.556, 0.77206, 37.3102, 203.18}, 401.18, -156.814, 711.822, 0.307374, -1.02259, 0.0604472},
    }};

    const double positionTol_m = 5.0e-5;                // 0.05 mm
    const double orientationTol_rad = 0.007 / kRadToDeg; // 0.007 deg

    for (std::size_t i = 0; i < points.size(); ++i) {
        const ToolPoint& p = points[i];
        const JointVector joints = JointVector::fromDegrees(
            {p.jointsDeg[0], p.jointsDeg[1], p.jointsDeg[2], p.jointsDeg[3], p.jointsDeg[4], p.jointsDeg[5]});

        const Pose actual = ForwardKinematics::toolPose(config, joints, flangeToTcp);
        const Pose expected = Pose::fromXYZRPY_mm_deg(p.x_mm, p.y_mm, p.z_mm, p.rollX_deg, p.pitchY_deg, p.yawZ_deg);

        const double posErr = (actual.translation_m() - expected.translation_m()).norm();
        const double oriErr = actual.rotationQuaternion().angularDistance(expected.rotationQuaternion());

        QVERIFY2(posErr <= positionTol_m,
                 qPrintable(QString("tool point %1 position error %2 mm").arg(int(i + 1)).arg(posErr * 1000.0)));
        QVERIFY2(oriErr <= orientationTol_rad,
                 qPrintable(QString("tool point %1 orientation error %2 deg").arg(int(i + 1)).arg(oriErr * kRadToDeg)));
    }
}

/// Checks PostureResolver::classify() reproduces the Nachi manual's shoulder/elbow/wrist labels
/// for 6 joint configurations (4 measured, 2 synthetic covering the elbow-above and wrist-flip
/// branches), and that fromLabels() maps labels back to the expected branch signs.
void NachiMZ04DTests::postureClassificationMatchesNachiManualRules()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const std::unique_ptr<PostureResolver> resolver = PostureResolverFactory::create(config);
    QVERIFY(resolver != nullptr);

    // Map a generic branch sign back to the preset's configured Nachi label name.
    const auto nameFor = [&](const std::string& axis, const std::optional<int>& branch) -> std::string {
        const auto it = config.posture.labels.find(axis);
        if (it == config.posture.labels.end() || !branch.has_value()) {
            return std::string();
        }
        return *branch == -1 ? it->second.negative : it->second.positive;
    };

    struct Case {
        std::array<double, 6> jointsDeg;
        const char* shoulder;
        const char* elbow;
        const char* wrist;
    };
    // First four rows are the Arm-config ground-truth set (docs/preset_references/nachi-mz04d.md);
    // the last two are synthetic cases that also exercise the elbow above (J3 < 0) and wrist
    // flip (J5 < 0) branches not present in the ground-truth set.
    const std::array<Case, 6> cases = {{
        {{25.5519, -22.283, 174.08, -0.767464, 29.1492, 155.426}, "lefty", "below", "non-flip"},
        {{23.124, -18.8593, 164.056, -0.569853, 35.8219, 157.649}, "lefty", "below", "non-flip"},
        {{-26.3452, -22.0674, 173.422, 1.03631, 29.5313, 205.746}, "righty", "below", "non-flip"},
        {{-23.4909, -17.9565, 161.556, 0.77206, 37.3102, 203.18}, "righty", "below", "non-flip"},
        {{-10.0, 0.0, -30.0, 0.0, 40.0, 0.0}, "righty", "above", "non-flip"},
        {{10.0, 0.0, 30.0, 0.0, -40.0, 0.0}, "lefty", "below", "flip"},
    }};

    for (const Case& c : cases) {
        const JointVector joints = JointVector::fromDegrees(
            {c.jointsDeg[0], c.jointsDeg[1], c.jointsDeg[2], c.jointsDeg[3], c.jointsDeg[4], c.jointsDeg[5]});
        const Result<ArmPosture> posture = resolver->classify(config, joints);
        QVERIFY(posture.ok());
        QCOMPARE(nameFor("shoulder", posture.value.shoulder), std::string(c.shoulder));
        QCOMPARE(nameFor("elbow", posture.value.elbow), std::string(c.elbow));
        QCOMPARE(nameFor("wrist", posture.value.wrist), std::string(c.wrist));
    }

    // Label round-trip: Nachi names map back to the expected generic branch signs.
    const Result<ArmPosture> fromLabels = resolver->fromLabels(config.posture, {
        {"shoulder", "righty"},
        {"elbow", "above"},
        {"wrist", "flip"},
    });
    QVERIFY(fromLabels.ok());
    QCOMPARE(*fromLabels.value.shoulder, -1); // righty (J1 < 0)
    QCOMPARE(*fromLabels.value.elbow, -1);    // above  (J3 < 0)
    QCOMPARE(*fromLabels.value.wrist, -1);    // flip   (J5 < 0)
}

/// Computes a target pose via FK from a known-good seed joint vector, solves IK for that target
/// using the same seed, and checks the solved joints reproduce the target position to within
/// 1e-6 m.
void NachiMZ04DTests::presetRunsFkAndSeededIkRoundTrip()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const SerialRobotKinematics robot(config);

    // Use a non-singular reference measurement (point 1) as both target and seed.
    const JointVector seed = JointVector::fromDegrees({28.1579, -18.8069, 163.839, -0.710019, 35.8922, 152.731});
    const Pose target = ForwardKinematics::flangePose(config, seed);

    IKRequest request;
    request.targetPose = target;
    request.seedJoint = seed;

    const IKResult result = robot.solve(request);
    QVERIFY(result.ok());

    const Pose solved = ForwardKinematics::flangePose(config, result.best().joints);
    QVERIFY((solved.translation_m() - target.translation_m()).norm() <= 1e-6);
}

/// QtTest entry point for NachiMZ04DTests: constructs the suite and runs it via QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runNachiMZ04DTests(int argc, char** argv)
{
    NachiMZ04DTests tests;
    return QTest::qExec(&tests, argc, argv);
}
