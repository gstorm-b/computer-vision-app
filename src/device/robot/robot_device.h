#ifndef ROBOT_DEVICE_H
#define ROBOT_DEVICE_H

#include "device/idevice.h"

/// JSON/display-string values for each RobotType, used by RobotTypeToString()/
/// RobotTypeFromString() below.
#define ROBOT_TYPE_KAWASAKI   "Kawasaki"
#define ROBOT_TYPE_NACHI      "Nachi"
#define ROBOT_TYPE_HUAYAN     "Huayan"

namespace vc::device {

/// Top-level dispatch handle for the robot family. Matches the Camera /
/// PLC sub-type pattern: each vendor implementation registers a new value
/// here; DeviceFactory::createRobotDevice() switches on this enum to pick
/// the concrete subclass.
enum RobotType {
    RobotTypeNone,  ///< No/unknown robot type.
    Kawasaki,       ///< Kawasaki vendor robot.
    Nachi,          ///< Nachi vendor robot.
    Huayan,         ///< Placeholder for future vendor.
};

/// Converts a RobotType to its JSON/display string (e.g. "Kawasaki").
/// @param t the robot type to convert
/// @return the matching ROBOT_TYPE_* string, or "" for RobotTypeNone/unrecognized values
[[maybe_unused]] static QString RobotTypeToString(RobotType t) {
    switch (t) {
    case RobotType::Kawasaki: return ROBOT_TYPE_KAWASAKI;
    case RobotType::Nachi:    return ROBOT_TYPE_NACHI;
    case RobotType::Huayan:   return ROBOT_TYPE_HUAYAN;
    case RobotType::RobotTypeNone:
        return "";
    }
    return "";
}

/// Converts a JSON/display string back to a RobotType.
/// @param t the string previously produced by RobotTypeToString()
/// @return the matching RobotType, or RobotTypeNone if `t` matches no known vendor
[[maybe_unused]] static RobotType RobotTypeFromString(QString t) {
    if (t == ROBOT_TYPE_KAWASAKI) return RobotType::Kawasaki;
    if (t == ROBOT_TYPE_NACHI)    return RobotType::Nachi;
    if (t == ROBOT_TYPE_HUAYAN)   return RobotType::Huayan;
    return RobotType::RobotTypeNone;
}

// =====================================================================
// RobotCfg — abstract config for the robot family
// =====================================================================
//
/// Per Rule 12.5, the abstract base only carries the family-level dispatch
/// (RobotType) and the family-level JSON header. Vendor-specific fields
/// live in the concrete subclass — kept intentionally empty here until
/// the first real vendor implementation lands.
class RobotCfg : public IDeviceCfg {
public:
    /// Returns the concrete vendor sub-type (e.g. Kawasaki, Nachi); implemented by
    /// each vendor-specific subclass.
    virtual RobotType robotType() const = 0;

    /// Returns DeviceType::Robot.
    DeviceType deviceType() const override {
        return DeviceType::Robot;
    }

    /// Serializes the family-level header shared by all robot configs.
    /// @return a JSON object with the robot-type key set from robotType()
    QJsonObject toJson() const override {
        QJsonObject obj;
        obj[DEVICE_JSK_ROBOT_TYPE] = RobotTypeToString(this->robotType());
        return obj;
    }

    /// Validates the family-level header: checks that `obj` carries the robot-type key
    /// and that it matches this config's own robotType(). Logs via LOG_DEV_ERR on either
    /// failure. Concrete subclasses call this base implementation before restoring their
    /// own vendor-specific fields.
    /// @param obj JSON object previously produced by toJson()
    /// @return true if the robot-type key is present and matches; false otherwise
    bool fromJson(const QJsonObject &obj) override {
        if (!obj.contains(DEVICE_JSK_ROBOT_TYPE)) {
            LOG_DEV_ERR << "RobotCfg: missing RobotType key";
            return false;
        }
        if (robotType() != RobotTypeFromString(obj[DEVICE_JSK_ROBOT_TYPE].toString())) {
            LOG_DEV_ERR << "RobotCfg: robot type mismatch -"
                        << obj[DEVICE_JSK_ROBOT_TYPE].toString();
            return false;
        }
        return true;
    }
};

// =====================================================================
// RobotDevice — abstract device for the robot family
// =====================================================================
//
/// Minimum surface area as required by IDevice. Vendor-specific motion /
/// teach-pendant / IO APIs are intentionally not declared here — they will
/// be added once the first concrete vendor lands and the shared abstraction
/// becomes concrete (Rule 12.5).
class RobotDevice : public IDevice {
    Q_OBJECT

public:
    /// Constructs the device, forwarding id/name/parent to IDevice.
    explicit RobotDevice(QString id, QString name, QObject* parent = nullptr)
        : IDevice(id, name, parent) {}

    /// Returns DeviceType::Robot.
    DeviceType deviceType() const override {
        return DeviceType::Robot;
    }

    /// Returns the concrete vendor sub-type (e.g. Kawasaki, Nachi); implemented by
    /// each vendor-specific subclass.
    virtual RobotType robotType() const = 0;

    /// Serializes the base IDevice fields plus the family-level robot-type key.
    /// @return a JSON object with IDevice::toJson()'s fields plus the robot-type key
    QJsonObject toJson() const override {
        QJsonObject obj = IDevice::toJson();
        obj.insert(DEVICE_JSK_ROBOT_TYPE, RobotTypeToString(this->robotType()));
        return obj;
    }
};

} // namespace vc::device

#endif // ROBOT_DEVICE_H
