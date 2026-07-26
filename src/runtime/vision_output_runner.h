#ifndef VISION_OUTPUT_RUNNER_H
#define VISION_OUTPUT_RUNNER_H

#include "runtime/device_runner.h"
#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"

#include <QMetaObject>


/// Runner for the VisionOutput device family: thread lifecycle, signal wiring,
/// and per-device runners (see VisionOutputRunner).
namespace vc::runtime {

/// Per-device thread controller for vc::device::VisionOutputDevice: exposes
/// thread-safe request*() entry points that queue work onto the device's own
/// worker thread and forwards its connection-status/error signals back to the
/// GUI thread.
class VisionOutputRunner : public DeviceRunner<vc::device::VisionOutputDevice> {
    Q_OBJECT

public:
    /// Wraps `vision_output`; the device is not attached to its worker thread
    /// until attach() (inherited from DeviceRunner) is called.
    explicit VisionOutputRunner(vc::device::VisionOutputDevice *vision_output,
                            QObject *parent = nullptr)
        : DeviceRunner(vision_output, parent) {}

    // ── Commission actions (safe from any thread) ─────────────────────────────
    /// Requests a connect via sig_connect(), queued onto the device thread.
    /// No-op while a previous request is still in flight (m_busy).
    void requestConnect()    { if (!m_busy) { m_busy = true; emit sig_connect();    } }
    /// Requests a disconnect via sig_disconnect(), queued onto the device thread.
    /// No-op while a previous request is still in flight (m_busy).
    void requestDisconnect() { if (!m_busy) { m_busy = true; emit sig_disconnect(); } }
    /// Builds a VisionOutputRequest from `positions` and pushes it to the device
    /// on the device's own thread via QMetaObject::invokeMethod, then emits
    /// resultRequestFinished() with the outcome.
    /// @param positions vision result positions to send to the output device
    void requestSendResult(const QVector<vc::device::VisionOutputPosition> &positions)
    {
        const QVector<vc::device::VisionOutputPosition> payload = positions;
        QMetaObject::invokeMethod(m_device, [this, payload]() {
            vc::device::VisionOutputRequest request(payload);
            const bool ok = m_device->pushRequest(&request);
            emit resultRequestFinished(
                ok,
                ok ? QStringLiteral("Vision output result sent.")
                   : QStringLiteral("Vision output result send failed."));
        }, Qt::QueuedConnection);
    }

signals:
    /// Emitted after requestSendResult() completes, reporting success/failure
    /// and a human-readable message.
    void resultRequestFinished(bool ok, QString message);

    // ── Internal queued triggers ──────────────────────────────────────────────
    /// Internal trigger queued to the device thread to call deviceConnect().
    void sig_connect();
    /// Internal trigger queued to the device thread to call deviceDisconnect().
    void sig_disconnect();

protected:
    /// Connects sig_connect()/sig_disconnect() to the device's connect/disconnect
    /// slots and the device's status/error signals back to this runner's
    /// handlers, all via Qt::QueuedConnection.
    void wireSignals() override {
        using Plc = vc::device::VisionOutputDevice;
        using Run = VisionOutputRunner;

        connect(this,     &Run::sig_connect,
                m_device, &Plc::deviceConnect,    Qt::QueuedConnection);
        connect(this,     &Run::sig_disconnect,
                m_device, &Plc::deviceDisconnect, Qt::QueuedConnection);

        connect(m_device, &Plc::connectStatusChanged,
                this,     &Run::onConnectStatusChanged, Qt::QueuedConnection);
        connect(m_device, &Plc::connectionFailed,
                this,     &Run::onConnectionFailed,     Qt::QueuedConnection);
    }

    /// Disconnects all signal wiring set up in wireSignals() between this
    /// runner and the device.
    void unwireSignals() override {
        disconnect(this,     nullptr, m_device, nullptr);
        disconnect(m_device, nullptr, this,     nullptr);
    }

private slots:
    /// Clears the busy flag and forwards the device's new connect status via
    /// connectStatusChanged().
    void onConnectStatusChanged(vc::device::ConnectStatus status) {
        m_busy = false;
        emit connectStatusChanged(status);
    }

    /// Clears the busy flag and forwards the connection failure via
    /// errorOccurred().
    void onConnectionFailed(const QString &msg) {
        m_busy = false;
        emit errorOccurred(msg);
    }

private:
    bool m_busy{false};   ///< True while a requestConnect()/requestDisconnect() call is in flight.
};

} // namespace vc::runtime


#endif // VISION_OUTPUT_RUNNER_H
