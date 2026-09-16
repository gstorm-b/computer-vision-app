#ifndef VISION_OUTPUT_RUNNER_H
#define VISION_OUTPUT_RUNNER_H

/**
 * @file vision_output_runner.h
 * @brief VisionOutputRunner — per-device thread runner for the VisionOutput device family:
 *        thread lifecycle, signal wiring, and request forwarding to the device's worker thread.
 */

#include "runtime/device_runner.h"
#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"

#include <QMetaObject>

namespace vc::runtime {

/**
 * @class VisionOutputRunner
 * @brief Per-device thread controller for vc::device::VisionOutputDevice: exposes thread-safe
 *        request*() entry points that queue work onto the device's own worker thread and
 *        forwards its connection-status/error signals back to the GUI thread.
 */
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
    void requestConnect()    override { if (!m_busy) { m_busy = true; emit sig_connect();    } }
    /// Requests a disconnect via sig_disconnect(), queued onto the device thread.
    /// No-op while a previous request is still in flight (m_busy).
    void requestDisconnect() override { if (!m_busy) { m_busy = true; emit sig_disconnect(); } }

    // ── Result output capability ──────────────────────────────────────────────
    /// Every device in this family implements IResultOutputDevice, so the whole family
    /// can carry the `vision_output` role.
    bool supportsResultOutput() const override { return true; }

    /**
     * @brief Hands `positions` to the device on the device's own thread via
     *        QMetaObject::invokeMethod.
     * @param[in] positions vision result positions to send to the output device
     * @post resultRequestFinished() is emitted asynchronously, from the device's worker thread,
     *       once the send completes.
     * @note The request-building that used to sit in this lambda moved into
     *       VisionOutputDevice::sendVisionResult(); the runner now only marshals threads, which
     *       is what lets a PLC-family runner implement the same capability without ever
     *       constructing a VisionOutputRequest.
     */
    void requestSendResult(const QVector<vc::device::VisionOutputPosition> &positions) override
    {
        const QVector<vc::device::VisionOutputPosition> payload = positions;
        QMetaObject::invokeMethod(m_device, [this, payload]() {
            QString message;
            const bool ok = m_device->sendVisionResult(payload, &message);
            emit resultRequestFinished(ok, message);
        }, Qt::QueuedConnection);
    }

signals:
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

        // PlcRunner and CameraRunner both forward this; only this family did not, and nothing
        // said so because the runner families are wired independently.
        //
        // DORMANT, and its presence must not be read as coverage: **no device in src/device/
        // emits IDevice::errorOccurred** (searched 2026-09-08 — backlog item 51). The forward is
        // correct and costs one line, so it is made now rather than left as a hole for whoever
        // lands item 51's other half to discover. Until then a vision-output device has no way
        // to report an error that is not a connection failure.
        connect(m_device, &Plc::errorOccurred,
                this,     &Run::errorOccurred,          Qt::QueuedConnection);
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
