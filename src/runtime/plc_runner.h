#ifndef PLC_RUNNER_H
#define PLC_RUNNER_H

#include "runtime/device_runner.h"
#include "device/device_capabilities.h"
#include "device/plc/plc_device.h"

namespace vc::runtime {

/// Family-level runner for the PLC device family; holds a PlcDevice* (the abstract base) so the
/// runner stays sub-type-agnostic. Vendor-specific consumers reach the concrete device via
/// `qobject_cast<McProtocolDevice *>(runner->typedDevice())` and connect to the device's vendor
/// signals directly (cross-thread queued). Mirrors CameraRunner / VisionOutputRunner: one flat
/// runner per family, no per-vendor subclass.
/// @note Commission mode: requestConnect() / requestDisconnect() from the GUI thread are queued
/// to the PLC thread; connection status comes back via connectStatusChanged().
/// @note Runtime mode: the concrete PLC device runs its own polling loop on its thread; the task
/// runtime thread listens to pollingUpdate() forwarded here.
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
    void requestConnect()    { if (!m_busy) { m_busy = true; emit sig_connect();    } }
    /// Requests a disconnection on the PLC thread (queued via sig_disconnect); ignored while a
    /// connect/disconnect request is already in flight.
    void requestDisconnect() { if (!m_busy) { m_busy = true; emit sig_disconnect(); } }
    /// Queues a digital I/O write of `value` to `tag` on the PLC thread.
    void requestWriteDigitalIo(const QString &tag, bool value)
    {
        emit sig_writeDigitalIo(tag, value);
    }
    /// Queues a word I/O write of `value` to `tag` on the PLC thread.
    void requestWriteWordIo(const QString &tag, qint16 value)
    {
        emit sig_writeWordIo(tag, value);
    }

signals:
    // ── Family-level signals forwarded from PLC thread ────────────────────────
    /// Emitted with the latest polled PLC value map, forwarded from the device's own
    /// pollingUpdate() signal.
    void pollingUpdate(std::shared_ptr<vc::device::PlcValueMap> map);
    /// Emitted with a set of updated tag values, forwarded from the device's valueChanged().
    void valueChanged(QMap<QString, QVariant> values);

    // ── Internal queued triggers ──────────────────────────────────────────────
    /// Internal queued trigger (Qt::QueuedConnection) that invokes PlcDevice::deviceConnect()
    /// on the PLC thread.
    void sig_connect();
    /// Internal queued trigger that invokes PlcDevice::deviceDisconnect() on the PLC thread.
    void sig_disconnect();
    /// Internal queued trigger that writes a digital I/O value on the PLC thread.
    void sig_writeDigitalIo(QString tag, bool value);
    /// Internal queued trigger that writes a word I/O value on the PLC thread.
    void sig_writeWordIo(QString tag, qint16 value);

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
        connect(this, &Run::sig_writeDigitalIo, m_device,
                [this](const QString &tag, bool value) {
            auto *writer = dynamic_cast<vc::device::IPlcIoWriter *>(m_device);
            if (!writer || !writer->writeDigitalIoByName(tag, value)) {
                emit errorOccurred(QStringLiteral("PLC digital write failed: %1").arg(tag));
            }
        }, Qt::QueuedConnection);
        connect(this, &Run::sig_writeWordIo, m_device,
                [this](const QString &tag, qint16 value) {
            auto *writer = dynamic_cast<vc::device::IPlcIoWriter *>(m_device);
            if (!writer || !writer->writeWordIoByName(tag, value)) {
                emit errorOccurred(QStringLiteral("PLC word write failed: %1").arg(tag));
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
};

} // namespace vc::runtime

#endif // PLC_RUNNER_H
