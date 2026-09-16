#ifndef PLC_RUNNER_H
#define PLC_RUNNER_H

/**
 * @file plc_runner.h
 * @brief PlcRunner — family-level thread runner for PLC devices, mediating GUI-thread
 *        commission requests and runtime polling updates across the PLC device's worker thread.
 */

#include "runtime/device_runner.h"
#include "device/device_capabilities.h"
#include "device/plc/plc_device.h"

#include <QMetaObject>

namespace vc::runtime {

/**
 * @class PlcRunner
 * @brief Family-level runner for the PLC device family; holds a PlcDevice* (the abstract base)
 *        so the runner stays sub-type-agnostic.
 *
 * Vendor-specific consumers reach the concrete device via
 * `qobject_cast<McProtocolDevice *>(runner->typedDevice())` and connect to the device's vendor
 * signals directly (cross-thread queued). Mirrors CameraRunner / VisionOutputRunner: one flat
 * runner per family, no per-vendor subclass.
 *
 * @note Commission mode: requestConnect() / requestDisconnect() from the GUI thread are queued
 *       to the PLC thread; connection status comes back via connectStatusChanged().
 * @note Runtime mode: the concrete PLC device runs its own polling loop on its thread; the task
 *       runtime thread listens to pollingUpdate() forwarded here.
 */
class PlcRunner : public DeviceRunner<vc::device::PlcDevice> {
    Q_OBJECT

public:
    /// Constructs the runner wrapping `plc`, forwarding to the DeviceRunner base.
    explicit PlcRunner(vc::device::PlcDevice *plc,
                       QObject *parent = nullptr)
        : DeviceRunner(plc, parent) {}

    // ── Commission actions (safe from any thread) ─────────────────────────────
    /// Requests a connection on the PLC thread (queued via sig_connect); ignored while a
    /// connect/disconnect request is already in flight.
    void requestConnect()    override { if (!m_busy) { m_busy = true; emit sig_connect();    } }
    /// Requests a disconnection on the PLC thread (queued via sig_disconnect); ignored while a
    /// connect/disconnect request is already in flight.
    void requestDisconnect() override { if (!m_busy) { m_busy = true; emit sig_disconnect(); } }
    /**
     * @brief Queues a digital I/O write of `value` to `tag` on the PLC thread.
     * @return the write's id; exactly one writeFinished() will carry it
     * @post writeFinished(id, ok, message) is emitted on this runner's thread when the write
     *       reaches a terminal state — including the cases where the device implements no writer
     *       at all, rejects the tag, or is torn down mid-flight. Never silence.
     * @note The id is allocated HERE and passed down, so a caller has it before the write has
     *       been queued, let alone attempted. Callers that ignore it compile and behave exactly
     *       as before.
     */
    quint64 requestWriteDigitalIo(const QString &tag, bool value)
    {
        const quint64 id = ++m_nextWriteId;
        emit sig_writeDigitalIo(id, tag, value);
        return id;
    }
    /// Queues a word I/O write of `value` to `tag` on the PLC thread; see
    /// requestWriteDigitalIo() for the completion contract.
    quint64 requestWriteWordIo(const QString &tag, qint16 value)
    {
        const quint64 id = ++m_nextWriteId;
        emit sig_writeWordIo(id, tag, value);
        return id;
    }

    // ── Simulated-input capability ────────────────────────────────────────────
    /**
     * @brief True when THIS PLC device can have its input values driven by software.
     *
     * Asked of the device, not answered by the family — the same discipline as
     * supportsResultOutput() below, and for the same reason: the PLC family is mixed. Only the
     * hardware-free device implements vc::device::IPlcInputSimulator, and it must stay that way.
     * A real PLC whose inputs the vision software could forge would be lying about the plant.
     *
     * A UI offering the poke controls asks this rather than testing for a sub-type, so the panel
     * cannot be shown for a device that would silently ignore it.
     */
    bool supportsInputSimulation() const
    {
        return dynamic_cast<const vc::device::IPlcInputSimulator *>(m_device) != nullptr;
    }

    /**
     * @brief Queues an input-value injection onto the PLC thread.
     * @param[in] tag   the input tag to drive.
     * @param[in] value the value to drive it to.
     * @note Safe from the GUI thread; that is the whole point. The device owns thread-affine
     *       state and injection emits a signal, so it must not be called across threads
     *       directly — the trap ModbusDeviceWidget already paid for with a direct
     *       IPlcIoWriter call.
     * @post On refusal the reason reaches the operator through errorOccurred().
     */
    void requestInjectInputValue(const QString &tag, const QVariant &value)
    {
        emit sig_injectInputValue(tag, value);
    }

