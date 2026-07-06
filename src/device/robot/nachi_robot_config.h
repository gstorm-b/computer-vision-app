#ifndef NACHI_ROBOT_CONFIG_H
#define NACHI_ROBOT_CONFIG_H

#include "device/robot/robot_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>

namespace vc::device {

// Minimum config skeleton for a Nachi robot — mirror of KawasakiRobotCfg.
// Vendor-specific fields will be added once integration starts.
class NachiRobotCfg : public RobotCfg {
    Q_GADGET

public:
    explicit NachiRobotCfg() : RobotCfg() {}

    const QMetaObject &getMetaObject() const override {
        return vc::device::NachiRobotCfg::staticMetaObject;
    }

    RobotType robotType() const override {
        return RobotType::Nachi;
    }

    QJsonObject toJson() const override {
        return RobotCfg::toJson();
    }

    bool fromJson(const QJsonObject &obj) override {
        return RobotCfg::fromJson(obj);
    }

    IDeviceCfg* clone() override {
        return new NachiRobotCfg(*this);
    }
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::NachiRobotCfg)

#endif // NACHI_ROBOT_CONFIG_H
