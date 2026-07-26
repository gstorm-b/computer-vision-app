#ifndef CAMERA_RUNNER_H
#define CAMERA_RUNNER_H

#include <QTimer>

#include "runtime/device_runner.h"
#include "runtime/device_command_queue.h"
#include "device/camera/camera_device.h"

namespace vc::runtime {

/// Per-camera thread runner (replaces the legacy CameraWorker that previously lived in
/// src/model); mediates between the GUI thread and the camera device's own HighPriority
/// worker thread.
///
/// Commission mode: widget code calls requestConnect() / requestSingleShot() etc. from the
/// GUI thread. These emit queued signals that the camera device processes on its own thread;
/// results arrive back via grabFinished(), parametersApplied(), etc. (also queued, so safe
/// for GUI slots).
///
/// Runtime mode: the camera stays on its own thread while the task runtime thread triggers
/// grabs and awaits grabFinished() via Qt::QueuedConnection; the runner remains the mediator
/// so no direct cross-thread calls are needed.
///
/// Camera requests route through the standard DeviceCommand model, with queue and timeout
/// behavior: Connect/Disconnect are rejected while another command is active, SingleShot is
/// queued FIFO while busy, and ApplyParams keeps only the latest pending request.
class CameraRunner : public DeviceRunner<vc::device::CameraDevice> {
    Q_OBJECT

public:
    /// Constructs the runner for `camera`, registering the queued-connection meta types
    /// (DeviceCommand, DeviceCommandResult, GrabResult) and wiring the single-shot
    /// m_activeCommandTimer to onActiveCommandTimedOut() for active-command timeout handling.
    /// @param camera the camera device this runner manages (not owned; must outlive the runner)
    /// @param parent optional QObject parent
    explicit CameraRunner(vc::device::CameraDevice *camera,
                          QObject *parent = nullptr)
        : DeviceRunner(camera, parent)
    {
        qRegisterMetaType<DeviceCommand>("vc::runtime::DeviceCommand");
        qRegisterMetaType<DeviceCommandResult>("vc::runtime::DeviceCommandResult");
        qRegisterMetaType<vc::device::GrabResult>("vc::device::GrabResult");
        m_activeCommandTimer.setSingleShot(true);
        connect(&m_activeCommandTimer, &QTimer::timeout,
                this, &CameraRunner::onActiveCommandTimedOut);
    }

    // ── Commission / runtime actions (safe from any thread) ──────────────────
    /// Submits a Connect command for this camera via submitCommand().
    void requestConnect()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::Connect,
                                            m_device->id()));
    }

    /// Submits a Disconnect command for this camera via submitCommand().
    void requestDisconnect()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::Disconnect,
                                            m_device->id()));
    }

    /// Submits a CameraSingleShot (grab) command for this camera via submitCommand(); queues
    /// FIFO behind any command already running.
    void requestSingleShot()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::CameraSingleShot,
                                            m_device->id()));
    }

    /// Submits a CameraApplyParams command for this camera via submitCommand(); replaces any
    /// already-pending CameraApplyParams command rather than queuing behind it.
    void requestApplyParams()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::CameraApplyParams,
                                            m_device->id()));
    }

    /// Validates and submits `command` for execution. Rejects it (via finishRejectedCommand())
    /// if its targetDeviceId doesn't match this camera's id, or if its kind isn't one of the
    /// supported camera commands (see isSupportedCommand()). Otherwise enqueues it under the
    /// policy from queuePolicyFor() and either runs it immediately (runCommand()) when the
    /// runner is idle, or queues it and logs the queued state via LOG_USER_INFO.
    /// @param command the command to submit
    /// @return the accepted/rejected result for this call; a queued command still returns
    /// "accepted" and completes later via commandFinished()
    DeviceCommandResult submitCommand(const DeviceCommand &command) override
    {
        if (command.targetDeviceId != m_device->id()) {
            DeviceCommandResult result = DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::InvalidTarget,
                QStringLiteral("Camera command target does not match this runner."));
            finishRejectedCommand(result);
            return result;
        }

        if (!isSupportedCommand(command.kind)) {
            DeviceCommandResult result = DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::UnsupportedCommand,
                QStringLiteral("Unsupported camera command."));
            finishRejectedCommand(result);
            return result;
        }

        const DeviceCommandQueuePolicy policy = queuePolicyFor(command.kind);
        const DeviceCommandEnqueueDecision decision =
            m_commandQueue.enqueue(command, policy, hasActiveCommand());
        if (!decision.accepted) {
            finishRejectedCommand(decision.rejection);
            return decision.rejection;
        }

        if (decision.shouldRunNow) {
            runCommand(command);
            return DeviceCommandResult::accepted(command);
        }

        const QString queueMessage = decision.replacedPending
                                         ? QStringLiteral("Camera command queued (replaced pending same-kind request).")
                                         : QStringLiteral("Camera command queued.");
        LOG_USER_INFO << "Camera command queued."
                      << "id=" << command.id
                      << "kind=" << deviceCommandKindToString(command.kind)
                      << "target=" << command.targetDeviceId
                      << "pending=" << decision.pendingCount;
        return DeviceCommandResult::accepted(command, queueMessage);
    }

