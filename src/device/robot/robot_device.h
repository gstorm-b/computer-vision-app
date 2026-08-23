#ifndef ROBOT_DEVICE_H
#define ROBOT_DEVICE_H

/**
 * @file robot_device.h
 * @brief Abstract robot device family: sub-type dispatch, the family-level config and device
 *        base classes.
 */

#include "device/idevice.h"

/// JSON/display-string values for each RobotType, used by RobotTypeToString()/
/// RobotTypeFromString() below.
#define ROBOT_TYPE_KAWASAKI   "Kawasaki"
#define ROBOT_TYPE_NACHI      "Nachi"
#define ROBOT_TYPE_HUAYAN     "Huayan"

namespace vc::device {

/**
 * @enum RobotType
 * @brief Top-level dispatch handle for the robot family. Each vendor implementation registers
 *        a value here; DeviceFactory::createRobotDevice() switches on this enum to pick the
 *        concrete subclass.
 */
enum RobotType {
    RobotTypeNone,  ///< No/unknown robot type.
    Kawasaki,       ///< Kawasaki vendor robot.
    Nachi,          ///< Nachi vendor robot.
    Huayan,         ///< Placeholder for future vendor.
};

/**
 * @brief Converts a RobotType to its JSON/display string (e.g. "Kawasaki").
 * @param[in] t the robot type to convert
 * @return the matching ROBOT_TYPE_* string, or "" for RobotTypeNone/unrecognized values
 */
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

/**
 * @brief Converts a JSON/display string back to a RobotType.
 * @param[in] t the string previously produced by RobotTypeToString()
 * @return the matching RobotType, or RobotTypeNone if `t` matches no known vendor
 */
[[maybe_unused]] static RobotType RobotTypeFromString(QString t) {
    if (t == ROBOT_TYPE_KAWASAKI) return RobotType::Kawasaki;
    if (t == ROBOT_TYPE_NACHI)    return RobotType::Nachi;
    if (t == ROBOT_TYPE_HUAYAN)   return RobotType::Huayan;
    return RobotType::RobotTypeNone;
}

/**
 * @class RobotCfg
 * @brief Abstract config for the robot family. Carries only the family-level dispatch
 *        (RobotType) and the family-level JSON header. Vendor-specific fields live in the
 *        concrete subclass.
 */
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

    /**
     * @brief Validates the family-level header: checks that `obj` carries the robot-type key
     *        and that it matches this config's own robotType(). Concrete subclasses call this
     *        before restoring their own vendor-specific fields.
     * @param[in] obj JSON object previously produced by toJson()
     * @return true if the robot-type key is present and matches; false otherwise
     */
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

/**
 * @class RobotDevice
 * @brief Abstract base for the robot device family. Carries only the minimum IDevice surface area;
 *        vendor-specific motion/IO APIs are added in concrete subclasses.
 */
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
