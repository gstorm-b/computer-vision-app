#ifndef VIRTUAL_PLC_DEVICE_H
#define VIRTUAL_PLC_DEVICE_H

/**
 * @file virtual_plc_device.h
 * @brief VirtualPlcDevice — a PLC that needs no hardware.
 */

#include <QMap>
#include <QString>

#include "device/device_capabilities.h"
#include "device/plc/plc_device.h"
#include "device/virtual/virtual_plc_config.h"

namespace vc::device {

/**
 * @class VirtualPlcDevice
 * @brief A PlcDevice that records what would have been written instead of writing it.
 *
 * Exists so the product can be demonstrated, and the signal contract exercised, with no PLC
 * on the network. Writes land in digitalWrites/wordWrites, where a caller can read back
 * exactly what the runtime asked the PLC to do.
 *
 * **Tag names are validated, not merely accepted.** A write to a tag that is not a valid MC
 * address is rejected the way a real PLC rejects it. A virtual device that accepted anything
 * would make a signal-mapping mistake invisible until the day it reaches real hardware —
 * which is both the most expensive place to find one and the exact thing this device exists
 * to avoid. The `M`/`D` convention is Mitsubishi's, because Mitsubishi MC is the only PLC
 * this product speaks and the tags in a project file are written for it.
 */
class VirtualPlcDevice : public PlcDevice, public IPlcTagProvider, public IPlcIoWriter {
public:
    explicit VirtualPlcDevice(const QString &id, const QString &name,
                              QObject *parent = nullptr);

    /// Reports Connected. There is nothing to reach.
    bool deviceConnect() override;
    /// Reports Disconnected.
    bool deviceDisconnect() override;
    /// True while the connection status is Connected.
    bool isDeviceConnected() const override;

    void deviceTerminate() override {}
    PlcType plcType() const override { return PlcType::VirtualPlc; }
    /// Always false: this device has no request queue to push onto.
    bool pushRequest(IRequest *) override { return false; }

    /// Takes ownership of `cfg`, copies it into the owned config, and re-publishes that.
    ///
    /// Overridden rather than inherited on purpose: IDevice::setDeviceConfig() stores a
    /// **non-owning** pointer, so handing the base a heap config would leave it pointing at
    /// an object nobody owns while this device's own member goes unused.
    void setDeviceConfig(IDeviceCfg *cfg) override;

    /// Returns `M0 … M<digitalTagCount-1>`.
    ///
    /// Without this the signal-map editor cannot bind anything to the device: it asks a PLC
    /// for its tag names and clears both lists when the device does not answer, so a task
    /// using a virtual PLC could never be configured.
    QStringList availableDigitalIoNames() const override;
    /// Returns `D0 … D<wordTagCount-1>`; see availableDigitalIoNames().
    QStringList availableWordIoNames() const override;

    /// Records `value` under `tag` if it is a valid M-address; rejects it otherwise.
    bool writeDigitalIoByName(const QString &tag, bool value) override;
    /// Records `value` under `tag` if it is a valid D-address; rejects it otherwise.
    bool writeWordIoByName(const QString &tag, qint16 value) override;

    QMap<QString, bool> digitalWrites;    ///< Every accepted bit write, by tag.
    QMap<QString, qint16> wordWrites;     ///< Every accepted word write, by tag.

private:
    VirtualPlcCfg m_config;
};

} // namespace vc::device

#endif // VIRTUAL_PLC_DEVICE_H
