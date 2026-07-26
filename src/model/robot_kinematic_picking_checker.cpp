#include "model/robot_kinematic_picking_checker.h"

#include <optional>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Posture/PostureResolver.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

namespace vc::model {

/// File-local helpers backing RobotKinematicPickingChecker: preset/tool
/// constants, mesh-collision profile loading, and posture-label resolution.
namespace {

/// The RobotKinematics component ships Nachi MZ04D as its only built-in C++
/// preset; the picking TCP is appended as the default tool from the config.
constexpr char kNachiMz04dPreset[] = "Nachi MZ04D";   ///< Only preset name accepted by buildRobotConfig().
constexpr char kPickingToolId[]    = "picking_tcp";   ///< Tool id assigned to the picking TCP appended to the robot config.

/// Simplified (voxel) mesh-collision profile — same asset the device runtime and
/// the widget tester use. Resolved deployed-first, source-tree fallback.
constexpr char kSimplifiedMeshProfileDeployedRel[] =
    "robot_assets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";  ///< Path relative to the deployed app directory.
constexpr char kSimplifiedMeshProfileSourceRel[] =
    "components/RobotKinematics/presets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";  ///< Path relative to a source-tree root (fallback).

/// Source-tree fallback resolver: walks up from the current working directory
/// and the application directory looking for `relative`.
/// @param relative path to resolve, relative to one of the candidate roots
/// @return absolute path to the first existing candidate found, or an empty
///         string if `relative` does not exist under any candidate root
QString resolveAssetPath(const QString& relative) {
    const QFileInfo direct(relative);
    if (direct.exists()) return direct.absoluteFilePath();

    QStringList roots;
    roots << QDir::currentPath() << QCoreApplication::applicationDirPath();
    QDir walker(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8 && walker.cdUp(); ++i)
        roots << walker.absolutePath();
    roots.removeDuplicates();

    for (const QString& root : roots) {
        const QFileInfo candidate(QDir(root).filePath(relative));
        if (candidate.exists()) return candidate.absoluteFilePath();
    }
    return {};
}

/// Lazily loaded, process-wide simplified mesh-collision profile: resolved
/// deployed-path-first (falling back to the source tree via resolveAssetPath())
/// and parsed once on first call, then cached for the life of the process.
/// @return pointer to the cached profile, or nullptr if the asset could not be
///         located or failed to parse
const RobotKinematics::MeshCollisionProfile* simplifiedMeshProfile() {
    static const std::optional<RobotKinematics::MeshCollisionProfile> profile =
        []() -> std::optional<RobotKinematics::MeshCollisionProfile> {
            QString path;
            const QFileInfo deployed(QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QString::fromLatin1(kSimplifiedMeshProfileDeployedRel)));
            if (deployed.exists())
                path = deployed.absoluteFilePath();
            else
                path = resolveAssetPath(QString::fromLatin1(kSimplifiedMeshProfileSourceRel));
            if (path.isEmpty())
                return std::nullopt;
            const RobotKinematics::Result<RobotKinematics::MeshCollisionProfile> res =
                RobotKinematics::MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
            if (!res.ok())
                return std::nullopt;
            return res.value;
        }();
    return profile ? &(*profile) : nullptr;
}

/// Builds the selected preset's robot config (currently only Nachi MZ04D is
/// recognised) and appends the picking TCP, positioned per `cfg`'s TCP offset,
/// as its default tool.
/// @param cfg source config naming the preset and giving the TCP offset to use
/// @param out output robot config; populated only when the preset is recognised
/// @return true if `cfg.presetName` matched a known preset and `out` was populated
bool buildRobotConfig(const vc::device::RobotKinematicCheckConfig& cfg,
                      RobotKinematics::SerialRobotConfig& out) {
    if (cfg.presetName != QLatin1String(kNachiMz04dPreset))
        return false;

    out = RobotKinematics::Presets::nachiMZ04D();
    RobotKinematics::Tool tool;
    tool.id          = kPickingToolId;
    tool.name        = cfg.tcpName.toStdString();
    tool.flangeToTcp = RobotKinematics::Pose::fromXYZRPY_mm_deg(
        cfg.tcpX, cfg.tcpY, cfg.tcpZ, cfg.tcpRoll, cfg.tcpPitch, cfg.tcpYaw);
    out.tools.push_back(tool);
    out.defaultToolId = kPickingToolId;
    return true;
}

/// Maps a preset posture label (e.g. "lefty") to a branch sign (-1/+1). An empty
/// or unrecognised label yields 0 ("any branch on this axis").
/// @param robot robot config providing the posture label table for `axis`
/// @param axis posture axis name (e.g. "shoulder", "elbow", "wrist")
/// @param label the label to resolve (as configured on a pick-path waypoint)
/// @return -1 or +1 for a recognised negative/positive label, 0 otherwise
int labelToSign(const RobotKinematics::SerialRobotConfig& robot,
                const std::string& axis, const QString& label) {
    if (label.isEmpty())
        return 0;
    const auto it = robot.posture.labels.find(axis);
    if (it == robot.posture.labels.end())
        return 0;
    const std::string value = label.toStdString();
    if (value == it->second.negative) return -1;
    if (value == it->second.positive) return 1;
    return 0;
}

/// Checks whether an IK solution's posture is on the required branch for every
/// constrained axis (sign != 0); axes with sign 0 are unconstrained and always
/// match.
/// @param p the arm posture reported by the IK solution being checked
/// @param reqShoulder required shoulder branch sign (-1/+1), or 0 for "any"
/// @param reqElbow required elbow branch sign (-1/+1), or 0 for "any"
/// @param reqWrist required wrist branch sign (-1/+1), or 0 for "any"
/// @return true if `p` satisfies every constrained axis
bool postureMatches(const RobotKinematics::ArmPosture& p,
                    int reqShoulder, int reqElbow, int reqWrist) {
    if (reqShoulder != 0 && !(p.shoulder.has_value() && *p.shoulder == reqShoulder)) return false;
    if (reqElbow    != 0 && !(p.elbow.has_value()    && *p.elbow    == reqElbow))    return false;
    if (reqWrist    != 0 && !(p.wrist.has_value()     && *p.wrist     == reqWrist))   return false;
    return true;
}

} // namespace

