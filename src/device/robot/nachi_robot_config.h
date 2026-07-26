#ifndef NACHI_ROBOT_CONFIG_H
#define NACHI_ROBOT_CONFIG_H

#include "device/robot/robot_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>

namespace vc::device {

/// Minimum config skeleton for a Nachi robot — mirror of KawasakiRobotCfg.
/// Vendor-specific fields will be added once integration starts.
class NachiRobotCfg : public RobotCfg {
    Q_GADGET

public:
    /// Constructs an empty config (base RobotCfg carries no state of its own yet).
    explicit NachiRobotCfg() : RobotCfg() {}

    /// Returns this class's static meta-object, used by the property-browser/gadget
    /// introspection machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::NachiRobotCfg::staticMetaObject;
    }

    /// Returns RobotType::Nachi.
    RobotType robotType() const override {
        return RobotType::Nachi;
    }

    /// Serializes this config; currently just delegates to RobotCfg::toJson() since
    /// no Nachi-specific fields exist yet.
    /// @return a JSON object containing the family-level robot-type key
    QJsonObject toJson() const override {
        return RobotCfg::toJson();
    }

    /// Restores this config; currently just delegates to RobotCfg::fromJson(), which
    /// validates the robot-type key matches Nachi.
    /// @param obj JSON object previously produced by toJson()
    /// @return true if the robot-type key is present and matches; false otherwise
    bool fromJson(const QJsonObject &obj) override {
        return RobotCfg::fromJson(obj);
    }

    /// Returns a heap-allocated copy of this config; caller takes ownership.
    IDeviceCfg* clone() override {
        return new NachiRobotCfg(*this);
    }
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::NachiRobotCfg)

#endif // NACHI_ROBOT_CONFIG_H
