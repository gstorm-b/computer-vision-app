#ifndef PLC_DEVICE_H
#define PLC_DEVICE_H

/**
 * @file plc_device.h
 * @brief Abstract PLC device family: sub-type dispatch, the family-level config and
 *        device base classes, and the shared value-map interface.
 */

#include <memory>
#include <QMap>
#include <QString>
#include <QVariant>
#include "device/idevice.h"
// IPlcIoWriter: the tracked-write defaults below perform the write through it.
#include "device/device_capabilities.h"

/// JSON-persisted PlcType key for the Mitsubishi MC vendor sub-type.
#define PLC_TYPE_MITSUBISHI_MC   "MitsubishiMc"
/// Modbus TCP, we are the master: we dial a slave and poll it.
#define PLC_TYPE_MODBUS_TCP_CLIENT  "ModbusTcpClient"
/// Modbus TCP, we are the slave: we own the register space and a PLC master reads/writes it.
#define PLC_TYPE_MODBUS_TCP_SERVER  "ModbusTcpServer"
/// Hardware-free PLC. Tokens are family-local — the camera family has its own `"Virtual"`
/// under a different JSON key — so there is no collision. This token is written into
/// customer project files and can never be changed once one has been saved.
#define PLC_TYPE_VIRTUAL         "Virtual"

namespace vc::device {

/**
 * @enum PlcType
 * @brief Family-level sub-type dispatch handle for the PLC family. Concrete
 *        vendors register a value here; DeviceFactory::createPlcDevice() switches
 *        on this enum to pick the concrete subclass.
 */
enum PlcType {
    PlcTypeNone,
    MitsubishiMc,
    ModbusTcpClient,   ///< Modbus TCP master: we poll a slave.
    ModbusTcpServer,   ///< Modbus TCP slave: a master polls us.
    /// No hardware: writes are recorded rather than sent. Named `VirtualPlc` because these
    /// enums are unscoped and every enumerator lands in `vc::device`.
    VirtualPlc,
};

/// @return the JSON-persisted string key for `t` (empty string for PlcTypeNone
/// or any unhandled value).
[[maybe_unused]] static QString PlcTypeToString(PlcType t) {
    switch (t) {
    case PlcType::MitsubishiMc:     return PLC_TYPE_MITSUBISHI_MC;
    case PlcType::ModbusTcpClient:  return PLC_TYPE_MODBUS_TCP_CLIENT;
    case PlcType::ModbusTcpServer:  return PLC_TYPE_MODBUS_TCP_SERVER;
    case PlcType::VirtualPlc:       return PLC_TYPE_VIRTUAL;
    case PlcType::PlcTypeNone:
        return "";
    }
    return "";
}

/// @return the PlcType matching the JSON-persisted string key `t`, or
/// PlcTypeNone if it does not match any known vendor key.
[[maybe_unused]] static PlcType PlcTypeFromString(QString t) {
    if (t == PLC_TYPE_MITSUBISHI_MC)    return PlcType::MitsubishiMc;
    if (t == PLC_TYPE_MODBUS_TCP_CLIENT) return PlcType::ModbusTcpClient;
    if (t == PLC_TYPE_MODBUS_TCP_SERVER) return PlcType::ModbusTcpServer;
    if (t == PLC_TYPE_VIRTUAL)          return PlcType::VirtualPlc;
    return PlcType::PlcTypeNone;
}

/**
 * @class PlcCfg
 * @brief Abstract config for the PLC family. Carries only the family-level
 *        dispatch field; concrete configs (McProtocolConfig for Mitsubishi MC,
 *        future OmronFinsConfig, …) inherit and add their protocol-specific
 *        Q_PROPERTYs.
 */
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

/**
 * @class PlcValueMap
 * @brief Abstract, vendor-agnostic holder for a PLC's polled register values.
 *        Concrete vendors provide a concrete value map (e.g. holding device-map
 *        tables) and expose it via clone() for snapshotting.
 */
class PlcValueMap {
public:
    virtual ~PlcValueMap() = default;
    /// @return a polymorphic deep copy of this value map.
    virtual std::shared_ptr<PlcValueMap> clone() const = 0;

    /**
     * @brief Reads the value this map currently holds for `tag`, in the family's own spelling.
     * @param[in]  tag   signal-map tag name, e.g. `M0100` (MC) or `HR00101` (Modbus).
     * @param[out] value receives the value on success; left untouched otherwise. May be null,
     *                   which turns this into a pure "is there a value?" query.
     * @return false when `tag` is not a name this family uses, or the map holds nothing for it.
     *
     * @note **"No value" and "the value is 0" are different answers, and every caller must be
     *       able to tell them apart.** A register that has never been polled holds nothing; a
     *       register the master has parked at 0 holds a value. Collapsing the two is exactly
     *       the defect backlog item 58 describes — a runtime that reads an absent register as
     *       0 selects camera 0, and 0 names no camera.
     *
     * Pure virtual so a new PLC family cannot quietly inherit a stub that answers "nothing" for
     * everything: that failure mode is invisible, because "nothing" is a legal answer.
     */
    virtual bool valueForTag(const QString &tag, QVariant *value) const = 0;
};

/**
 * @class PlcDevice
 * @brief Abstract base for the PLC family. Concrete vendors (McProtocolDevice for
 *        Mitsubishi MC, future OmronFinsDevice, SiemensS7Device, …) inherit from
 *        this base. The base only carries the family-level dispatch (plcType())
 *        and the family JSON header; vendor-specific surface lives entirely on the
 *        concrete subclass.
 */
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

