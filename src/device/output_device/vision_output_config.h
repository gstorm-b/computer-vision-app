#ifndef VISION_OUTPUT_CONFIG_H
#define VISION_OUTPUT_CONFIG_H

#include "device/idevice_config.h"
#include "core/logger/app_logger.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace vc::device {

// Family-level sub-type dispatch handle. Mirrors CameraType / RobotType.
// Concrete vendors register a value here; DeviceFactory::createVisionOutput()
// switches on this enum to pick the concrete subclass.
enum VisionOutputType : int {
    VisionOutputTypeNone,
    VisionTCPIP,        // TCP/IP server transport (software listens)
    VisionTcpipClient,  // TCP/IP client transport (software dials out)
    VisionSerial,       // placeholder for future transport
};

inline QString VisionOutputTypeToString(VisionOutputType t) {
    switch (t) {
    case VisionTCPIP:            return QStringLiteral("VisionTCPIP");
    case VisionTcpipClient:      return QStringLiteral("VisionTcpipClient");
    case VisionSerial:           return QStringLiteral("VisionSerial");
    case VisionOutputTypeNone:   return QString();
    }
    return QString();
}

inline VisionOutputType VisionOutputTypeFromString(const QString &t) {
    if (t == QLatin1String("VisionTCPIP"))       return VisionTCPIP;
    if (t == QLatin1String("VisionTcpipClient")) return VisionTcpipClient;
    if (t == QLatin1String("VisionSerial"))      return VisionSerial;
    return VisionOutputTypeNone;
}

// =====================================================================
// RobotKinematicCheckConfig — optional reachability/singularity gate
// =====================================================================
//
// When enabled, the vision output device runs inverse kinematics on each
// outgoing pick pose (built from x,y,z,r via the top-down pick convention, at
// the configured TCP) using the selected robot preset, and flags poses that
// are out of reach / out of joint limits / singular. When collisionCheckEnabled
// is also set, each reachable solution is additionally run through the Coal
// mesh self-collision check. Plain value type (no RobotKinematics dependency)
// so the config header stays light; the device builds the solver from these
// fields. Custom presets are not authored here (preset selection only, by name;
// the only built-in preset is "Nachi MZ04D").
//
// PickPathPoint — one waypoint of the picking path, expressed as a 6-axis offset
// from the matching (pick) pose, applied in the TOOL frame (pose = pick * offset).
// The posture branch fields hold preset-specific labels (e.g. "lefty"/"righty");
// an empty label means "any branch on that axis". The whole path is "pickable"
// only when every waypoint is reachable on the required posture branch (and
// collision-free when the collision check is enabled).
struct PickPathPoint {
    double dx = 0.0, dy = 0.0, dz = 0.0;            // mm   offset from pick pose
    double dRoll = 0.0, dPitch = 0.0, dYaw = 0.0;   // deg  offset from pick pose
    QString shoulder;                               // preset label, "" = any
    QString elbow;                                  // preset label, "" = any
    QString wrist;                                  // preset label, "" = any

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["dx"] = dx; obj["dy"] = dy; obj["dz"] = dz;
        obj["dRoll"] = dRoll; obj["dPitch"] = dPitch; obj["dYaw"] = dYaw;
        obj["shoulder"] = shoulder;
        obj["elbow"]    = elbow;
        obj["wrist"]    = wrist;
        return obj;
    }

    void fromJson(const QJsonObject &obj) {
        dx = obj["dx"].toDouble(0.0); dy = obj["dy"].toDouble(0.0); dz = obj["dz"].toDouble(0.0);
        dRoll  = obj["dRoll"].toDouble(0.0);
        dPitch = obj["dPitch"].toDouble(0.0);
        dYaw   = obj["dYaw"].toDouble(0.0);
        shoulder = obj["shoulder"].toString();
        elbow    = obj["elbow"].toString();
        wrist    = obj["wrist"].toString();
    }
};

struct RobotKinematicCheckConfig {
    bool    enabled   = false;
    bool    collisionCheckEnabled = false;       // run Coal mesh self-collision too
    QString presetName;                          // e.g. "Nachi MZ04D"
    QString tcpName   = QStringLiteral("tcp");   // flange -> TCP (mm, deg)
    double  tcpX = 0.0, tcpY = 0.0, tcpZ = 0.0;
    double  tcpRoll = 0.0, tcpPitch = 0.0, tcpYaw = 0.0;
    // Picking-path waypoints checked in addition to the pick pose. Empty => the
    // single pick pose is checked (legacy behaviour).
    QVector<PickPathPoint> pickPath;

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

// =====================================================================
// VisionOutputDeviceCfg — abstract config for the vision-output family
// =====================================================================
//
// Carries only the family-level dispatch field. Concrete configs
// (VisionTcpipDeviceCfg, future VisionSerialDeviceCfg, …) inherit and add
// their transport-specific Q_PROPERTYs.
//
class VisionOutputDeviceCfg : public IDeviceCfg {
public:
    virtual VisionOutputType visionOutputType() const = 0;

    DeviceType deviceType() const override {
        return DeviceType::VisionOutput;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj[DEVICE_JSK_VOUT_TYPE] =
            VisionOutputTypeToString(this->visionOutputType());
        obj[DEVICE_JSK_VOUT_KCHECK] = m_kinematicCheck.toJson();
        return obj;
    }

    bool fromJson(const QJsonObject &obj) override {
        if (!obj.contains(DEVICE_JSK_VOUT_TYPE)) {
            LOG_DEV_ERR << "VisionOutputDeviceCfg: missing VisionOutputType key";
            return false;
        }
        if (visionOutputType() !=
            VisionOutputTypeFromString(obj[DEVICE_JSK_VOUT_TYPE].toString())) {
            LOG_DEV_ERR << "VisionOutputDeviceCfg: type mismatch -"
                        << obj[DEVICE_JSK_VOUT_TYPE].toString();
            return false;
        }
        m_kinematicCheck.fromJson(obj[DEVICE_JSK_VOUT_KCHECK].toObject());
        return true;
    }

    // Robot kinematic reachability check (Phase 2). Shared by every concrete
    // vision-output transport (server / client).
    RobotKinematicCheckConfig m_kinematicCheck;
};

} // namespace vc::device

#endif // VISION_OUTPUT_CONFIG_H
