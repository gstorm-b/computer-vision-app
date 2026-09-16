#ifndef VIRTUAL_PLC_DEVICE_H
#define VIRTUAL_PLC_DEVICE_H

/**
 * @file virtual_plc_device.h
 * @brief VirtualPlcDevice — a PLC that needs no hardware.
 */

#include <QMap>
#include <QString>
#include <QVariant>

#include <utility>

#include "device/device_capabilities.h"
#include "device/plc/plc_device.h"
#include "device/virtual/virtual_plc_config.h"

namespace vc::device {

/**
 * @class VirtualPlcValueMap
 * @brief The driven-input snapshot a VirtualPlcDevice publishes on pollingUpdate().
 *
 * Exists so a hardware-free runtime can answer *"what does the PLC currently hold for this
 * tag?"* — the question C6 makes `setup()` ask. Without it the only PLC that needs no hardware
 * is also the only one that cannot be asked, which would put the startup-selection defect
 * (backlog item 58) permanently out of reach of an automated test.
 *
 * Holds a plain tag→value copy rather than an address space: the virtual device has no register
 * areas to model, and inventing some would be simulating the wrong thing.
 */
class VirtualPlcValueMap : public PlcValueMap {
public:
    VirtualPlcValueMap() = default;
    explicit VirtualPlcValueMap(QMap<QString, QVariant> values) : m_values(std::move(values)) {}

    std::shared_ptr<PlcValueMap> clone() const override {
        return std::make_shared<VirtualPlcValueMap>(*this);
    }

    /// Answers only for tags that have actually been driven; an undriven tag is absent, not 0.
    bool valueForTag(const QString &tag, QVariant *value) const override {
        const auto it = m_values.constFind(tag.trimmed());
        if (it == m_values.constEnd()) {
            return false;
        }
        if (value) {
            *value = it.value();
        }
        return true;
    }

private:
    QMap<QString, QVariant> m_values;
};

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
class VirtualPlcDevice : public PlcDevice,
                         public IPlcTagProvider,
                         public IPlcIoWriter,
                         public IPlcInputSimulator {
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

    /**
     * @brief Drives an input tag as if the PLC program had changed it, and publishes it.
     *
     * This is what makes a hardware-free project able to run at all. Every runtime state past
     * Ready begins with the PLC changing an input — `bExecuteTrigger`, `bErrorReset`,
     * `nActiveCamera`, `nActivePatternGroup` — so without this, a project with no PLC on the
     * network could be built, opened, configured and taken to Ready, and then nothing.
     *
     * `M` tags are stored as bool and `D` tags as qint16, matching what a real PLC device
     * publishes for a bit area and a register area. That distinction is load-bearing:
     * LocalizationRuntimeController treats a non-numeric value on `nActiveCamera` as *"this
     * number signal is mapped to a coil"*, so coercing by prefix here is what lets a genuine
     * mapping mistake still be reported as one.
     *
     * @param[in]  tag   `M<address>` or `D<address>`; anything else is refused.
     * @param[out] error human-readable reason on refusal; may be null.
     * @return true if the tag was accepted, stored and published.
     * @note Must run on the device's own thread — it emits valueChanged(). PlcRunner::
     *       requestInjectInputValue() is the way in from the GUI thread.
     */
    bool injectInputValue(const QString &tag, const QVariant &value,
                          QString *error = nullptr) override;
    /// Returns every driven input tag and its value; see IPlcInputSimulator.
    QMap<QString, QVariant> simulatedInputs() const override { return m_inputValues; }

    /**
     * @brief Queues a connection-status change, as if the link had changed on its own.
     *
     * Queued rather than immediate so the change is delivered through the event loop, the way
     * a real device's status arrives from its own thread — a direct setConnectionStatus() from
     * a caller's thread would emit connectStatusChanged() off the device's own thread and let
     * that caller observe an ordering the hardware can never produce.
     *
     * Same contract, and the same wording, as VirtualCameraDevice::forceConnectionStatus().
     * It exists here because losing the PLC is a runtime case with its own fault code
     * (LocalizationFaultCode::PlcLost) and there was no hardware-free way to produce one.
     */
    void forceConnectionStatus(ConnectStatus status);

    QMap<QString, bool> digitalWrites;    ///< Every accepted bit write, by tag.
    QMap<QString, qint16> wordWrites;     ///< Every accepted word write, by tag.

    /**
     * @brief Tags whose writes this device refuses, and how many times each still refuses.
     *
     * A hardware-free way to produce the one failure the handshake write policy exists for:
     * the link is up, the device answers, and the write is refused anyway
     * (Phase 9 / E4, LocalizationFaultCode::PlcWriteFailed). Losing the CONNECTION already had
     * forceConnectionStatus(); losing a WRITE on a healthy link had nothing.
     *
     * A count rather than a flag, so a test can say "fail twice, then succeed" and exercise the
     * retry budget from both sides. `-1` means refuse forever. Decremented on each refusal.
     */
    QMap<QString, int> failWritesForTag;

private:
    VirtualPlcCfg m_config;
    /// Driven input values, keyed by tag.
    ///
    /// **Deliberately not the same store as digitalWrites/wordWrites.** A value the runtime
    /// wrote must never read back as an input. Sharing one store would make the handshake
    /// self-completing — the runtime publishes `bMatchingFinished`, reads it back, and the cycle
    /// appears to work — and it would hide the mapping mistake where two logical signals are
    /// bound to the same tag. Real hardware keeps them apart because the plant owns the inputs,
    /// and a simulation that does otherwise stops proving anything.
    QMap<QString, QVariant> m_inputValues;
};

} // namespace vc::device

#endif // VIRTUAL_PLC_DEVICE_H