signals:
    // ── Results forwarded from camera thread ──────────────────────────────────
    /// Re-emitted (GUI-thread side) whenever the camera device's grabFinished signal fires,
    /// carrying the grab outcome/frame data in `result`.
    void grabFinished(vc::device::GrabResult result);
    /// Re-emitted (GUI-thread side) whenever the camera device's parametersApplied signal
    /// fires; `ok` reports whether the parameter apply succeeded.
    void parametersApplied(bool ok);

    // ── Internal queued triggers (→ camera thread) ────────────────────────────
    /// Internal trigger, queued-connected (see wireSignals()) to CameraDevice::deviceConnect();
    /// emitted by runCommand() to start a connect attempt on the camera thread.
    void sig_connect();
    /// Internal trigger, queued-connected (see wireSignals()) to
    /// CameraDevice::deviceDisconnect(); emitted by runCommand() to start a disconnect on the
    /// camera thread.
    void sig_disconnect();
    /// Internal trigger, queued-connected (see wireSignals()) to CameraDevice::grabSingleShot();
    /// emitted by runCommand() to start a single-shot grab on the camera thread.
    void sig_singleShot();
    /// Internal trigger, queued-connected (see wireSignals()) to
    /// CameraDevice::applyParametersChange(); emitted by runCommand() to apply pending
    /// parameters on the camera thread.
    void sig_applyParams();

protected:
    /// Connects the internal trigger signals (sig_connect/sig_disconnect/sig_singleShot/
    /// sig_applyParams) to their matching CameraDevice slots, and the device's status/result/
    /// error signals back to this runner's handlers — all via Qt::QueuedConnection so calls
    /// cross safely between the GUI thread and the camera's worker thread.
    void wireSignals() override {
        using Cam = vc::device::CameraDevice;
        using Run = CameraRunner;

        connect(this,     &Run::sig_connect,
                m_device, &Cam::deviceConnect,         Qt::QueuedConnection);
        connect(this,     &Run::sig_disconnect,
                m_device, &Cam::deviceDisconnect,      Qt::QueuedConnection);
        connect(this,     &Run::sig_singleShot,
                m_device, &Cam::grabSingleShot,        Qt::QueuedConnection);
        connect(this,     &Run::sig_applyParams,
                m_device, &Cam::applyParametersChange, Qt::QueuedConnection);

        connect(m_device, &Cam::connectStatusChanged,
                this,     &Run::onConnectStatusChanged, Qt::QueuedConnection);
        connect(m_device, &Cam::connectionFailed,
                this,     &Run::onConnectionFailed,     Qt::QueuedConnection);
        connect(m_device, &Cam::grabFinished,
                this,     &Run::onGrabFinished,         Qt::QueuedConnection);
        connect(m_device, &Cam::parametersApplied,
                this,     &Run::onParametersApplied,    Qt::QueuedConnection);
        connect(m_device, &Cam::errorOccurred,
                this,     &Run::errorOccurred,          Qt::QueuedConnection);
    }

    /// Disconnects every signal/slot connection previously established by wireSignals() between
    /// this runner and m_device, in both directions.
    void unwireSignals() override {
        disconnect(this,     nullptr, m_device, nullptr);
        disconnect(m_device, nullptr, this,     nullptr);
    }

