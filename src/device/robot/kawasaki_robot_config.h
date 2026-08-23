#ifndef KAWASAKI_ROBOT_CONFIG_H
#define KAWASAKI_ROBOT_CONFIG_H

/**
 * @file kawasaki_robot_config.h
 * @brief Kawasaki robot configuration stub (KawasakiRobotCfg).
 */

#include "device/robot/robot_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>

namespace vc::device {

/**
 * @class KawasakiRobotCfg
 * @brief Minimum config skeleton for a Kawasaki robot. Carries only the family/sub-type
 *        dispatch so it round-trips through JSON; vendor-specific fields will be added when
 *        integration starts.
 */
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

    /**
     * @brief Populates the config from JSON by delegating to RobotCfg::fromJson().
     * @param[in] obj JSON object expected to carry the RobotType dispatch key
     * @return true if the base parse succeeds and the robot type matches
     */
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