    // ── Tracked writes (Phase 9 / E1-E2) ──────────────────────────────────────────────────
    //
    // IPlcIoWriter's bool return has never meant "written". MC queues and returns true;
    // ModbusTcpClientDevice parks a write that collided with an in-flight transaction and
    // returns true; both are correct and both leave the caller unable to tell a write that
    // reached the PLC from one that was dropped. That is why D3's retry policy had nothing to
    // retry ON.
    //
    // These add the missing half WITHOUT changing any existing signature: the bool methods keep
    // their meaning and their callers. A caller that needs to know the outcome uses the tracked
    // form and waits for ioWriteFinished() carrying the id it was handed.

    /**
     * @brief Writes a digital tag and reports the outcome exactly once via ioWriteFinished().
     * @param id    completion id, allocated by the CALLER; must not be 0
     * @param tag   family-spelled digital tag
     * @param value value to write
     * @post exactly one ioWriteFinished() carries `id`
     *
     * @note The id is **taken, not returned**, and that is a threading decision rather than a
     *       style one. PlcRunner hands its caller an id synchronously and only then queues the
     *       write onto the device thread; if the device minted its own id there, the runner would
     *       need a device-id → runner-id map written on one thread and read on another. One id
     *       space, owned by the caller, removes the map instead of guarding it.
     */
    virtual void writeDigitalIoTracked(quint64 id, const QString &tag, bool value);
    /// Word-write counterpart of writeDigitalIoTracked(); same contract.
    virtual void writeWordIoTracked(quint64 id, const QString &tag, qint16 value);

signals:
    /// Emitted after a polling cycle with a snapshot of the current register
    /// values.
    void pollingUpdate(std::shared_ptr<vc::device::PlcValueMap> value_map);
    /// Emitted when tracked signal values change, keyed by signal name.
    void valueChanged(QMap<QString, QVariant> values);
    /**
     * @brief Terminal outcome of a tracked write.
     * @param[out] id      the id writeDigitalIoTracked()/writeWordIoTracked() returned
     * @param[out] ok      whether the value actually reached the PLC
     * @param[out] message on failure, WHY — "the write did not happen" without a reason sends a
     *                     commissioning engineer nowhere
     * @note Exactly one per tracked submission, from every terminal path including the ones that
     *       abandon a queue. A write that vanishes is worse than one that fails: the caller waits
     *       forever.
     */
    void ioWriteFinished(quint64 id, bool ok, QString message);

protected:
    /// Hands out the next completion id. 0 is reserved for "not tracked".
    quint64 nextIoWriteId() { return m_next_io_write_id++; }

    /// Emits ioWriteFinished() for `id`, unless `id` is 0.
    void resolveIoWrite(quint64 id, bool ok, const QString &message) {
        if (id == 0) return;
        emit ioWriteFinished(id, ok, message);
    }

private:
    quint64 m_next_io_write_id{1};   ///< Next tracked-write id; 0 means untracked.
};

/// Default: perform the write through IPlcIoWriter and resolve immediately with what it reported.
///
/// Correct as-is for the families whose writes ARE synchronous — ModbusTcpServerDevice writes
/// into its own register space, VirtualPlcDevice into a map — so neither has to restate the
/// contract. The asynchronous families (MC, Modbus client) override.
///
/// A device that implements no writer at all resolves as **failed**, never silently: that is the
/// same rule the queue-abandon paths follow, for the same reason.
inline void PlcDevice::writeDigitalIoTracked(quint64 id, const QString &tag, bool value) {
    auto *writer = dynamic_cast<IPlcIoWriter *>(this);
    if (!writer) {
        resolveIoWrite(id, false,
                       QStringLiteral("This PLC device does not support digital IO writes."));
        return;
    }
    const bool ok = writer->writeDigitalIoByName(tag, value);
    resolveIoWrite(id, ok,
                   ok ? QStringLiteral("OK")
                      : QStringLiteral("The device rejected the write to \"%1\".").arg(tag));
}

inline void PlcDevice::writeWordIoTracked(quint64 id, const QString &tag, qint16 value) {
    auto *writer = dynamic_cast<IPlcIoWriter *>(this);
    if (!writer) {
        resolveIoWrite(id, false,
                       QStringLiteral("This PLC device does not support word IO writes."));
        return;
    }
    const bool ok = writer->writeWordIoByName(tag, value);
    resolveIoWrite(id, ok,
                   ok ? QStringLiteral("OK")
                      : QStringLiteral("The device rejected the write to \"%1\".").arg(tag));
}

} // namespace vc::device

#endif // PLC_DEVICE_H
