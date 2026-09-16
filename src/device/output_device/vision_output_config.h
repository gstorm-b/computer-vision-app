#ifndef VISION_OUTPUT_CONFIG_H
#define VISION_OUTPUT_CONFIG_H

/**
 * @file vision_output_config.h
 * @brief Config types and JSON (de)serialization for the vision-output device family (the
 *        software side that streams matching results / raw bytes out to an external system).
 */

#include "device/idevice_config.h"
#include "device/robot_kinematic_check_config.h"
#include "core/logger/app_logger.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace vc::device {

/**
 * @enum VisionOutputType
 * @brief Family-level sub-type dispatch handle. Mirrors CameraType / RobotType.
 *        Concrete vendors register a value here; DeviceFactory::createVisionOutput()
 *        switches on this enum to pick the concrete subclass.
 */
enum VisionOutputType : int {
    VisionOutputTypeNone,     ///< Sentinel: no vision-output sub-type selected.
    VisionTCPIP,        ///< TCP/IP server transport (software listens)
    VisionTcpipClient,  ///< TCP/IP client transport (software dials out)
    VisionSerial,       ///< placeholder for future transport
    /// No transport at all: results are captured in memory instead of sent. Named
    /// `VirtualVisionOutput` because these enums are unscoped and every enumerator lands in
    /// `vc::device`, so each family needs its own spelling.
    VirtualVisionOutput,
};

/// Converts a VisionOutputType to its JSON string label (e.g. "VisionTCPIP");
/// returns an empty string for VisionOutputTypeNone or any unrecognized value.
inline QString VisionOutputTypeToString(VisionOutputType t) {
    switch (t) {
    case VisionTCPIP:            return QStringLiteral("VisionTCPIP");
    case VisionTcpipClient:      return QStringLiteral("VisionTcpipClient");
    case VisionSerial:           return QStringLiteral("VisionSerial");
    case VirtualVisionOutput:    return QStringLiteral("Virtual");
    case VisionOutputTypeNone:   return QString();
    }
    return QString();
}

/// Parses a VisionOutputType from its JSON string label; returns VisionOutputTypeNone
/// if `t` does not match any known label.
inline VisionOutputType VisionOutputTypeFromString(const QString &t) {
    if (t == QLatin1String("VisionTCPIP"))       return VisionTCPIP;
    if (t == QLatin1String("VisionTcpipClient")) return VisionTcpipClient;
    if (t == QLatin1String("VisionSerial"))      return VisionSerial;
    if (t == QLatin1String("Virtual"))           return VirtualVisionOutput;
    return VisionOutputTypeNone;
}

// PickPathPoint and RobotKinematicCheckConfig used to be defined here. They moved to
// `device/robot_kinematic_check_config.h` (included above, so every existing user of this header
// still compiles) because they describe the robot, not this transport family, and a PLC-family
// device serving the vision-output role has to be able to carry them too.

/**
 * @class VisionOutputDeviceCfg
 * @brief Abstract config for the vision-output device family. Carries only the family-level
 *        dispatch field (visionOutputType()) and the shared kinematic-check config; concrete
 *        configs (VisionTcpipDeviceCfg, future VisionSerialDeviceCfg, …) inherit and add their
 *        transport-specific Q_PROPERTYs.
 */
class VisionOutputDeviceCfg : public IDeviceCfg {
public:
    /// Returns the concrete vision-output sub-type (server/client/serial/…) this config
    /// is for; used both for factory dispatch and for the JSON type-tag round-trip.
    virtual VisionOutputType visionOutputType() const = 0;

    /// Returns DeviceType::VisionOutput.
    DeviceType deviceType() const override {
        return DeviceType::VisionOutput;
    }

    /// Serializes the family-level fields (VisionOutputType tag and the kinematic-check
    /// config) to JSON. Concrete subclasses override to add their own fields on top.
    QJsonObject toJson() const override {
        QJsonObject obj;
        obj[DEVICE_JSK_VOUT_TYPE] =
            VisionOutputTypeToString(this->visionOutputType());
        obj[DEVICE_JSK_VOUT_KCHECK] = m_kinematicCheck.toJson();
        return obj;
    }

    /// Restores the family-level fields from JSON. Fails (returning false, without
    /// modifying state) if the VisionOutputType key is missing or its value doesn't
    /// match this config's visionOutputType().
    /// @return true on success, false if the type key is missing or mismatched
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

    /// Robot kinematic reachability check (Phase 2). Shared by every concrete
    /// vision-output transport (server / client).
    RobotKinematicCheckConfig m_kinematicCheck;
};

} // namespace vc::device

#endif // VISION_OUTPUT_CONFIG_H
