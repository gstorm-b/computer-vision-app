#ifndef ROBOT_KINEMATIC_CHECK_CONFIG_H
#define ROBOT_KINEMATIC_CHECK_CONFIG_H

/**
 * @file robot_kinematic_check_config.h
 * @brief Robot reachability/singularity check settings: the picking-path waypoint type and the
 *        check config itself.
 *
 * These types used to live in `output_device/vision_output_config.h`, which made them reachable
 * only through the vision-output device family. They are settings about the *robot*, not about a
 * transport, and any device that can serve the vision-output role has to be able to supply them
 * — see IResultOutputDevice::robotKinematicCheckConfig(). They live here so a PLC-family device
 * can carry them without depending on the vision-output family.
 *
 * @note Nothing about the JSON changed in the move. `VisionOutputDeviceCfg` still owns the
 *       `m_kinematicCheck` member and still writes it under the same key, so project files
 *       written before the move load unchanged.
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace vc::device {

/**
 * @struct PickPathPoint
 * @brief One waypoint of the picking path. By default each axis is an offset from the
 *        matching (pick) pose, applied in the TOOL frame (pose = pick * offset). Any axis
 *        whose abs* flag is set is instead an ABSOLUTE value in the robot BASE frame,
 *        replacing whatever the composition produced on that axis. The posture branch
 *        fields hold preset-specific labels (e.g. "lefty"/"righty"); an empty label means "any
 *        branch on that axis". The whole path is "pickable" only when every waypoint is
 *        reachable on the required posture branch (and collision-free when the collision
 *        check is enabled).
 *
 * @note Mixing absolute and relative axes cannot be expressed as a single transform
 *       product. RobotKinematicPickingChecker::isPickable composes the tool-frame offset
 *       first, decomposes the result to base-frame XYZ+RPY, overwrites the flagged axes,
 *       and rebuilds the pose.
 * @note All flags default to false, which is exactly the pre-Phase-5 all-relative
 *       behaviour; documents written before the flags existed load that way.
 */
struct PickPathPoint {
    double dx = 0.0, dy = 0.0, dz = 0.0;            ///< mm   offset from pick pose, or absolute base-frame position when the matching abs flag is set
    double dRoll = 0.0, dPitch = 0.0, dYaw = 0.0;   ///< deg  offset from pick pose, or absolute base-frame rotation when the matching abs flag is set
    bool absX = false, absY = false, absZ = false;              ///< Treat dx/dy/dz as absolute base-frame positions instead of offsets.
    bool absRoll = false, absPitch = false, absYaw = false;     ///< Treat dRoll/dPitch/dYaw as absolute base-frame rotations instead of offsets.
    QString shoulder;                               ///< preset label, "" = any
    QString elbow;                                  ///< preset label, "" = any
    QString wrist;                                  ///< preset label, "" = any

    /// Serializes this waypoint (the six axis values, their absolute/relative flags, and
    /// the posture branch labels) to JSON.
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["dx"] = dx; obj["dy"] = dy; obj["dz"] = dz;
        obj["dRoll"] = dRoll; obj["dPitch"] = dPitch; obj["dYaw"] = dYaw;
        obj["absX"] = absX; obj["absY"] = absY; obj["absZ"] = absZ;
        obj["absRoll"] = absRoll; obj["absPitch"] = absPitch; obj["absYaw"] = absYaw;
        obj["shoulder"] = shoulder;
        obj["elbow"]    = elbow;
        obj["wrist"]    = wrist;
        return obj;
    }

    /// Restores this waypoint from JSON written by toJson(); missing numeric fields
    /// default to 0.0, missing labels default to an empty string, and missing abs* flags
    /// default to false (all-relative, the behaviour before the flags existed).
    void fromJson(const QJsonObject &obj) {
        dx = obj["dx"].toDouble(0.0); dy = obj["dy"].toDouble(0.0); dz = obj["dz"].toDouble(0.0);
        dRoll  = obj["dRoll"].toDouble(0.0);
        dPitch = obj["dPitch"].toDouble(0.0);
        dYaw   = obj["dYaw"].toDouble(0.0);
        absX = obj["absX"].toBool(false);
        absY = obj["absY"].toBool(false);
        absZ = obj["absZ"].toBool(false);
        absRoll  = obj["absRoll"] .toBool(false);
        absPitch = obj["absPitch"].toBool(false);
        absYaw   = obj["absYaw"]  .toBool(false);
        shoulder = obj["shoulder"].toString();
        elbow    = obj["elbow"].toString();
        wrist    = obj["wrist"].toString();
    }
};

