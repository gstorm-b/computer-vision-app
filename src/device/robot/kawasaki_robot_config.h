#ifndef KAWASAKI_ROBOT_CONFIG_H
#define KAWASAKI_ROBOT_CONFIG_H

#include "device/robot/robot_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>

namespace vc::device {

// Minimum config skeleton for a Kawasaki robot. Fields will be added when
// the vendor integration starts; for now the class only carries the
// family/sub-type dispatch so it round-trips through JSON.
class KawasakiRobotCfg : public RobotCfg {
    Q_GADGET

public:
    explicit KawasakiRobotCfg() : RobotCfg() {}

    const QMetaObject &getMetaObject() const override {
        return vc::device::KawasakiRobotCfg::staticMetaObject;
    }

    RobotType robotType() const override {
        return RobotType::Kawasaki;
    }

    QJsonObject toJson() const override {
        return RobotCfg::toJson();
    }

    bool fromJson(const QJsonObject &obj) override {
        return RobotCfg::fromJson(obj);
    }

    IDeviceCfg* clone() override {
        return new KawasakiRobotCfg(*this);
    }
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::KawasakiRobotCfg)

#endif // KAWASAKI_ROBOT_CONFIG_H
