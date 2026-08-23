#ifndef VIRTUAL_PLC_CONFIG_H
#define VIRTUAL_PLC_CONFIG_H

/**
 * @file virtual_plc_config.h
 * @brief VirtualPlcCfg — configuration for the hardware-free VirtualPlcDevice.
 */

#include "core/qgadget_macro.h"
#include "device/plc/plc_device.h"

namespace vc::device {

/**
 * @class VirtualPlcCfg
 * @brief Configuration for a PLC with nothing on the other end.
 *
 * Carries no address, port or protocol timeout — this device connects to nothing, and
 * inventing those would be inventing settings that lie. What it does carry is the **size of
 * the tag space it advertises**, and that is not cosmetic: the signal-map editor builds its
 * tag lists from `availableDigitalIoNames()`/`availableWordIoNames()`, so a virtual PLC with
 * no tags cannot have a signal map bound to it at all, and the task it belongs to can never
 * be configured.
 *
 * The tags are `M0…M<digitalTagCount-1>` and `D0…D<wordTagCount-1>` — Mitsubishi's spelling,
 * because that is the only PLC this product speaks and the tags already written into project
 * files are written for it.
 */
class VirtualPlcCfg : public PlcCfg {
    Q_GADGET

    G_PROPERTY_NUMBER_READWRITE(int, digitalTagCount, 0, 8192, "Digital tags (M0…)")  ///< How many M tags the signal-map editor is offered.
    G_PROPERTY_NUMBER_READWRITE(int, wordTagCount, 0, 8192, "Word tags (D0…)")        ///< How many D tags the signal-map editor is offered.

public:
    /// Constructs the config with a tag space large enough for a typical localization task.
    explicit VirtualPlcCfg() = default;

    /// Returns this class's Qt meta-object, used by the gadget property-browser machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::VirtualPlcCfg::staticMetaObject;
    }

    /// Always PlcType::VirtualPlc.
    PlcType plcType() const override {
        return PlcType::VirtualPlc;
    }

    /// Serializes the tag-space sizes on top of the family-level PlcType key.
    QJsonObject toJson() const override {
        QJsonObject obj = PlcCfg::toJson();
        obj["DigitalTagCount"] = m_digitalTagCount;
        obj["WordTagCount"]    = m_wordTagCount;
        return obj;
    }

    /// Restores the tag-space sizes; PlcCfg::fromJson() validates the sub-type first.
    bool fromJson(const QJsonObject &obj) override {
        if (!PlcCfg::fromJson(obj)) {
            return false;
        }
        m_digitalTagCount = obj["DigitalTagCount"].toInt(kDefaultDigitalTagCount);
        m_wordTagCount    = obj["WordTagCount"].toInt(kDefaultWordTagCount);
        return true;
    }

    /// Allocates and returns a heap copy; the caller owns it.
    IDeviceCfg *clone() override {
        return new VirtualPlcCfg(*this);
    }

private:
    /// Enough M tags for a localization task's bit signals with room to spare; picked so a
    /// freshly created virtual PLC is immediately usable in the signal-map editor rather than
    /// needing to be configured before it can be configured.
    static constexpr int kDefaultDigitalTagCount = 128;
    /// Same reasoning for the D (word) tags.
    static constexpr int kDefaultWordTagCount = 128;

    int m_digitalTagCount{kDefaultDigitalTagCount};
    int m_wordTagCount{kDefaultWordTagCount};
};

} // namespace vc::device

#endif // VIRTUAL_PLC_CONFIG_H
