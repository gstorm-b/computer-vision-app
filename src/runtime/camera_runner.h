#ifndef CAMERA_RUNNER_H
#define CAMERA_RUNNER_H

/**
 * @file camera_runner.h
 * @brief CameraRunner — per-camera thread runner mediating GUI-thread requests and the
 *        camera device's own worker thread via the DeviceCommand model.
 */

#include <QTimer>

#include "runtime/device_runner.h"
#include "runtime/device_command_queue.h"
#include "device/camera/camera_device.h"

namespace vc::runtime {

/**
 * @class CameraRunner
 * @brief Per-camera thread runner (replaces the legacy CameraWorker that previously lived in
 *        src/model); mediates between the GUI thread and the camera device's own HighPriority
 *        worker thread.
 *
 * Commission mode: widget code calls requestConnect() / requestSingleShot() etc. from the GUI
 * thread. These emit queued signals that the camera device processes on its own thread; results
 * arrive back via grabFinished(), parametersApplied(), etc. (also queued, so safe for GUI
 * slots).
 *
 * Runtime mode: the camera stays on its own thread while the task runtime thread triggers grabs
 * and awaits grabFinished() via Qt::QueuedConnection; the runner remains the mediator so no
 * direct cross-thread calls are needed.
 *
 * Camera requests route through the standard DeviceCommand model, with queue and timeout
 * behavior: Connect/Disconnect are rejected while another command is active, SingleShot is
 * queued FIFO while busy, and ApplyParams keeps only the latest pending request.
 *
 * **Grab retry lives here, not in the caller.** A failed single-shot is re-issued up to
 * kMaxGrabAttempts times before the command is failed, so one bad frame does not interrupt
 * an automatic runtime cycle. Two consequences follow from that placement and are relied on
 * upstream:
 *  - the caller observes exactly ONE outcome per command. grabFinished() is deliberately
 *    not re-emitted for an intermediate failure, so LocalizationRuntimeController never
 *    sees the retries and cannot mistake one for a finished cycle;
 *  - reaching LocalizationFaultCode::CameraGrabTimeout therefore means every attempt
 *    failed, not that one did. A repeating 102 is a real hardware problem.
 *
 * The retry budget resets when a CameraSingleShot command starts, not when one succeeds,
 * so failures during commissioning cannot consume the next runtime cycle's budget. Each
 * attempt gets its own watchdog window (see kSingleShotTimeoutMs).
 *
 * @note The runner cannot rescue a camera whose own thread is blocked inside a driver
 *       call. Its watchdog only ends the *command*; the device is responsible for
 *       reporting every grab through grabFinished() and for publishing LostConnected when
 *       it detects removal (see BaslerGigECamera::publishRemovalIfDetected()).
 *
 * @note Threading: the wrapped camera device lives on its own worker QThread once attached
 *       (see DeviceRunner); requestConnect()/requestDisconnect()/requestSingleShot()/
 *       requestApplyParams()/submitCommand() are all safe to call from any thread, since they
 *       only touch this runner's own command queue/timer and dispatch to the device via
 *       Qt::QueuedConnection triggers (see wireSignals()).
 */
class CameraRunner : public DeviceRunner<vc::device::CameraDevice> {
    Q_OBJECT

public:
    /// Watchdog window for a single-shot grab, in milliseconds.
    ///
    /// This MUST stay above the camera's own blocking-grab timeout
    /// (BaslerGigECamera::kDefaultGrabTimeoutMs, 5000 ms). It used to share the generic
    /// 3000 ms default, so the runner gave up while the camera was still legitimately
    /// waiting: the command was reported TimedOut and the real grabFinished then arrived
    /// with no active command to resolve.
    static constexpr int kSingleShotTimeoutMs = 8000;

    /// Watchdog window for connect/disconnect/apply-params commands, in milliseconds.
    static constexpr int kDefaultCommandTimeoutMs = 3000;

    /// Total single-shot attempts before the command is failed. A transient grab failure
    /// is retried in the runner so one bad frame does not interrupt the automatic cycle;
    /// exhausting these attempts is what surfaces as LocalizationFaultCode::CameraGrabTimeout.
    /// @note This is an attempt count, not a retry count: 6 means one initial grab plus
    ///       five retries.
    static constexpr int kMaxGrabAttempts = 6;