private slots:
    /// Handles the camera device's connectStatusChanged signal. If a Connect or Disconnect
    /// command is currently active, resolves it (succeeded/failed via finishActiveCommand())
    /// based on `status`; the status is then re-emitted as connectStatusChanged() to listeners
    /// regardless of whether a command was active.
    /// @param status the camera's new connection status
    void onConnectStatusChanged(vc::device::ConnectStatus status) {
        if (!hasActiveCommand()) {
            emit connectStatusChanged(status);
            return;
        }

        if (m_activeCommand.kind == DeviceCommandKind::Connect) {
            if (status == vc::device::ConnectStatus::Connected) {
                finishActiveCommand(DeviceCommandResult::succeeded(
                    m_activeCommand,
                    QStringLiteral("Camera connected.")));
            } else if (status == vc::device::ConnectStatus::ConnectFailed ||
                       status == vc::device::ConnectStatus::LostConnected) {
                finishActiveCommand(DeviceCommandResult::failed(
                    m_activeCommand,
                    DeviceCommandResultCode::DeviceError,
                    QStringLiteral("Camera connection failed.")));
            }
        } else if (m_activeCommand.kind == DeviceCommandKind::Disconnect) {
            if (status == vc::device::ConnectStatus::Disconnected ||
                status == vc::device::ConnectStatus::NoConnection) {
                finishActiveCommand(DeviceCommandResult::succeeded(
                    m_activeCommand,
                    QStringLiteral("Camera disconnected.")));
            }
        }

        emit connectStatusChanged(status);
    }

    /// Handles the camera device's connectionFailed signal: fails the currently active command
    /// (if any) with DeviceCommandResultCode::DeviceError using `msg`, then re-emits
    /// errorOccurred() with `msg`.
    void onConnectionFailed(const QString &msg) {
        if (hasActiveCommand()) {
            finishActiveCommand(DeviceCommandResult::failed(
                m_activeCommand,
                DeviceCommandResultCode::DeviceError,
                msg));
        }
        emit errorOccurred(msg);
    }

    /// Handles the camera device's grabFinished signal. If a CameraSingleShot command is
    /// active, resolves it succeeded or failed based on `result.isGrabSuccess` (carrying
    /// `result.msg` in the result payload's "message" entry), then always re-emits
    /// grabFinished() with `result`.
    /// @param result the grab outcome reported by the camera device
    void onGrabFinished(vc::device::GrabResult result) {
        QVariantMap payload;
        payload.insert(QStringLiteral("message"), result.msg);
        if (!hasActiveCommand()) {
            emit grabFinished(result);
            return;
        }

        if (m_activeCommand.kind == DeviceCommandKind::CameraSingleShot &&
            result.isGrabSuccess) {
            finishActiveCommand(DeviceCommandResult::succeeded(
                m_activeCommand,
                result.msg,
                payload));
        } else if (m_activeCommand.kind == DeviceCommandKind::CameraSingleShot) {
            finishActiveCommand(DeviceCommandResult::failed(
                m_activeCommand,
                DeviceCommandResultCode::DeviceError,
                result.msg,
                payload));
        }
        emit grabFinished(result);
    }

    /// Handles the camera device's parametersApplied signal. If a CameraApplyParams command is
    /// active, resolves it succeeded or failed based on `ok`, then always re-emits
    /// parametersApplied() with `ok`.
    /// @param ok whether the parameter apply succeeded on the camera device
    void onParametersApplied(bool ok) {
        if (!hasActiveCommand()) {
            emit parametersApplied(ok);
            return;
        }

        if (m_activeCommand.kind == DeviceCommandKind::CameraApplyParams && ok) {
            finishActiveCommand(DeviceCommandResult::succeeded(
                m_activeCommand,
                QStringLiteral("Camera parameters applied.")));
        } else if (m_activeCommand.kind == DeviceCommandKind::CameraApplyParams) {
            finishActiveCommand(DeviceCommandResult::failed(
                m_activeCommand,
                DeviceCommandResultCode::DeviceError,
                QStringLiteral("Camera parameters apply failed.")));
        }
        emit parametersApplied(ok);
    }

    /// Slot invoked by m_activeCommandTimer's timeout: if a command is still active, fails it
    /// with DeviceCommandResultCode::TimedOut.
    void onActiveCommandTimedOut()
    {
        if (!hasActiveCommand()) {
            return;
        }

        const DeviceCommand timedOut = m_activeCommand;
        finishActiveCommand(DeviceCommandResult::failed(
            timedOut,
            DeviceCommandResultCode::TimedOut,
            QStringLiteral("Camera command timed out.")));
    }

