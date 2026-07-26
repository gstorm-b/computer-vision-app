#ifndef KAWASAKI_ROBOT_CONFIG_H
#define KAWASAKI_ROBOT_CONFIG_H

#include "device/robot/robot_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>

namespace vc::device {

/// Minimum config skeleton for a Kawasaki robot. Fields will be added when
/// the vendor integration starts; for now the class only carries the
/// family/sub-type dispatch so it round-trips through JSON.
class KawasakiRobotCfg : public RobotCfg {
    Q_GADGET

public:
    /// Default-constructs the config, deferring all shared state to RobotCfg.
    explicit KawasakiRobotCfg() : RobotCfg() {}

    /// Returns the Q_GADGET static meta-object used for property introspection.
    const QMetaObject &getMetaObject() const override {
        return vc::device::KawasakiRobotCfg::staticMetaObject;
    }

    /// Returns RobotType::Kawasaki, identifying this config's vendor sub-type.
    RobotType robotType() const override {
        return RobotType::Kawasaki;
    }

    /// Serializes the config to JSON by delegating to RobotCfg::toJson(); no
    /// vendor-specific fields exist yet.
    QJsonObject toJson() const override {
        return RobotCfg::toJson();
    }

    /// Populates the config from JSON by delegating to RobotCfg::fromJson().
    /// @param obj JSON object expected to carry the RobotType dispatch key
    /// @return true if the base parse succeeds and the robot type matches
    bool fromJson(const QJsonObject &obj) override {
        return RobotCfg::fromJson(obj);
    }

    /// Returns a heap-allocated copy of this config; caller takes ownership.
    IDeviceCfg* clone() override {
        return new KawasakiRobotCfg(*this);
    }
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::KawasakiRobotCfg)

#endif // KAWASAKI_ROBOT_CONFIG_H