    // ── Result output capability ──────────────────────────────────────────────
    /**
     * @brief True when THIS PLC device also implements vc::device::IResultOutputDevice.
     *
     * @warning Asked of the device, not answered by the family. Unlike VisionOutputRunner — where every
     * device in the family outputs results — the PLC family is mixed: the Modbus client and
     * server publish results into their register map, the MC-protocol device does not. Answering
     * a flat `true` here would let `LocalizationRuntimeController` accept an MC PLC for the
     * `vision_output` role and then hang the first cycle waiting for a send that never happens,
     * which is precisely the failure IDeviceRunner::supportsResultOutput() warns about.
     *
     * Without this override the opposite failure occurred, and it is what a Modbus server bound
     * to both roles hit: the device implemented `IResultOutputDevice`, the task passed its runner
     * through correctly, and setup still refused it with "Device bound to vision_output cannot
     * output results" — because the capability was declared on the device and only ever read off
     * the runner.
     */
    bool supportsResultOutput() const override
    {
        return dynamic_cast<const vc::device::IResultOutputDevice *>(m_device) != nullptr;
    }

    /**
     * @brief Hands `positions` to the PLC device on its own thread, when it can output results.
     * @param[in] positions vision result positions to publish.
     * @post resultRequestFinished() is emitted exactly once — from the device thread on success
     *       or failure, or synchronously when the device cannot output results at all.
     * @note Mirrors VisionOutputRunner::requestSendResult(): the runner only marshals threads and
     *       never builds a request, which is what lets two unrelated families fill one role.
     */
    void requestSendResult(const QVector<vc::device::VisionOutputPosition> &positions) override
    {
        if (dynamic_cast<vc::device::IResultOutputDevice *>(m_device) == nullptr) {
            // Answers rather than staying silent: the controller waits on
            // resultRequestFinished(), so a dropped request stalls the cycle instead of failing
            // it. Reachable only if a device changes capability after setup validated it.
            emit resultRequestFinished(
                false, QStringLiteral("PLC device cannot output vision results."));
            return;
        }

        const QVector<vc::device::VisionOutputPosition> payload = positions;
        QMetaObject::invokeMethod(m_device, [this, payload]() {
            QString message;
            // Re-resolved ON the device thread rather than captured: the cast is cheap and a
            // pointer captured here would be dereferenced on another thread from where it was
            // checked.
            auto *output = dynamic_cast<vc::device::IResultOutputDevice *>(m_device);
            const bool ok = output && output->sendVisionResult(payload, &message);
            if (!output) {
                message = QStringLiteral("PLC device cannot output vision results.");
            }
            emit resultRequestFinished(ok, message);
        }, Qt::QueuedConnection);
    }

signals:
    // ── Family-level signals forwarded from PLC thread ────────────────────────
    /**
     * @brief Emitted with the latest polled PLC value map, forwarded from the device's own
     *        pollingUpdate() signal.
     * @param[in] map latest polled tag/value snapshot.
     */
    void pollingUpdate(std::shared_ptr<vc::device::PlcValueMap> map);
    /**
     * @brief Emitted with a set of updated tag values, forwarded from the device's
     *        valueChanged().
     * @param[in] values tag/value pairs that changed since the previous poll.
     */
    void valueChanged(QMap<QString, QVariant> values);

    // ── Internal queued triggers ──────────────────────────────────────────────
    /// Internal queued trigger (Qt::QueuedConnection) that invokes PlcDevice::deviceConnect()
    /// on the PLC thread.
    void sig_connect();
    /// Internal queued trigger that invokes PlcDevice::deviceDisconnect() on the PLC thread.
    void sig_disconnect();
    /// Internal queued trigger that writes a digital I/O value on the PLC thread.
    void sig_writeDigitalIo(quint64 id, QString tag, bool value);
    /// Internal queued trigger that writes a word I/O value on the PLC thread.
    void sig_writeWordIo(quint64 id, QString tag, qint16 value);