private:
    /// Pointer-to-member-function type for the no-argument internal trigger signals
    /// (sig_connect, sig_disconnect, sig_singleShot, sig_applyParams).
    using TriggerSignal = void (CameraRunner::*)();

    /// Returns whether `kind` is one of the camera command kinds this runner accepts:
    /// Connect, Disconnect, CameraSingleShot, or CameraApplyParams.
    static bool isSupportedCommand(DeviceCommandKind kind)
    {
        switch (kind) {
        case DeviceCommandKind::Connect:
        case DeviceCommandKind::Disconnect:
        case DeviceCommandKind::CameraSingleShot:
        case DeviceCommandKind::CameraApplyParams:
            return true;
        case DeviceCommandKind::Unknown:
        default:
            return false;
        }
    }

    /// Returns the queueing policy to apply for `kind`: Connect/Disconnect reject while busy,
    /// CameraSingleShot queues FIFO while busy, CameraApplyParams replaces any pending command
    /// of the same kind, and unsupported/unknown kinds default to rejecting while busy.
    static DeviceCommandQueuePolicy queuePolicyFor(DeviceCommandKind kind)
    {
        switch (kind) {
        case DeviceCommandKind::Connect:
        case DeviceCommandKind::Disconnect:
            return DeviceCommandQueuePolicy::RejectWhenBusy;
        case DeviceCommandKind::CameraSingleShot:
            return DeviceCommandQueuePolicy::QueueWhenBusy;
        case DeviceCommandKind::CameraApplyParams:
            return DeviceCommandQueuePolicy::ReplacePendingSameKind;
        case DeviceCommandKind::Unknown:
        default:
            return DeviceCommandQueuePolicy::RejectWhenBusy;
        }
    }

    /// Returns true if a command is currently running (m_activeCommand has a non-empty id).
    bool hasActiveCommand() const
    {
        return !m_activeCommand.id.isEmpty();
    }

    /// Starts executing `command` on the camera thread: looks up its trigger signal via
    /// triggerFor(), rejects the command (finishRejectedCommand()) if no trigger exists,
    /// otherwise records it as m_activeCommand, (re)starts m_activeCommandTimer for
    /// activeTimeoutMs(command), logs the start via LOG_USER_INFO, and emits the trigger signal.
    void runCommand(const DeviceCommand &command)
    {
        const TriggerSignal trigger = triggerFor(command.kind);
        if (trigger == nullptr) {
            DeviceCommandResult result = DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::UnsupportedCommand,
                QStringLiteral("Camera command trigger is not available."));
            finishRejectedCommand(result);
            return;
        }

        m_activeCommand = command;
        m_activeCommandTimer.start(activeTimeoutMs(command));
        LOG_USER_INFO << "Camera command started."
                      << "id=" << command.id
                      << "kind=" << deviceCommandKindToString(command.kind)
                      << "target=" << command.targetDeviceId
                      << "timeoutMs=" << activeTimeoutMs(command)
                      << "pending=" << m_commandQueue.pendingCount();
        (this->*trigger)();
    }

    /// Maps `kind` to the internal trigger signal (sig_connect/sig_disconnect/sig_singleShot/
    /// sig_applyParams) that starts it on the camera thread.
    /// @return the matching trigger signal, or nullptr if `kind` is unsupported
    TriggerSignal triggerFor(DeviceCommandKind kind) const
    {
        switch (kind) {
        case DeviceCommandKind::Connect:
            return &CameraRunner::sig_connect;
        case DeviceCommandKind::Disconnect:
            return &CameraRunner::sig_disconnect;
        case DeviceCommandKind::CameraSingleShot:
            return &CameraRunner::sig_singleShot;
        case DeviceCommandKind::CameraApplyParams:
            return &CameraRunner::sig_applyParams;
        case DeviceCommandKind::Unknown:
        default:
            return nullptr;
        }
    }

    /// Returns `command.timeoutMs` if positive, otherwise a default timeout of 3000 ms.
    static int activeTimeoutMs(const DeviceCommand &command)
    {
        return command.timeoutMs > 0 ? command.timeoutMs : 3000;
    }

    /// Logs a rejected command via LOG_USER_INFO and emits commandFinished(result).
    void finishRejectedCommand(const DeviceCommandResult &result)
    {
        LOG_USER_INFO << "Camera command rejected."
                      << "id=" << result.commandId
                      << "kind=" << deviceCommandKindToString(result.kind)
                      << "target=" << result.targetDeviceId
                      << "status=" << deviceCommandResultStatusToString(result.status)
                      << "code=" << deviceCommandResultCodeToString(result.code)
                      << "msg=" << result.message;
        emit commandFinished(result);
    }

    /// Completes the active command with `result`: stops m_activeCommandTimer, logs the
    /// outcome via LOG_USER_INFO, emits commandFinished(result), clears m_activeCommand, and
    /// dispatches the next queued command via dispatchNextCommand().
    void finishActiveCommand(const DeviceCommandResult &result)
    {
        m_activeCommandTimer.stop();
        LOG_USER_INFO << "Camera command finished."
                      << "id=" << result.commandId
                      << "kind=" << deviceCommandKindToString(result.kind)
                      << "target=" << result.targetDeviceId
                      << "status=" << deviceCommandResultStatusToString(result.status)
                      << "code=" << deviceCommandResultCodeToString(result.code)
                      << "msg=" << result.message;
        emit commandFinished(result);
        clearActiveCommand();
        dispatchNextCommand();
    }

    /// Resets m_activeCommand to a default-constructed (empty-id) DeviceCommand and stops
    /// m_activeCommandTimer.
    void clearActiveCommand()
    {
        m_activeCommand = DeviceCommand();
        m_activeCommandTimer.stop();
    }

    /// If no command is currently active and the queue has a pending entry, dequeues it and
    /// starts it via runCommand(). No-op otherwise.
    void dispatchNextCommand()
    {
        if (hasActiveCommand() || !m_commandQueue.hasPending()) {
            return;
        }

        runCommand(m_commandQueue.takeNext());
    }

    DeviceCommandQueue m_commandQueue;      ///< FIFO queue of camera commands awaiting dispatch.
    QTimer m_activeCommandTimer;            ///< Single-shot timer that fails m_activeCommand if it doesn't finish in time.
    DeviceCommand m_activeCommand;          ///< Command currently running on the camera thread; empty id means none is active.
};

} // namespace vc::runtime

#endif // CAMERA_RUNNER_H