    /**
     * @brief Constructs the runner for `camera`, registering the queued-connection meta types
     *        (DeviceCommand, DeviceCommandResult, GrabResult) and wiring the single-shot
     *        m_activeCommandTimer to onActiveCommandTimedOut() for active-command timeout
     *        handling.
     * @param[in] camera the camera device this runner manages (not owned; must outlive the runner)
     * @param[in] parent optional QObject parent
     */
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
        m_grabFailedCount = 0;
    }

    // ── Commission / runtime actions (safe from any thread) ──────────────────
    /// Submits a Connect command for this camera via submitCommand().
    void requestConnect() override
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::Connect,
                                            m_device->id()));
    }

    /// Submits a Disconnect command for this camera via submitCommand().
    void requestDisconnect() override
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

    /// Submits a CameraContinuousStart (live-view) command for this camera.
    /// @note Resolves on the device reporting its streaming state, not on a frame. Frames arrive
    ///       separately on continuousFrameReady() and carry no command outcome.
    void requestContinuousStart()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::CameraContinuousStart,
                                            m_device->id()));
    }

    /// Submits a CameraContinuousStop command for this camera. Succeeds even when nothing was
    /// running — the device reports its state either way.
    void requestContinuousStop()
    {
        submitCommand(DeviceCommand::create(DeviceCommandKind::CameraContinuousStop,
                                            m_device->id()));
    }

    /**
     * @brief Submits a manual backlight on/off command for this camera.
     *
     * @param[in] on true to force the lamp on and suppress auto-backlight, false to release it.
     *
     * @note Refused here, not at the device, when the camera has no digital I/O — the caller gets
     *       an immediate rejection naming the reason instead of a command that sits until its
     *       watchdog fires. This is the same shape as PlcRunner::requestSendResult() refusing a
     *       device that cannot output results, and for the same reason: a capability question has
     *       a known answer before any thread hop, so answering it late only loses the reason.
     */
    void requestBacklight(bool on)
    {
        const DeviceCommand command = DeviceCommand::create(
            on ? DeviceCommandKind::CameraBacklightOn : DeviceCommandKind::CameraBacklightOff,
            m_device->id());

        if (!m_device->hasIOPort()) {
            finishRejectedCommand(DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::UnsupportedCommand,
                QStringLiteral("This camera has no digital I/O, so it cannot drive a backlight.")));
            return;
        }

        submitCommand(command);
    }

    /**
     * @brief Validates and submits `command` for execution. Rejects it (via
     *        finishRejectedCommand()) if its targetDeviceId doesn't match this camera's id, or
     *        if its kind isn't one of the supported camera commands (see isSupportedCommand()).
     *        Otherwise enqueues it under the policy from queuePolicyFor() and either runs it
     *        immediately (runCommand()) when the runner is idle, or queues it and logs the
     *        queued state via LOG_USER_INFO.
     * @param[in] command the command to submit
     * @return the accepted/rejected result for this call; a queued command still returns
     *         "accepted" and completes later via commandFinished()
     * @post On acceptance, the command's terminal outcome (succeeded/failed/timed out) is
     *       always reported later via commandFinished(), whether it ran immediately or was
     *       queued.
     */
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
    /**
     * @brief Re-emitted (GUI-thread side) whenever the camera device's grabFinished signal
     *        fires.
     * @param[in] result the grab outcome/frame data reported by the camera device.
     */
    void grabFinished(vc::device::GrabResult result);
    /**
     * @brief Re-emitted (GUI-thread side) whenever the camera device's parametersApplied
     *        signal fires.
     * @param[in] ok whether the parameter apply succeeded.
     */
    void parametersApplied(bool ok);

    /**
     * @brief Re-emitted (GUI-thread side) for every frame of a continuous stream.
     *
     * @warning Deliberately NOT routed through grabFinished(). That signal resolves the runner's
     * in-flight command and feeds the single-shot retry budget; a stream arriving there would
     * resolve commands that are not running and spend a budget that is not theirs. Nothing in
     * this path touches m_activeCommand.
     * @param[in] result the frame; always a success with a non-empty cv::Mat.
     */
    void continuousFrameReady(vc::device::GrabResult result);

    /**
     * @brief Re-emitted (GUI-thread side) whenever the camera reports its streaming state.
     * @param[in] active true while continuous acquisition is running.
     * @note The authority for a live-view toggle: streaming also stops for reasons the UI did not
     *       cause — a single shot pre-empting it, a disconnect, a pulled cable.
     */
    void continuousStateChanged(bool active);

    /**
     * @brief Re-emitted (GUI-thread side) whenever the camera reports the backlight's level.
     * @param[in] on whether the backlight is currently driven on.
     * @note The authority for a backlight toggle, for the same reason continuousStateChanged() is
     *       the authority for live view: the lamp is also switched by the auto-backlight sequence
     *       around every grab, which no widget click can predict.
     */
    void backlightStateChanged(bool on);

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
    /// Internal trigger, queued-connected to CameraDevice::startContinuousShot().
    void sig_continuousStart();
    /// Internal trigger, queued-connected to CameraDevice::stopContinuousShot().
    void sig_continuousStop();
    /// Internal trigger, queued-connected to CameraDevice::setBacklightOverride(true).
    void sig_backlightOn();
    /// Internal trigger, queued-connected to CameraDevice::setBacklightOverride(false).
    void sig_backlightOff();

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
        connect(this,     &Run::sig_continuousStart,
                m_device, &Cam::startContinuousShot,   Qt::QueuedConnection);
        connect(this,     &Run::sig_continuousStop,
                m_device, &Cam::stopContinuousShot,    Qt::QueuedConnection);
        // Lambdas rather than two device methods: the command kinds are a boolean pair, and the
        // trigger signals must stay no-argument for triggerFor()'s TriggerSignal type. m_device is
        // the context object, so both still run on the camera thread and are torn down by
        // unwireSignals()'s disconnect(this, nullptr, m_device, nullptr).
        connect(this, &Run::sig_backlightOn, m_device,
                [device = m_device] { device->setBacklightOverride(true); },
                Qt::QueuedConnection);
        connect(this, &Run::sig_backlightOff, m_device,
                [device = m_device] { device->setBacklightOverride(false); },
                Qt::QueuedConnection);

        connect(m_device, &Cam::connectStatusChanged,
                this,     &Run::onConnectStatusChanged, Qt::QueuedConnection);
        connect(m_device, &Cam::connectionFailed,
                this,     &Run::onConnectionFailed,     Qt::QueuedConnection);
        connect(m_device, &Cam::grabFinished,
                this,     &Run::onGrabFinished,         Qt::QueuedConnection);
        connect(m_device, &Cam::parametersApplied,
                this,     &Run::onParametersApplied,    Qt::QueuedConnection);
        // Straight through to listeners: a continuous frame is not a command outcome, so it must
        // not reach onGrabFinished() and the retry policy that lives there.
        connect(m_device, &Cam::continuousFrameReady,
                this,     &Run::continuousFrameReady,   Qt::QueuedConnection);
        connect(m_device, &Cam::continuousStateChanged,
                this,     &Run::onContinuousStateChanged, Qt::QueuedConnection);
        connect(m_device, &Cam::backlightStateChanged,
                this,     &Run::onBacklightStateChanged,  Qt::QueuedConnection);
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
    /**
     * @brief Handles the camera device's connectStatusChanged signal. If a Connect or
     *        Disconnect command is currently active, resolves it (succeeded/failed via
     *        finishActiveCommand()) based on `status`; the status is then re-emitted as
     *        connectStatusChanged() to listeners regardless of whether a command was active.
     * @param[in] status the camera's new connection status
     */
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

    /**
     * @brief Handles the camera device's grabFinished signal, applying the grab-retry policy.
     *
     * With a CameraSingleShot command active:
     *  - success resolves the command and clears the retry counter;
     *  - failure re-issues the grab (restarting the watchdog for the new attempt) until
     *    kMaxGrabAttempts is reached, then fails the command.
     *
     * @param[in] result the grab outcome reported by the camera device
     * @note grabFinished() is re-emitted only for the outcome the caller should act on —
     *       an intermediate failure returns early instead. That is what lets the runtime
     *       controller treat one command as one cycle result.
     */
    void onGrabFinished(vc::device::GrabResult result) {
        QVariantMap payload;
        payload.insert(QStringLiteral("message"), result.msg);
        if (!hasActiveCommand()) {
            emit grabFinished(result);
            return;
        }

        if (m_activeCommand.kind == DeviceCommandKind::CameraSingleShot &&
            result.isGrabSuccess) {
            m_grabFailedCount = 0;
            finishActiveCommand(DeviceCommandResult::succeeded(
                m_activeCommand,
                result.msg,
                payload));
        } else if (m_activeCommand.kind == DeviceCommandKind::CameraSingleShot) {
            m_grabFailedCount++;
            if (m_grabFailedCount >= kMaxGrabAttempts) {
                LOG_USER_WARN << "Camera grab failed on every attempt."
                              << "target=" << m_activeCommand.targetDeviceId
                              << "attempts=" << m_grabFailedCount
                              << "msg=" << result.msg;
                finishActiveCommand(DeviceCommandResult::failed(
                    m_activeCommand,
                    DeviceCommandResultCode::DeviceError,
                    result.msg,
                    payload));
            } else {
                // Give the retry its own watchdog window. The timer was started once in
                // runCommand() and never restarted, so the whole retry chain shared a
                // single timeout: a grab that fails slowly used it up on the first
                // attempt and the later attempts never ran at all.
                LOG_DEV_INFO << "Camera grab failed, retrying."
                             << "target=" << m_activeCommand.targetDeviceId
                             << "attempt=" << m_grabFailedCount
                             << "of=" << kMaxGrabAttempts;
                m_activeCommandTimer.start(activeTimeoutMs(m_activeCommand));
                emit sig_singleShot();
                return;
            }
        }
        emit grabFinished(result);
    }

    /**
     * @brief Handles the camera device's continuousStateChanged signal: resolves an active
     *        CameraContinuousStart/Stop command against the reported state, then always re-emits
     *        continuousStateChanged().
     *
     * The device reports its state after every start/stop request, including ones that changed
     * nothing, so both commands always resolve — an idempotent stop succeeds immediately instead
     * of waiting out the watchdog.
     *
     * @param[in] active whether continuous acquisition is now running.
     * @note A report arriving with a CameraSingleShot active is NOT a command outcome: a single
     *       shot pre-empts streaming, so `active=false` here is a side effect of that grab, not
     *       its result. Only the two continuous kinds are resolved.
     */
    void onContinuousStateChanged(bool active)
    {
        if (hasActiveCommand()) {
            const bool startedOk =
                (m_activeCommand.kind == DeviceCommandKind::CameraContinuousStart) && active;
            const bool stoppedOk =
                (m_activeCommand.kind == DeviceCommandKind::CameraContinuousStop) && !active;
            const bool isContinuousCommand =
                (m_activeCommand.kind == DeviceCommandKind::CameraContinuousStart)
                || (m_activeCommand.kind == DeviceCommandKind::CameraContinuousStop);

            if (startedOk || stoppedOk) {
                finishActiveCommand(DeviceCommandResult::succeeded(m_activeCommand));
            } else if (isContinuousCommand) {
                finishActiveCommand(DeviceCommandResult::failed(
                    m_activeCommand,
                    DeviceCommandResultCode::DeviceError,
                    QStringLiteral("Camera did not reach the requested continuous state.")));
            }
        }
        emit continuousStateChanged(active);
    }

    /**
     * @brief Handles the camera's backlightStateChanged signal: resolves an active
     *        CameraBacklightOn/Off command against the reported level, then always re-emits.
     *
     * @param[in] on whether the backlight is now driven on.
     * @note Only the two backlight kinds are resolved. The lamp is switched by the auto-backlight
     *       sequence around every grab as well, so a report arriving with a CameraSingleShot
     *       active is a side effect of that grab, not its outcome — resolving it here would end
     *       the grab command the moment the lamp came on, before there was an image.
     */
    void onBacklightStateChanged(bool on)
    {
        if (hasActiveCommand()) {
            const bool isBacklightCommand =
                (m_activeCommand.kind == DeviceCommandKind::CameraBacklightOn)
                || (m_activeCommand.kind == DeviceCommandKind::CameraBacklightOff);
            const bool wanted = (m_activeCommand.kind == DeviceCommandKind::CameraBacklightOn);

            if (isBacklightCommand && on == wanted) {
                finishActiveCommand(DeviceCommandResult::succeeded(m_activeCommand));
            } else if (isBacklightCommand) {
                finishActiveCommand(DeviceCommandResult::failed(
                    m_activeCommand,
                    DeviceCommandResultCode::DeviceError,
                    QStringLiteral("Camera did not reach the requested backlight state.")));
            }
        }
        emit backlightStateChanged(on);
    }

    /**
     * @brief Handles the camera device's parametersApplied signal. If a CameraApplyParams
     *        command is active, resolves it succeeded or failed based on `ok`, then always
     *        re-emits parametersApplied() with `ok`.
     * @param[in] ok whether the parameter apply succeeded on the camera device
     */
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

    /// Returns whether `kind` is one of the camera command kinds this runner accepts.
    static bool isSupportedCommand(DeviceCommandKind kind)
    {
        switch (kind) {
        case DeviceCommandKind::Connect:
        case DeviceCommandKind::Disconnect:
        case DeviceCommandKind::CameraSingleShot:
        case DeviceCommandKind::CameraApplyParams:
        case DeviceCommandKind::CameraContinuousStart:
        case DeviceCommandKind::CameraContinuousStop:
        case DeviceCommandKind::CameraBacklightOn:
        case DeviceCommandKind::CameraBacklightOff:
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
        case DeviceCommandKind::CameraContinuousStart:
        case DeviceCommandKind::CameraContinuousStop:
            // Queued, not rejected. Both are short and the operator's intent is a toggle: a stop
            // arriving while a grab is still finishing must still take effect, and rejecting it
            // would leave the camera streaming with the button showing stopped.
            return DeviceCommandQueuePolicy::QueueWhenBusy;
        case DeviceCommandKind::CameraBacklightOn:
        case DeviceCommandKind::CameraBacklightOff:
            // Queued for the same reason as the continuous pair: it is a toggle the operator
            // drives, and a press arriving while a grab finishes must still take effect rather
            // than be dropped with the button left showing the wrong lamp state.
            return DeviceCommandQueuePolicy::QueueWhenBusy;
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

        // Reset the retry budget per command. It used to clear only on a successful grab,
        // so failures during commissioning carried into the next runtime cycle and could
        // consume the budget before that cycle's first attempt.
        if (command.kind == DeviceCommandKind::CameraSingleShot) {
            m_grabFailedCount = 0;
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
        case DeviceCommandKind::CameraContinuousStart:
            return &CameraRunner::sig_continuousStart;
        case DeviceCommandKind::CameraContinuousStop:
            return &CameraRunner::sig_continuousStop;
        case DeviceCommandKind::CameraBacklightOn:
            return &CameraRunner::sig_backlightOn;
        case DeviceCommandKind::CameraBacklightOff:
            return &CameraRunner::sig_backlightOff;
        case DeviceCommandKind::Unknown:
        default:
            return nullptr;
        }
    }

    /// Returns `command.timeoutMs` if positive, otherwise the per-kind default:
    /// kSingleShotTimeoutMs for a grab, kDefaultCommandTimeoutMs for everything else.
    static int activeTimeoutMs(const DeviceCommand &command)
    {
        if (command.timeoutMs > 0) {
            return command.timeoutMs;
        }
        return command.kind == DeviceCommandKind::CameraSingleShot
                   ? kSingleShotTimeoutMs
                   : kDefaultCommandTimeoutMs;
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
    int m_grabFailedCount;
};

} // namespace vc::runtime

#endif // CAMERA_RUNNER_H
