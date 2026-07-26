#ifndef PLC_DEVICE_H
#define PLC_DEVICE_H

#include <memory>
#include <QMap>
#include "device/idevice.h"

/// JSON-persisted PlcType key for the Mitsubishi MC vendor sub-type.
#define PLC_TYPE_MITSUBISHI_MC   "MitsubishiMc"

/// Abstract PLC device family: sub-type dispatch, the family-level config and
/// device base classes, and the shared value-map interface.
namespace vc::device {

/// Family-level sub-type dispatch handle for the PLC family. Concrete
/// vendors register a value here; DeviceFactory::createPlcDevice() switches
/// on this enum to pick the concrete subclass.
enum PlcType {
    PlcTypeNone,
    MitsubishiMc,
};

/// @return the JSON-persisted string key for `t` (empty string for PlcTypeNone
/// or any unhandled value).
[[maybe_unused]] static QString PlcTypeToString(PlcType t) {
    switch (t) {
    case PlcType::MitsubishiMc:  return PLC_TYPE_MITSUBISHI_MC;
    case PlcType::PlcTypeNone:
        return "";
    }
    return "";
}

/// @return the PlcType matching the JSON-persisted string key `t`, or
/// PlcTypeNone if it does not match any known vendor key.
[[maybe_unused]] static PlcType PlcTypeFromString(QString t) {
    if (t == PLC_TYPE_MITSUBISHI_MC) return PlcType::MitsubishiMc;
    return PlcType::PlcTypeNone;
}

/// Abstract config for the PLC family. Carries only the family-level
/// dispatch field; concrete configs (McProtocolConfig for Mitsubishi MC,
/// future OmronFinsConfig, …) inherit and add their protocol-specific
/// Q_PROPERTYs.
class PlcCfg : public IDeviceCfg {
public:
    /// @return the concrete vendor sub-type this config belongs to.
    virtual PlcType plcType() const = 0;

    DeviceType deviceType() const override {
        return DeviceType::PLC;
    }

    /// @return a JSON object with just the family-level PlcType key;
    /// concrete subclasses call this and add their own fields.
    QJsonObject toJson() const override {
        QJsonObject obj;
        obj[DEVICE_JSK_PLC_TYPE] = PlcTypeToString(this->plcType());
        return obj;
    }

    /// Validates that `obj` carries the PlcType key and that it matches
    /// this config's own plcType(); does not read any other field.
    /// @return false (and logs) if the key is missing or the type mismatches
    bool fromJson(const QJsonObject &obj) override {
        if (!obj.contains(DEVICE_JSK_PLC_TYPE)) {
            LOG_DEV_ERR << "PlcCfg: missing PlcType key";
            return false;
        }
        if (plcType() != PlcTypeFromString(obj[DEVICE_JSK_PLC_TYPE].toString())) {
            LOG_DEV_ERR << "PlcCfg: type mismatch -"
                        << obj[DEVICE_JSK_PLC_TYPE].toString();
            return false;
        }
        return true;
    }
};

/// Abstract, vendor-agnostic holder for a PLC's polled register values.
/// Concrete vendors provide a concrete value map (e.g. holding device-map
/// tables) and expose it via clone() for snapshotting.
class PlcValueMap {
public:
    virtual ~PlcValueMap() = default;
    /// @return a polymorphic deep copy of this value map.
    virtual std::shared_ptr<PlcValueMap> clone() const = 0;
};

/// Abstract base for the PLC family. Concrete vendors (McProtocolDevice for
/// Mitsubishi MC, future OmronFinsDevice, SiemensS7Device, …) inherit from
/// this base. The base only carries the family-level dispatch (plcType())
/// and the family JSON header; vendor-specific surface lives entirely on the
/// concrete subclass.
class PlcDevice : public IDevice {
    Q_OBJECT

public:
    /// Constructs the device with its family-level identity; vendor-specific
    /// setup happens in the concrete subclass.
    explicit PlcDevice(QString id, QString name, QObject* parent = nullptr)
        : IDevice(id, name, parent) {}

    DeviceType deviceType() const override {
        return DeviceType::PLC;
    }

    /// @return the concrete vendor sub-type this device implements.
    virtual PlcType plcType() const = 0;

    /// @return the base IDevice JSON plus the family-level PlcType key;
    /// concrete subclasses call this and add their own fields.
    QJsonObject toJson() const override {
        QJsonObject obj = IDevice::toJson();
        obj.insert(DEVICE_JSK_PLC_TYPE, PlcTypeToString(this->plcType()));
        return obj;
    }

signals:
    /// Emitted after a polling cycle with a snapshot of the current register
    /// values.
    void pollingUpdate(std::shared_ptr<vc::device::PlcValueMap> value_map);
    /// Emitted when tracked signal values change, keyed by signal name.
    void valueChanged(QMap<QString, QVariant> values);
};

} // namespace vc::device

#endif // PLC_DEVICE_H