/**
 * @struct RobotKinematicCheckConfig
 * @brief Optional reachability/singularity gate: when enabled, the result-output device runs
 *        inverse kinematics on each outgoing pick pose (built from x,y,z,r via the top-down
 *        pick convention, at the configured TCP) using the selected robot preset, and flags
 *        poses that are out of reach / out of joint limits / singular. When
 *        collisionCheckEnabled is also set, each reachable solution is additionally run
 *        through the Coal mesh self-collision check. Plain value type (no RobotKinematics
 *        dependency) so the config header stays light; the device builds the solver from
 *        these fields. Custom presets are not authored here (preset selection only, by name;
 *        the only built-in preset is "Nachi MZ04D").
 */
struct RobotKinematicCheckConfig {
    bool    enabled   = false;                   ///< Enables the kinematic reachability gate.
    bool    collisionCheckEnabled = false;       ///< run Coal mesh self-collision too
    QString presetName;                          ///< e.g. "Nachi MZ04D"
    QString tcpName   = QStringLiteral("tcp");   ///< flange -> TCP (mm, deg)
    double  tcpX = 0.0, tcpY = 0.0, tcpZ = 0.0;            ///< Flange->TCP translation offset (mm).
    double  tcpRoll = 0.0, tcpPitch = 0.0, tcpYaw = 0.0;   ///< Flange->TCP rotation offset (deg).
    /// Picking-path waypoints checked in addition to the pick pose. Empty => the
    /// single pick pose is checked (legacy behaviour).
    QVector<PickPathPoint> pickPath;

    /// Serializes the kinematic-check config (enabled flags, preset name, TCP offset,
    /// and pick-path waypoints) to JSON.
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["enabled"]    = enabled;
        obj["collisionCheckEnabled"] = collisionCheckEnabled;
        obj["presetName"] = presetName;
        obj["tcpName"]    = tcpName;
        obj["tcpX"] = tcpX; obj["tcpY"] = tcpY; obj["tcpZ"] = tcpZ;
        obj["tcpRoll"] = tcpRoll; obj["tcpPitch"] = tcpPitch; obj["tcpYaw"] = tcpYaw;
        QJsonArray pathArr;
        for (const PickPathPoint &p : pickPath)
            pathArr.append(p.toJson());
        obj["pickPath"] = pathArr;
        return obj;
    }

    /// Restores this config from JSON written by toJson(); any missing field falls back
    /// to its default value (kinematic check disabled, "tcp" TCP name, empty pick path).
    void fromJson(const QJsonObject &obj) {
        enabled    = obj["enabled"].toBool(false);
        collisionCheckEnabled = obj["collisionCheckEnabled"].toBool(false);
        presetName = obj["presetName"].toString();
        tcpName    = obj["tcpName"].toString(QStringLiteral("tcp"));
        tcpX = obj["tcpX"].toDouble(0.0); tcpY = obj["tcpY"].toDouble(0.0); tcpZ = obj["tcpZ"].toDouble(0.0);
        tcpRoll = obj["tcpRoll"].toDouble(0.0);
        tcpPitch = obj["tcpPitch"].toDouble(0.0);
        tcpYaw = obj["tcpYaw"].toDouble(0.0);
        pickPath.clear();
        const QJsonArray pathArr = obj["pickPath"].toArray();
        for (const QJsonValue &v : pathArr) {
            PickPathPoint p;
            p.fromJson(v.toObject());
            pickPath.append(p);
        }
    }
};

} // namespace vc::device

#endif // ROBOT_KINEMATIC_CHECK_CONFIG_H