    /**
     * @brief Terminal outcome of a write requested through requestWriteDigitalIo()/
     *        requestWriteWordIo(), delivered on this runner's thread.
     * @param[out] id      the id the request returned
     * @param[out] ok      whether the value actually reached the PLC
     * @param[out] message on failure, why
     *
     * @note **A narrow signal, not a DeviceCommand kind — decided in Phase 9 / E3 and recorded
     *       there.** `DeviceCommandQueue` is a one-at-a-time FIFO with `RejectWhenBusy` /
     *       `QueueFull` policies, and PLC handshake writes arrive in bursts —
     *       `publishInitialReadyOutputs()` sends ten. Routing them through it would reintroduce
     *       precisely the dropped-write defect the Modbus client's deferred-write parking exists
     *       to fix.
     */
    void writeFinished(quint64 id, bool ok, QString message);
    /// Internal queued trigger that drives a simulated input value on the PLC thread.
    void sig_injectInputValue(QString tag, QVariant value);

protected:
    /// Connects this runner's queued triggers to the concrete PlcDevice, and the device's
    /// status/error/polling/value signals back to this runner, once attached on the PLC thread.
    /// Digital/word write triggers are routed through IPlcIoWriter and emit errorOccurred() on
    /// failure (missing writer or write rejected by the device).
    void wireSignals() override {
        using Plc = vc::device::PlcDevice;
        using Run = PlcRunner;

        connect(this,     &Run::sig_connect,
                m_device, &Plc::deviceConnect,    Qt::QueuedConnection);
        connect(this,     &Run::sig_disconnect,
                m_device, &Plc::deviceDisconnect, Qt::QueuedConnection);
        // The tracked forms carry the id down and resolve it from every terminal path, including
        // the ones a bool return could never reach: a queued MC write abandoned by a disconnect,
        // a Modbus write parked and then dropped. The device resolves; the forward below turns
        // that into writeFinished() on this thread.
        connect(this, &Run::sig_writeDigitalIo, m_device,
                [this](quint64 id, const QString &tag, bool value) {
            m_device->writeDigitalIoTracked(id, tag, value);
        }, Qt::QueuedConnection);
        connect(this, &Run::sig_writeWordIo, m_device,
                [this](quint64 id, const QString &tag, qint16 value) {
            m_device->writeWordIoTracked(id, tag, value);
        }, Qt::QueuedConnection);

        // errorOccurred() is PRESERVED, not replaced: the device panels and the recovery policy
        // both listen to it, and dropping it would make a failed write silent for every consumer
        // that has not been taught about writeFinished() yet.
        connect(m_device, &Plc::ioWriteFinished, this,
                [this](quint64 id, bool ok, const QString &message) {
            if (!ok) {
                emit errorOccurred(QStringLiteral("PLC write failed: %1").arg(message));
            }
            emit writeFinished(id, ok, message);
        }, Qt::QueuedConnection);

        connect(this, &Run::sig_injectInputValue, m_device,
                [this](const QString &tag, const QVariant &value) {
            // Re-resolved on the device thread rather than captured, like requestSendResult():
            // a pointer checked on one thread and dereferenced on another proves nothing.
            auto *simulator = dynamic_cast<vc::device::IPlcInputSimulator *>(m_device);
            if (!simulator) {
                emit errorOccurred(
                    QStringLiteral("This PLC device cannot have its inputs driven: %1").arg(tag));
                return;
            }
            QString reason;
            if (!simulator->injectInputValue(tag, value, &reason)) {
                emit errorOccurred(reason);
            }
        }, Qt::QueuedConnection);

        connect(m_device, &Plc::connectStatusChanged,
                this,     &Run::onConnectStatusChanged, Qt::QueuedConnection);
        connect(m_device, &Plc::connectionFailed,
                this,     &Run::onConnectionFailed,     Qt::QueuedConnection);
        connect(m_device, &Plc::pollingUpdate,
                this,     &Run::pollingUpdate,          Qt::QueuedConnection);
        connect(m_device, &Plc::valueChanged,
                this,     &Run::valueChanged,           Qt::QueuedConnection);
        connect(m_device, &Plc::errorOccurred,
                this,     &Run::errorOccurred,          Qt::QueuedConnection);
    }

    /// Disconnects all signal connections between this runner and the device (used before
    /// detaching the device to another thread).
    void unwireSignals() override {
        disconnect(this,     nullptr, m_device, nullptr);
        disconnect(m_device, nullptr, this,     nullptr);
    }

private slots:
    /// Clears the busy flag and forwards the device's new connection status via
    /// connectStatusChanged().
    void onConnectStatusChanged(vc::device::ConnectStatus status) {
        m_busy = false;
        emit connectStatusChanged(status);
    }

    /// Clears the busy flag and forwards the device's connection failure via errorOccurred().
    void onConnectionFailed(const QString &msg) {
        m_busy = false;
        emit errorOccurred(msg);
    }

private:
    bool m_busy{false};  ///< True while a connect/disconnect request is in flight (guards against duplicate requests).
    /// Next write id. Allocated on the REQUESTING thread and passed down, so the caller holds it
    /// before the write is queued. Starts at 0 and pre-increments, so no write is ever id 0 —
    /// which the device layer reserves for "untracked".
    quint64 m_nextWriteId{0};
};

} // namespace vc::runtime

#endif // PLC_RUNNER_H