RobotKinematicPickingChecker::RobotKinematicPickingChecker(
    const calib::Calibrator& calibrator, vc::device::RobotKinematicCheckConfig config)
    : m_calibrator(calibrator), m_config(std::move(config)) {
    m_robotValid = buildRobotConfig(m_config, m_robot);

    // Prepare the picking-path waypoints once (posture labels resolved to branch
    // signs against the preset). An empty config path degrades to a single
    // zero-offset, unconstrained waypoint (the bare pick pose).
    if (m_config.pickPath.isEmpty()) {
        m_waypoints.push_back(Waypoint{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0});
    } else {
        for (const vc::device::PickPathPoint& p : m_config.pickPath) {
            Waypoint wp;
            wp.dx = p.dx; wp.dy = p.dy; wp.dz = p.dz;
            wp.dRoll = p.dRoll; wp.dPitch = p.dPitch; wp.dYaw = p.dYaw;
            wp.shoulder = labelToSign(m_robot, "shoulder", p.shoulder);
            wp.elbow    = labelToSign(m_robot, "elbow",    p.elbow);
            wp.wrist    = labelToSign(m_robot, "wrist",    p.wrist);
            m_waypoints.push_back(wp);
        }
    }
}

bool RobotKinematicPickingChecker::imageToWorld(double imgX, double imgY,
                                                double imgAngleDeg,
                                                mtc::WorldPickPose& out) const {
    if (!m_calibrator.isCalibrated())
        return false;

    const cv::Point3f robot = m_calibrator.imageToRobot(
        cv::Point2f(static_cast<float>(imgX), static_cast<float>(imgY)));
    out.x_mm  = robot.x;
    out.y_mm  = robot.y;
    out.z_mm  = robot.z;
    // out.r_deg = m_calibrator.rotateImageToRobot(imgAngleDeg, false);
    out.r_deg = -m_calibrator.rotateImageToRobot(imgAngleDeg, false);
    return true;
}

bool RobotKinematicPickingChecker::collisionCheckEnabled() const {
    return m_config.collisionCheckEnabled;
}

bool RobotKinematicPickingChecker::isPickable(const mtc::WorldPickPose& pose,
                                              bool withCollision) const {
    if (!m_robotValid)
        return false;

    // Base pick pose in the robot/world frame. Path offsets compose in the TOOL
    // frame: waypoint = pickPose * offset.
    const RobotKinematics::Pose pickPose = RobotKinematics::Pose::fromXYZRPY_mm_deg(
        pose.x_mm, pose.y_mm, pose.z_mm, 0.0, 0.0, pose.r_deg);

    // Collision profile is loaded only when requested. When unavailable the
    // collision check degrades to advisory (it does not reject a solution).
    const RobotKinematics::MeshCollisionProfile* profile =
        withCollision ? simplifiedMeshProfile() : nullptr;
    const RobotKinematics::SerialRobotKinematics solver(m_robot);

    // The path is pickable only when EVERY waypoint has a reachable IK solution
    // on its required posture branch (collision-free when the check is active).
    for (const Waypoint& wp : m_waypoints) {
        const RobotKinematics::Pose offset = RobotKinematics::Pose::fromXYZRPY_mm_deg(
            wp.dx, wp.dy, wp.dz, wp.dRoll, wp.dPitch, wp.dYaw);

        RobotKinematics::IKRequest req;
        req.targetPose = pickPose * offset;
        req.tool = RobotKinematics::ToolId{kPickingToolId};

        const Eigen::Vector3d eulerZyx = req.targetPose.isometry().linear().canonicalEulerAngles(2, 1, 0);

        // qDebug() << "Check Pose"
        //          << RobotKinematics::units::toMm(req.targetPose.translation_m().x())
        //          << RobotKinematics::units::toMm(req.targetPose.translation_m().y())
        //          << RobotKinematics::units::toMm(req.targetPose.translation_m().z())
        //          << RobotKinematics::units::toDeg(eulerZyx[0])
        //          << RobotKinematics::units::toDeg(eulerZyx[1])
        //          << RobotKinematics::units::toDeg(eulerZyx[2]);

        const RobotKinematics::IKResult ik = solver.solveAll(req);
        if (!ik.ok())
            return false;

        bool waypointOk = false;
        for (const RobotKinematics::IKSolution& sol : ik.solutions) {
            if (!postureMatches(sol.posture, wp.shoulder, wp.elbow, wp.wrist))
                continue;
            if (profile) {
                RobotKinematics::MeshCollisionCheckRequest creq;
                creq.joints = sol.joints;
                const RobotKinematics::CollisionCheckResult res =
                    RobotKinematics::CollisionBackends::checkMesh(m_robot, *profile, creq);
                if (res.ok() && res.hasCollision)
                    continue;   // this branch collides; try another solution
            }
            waypointOk = true;
            break;
        }
        if (!waypointOk)
            return false;
    }
    return true;
}

} // namespace vc::model
