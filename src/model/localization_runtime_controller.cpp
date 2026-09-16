#include "model/localization_runtime_controller.h"

#include <QMetaObject>
#include <QTimer>

#include <memory>

#include "core/logger/app_logger.h"
#include "device/plc/plc_device.h"
#include "matching/match_group.h"
#include "matching/match_pattern.h"
#include "model/robot_kinematic_picking_checker.h"
#include "model/task_device_binding.h"
#include "RobotKinematics/Core/Pose.h"
#include "RobotKinematics/Core/Units.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/vision_output_runner.h"

/// Task-localization runtime: binds the camera/primary-PLC/vision-output device roles,
/// drives the grab -> match -> send cycle state machine per execute trigger, and applies
/// per-role connection-recovery policies (retry/escalate) on connection loss.
namespace vc::model {

/// Internal helpers used only within this translation unit: connection-status health
/// checks/naming and recovery log-message formatting.
namespace {

/// Returns true only for ConnectStatus::Connected; every other status (including
/// LostConnected/Connecting) is treated as unhealthy for readiness purposes.
bool isHealthyStatus(vc::device::ConnectStatus status)
{
    return status == vc::device::ConnectStatus::Connected;
}

/// Maps a ConnectStatus value to its human-readable name for log/status messages.
/// @return the status name, or "Unknown" for any value not covered by the switch
QString connectStatusName(vc::device::ConnectStatus status)
{
    switch (status) {
    case vc::device::ConnectStatus::NoConnection:
        return QStringLiteral("NoConnection");
    case vc::device::ConnectStatus::Disconnected:
        return QStringLiteral("Disconnected");
    case vc::device::ConnectStatus::Connected:
        return QStringLiteral("Connected");
    case vc::device::ConnectStatus::LostConnected:
        return QStringLiteral("LostConnected");
    case vc::device::ConnectStatus::ConnectFailed:
        return QStringLiteral("ConnectFailed");
    case vc::device::ConnectStatus::Connecting:
        return QStringLiteral("Connecting");
    }

    return QStringLiteral("Unknown");
}

/// Formats a "recovering" log/status message reporting the role, target device, current
/// connection status, and the retry interval. There is no retry budget to report: the
/// runtime retries until it is torn down.
/// @return the formatted message string
QString buildRecoveryProgressMessage(const LocalizationRecoveryPolicy &policy,
                                     const QString &deviceId,
                                     vc::device::ConnectStatus status)
{
    return QStringLiteral("Task runtime recovering: role=%1 deviceId=%2 status=%3 retrying every %4 ms until reconnected")
        .arg(policy.roleName,
             deviceId,
             connectStatusName(status),
             QString::number(policy.retryIntervalMs));
}

/// Formats a "ready again" message announcing that a role recovered, reporting how many
/// attempts it took and how long the outage lasted. The elapsed time is the number an
/// operator actually wants after the fact; the attempt count alone does not give it,
/// because attempts are no longer capped at a known maximum.
/// @return the formatted message string
QString buildRecoveryReadyMessage(const LocalizationRecoveryPolicy &policy,
                                  const QString &deviceId,
                                  int retryCount,
                                  const QDateTime &outageStartedAt)
{
    const qint64 elapsedMs = outageStartedAt.isValid()
                                 ? outageStartedAt.msecsTo(QDateTime::currentDateTime())
                                 : 0;

    return QStringLiteral("Task runtime ready: role=%1 deviceId=%2 recovered after %3 attempt(s) in %4 s")
        .arg(policy.roleName,
             deviceId,
             QString::number(retryCount),
             QString::number(elapsedMs / 1000.0, 'f', 1));
}

} // namespace

/// Registers the queued-connection meta-types used by this controller's signals
/// (CycleResult, TaskLogEntry, LocalizationFaultCode, CameraWorkspace, and the shared
/// robot-picking-checker pointer) so they can cross thread/queued signal boundaries.
LocalizationRuntimeController::LocalizationRuntimeController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<LocalizationRuntimeController::CycleResult>(
        "vc::model::LocalizationRuntimeController::CycleResult");
    qRegisterMetaType<LocalizationRuntimeController::TaskLogEntry>(
        "vc::model::LocalizationRuntimeController::TaskLogEntry");
    qRegisterMetaType<LocalizationFaultCode>("vc::model::LocalizationFaultCode");
    qRegisterMetaType<CameraWorkspace>("CameraWorkspace");
    qRegisterMetaType<CameraWorkspace>("vc::model::CameraWorkspace");
    qRegisterMetaType<std::shared_ptr<mtc::IRobotPickingChecker>>(
        "std::shared_ptr<mtc::IRobotPickingChecker>");

    // Fault auto-recovery. The timer is owned by this controller, so a runtime that ends
    // during the wait takes the pending recovery down with it — the same lifetime
    // guarantee the reconnect retries rely on.
    m_faultRecoverTimer.setSingleShot(true);
    m_faultRecoverTimer.setInterval(kFaultAutoRecoverMs);
    connect(&m_faultRecoverTimer, &QTimer::timeout, this, [this]() {
        // Read the code before recovering: recoverFromFault() clears the pending result.
        const LocalizationFaultCode code = m_pendingCycleResult.faultCode;
        m_consecutiveAutoRecoveries += 1;

        // A fault that keeps auto-clearing is a real problem, but reporting every
        // occurrence would bury the very log this path exists to keep readable.
        if (m_consecutiveAutoRecoveries % kAutoRecoverWarnStride == 0) {
            LOG_USER_WARN << "Repeating auto-recovered localization fault."
                          << "count=" << m_consecutiveAutoRecoveries
                          << "lastFault=" << localizationFaultCodeName(code);
        } else {
            LOG_DEV_INFO << "Localization fault auto-cleared."
                         << "count=" << m_consecutiveAutoRecoveries
                         << "fault=" << localizationFaultCodeName(code);
        }

        recoverFromFault(
            QStringLiteral("Fault auto-cleared after %1 ms (no bErrorReset received).")
                .arg(kFaultAutoRecoverMs));
    });

    // Phase 9 / E4. Single-shot and cancellable so a retry dies with its cycle: firing a stale
    // handshake value into the NEXT cycle would publish a bMatchingFinished the PLC would read as
    // belonging to the run in progress.
    m_plcWriteRetryTimer.setSingleShot(true);
    connect(&m_plcWriteRetryTimer, &QTimer::timeout, this, [this]() {
        const QList<quint64> due = m_plcWriteRetryQueue;
        m_plcWriteRetryQueue.clear();
        for (const quint64 id : due) {
            reissueTrackedWrite(id);
        }
    });
}

/// Stores the task's localization config and rebuilds the PLC tag / signal-name
/// mapping table from it.
/// @param config the new task-localization configuration to adopt
void LocalizationRuntimeController::configure(const TaskLocalizeConfig &config)
{
    m_config = config;
    m_signalMapper.configure(m_config);
}

/// Switches the active camera role to `cameraNumber`: disconnects the previous camera
/// runner (if different), rebinds recovery/signal connections to the new one, revalidates
/// its calibration, rebuilds the robot-picking checker, and requests a connect. Ignored
/// while a cycle is running, and faults out (bTaskFault/nFaultCode) if no runner is
/// registered for `cameraNumber` or its calibration is invalid.
/// @param cameraNumber the 1-based camera role number to activate
void LocalizationRuntimeController::setActiveCameraNumber(int cameraNumber)
{
    if (m_cycleState == CycleState::Running) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Camera change ignored while cycle is running."));
        return;
    }

    // One validator, shared with setup() (Task C2). This is where a written 0 is caught; it used
    // to be swallowed by a `> 0` guard in handlePlcValues() and never reached a setter at all.
    //
    // A refused number is a FAILURE, not a no-op. It used to publish the signals below and
    // return — leaving m_cycleState at ReadyForTrigger and m_activeCameraNumber unchanged, so
    // the rejection had no memory: a trigger was still accepted and ran on the OLD camera, and
    // the next markRuntimeReady() republished bTaskReady/bCameraValid/bTaskFault as if nothing
    // had happened. The latch and the Faulted transition below are what fixed that, and they are
    // preserved verbatim by this extraction.
    const IndexValidation check = validateCameraNumber(cameraNumber);
    if (!check.accepted()) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Camera change refused: %1").arg(check.message));
        m_activeCameraSelectionRejected = true;
        publishBoolSignal(QStringLiteral("bTaskReady"), false);
        publishBoolSignal(QStringLiteral("bCameraValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        // CameraNotRegistered, not CameraLost: nothing was lost. The number the PLC wrote names no
        // camera this project has, and a master that reads 100 goes looking at cabling for what is
        // a selection problem — the same mis-signposting item 55 fixed for the message text.
        publishNumberSignal(
            QStringLiteral("nFaultCode"),
            localizationFaultCodeValue(LocalizationFaultCode::CameraNotRegistered));
        m_cycleState = CycleState::Faulted;
        emit runtimeFault(check.message);

        // m_activeCameraNumber is deliberately NOT set to the rejected number. Keeping the
        // previously-bound camera is what lets a later valid write re-arm the runtime with no
        // operator action — and rebuildPickingChecker() and buildVisionOutputPositions() both
        // read it, so pointing it at a camera with no runner would break them too.
        return;
    }

    // The number named a real, registered camera, so THIS signal's selection is settled. Cleared
    // here rather than after the calibration check below on purpose: an uncalibrated camera is a
    // different fault with its own gate (validateActiveCameraCalibration()), and conflating the
    // two would make a calibration problem unrecoverable by re-selecting the same camera.
    m_activeCameraSelectionRejected = false;

    auto *previousRunner = activeCameraRunner();
    m_activeCameraNumber = cameraNumber;
    m_activeCameraFromProjectDefault = false;
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    bindActiveCameraRole(m_activeCameraNumber);
    // The adopted number goes on the STATUS signal, never back onto nActiveCamera. That tag is
    // the master's own command register: echoing it is a write race on a server binding and is
    // refused outright on a client binding whose command registers are input registers
    // (backlog item 60, field-confirmed 2026-09-08).
    publishNumberSignal(QStringLiteral("nActiveCameraStatus"), cameraNumber);

    auto *newRunner = activeCameraRunner();
    if (previousRunner && previousRunner != newRunner) {
        previousRunner->requestDisconnect();
    }

    const bool calibrated = validateActiveCameraCalibration();
    // Rebuild the pickability checker against the new camera's calibration.
    rebuildPickingChecker();
    publishBoolSignal(QStringLiteral("bCameraValid"), calibrated);
    if (!calibrated) {
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid));
        m_cycleState = CycleState::Faulted;
        emit runtimeFault(QStringLiteral("Active camera calibration is invalid."));
        return;
    }

    if (newRunner) {
        newRunner->requestConnect();
    }
    // Refresh the active workspace from the new camera. Shared with setup() so the two paths
    // cannot drift — before this was a helper, only the setter did it, which is why a runtime
    // that was never told to change camera ran the whole session on a default workspace.
    applyActiveCameraWorkspace();
    if (allRequiredRolesHealthy()) {
        markRuntimeReady(QStringLiteral("Active camera changed. Runtime ready."));
    }
}

/// Switches the active pattern group used for matching and publishes bPatternValid;
/// raises bTaskFault/nFaultCode(PatternNotRegistered) if the group has no usable train images.
/// Ignored while a cycle is running.
/// @param patternGroupNumber the pattern-group key to activate
void LocalizationRuntimeController::setActivePatternGroupNumber(int patternGroupNumber)
{
    if (m_cycleState == CycleState::Running) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Pattern group change ignored while cycle is running."));
        return;
    }

    // Range checked BEFORE m_activePatternGroupNumber is committed: an index that can never name
    // a group must not become the active one, or validateActivePatternGroup() would keep
    // re-reading it. Same validator setup() uses (Task C2).
    const IndexValidation check = validatePatternGroupNumber(patternGroupNumber);
    if (!check.accepted()) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Pattern group change refused: %1").arg(check.message));
        m_activePatternGroupSelectionRejected = true;
        publishBoolSignal(QStringLiteral("bTaskReady"), false);
        publishBoolSignal(QStringLiteral("bPatternValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternNotRegistered));
        m_cycleState = CycleState::Faulted;
        emit runtimeFault(check.message);
        return;
    }

    // Committed before validating because validateActivePatternGroup() reads this member — but
    // NOT echoed back to the PLC until it is known good. Publishing first meant a rejected group
    // number was written straight back onto the master's own command register, which reads as the
    // task confirming a selection it had in fact refused. The camera setter never did that; this
    // one did, and the two now agree.
    m_activePatternGroupNumber = patternGroupNumber;
    const bool valid = validateActivePatternGroup();
    m_activePatternGroupSelectionRejected = !valid;
    publishBoolSignal(QStringLiteral("bPatternValid"), valid);
    if (valid) {
        m_activePatternGroupFromProjectDefault = false;
        // Status signal, not the command register — see setActiveCameraNumber().
        publishNumberSignal(QStringLiteral("nActivePatternGroupStatus"), patternGroupNumber);
    }
    if (!valid) {
        // Same failure shape as an unregistered camera number: the rejection must be recorded in
        // the cycle state, not published and forgotten. Without the Faulted transition the next
        // trigger was accepted and the failure only surfaced when startCycle() re-validated —
        // one consumed trigger late, with bTaskFault already wiped by publishCycleStartOutputs().
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Pattern group change refused: no pattern group is "
                                     "registered for number %1.").arg(patternGroupNumber));
        publishBoolSignal(QStringLiteral("bTaskReady"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternNotRegistered));
        m_cycleState = CycleState::Faulted;
        emit runtimeFault(QStringLiteral("No pattern group is registered for number %1.")
                              .arg(patternGroupNumber));
        return;
    }

    // The re-arm this setter never had. Faulting without it would strand the runtime on the
    // ordinary sequence "PLC writes a bad group number, then the correct one": the second write
    // would publish bPatternValid=true and return, leaving the state Faulted and bTaskReady false
    // with no way out but an operator bErrorReset — which contradicts the rule that the runtime
    // must never park on a transient fault waiting for a person.
    //
    // Unlike the group number, m_activePatternGroupNumber IS committed above before validating,
    // because validateActivePatternGroup() reads it. That is safe precisely because this path
    // re-arms: a rejected number is replaced by the next valid write.
    if (allRequiredRolesHealthy()) {
        markRuntimeReady(QStringLiteral("Active pattern group changed. Runtime ready."));
    }
}

/// Resolves the active camera's workspace from the task config into both the context copy and the
/// cached member, or clears both when no camera runner is bound.
///
/// One helper, two callers — setup() and setActiveCameraNumber() — because the two members must
/// always be written together. The cached member is not reset by setup(), so clearing on the
/// no-runner path is what stops a workspace from a previous runtime session surviving into one
/// whose camera role never bound.
void LocalizationRuntimeController::applyActiveCameraWorkspace()
{
    auto *runner = activeCameraRunner();
    if (!runner || !runner->device()) {
        m_context.activeCameraWorkspace = CameraWorkspace();
        m_activeCameraWorkspace = m_context.activeCameraWorkspace;
        return;
    }

    m_context.activeCameraWorkspace = m_config.cameraWorkspace(runner->device()->id());
    m_activeCameraWorkspace = m_context.activeCameraWorkspace;
}

/// Range first, registration second. They are different failures and deserve different messages:
/// 0 or 99 is a number that can never name a camera, while 3 is a number that could but was not
/// bound in this project. The bounds come from TaskDeviceBinding, which is also what
/// TaskDeviceBinding::fromJson() enforces at load — so this check and that one agree by
/// construction rather than by a constant repeated in two places.
LocalizationRuntimeController::IndexValidation
LocalizationRuntimeController::validateCameraNumber(int cameraNumber) const
{
    if (cameraNumber < vc::model::TaskDeviceBinding::kMinCameraNumber ||
        cameraNumber > vc::model::TaskDeviceBinding::kMaxCameraNumber) {
        return {IndexVerdict::OutOfRange,
                QStringLiteral("Camera number %1 is outside the valid range %2..%3.")
                    .arg(cameraNumber)
                    .arg(vc::model::TaskDeviceBinding::kMinCameraNumber)
                    .arg(vc::model::TaskDeviceBinding::kMaxCameraNumber)};
    }

    if (!m_context.cameraRunners.value(cameraNumber)) {
        return {IndexVerdict::NotRegistered,
                QStringLiteral("No camera is registered for number %1.").arg(cameraNumber)};
    }

    return {};
}

/// Range only. Whether a group in range actually has usable train images is a separate question
/// with its own gate (validateActivePatternGroup()), and conflating the two would make a group
/// with no patterns report as an illegal index.
LocalizationRuntimeController::IndexValidation
LocalizationRuntimeController::validatePatternGroupNumber(int patternGroupNumber) const
{
    if (!mtc::MatchGroup::validateIndexRange(patternGroupNumber)) {
        return {IndexVerdict::OutOfRange,
                QStringLiteral("Pattern group number %1 is outside the valid range %2..%3.")
                    .arg(patternGroupNumber)
                    .arg(mtc::MatchGroup::getMinGroupRange())
                    .arg(mtc::MatchGroup::getMaxGroupRange())};
    }

    return {};
}

QStringList LocalizationRuntimeController::requiredSignalNames()
{
    return {QStringLiteral("bExecuteTrigger"),
            QStringLiteral("bTaskReady"),
            QStringLiteral("bMatchingFinished"),
            QStringLiteral("bTaskFault"),
            QStringLiteral("nFaultCode")};
}

/// The gate backlog item 1 asked for. Runs at setup(), before any role is asked to connect, so a
/// misconfigured map is refused with a message naming the signal rather than producing a runtime
/// that starts, goes quiet, and looks like a dead PLC.
void LocalizationRuntimeController::validateSignalMap(SetupResult *result)
{
    if (!result) {
        return;
    }

    const QStringList required = requiredSignalNames();
    const QMetaObject &meta = TaskLocalizeConfig::staticMetaObject;

    // Tag lists come from the bound device, per kind. A device that provides neither list is not
    // a fault: "this PLC family cannot enumerate its tags" is a different statement from "the tag
    // is wrong", and treating the first as the second would refuse every device that simply does
    // not implement the interface.
    vc::device::IPlcTagProvider *tagProvider = nullptr;
    if (auto *runner = primaryPlcRunner()) {
        tagProvider = dynamic_cast<vc::device::IPlcTagProvider *>(runner->device());
    }
    QSet<QString> digitalTags;
    QSet<QString> wordTags;
    if (tagProvider) {
        const QStringList digital = tagProvider->availableDigitalIoNames();
        const QStringList word = tagProvider->availableWordIoNames();
        digitalTags = QSet<QString>(digital.cbegin(), digital.cend());
        wordTags = QSet<QString>(word.cbegin(), word.cend());
    } else {
        LOG_DEV_INFO << "Signal-map gate: the bound PLC provides no tag list; "
                        "orphan checking is skipped for" << m_context.primaryPlcDeviceId;
    }

    QMap<QString, QString> tagOwners;   // tag -> first signal that claimed it

    for (const QString &signalName : LocalizationSignalMapper::signalFieldNames()) {
        const QString tag = LocalizationSignalMapper::configuredTag(m_config, signalName);
        const QString label = vc::gadget_meta::displayName(meta, signalName.toUtf8().constData());

        if (tag.isEmpty()) {
            if (required.contains(signalName)) {
                result->errors.append(
                    QStringLiteral("Required signal \"%1\" (%2) is not mapped to a PLC tag.")
                        .arg(label, signalName));
            } else {
                const QString message =
                    QStringLiteral("Optional signal \"%1\" (%2) is not mapped; the runtime will "
                                   "not report it to the PLC.").arg(label, signalName);
                appendTaskLog(QStringLiteral("WARN"), message);
                LOG_USER_WARN << message;
            }
            continue;
        }

        // Two signals on one tag is a wiring fault, not a developer note. The mapper already
        // notices it, but only at LOG_DEV_ERR, where no commissioning engineer will see it — and
        // it silently keeps the first mapping, so the second signal simply never arrives.
        if (tagOwners.contains(tag)) {
            result->errors.append(
                QStringLiteral("Signals \"%1\" and \"%2\" are both mapped to tag %3.")
                    .arg(tagOwners.value(tag), label, tag));
            continue;
        }
        tagOwners.insert(tag, label);

        if (!tagProvider) {
            continue;
        }

        // Per kind, never against the union: a bit signal pointed at a word tag is an orphan for
        // that signal even though the tag exists on the device, and checking the union would let
        // exactly that mistake through.
        const bool isBitSignal = signalName.startsWith(QLatin1Char('b'));
        const QSet<QString> &expected = isBitSignal ? digitalTags : wordTags;
        if (!expected.contains(tag)) {
            result->errors.append(
                QStringLiteral("Signal \"%1\" (%2) is mapped to tag %3, which the device %4 does "
                               "not provide as %5.")
                    .arg(label,
                         signalName,
                         tag,
                         m_context.primaryPlcDeviceId,
                         isBitSignal ? QStringLiteral("a bit") : QStringLiteral("a register")));
        }
    }
}

/// Reads the index the PLC currently holds for `signalName`. See the header for why "unmapped",
/// "not read yet" and "holds 0" must stay three distinct answers.
bool LocalizationRuntimeController::commandedIndexFromPlc(const QString &signalName,
                                                          int *number,
                                                          bool *typeMismatch) const
{
    if (typeMismatch) {
        *typeMismatch = false;
    }

    const QString tag = m_signalMapper.tagForSignalName(signalName);
    if (tag.isEmpty()) {
        return false;   // unmapped: the project default is the only answer available
    }
    if (!m_context.plcSnapshot) {
        return false;   // never read: NOT the same as holding 0
    }

    QVariant value;
    if (!m_context.plcSnapshot->valueForTag(tag, &value)) {
        return false;   // the PLC has no value for this tag
    }

    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok) {
        // A number signal bound to a bit area. Reported as the mapping mistake it is rather than
        // silently becoming 0 or 1 — which would then be refused as an out-of-range index and
        // send a commissioning engineer looking at the master's program instead of the map.
        if (typeMismatch) {
            *typeMismatch = true;
        }
        return false;
    }

    if (number) {
        *number = parsed;
    }
    return true;
}

/// One line, on the ready path, naming what the runtime actually selected and where each half
/// came from.
///
/// "The task is on camera 1" and "the task was told to use camera 1" are different facts, and
/// only the second means the master and the task agree. Before this line the log said neither,
/// so a cell running the wrong camera looked identical to one running the right one.
void LocalizationRuntimeController::logStartupSelectionSummary()
{
    const QString cameraSource = m_activeCameraFromProjectDefault
                                     ? QStringLiteral("project default (firstKey)")
                                     : QStringLiteral("commanded");
    const QString groupSource = m_activePatternGroupFromProjectDefault
                                    ? QStringLiteral("project default (firstKey)")
                                    : QStringLiteral("commanded");
    const QString cameraDeviceId = m_context.cameraDeviceIds.value(m_activeCameraNumber);

    QString workspace = QStringLiteral("crop=off condition=off");
    if (m_activeCameraWorkspace.useWorkspace || m_activeCameraWorkspace.useConditionWorkspace) {
        const cv::Rect2f &roi = m_activeCameraWorkspace.conditionRoi;
        workspace = QStringLiteral("crop=%1 condition=%2 conditionRoi=(%3,%4 %5x%6)")
                        .arg(m_activeCameraWorkspace.useWorkspace ? QStringLiteral("on")
                                                                  : QStringLiteral("off"),
                             m_activeCameraWorkspace.useConditionWorkspace
                                 ? QStringLiteral("on")
                                 : QStringLiteral("off"))
                        .arg(roi.x)
                        .arg(roi.y)
                        .arg(roi.width)
                        .arg(roi.height);
    }

    const QString summary =
        QStringLiteral("Runtime selection: camera %1 (%2, device %3), pattern group %4 (%5); "
                       "workspace %6.")
            .arg(QString::number(m_activeCameraNumber),
                 cameraSource,
                 cameraDeviceId.isEmpty() ? QStringLiteral("-") : cameraDeviceId,
                 QString::number(m_activePatternGroupNumber),
                 groupSource,
                 workspace);

    LOG_USER_INFO << summary;
    // Also on the operator's own task log, not only in the app log: which camera and group the
    // runtime settled on — and whether anyone chose them — is exactly what an operator checks
    // when a cell picks from the wrong place.
    appendTaskLog(QStringLiteral("INFO"), summary);
}

/// Replaces the stored per-role connection-recovery policies (retry interval, which statuses
/// are retryable) used for the camera, primary-PLC, and vision-output roles. Does not touch any
/// live connection, and does NOT reach roles that are already bound: bindRoleContext() copies the
/// policy into the role's context and recovery reads that copy, so a new policy takes effect only
/// when the role is next bound (setup(), or an active-camera change for the camera role). No
/// production caller exists — see the header.
void LocalizationRuntimeController::setRecoveryPolicies(
    const LocalizationRecoveryPolicy &cameraPolicy,
    const LocalizationRecoveryPolicy &plcPolicy,
    const LocalizationRecoveryPolicy &visionOutputPolicy)
{
    m_cameraRecoveryPolicy = cameraPolicy;
    m_plcRecoveryPolicy = plcPolicy;
    m_visionOutputRecoveryPolicy = visionOutputPolicy;
}

/// (Re)initializes the runtime from `context`: adopts its config, resets all role
/// bindings and cycle state, validates that the primary-PLC, vision-output, and every
/// declared camera role has a runner, wires the PLC valueChanged connection, binds the
/// fixed and active-camera roles, validates the active pattern group and camera
/// calibration, rebuilds the robot-picking checker, and (if everything validated) kicks
/// off connect requests for all three roles. Returns a SetupResult with `valid` true only
/// when all required roles/config resolved; `errors` lists every missing/invalid piece found.
LocalizationRuntimeController::SetupResult
LocalizationRuntimeController::setup(const RuntimeContext &context)
{
    SetupResult result;
    m_valid = false;
    m_context = context;
    m_config = context.config;
    m_signalMapper.configure(m_config);
    m_cycleState = CycleState::NotReady;
    m_lastExecuteTrigger = false;
    // Reset alongside m_lastExecuteTrigger. Two edge-detected inputs initialised by different
    // rules for no stated reason: without this, a bErrorReset left true across a restart is not
    // seen as a rising edge, and the first acknowledge of the session is swallowed.
    m_lastErrorReset = false;
    // A fresh context carries a fresh selection: whatever the master commanded against the
    // previous runtime session must not hold this one down.
    m_activeCameraSelectionRejected = false;
    m_activePatternGroupSelectionRejected = false;
    m_startupSelectionFault.clear();
    m_activeCycleId = 0;
    m_pendingCycleResult = CycleResult();
    resetRuntimeBindings();

    result.primaryPlcDeviceId = context.primaryPlcDeviceId;
    result.visionOutputDeviceId = context.visionOutputDeviceId;
    const QMap<int, QString> cameraMap = context.cameraDeviceIds;

    if (result.primaryPlcDeviceId.isEmpty() || !context.primaryPlcRunner) {
        result.errors.append(QStringLiteral("Missing primary_plc role runner."));
    }
    if (result.visionOutputDeviceId.isEmpty() || !context.visionOutputRunner) {
        result.errors.append(QStringLiteral("Missing vision_output role runner."));
    } else if (!context.visionOutputRunner->supportsResultOutput()) {
        // Refused here rather than at the first send: a runner that cannot output results would
        // otherwise be accepted at setup and only surface at the end of the first cycle, after a
        // grab and a match, as a send failure with no explanation of why the device was wrong.
        result.errors.append(
            QStringLiteral("Device bound to vision_output cannot output results: %1")
                .arg(result.visionOutputDeviceId));
    }

    if (cameraMap.isEmpty() || context.cameraRunners.isEmpty()) {
        result.errors.append(QStringLiteral("Missing camera_number role binding."));
    } else {
        for (auto it = cameraMap.cbegin(); it != cameraMap.cend(); ++it) {
            if (!context.cameraRunners.value(it.key())) {
                result.errors.append(QStringLiteral("camera_number_%1 runner is not registered: %2")
                                         .arg(it.key())
                                         .arg(it.value()));
            }
        }
    }

    disconnect(m_plcValueConnection);
    if (context.primaryPlcRunner) {
        m_plcValueConnection = connect(
            context.primaryPlcRunner.data(),
            &vc::runtime::PlcRunner::valueChanged,
            this,
            &LocalizationRuntimeController::handlePlcValues,
            Qt::UniqueConnection);
    }

    bindFixedRoleRunners();

    // AFTER bindFixedRoleRunners(), and that ordering is load-bearing: primaryPlcRunner() resolves
    // through m_recoveryContexts, which only that call populates. Run earlier, the gate finds no
    // tag provider and silently skips every orphan check — passing while proving nothing.
    // Still well before any role is asked to connect, which is what matters: a map that cannot
    // work is refused with a message naming the signal rather than started and left to go quiet.
    validateSignalMap(&result);

    m_activeCameraNumber = context.activeCameraNumber;
    m_activeCameraFromProjectDefault = false;

    // The PLC owns this signal, so ask the PLC first (Task C6). A commanded value beats the
    // project's binding order — including a commanded 0, which then faults through the validator
    // below exactly as a written 0 does. That equivalence is the point: C6 changes where the
    // startup value comes from, never what an invalid value means (D7).
    {
        int commanded = 0;
        bool typeMismatch = false;
        if (commandedIndexFromPlc(QStringLiteral("nActiveCamera"), &commanded, &typeMismatch)) {
            const IndexValidation check = validateCameraNumber(commanded);
            if (check.accepted()) {
                m_activeCameraNumber = commanded;
            } else {
                // A refused number is NOT adopted, exactly as setActiveCameraNumber() refuses to
                // adopt one: the runtime keeps a usable camera bound (the project default, below)
                // so that a later valid write can re-arm it with no operator action. The latch is
                // what keeps it out of Ready meanwhile.
                m_activeCameraSelectionRejected = true;
                m_startupSelectionFault = check.message;
            }
        } else if (typeMismatch) {
            result.errors.append(
                QStringLiteral("Signal \"nActiveCamera\" is mapped to a tag that does not hold a "
                               "number; check whether it is bound to a bit area."));
        }
    }

    if (m_activeCameraNumber < 0 && !cameraMap.isEmpty()) {
        // -1 means "first available". buildRuntimeContext() already resolves it before it gets
        // here, so this fallback is dead for the production caller — kept, and recorded rather
        // than deleted, because the contract tests construct RuntimeContexts directly.
        m_activeCameraNumber = cameraMap.firstKey();
        m_activeCameraFromProjectDefault = true;
    }

    // The gate backlog item 55 is about. Until this call, setup() ran NO index check: an
    // unregistered active camera fell straight through to validateActiveCameraCalibration() and
    // was reported as "Active camera calibration is invalid." — sending a commissioning engineer
    // to the calibration screen for what is a binding problem. The fault code was wrong for the
    // same reason: CalibrationInvalid for a camera that does not exist. It now reports
    // CameraNotRegistered, which is what the number actually failed to do.
    //
    // Only when the number actually resolved. A still-negative value here means the map was
    // empty, which setup() has already reported as a missing role binding — complaining that -1
    // is out of range on top of that would bury the real message under a derived one.
    const IndexValidation cameraCheck = m_activeCameraNumber < 0
                                            ? IndexValidation{}
                                            : validateCameraNumber(m_activeCameraNumber);
    if (!cameraCheck.accepted()) {
        result.errors.append(cameraCheck.message);
        publishBoolSignal(QStringLiteral("bTaskReady"), false);
        publishBoolSignal(QStringLiteral("bCameraValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(
            QStringLiteral("nFaultCode"),
            localizationFaultCodeValue(LocalizationFaultCode::CameraNotRegistered));
    }

    bindActiveCameraRole(m_activeCameraNumber);
    // Resolve the active camera's workspace HERE, not only on a camera change. Until this call
    // existed, a runtime that started and was never commanded to switch camera ran its whole
    // session with both workspace members default: useWorkspace false made the matcher search the
    // full frame instead of the commissioned ROI, and useConditionWorkspace false meant
    // outSideConditionRoiCheck() was never called, so every object kept
    // m_isOutsideConditionRoi == false and buildVisionOutputPositions() admitted it. Objects the
    // commissioning engineer had fenced out were converted to robot coordinates and sent as pick
    // targets, on the first cycle of every session, with nothing logged and every lamp green.
    applyActiveCameraWorkspace();
    m_activePatternGroupNumber = context.activePatternGroupNumber;
    m_activePatternGroupFromProjectDefault = false;

    // Same rule for the other index the PLC owns; see the camera block above.
    {
        int commanded = 0;
        bool typeMismatch = false;
        if (commandedIndexFromPlc(QStringLiteral("nActivePatternGroup"), &commanded,
                                  &typeMismatch)) {
            const IndexValidation check = validatePatternGroupNumber(commanded);
            if (check.accepted()) {
                m_activePatternGroupNumber = commanded;
            } else {
                m_activePatternGroupSelectionRejected = true;
                if (m_startupSelectionFault.isEmpty()) {
                    m_startupSelectionFault = check.message;
                }
            }
        } else if (typeMismatch) {
            result.errors.append(
                QStringLiteral("Signal \"nActivePatternGroup\" is mapped to a tag that does not "
                               "hold a number; check whether it is bound to a bit area."));
        }
    }

    if (m_activePatternGroupNumber < 0 && !context.patternGroups.isEmpty()) {
        m_activePatternGroupNumber = context.patternGroups.firstKey();
        m_activePatternGroupFromProjectDefault = true;
    }

    // Same rule as the camera above: a still-negative number means there were no groups to pick
    // from, and validateActivePatternGroup() reports that as "Active pattern group is missing.",
    // which is the message a commissioning engineer needs. A range complaint about -1 would
    // replace it with a worse one.
    const IndexValidation groupCheck = m_activePatternGroupNumber < 0
                                           ? IndexValidation{}
                                           : validatePatternGroupNumber(m_activePatternGroupNumber);
    if (!groupCheck.accepted()) {
        result.errors.append(groupCheck.message);
        publishBoolSignal(QStringLiteral("bTaskReady"), false);
        publishBoolSignal(QStringLiteral("bPatternValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternNotRegistered));
    }

    // Guarded by the index verdicts: an index that names nothing makes the content checks
    // meaningless, and running them anyway is what produced item 55's misleading message.
    if (groupCheck.accepted() && !validateActivePatternGroup(&result.errors)) {
        publishBoolSignal(QStringLiteral("bPatternValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternNotRegistered));
    }

    if (cameraCheck.accepted() && !validateActiveCameraCalibration(&result.errors)) {
        publishBoolSignal(QStringLiteral("bCameraValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid));
    }

    // Build the robot-pickability checker once for the runtime (active camera +
    // robot config now established); rebuilt later only on active-camera change.
    //
    // An ENABLED check with no usable checker is a hard setup error (Phase 9 / F1). It used to
    // be silent: a misspelled preset built a checker that answered false for every pose, so the
    // cell ran, matched, and sent nothing — reading on the dashboard as "nothing is pickable
    // today" rather than as a settings mistake. The two reasons are reported separately because
    // they send the engineer to different places.
    //
    // Suppressed only when the camera INDEX was itself refused: m_activeCameraNumber then names
    // no camera at all, so "camera 7 has no usable calibration" is derived noise on top of the
    // message that names the actual problem — the same rule the index/content checks above
    // follow. An invalid calibration does NOT suppress it: that the commissioned check cannot
    // run is a second fact about a second setting, and the operator needs both.
    const QString pickCheckError = rebuildPickingChecker();
    if (!pickCheckError.isEmpty() && cameraCheck.accepted()) {
        result.errors.append(pickCheckError);
    }

    result.valid = result.errors.isEmpty();
    m_valid = result.valid;

    // USER level and the task log, not LOG_DEV_ERR.
    //
    // These are the reasons the runtime is refusing to start. Field-confirmed 2026-09-08: the
    // signal-map gate wrote *"Signal \"Camera selection\" (nActiveCamera) is mapped to tag
    // IR01000, which the device 05 does not provide as a register."* to the developer log, while
    // the operator saw only "Runtime start aborted: setupTask failed" — which names nothing and
    // is unactionable. A refusal the operator cannot read is barely better than the silent start
    // the gate was built to replace.
    for (const QString &error : result.errors) {
        LOG_USER_ERR << error;
        appendTaskLog(QStringLiteral("ERROR"), error);
    }

    if (m_valid) {
        // Exactly once per beginRuntime(), on the ready path, before the connect requests — so it
        // is in the log ahead of whatever the connections then report.
        logStartupSelectionSummary();
        if (!m_startupSelectionFault.isEmpty()) {
            // Published here, not through result.errors, so the runtime stays VALID and can be
            // re-armed by the next good write. See m_startupSelectionFault for what happened when
            // this was a setup error instead.
            publishBoolSignal(QStringLiteral("bTaskReady"), false);
            publishBoolSignal(QStringLiteral("bTaskFault"), true);
            if (m_activeCameraSelectionRejected) {
                publishBoolSignal(QStringLiteral("bCameraValid"), false);
            }
            if (m_activePatternGroupSelectionRejected) {
                publishBoolSignal(QStringLiteral("bPatternValid"), false);
            }
            publishNumberSignal(
                QStringLiteral("nFaultCode"),
                localizationFaultCodeValue(m_activeCameraSelectionRejected
                                               ? LocalizationFaultCode::CameraNotRegistered
                                               : LocalizationFaultCode::PatternNotRegistered));
            m_cycleState = CycleState::Faulted;
            appendTaskLog(QStringLiteral("ERROR"), m_startupSelectionFault);
            LOG_USER_ERR << m_startupSelectionFault;
            emit runtimeFault(m_startupSelectionFault);
        }
        // Announce the resolved selection here too, not only from publishInitialReadyOutputs():
        // a runtime that sets up valid but is still waiting for a device to connect has already
        // chosen, and the master is entitled to read what it chose before it goes ready.
        publishNumberSignal(QStringLiteral("nActiveCameraStatus"), m_activeCameraNumber);
        publishNumberSignal(QStringLiteral("nActivePatternGroupStatus"), m_activePatternGroupNumber);
        requestRoleConnectNow(RunnerRole::PrimaryPlc);
        requestRoleConnectNow(RunnerRole::VisionOutput);
        requestRoleConnectNow(RunnerRole::Camera);
        if (allRequiredRolesHealthy()) {
            markRuntimeReady(QStringLiteral("Runtime ready."));
        } else {
            appendTaskLog(QStringLiteral("INFO"),
                          QStringLiteral("Runtime setup valid. Waiting for device connections."));
        }
    } else {
        m_cycleState = CycleState::Faulted;
    }

    return result;
}

/// Manual (non-PLC-triggered) request to start a localization cycle; behaves like an
/// execute-trigger rising edge. Logs and returns without effect if the runtime failed
/// setup, or if it is not currently in the ReadyForTrigger state.
void LocalizationRuntimeController::execute()
{
    if (!m_valid) {
        LOG_DEV_ERR << "LocalizationRuntimeController::execute - runtime is not set up";
        return;
    }

    if (m_cycleState != CycleState::ReadyForTrigger) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Manual localization request ignored: task is not ready."));
        return;
    }

    startCycle();
}

/// Slot for the primary PLC's raw tag values: maps each tag to a named signal via
/// m_signalMapper, emits signalChanged for all of them, and reacts to the four
/// recognized control signals — nActiveCamera (switches camera), nActivePatternGroup
/// (switches pattern group), bExecuteTrigger (starts a cycle on rising edge, or on
/// falling edge either clears bMatchingFinished and re-arms/recovers depending on whether
/// the pending cycle faulted), and bErrorReset (acknowledges a fault on rising edge).
void LocalizationRuntimeController::handlePlcValues(const QMap<QString, QVariant> &values)
{
    if (values.isEmpty()) {
        LOG_USER_ERR << "Communication device passed an empty values map.";
        return;
    }

    const QList<LocalizationSignalEvent> events = m_signalMapper.mapValues(values);
    for (const LocalizationSignalEvent &event : events) {
        emit signalChanged(event.name, event.value);

        // Both active-index signals are passed to their setter UNFILTERED. There used to be a
        // `> 0` guard here, which silently swallowed a written 0: no setter ran, so nothing
        // faulted and the task stayed Ready on the previously selected index. It was not a
        // contract — nothing documents 0, the "unset" sentinel in this class is -1, and the two
        // sibling entry points (TaskLocalization's onSignalChange* slots and the manual
        // queueSetActive* path) both pass the value through "regardless". Range and registration
        // are the setters' job, and they are the only place that decides what a number means.
        if (event.name == QStringLiteral("nActiveCamera")) {
            bool ok = false;
            const int cameraNumber = event.value.toInt(&ok);
            if (!ok) {
                // Distinct from a bad index: a non-numeric value on a number signal means the tag
                // is mapped to a bit area, which no index check can diagnose.
                reportSignalTypeMismatch(QStringLiteral("nActiveCamera"), event);
            } else {

                /// temp debug
                qDebug() << "Handle plc value: camera number:" << cameraNumber;


                setActiveCameraNumber(cameraNumber);
            }
        } else if (event.name == QStringLiteral("nActivePatternGroup")) {
            bool ok = false;
            const int patternGroupNumber = event.value.toInt(&ok);
            if (!ok) {
                reportSignalTypeMismatch(QStringLiteral("nActivePatternGroup"), event);
            } else {
                setActivePatternGroupNumber(patternGroupNumber);
            }
        } else if (event.name == QStringLiteral("bExecuteTrigger")) {
            const bool trigger = event.value.toBool();
            const bool risingEdge = trigger && !m_lastExecuteTrigger;
            const bool fallingEdge = !trigger && m_lastExecuteTrigger;
            m_lastExecuteTrigger = trigger;

            if (risingEdge) {
                if (m_cycleState == CycleState::ReadyForTrigger) {
                    startCycle();
                } else {
                    appendTaskLog(QStringLiteral("WARN"),
                                  QStringLiteral("Trigger ignored: task is not ready."));
                }
            } else if (fallingEdge &&
                       m_cycleState == CycleState::WaitingTriggerReset) {
                publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
                if (m_pendingCycleResult.faulted) {
                    // The fault now comes to rest: the trigger is low, so nothing else
                    // will move the runtime out of this state on its own. This is the
                    // point the runtime used to stay stuck at until the task was
                    // restarted, and it is where the way out has to be armed.
                    m_cycleState = CycleState::Recovering;
                    armFaultAutoRecovery();
                } else {
                    // Leave WaitingTriggerReset FIRST. This falling edge is the handshake
                    // completing — the PLC has dropped the trigger, so it has read the latched
                    // results — and it is the one legitimate exit from this state. markRuntimeReady()
                    // now refuses to re-arm while the handshake is outstanding, so it would refuse
                    // here too if the state were still set. The fault branch above already clears
                    // the state before arming its own way out, for the same reason.
                    //
                    // NotReady rather than ReadyForTrigger: markRuntimeReady() promotes it only if
                    // its validation passes, so a runtime whose pattern group or calibration went
                    // bad during the cycle stays honestly not-ready instead of being declared armed.
                    m_cycleState = CycleState::NotReady;
                    markRuntimeReady(QStringLiteral("Trigger reset. Runtime ready."));
                }
            }
        } else if (event.name == QStringLiteral("bErrorReset")) {
            const bool reset = event.value.toBool();
            const bool risingEdge = reset && !m_lastErrorReset;
            m_lastErrorReset = reset;

            // Rising edge only. A level-triggered acknowledge would re-clear the fault
            // on every PLC scan for as long as the operator holds the bit, which would
            // make a genuinely repeating fault invisible.
            if (risingEdge) {
                acknowledgeFault();
            }
        }
    }
}

/// Tears down all live signal/slot connections and recovery contexts for the three
/// device roles (PLC value updates, camera grab/command results, vision-output result)
/// ahead of a fresh setup().
void LocalizationRuntimeController::resetRuntimeBindings()
{
    cancelFaultAutoRecovery();
    m_lastRoleError.clear();

    disconnect(m_plcValueConnection);
    m_plcValueConnection = QMetaObject::Connection();
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    disconnect(m_visionOutputResultConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();
    m_visionOutputResultConnection = QMetaObject::Connection();

    clearRoleContext(RunnerRole::PrimaryPlc);
    clearRoleContext(RunnerRole::VisionOutput);
    clearRoleContext(RunnerRole::Camera);
}

/// Binds the two fixed device roles (primary PLC and vision output) from m_context to
/// their recovery contexts, if their runners are present.
void LocalizationRuntimeController::bindFixedRoleRunners()
{
    const QString plcId = m_context.primaryPlcDeviceId;
    if (m_context.primaryPlcRunner) {
        bindRoleContext(RunnerRole::PrimaryPlc,
                        m_context.primaryPlcRunner.data(),
                        plcId,
                        m_plcRecoveryPolicy);
    }

    const QString visionOutputId = m_context.visionOutputDeviceId;
    if (m_context.visionOutputRunner) {
        bindRoleContext(RunnerRole::VisionOutput,
                        m_context.visionOutputRunner.data(),
                        visionOutputId,
                        m_visionOutputRecoveryPolicy);
    }
}

/// Binds the Camera role's recovery context to the runner registered for
/// `cameraNumber`; clears the role context instead if no device id/runner is registered
/// for that camera number.
/// @param cameraNumber the camera role number to bind as active
void LocalizationRuntimeController::bindActiveCameraRole(int cameraNumber)
{
    const QString cameraId = m_context.cameraDeviceIds.value(cameraNumber);
    auto runner = m_context.cameraRunners.value(cameraNumber);
    if (cameraId.isEmpty() || !runner) {
        clearRoleContext(RunnerRole::Camera);
        return;
    }

    bindRoleContext(RunnerRole::Camera,
                    runner.data(),
                    cameraId,
                    m_cameraRecoveryPolicy);
}

/// Replaces the recovery context for `role` (clearing any previous one) and connects the
/// role-specific connectStatusChanged/errorOccurred handlers on `runner`. No-op if
/// `runner` is null.
/// @param role which device role this binding is for
/// @param runner the device runner backing the role
/// @param deviceId the device id associated with `runner`, stored for logging/messages
/// @param policy the recovery policy (retry limits/interval) to apply for this role
void LocalizationRuntimeController::bindRoleContext(
    RunnerRole role,
    vc::runtime::IDeviceRunner *runner,
    const QString &deviceId,
    const LocalizationRecoveryPolicy &policy)
{
    clearRoleContext(role);

    if (!runner) {
        return;
    }

    RoleRecoveryContext context;
    context.roleName = policy.roleName;
    context.deviceId = deviceId;
    context.runner = runner;
    context.policy = policy;

    // The two handles are stored so clearRoleContext() can drop exactly these and nothing else.
    switch (role) {
    case RunnerRole::PrimaryPlc:
        context.statusConnection =
            connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onPrimaryPlcStatusChanged,
                Qt::UniqueConnection);
        context.errorConnection =
            connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onPrimaryPlcError,
                Qt::UniqueConnection);
        // Only this role writes, so only this role reports completions (Phase 9 / E4). The
        // runner is a PlcRunner by construction for PrimaryPlc; the cast is checked rather than
        // assumed because a future role binding could hand something else in.
        if (auto *plcRunner = qobject_cast<vc::runtime::PlcRunner *>(runner)) {
            context.writeConnection =
                connect(plcRunner, &vc::runtime::PlcRunner::writeFinished,
                    this, &LocalizationRuntimeController::onPlcWriteFinished,
                    Qt::UniqueConnection);
        }
        break;
    case RunnerRole::VisionOutput:
        context.statusConnection =
            connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onVisionOutputStatusChanged,
                Qt::UniqueConnection);
        context.errorConnection =
            connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onVisionOutputError,
                Qt::UniqueConnection);
        break;
    case RunnerRole::Camera:
        context.statusConnection =
            connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onCameraStatusChanged,
                Qt::UniqueConnection);
        context.errorConnection =
            connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onCameraError,
                Qt::UniqueConnection);
        break;
    }

    m_recoveryContexts.insert(roleKey(role), context);
}

/// Disconnects exactly the two connections bindRoleContext() made for `role` and removes its
/// recovery context, if one is registered.
///
/// @warning **Never widen this back to `disconnect(context.runner, nullptr, this, nullptr)`.**
/// One device can legitimately serve two roles: a ModbusTcpServerDevice fills `primary_plc` and
/// `vision_output` in the same task, which is the configuration Phase 8/B1 made possible and the
/// one running on the owner's cell. The blanket form removed EVERY signal from that runner to this
/// controller, so clearing either role also dropped `m_plcValueConnection` — and with it every
/// bExecuteTrigger, bErrorReset and index write — while `bTaskReady` stayed true and the device
/// still reported Connected. Nothing failed and nothing was logged.
///
/// That was survivable only by line order: `setup()` re-made `m_plcValueConnection` a few lines
/// before `bindFixedRoleRunners()`. Storing the handles makes the safety structural instead, so
/// the ordering in `setup()` is no longer load-bearing.
void LocalizationRuntimeController::clearRoleContext(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    const RoleRecoveryContext context = m_recoveryContexts.value(key);
    disconnect(context.statusConnection);
    disconnect(context.errorConnection);
    disconnect(context.writeConnection);
    m_recoveryContexts.remove(key);

    // A pending write belongs to the runner that is going away; its completion can no longer
    // arrive, and a retry aimed at it would be aimed at nothing.
    if (role == RunnerRole::PrimaryPlc) {
        clearPendingWrites();
    }
}

/// Checks that the PrimaryPlc, VisionOutput, and Camera roles are all bound and their
/// runner's device reports ConnectStatus::Connected.
/// @return true only if all three required roles are present and healthy
bool LocalizationRuntimeController::allRequiredRolesHealthy() const
{
    for (RunnerRole role : {RunnerRole::PrimaryPlc, RunnerRole::VisionOutput, RunnerRole::Camera}) {
        const int key = roleKey(role);
        if (!m_recoveryContexts.contains(key)) {
            return false;
        }

        const RoleRecoveryContext context = m_recoveryContexts.value(key);
        if (!context.runner || !context.runner->device() ||
            !isHealthyStatus(context.runner->device()->connectStatus())) {
            return false;
        }
    }

    return true;
}

/// Transitions to CycleState::ReadyForTrigger, publishes the initial ready outputs, logs
/// `message` at INFO, and emits runtimeReady(message). No-op while a cycle is running or while
/// the trigger handshake is still outstanding, or if setup is invalid, a required role is
/// unhealthy, the pattern group is invalid, or the active camera calibration is invalid.
/// @param message text to log and include in the runtimeReady signal
void LocalizationRuntimeController::markRuntimeReady(const QString &message)
{
    // WaitingTriggerReset counts as busy, not as idle. The PLC is still holding bExecuteTrigger
    // high and has not yet read bMatchingFinished / bMatchingDetected / nDetectedNumber, which
    // publishCycleSuccessOutputs() latched for it. Re-arming here would run
    // publishInitialReadyOutputs() and wipe all three before the PLC consumed them, breaking the
    // rising-edge handshake the falling-edge path exists to protect.
    //
    // This guard is why the two active-index setters below can safely re-arm on their success
    // path: without it, a PLC write of nActiveCamera or nActivePatternGroup arriving while the
    // trigger was still asserted would silently swallow a completed cycle's result.
    if (m_cycleState == CycleState::Running ||
        m_cycleState == CycleState::WaitingTriggerReset) {
        return;
    }

    if (!m_valid || !allRequiredRolesHealthy() ||
        !validateActivePatternGroup() ||
        !validateActiveCameraCalibration()) {
        return;
    }

    // A refused index is never adopted, so the checks above cannot see one: they read the last
    // GOOD camera number and pattern group, both of which still validate. Every re-arm path in
    // this class funnels through here, so this is the one place that can make a refusal stick —
    // and it must, or a valid write to one index signal silently forgives a refusal on the
    // other, which is exactly what a valid nActivePatternGroup used to do to a rejected
    // nActiveCamera: Ready again, bCameraValid true again, with the master's command register
    // still holding the number the task had refused.
    //
    // Each latch is cleared only by an accepted value for its OWN signal. bErrorReset does not
    // clear it either, matching how a PatternNotRegistered fault behaves: acknowledging is not
    // repairing.
    if (m_activeCameraSelectionRejected || m_activePatternGroupSelectionRejected) {
        return;
    }

    m_cycleState = CycleState::ReadyForTrigger;
    // Publishing bCameraValid/bPatternValid as unconditional true is only honest because the
    // gate above is exhaustive: roles healthy covers the camera binding, the two validate*
    // calls cover calibration and train images, and the two latches cover a refused selection.
    // Anything added to the meaning of either flag has to be added to that gate as well.
    publishInitialReadyOutputs();
    appendTaskLog(QStringLiteral("INFO"), message);
    emit runtimeReady(message);
}

/// Emits signalChanged(name, value) and, if `name` maps to a PLC tag via the signal
/// mapper, also writes `value` to that tag as digital I/O through the primary PLC
/// runner. No PLC write occurs if no primary PLC runner is currently bound.
/// @param name the logical signal name (e.g. "bTaskReady")
/// @param value the boolean value to publish
/// The five outputs the PLC's own program blocks on. Everything else this task publishes is
/// advisory: a lost bMatchingLowArea costs a lamp, a lost bMatchingFinished hangs the PLC.
bool LocalizationRuntimeController::isHandshakeSignal(const QString &signalName)
{
    static const QSet<QString> kHandshakeSignals = {
        QStringLiteral("bTaskReady"),        QStringLiteral("bMatchingFinished"),
        QStringLiteral("bTaskFault"),        QStringLiteral("nFaultCode"),
        QStringLiteral("nDetectedNumber"),
    };
    return kHandshakeSignals.contains(signalName);
}

/// Records `id` as owed a completion, unless tracking does not apply.
void LocalizationRuntimeController::trackHandshakeWrite(quint64 id, const QString &signalName,
                                                        const QString &tag, bool isBool,
                                                        bool boolValue, int wordValue,
                                                        int attempts)
{
    if (id == 0 || !isHandshakeSignal(signalName)) {
        return;
    }

    // The abort path publishes bTaskFault and nFaultCode — two of the five — over the link that
    // just failed. Tracking those would re-enter this machinery, and on a dead link it would
    // never terminate. Escalation is attempted once; if it too fails, the task log and
    // runtimeFault() carry the news, and they reach the operator without the PLC.
    if (m_plcWriteEscalating) {
        return;
    }

    // A disconnected role is the RECOVERY policy's business. Counting its writes against this
    // budget would raise 301 on every cable pull, on top of the PlcLost the recovery path
    // already reports — two faults for one cause, the second of them wrong.
    const RoleRecoveryContext &plc = m_recoveryContexts[roleKey(RunnerRole::PrimaryPlc)];
    if (!plc.runner || !plc.runner->device()
        || plc.runner->device()->connectStatus() != vc::device::ConnectStatus::Connected) {
        return;
    }

    TrackedWrite write;
    write.signalName = signalName;
    write.tag = tag;
    write.isBool = isBool;
    write.boolValue = boolValue;
    write.wordValue = wordValue;
    write.attempts = attempts;
    m_pendingWrites.insert(id, write);
}

void LocalizationRuntimeController::publishBoolSignal(const QString &name, bool value)
{
    emit signalChanged(name, value);

    const QString tag = m_signalMapper.tagForSignalName(name);
    if (tag.isEmpty()) {
        return;
    }

    if (auto *runner = primaryPlcRunner()) {
        const quint64 id = runner->requestWriteDigitalIo(tag, value);
        trackHandshakeWrite(id, name, tag, /*isBool*/ true, value, 0, /*attempts*/ 1);
    }
}

/// Emits signalChanged(name, value) and, if `name` maps to a PLC tag via the signal
/// mapper, also writes `value` (narrowed to qint16) to that tag as word I/O through the
/// primary PLC runner. No PLC write occurs if no primary PLC runner is currently bound.
/// @param name the logical signal name (e.g. "nFaultCode")
/// @param value the integer value to publish; truncated to 16 bits when written to the PLC
void LocalizationRuntimeController::publishNumberSignal(const QString &name, int value)
{
    emit signalChanged(name, value);

    const QString tag = m_signalMapper.tagForSignalName(name);
    if (tag.isEmpty()) {
        return;
    }

    if (auto *runner = primaryPlcRunner()) {
        const quint64 id = runner->requestWriteWordIo(tag, static_cast<qint16>(value));
        trackHandshakeWrite(id, name, tag, /*isBool*/ false, false, value, /*attempts*/ 1);
    }
}

/// Matches a completion against the pending set: forget it on success, re-issue while the budget
/// lasts, escalate when it is gone.
///
/// An id that is not in the set is not an error — every advisory write and every write issued
/// while the role was disconnected is deliberately untracked, and they all report here too.
void LocalizationRuntimeController::onPlcWriteFinished(quint64 id, bool ok, const QString &message)
{
    const auto it = m_pendingWrites.constFind(id);
    if (it == m_pendingWrites.cend()) {
        return;
    }
    const TrackedWrite write = it.value();
    m_pendingWrites.erase(it);

    if (ok) {
        return;
    }

    if (write.attempts < kPlcWriteRetryBudget) {
        LOG_DEV_INFO << "Handshake write failed; retrying."
                     << "signal=" << write.signalName << "tag=" << write.tag
                     << "attempt=" << write.attempts << "reason=" << message;
        // Re-issued after a delay rather than immediately: the failure this budget exists to
        // absorb is a momentary collision, and an instant re-issue would hit the same one.
        m_plcWriteRetryQueue.append(id);
        m_pendingWrites.insert(id, write);   // put it back for reissueTrackedWrite() to read
        if (!m_plcWriteRetryTimer.isActive()) {
            m_plcWriteRetryTimer.start(kPlcWriteRetryDelayMs);
        }
        return;
    }

    escalatePlcWriteFailure(write, message);
}

/// Re-issues the write recorded under `id` under a NEW id, carrying the attempt count forward.
void LocalizationRuntimeController::reissueTrackedWrite(quint64 id)
{
    const auto it = m_pendingWrites.constFind(id);
    if (it == m_pendingWrites.cend()) {
        return;   // cancelled with its cycle
    }
    const TrackedWrite write = it.value();
    m_pendingWrites.erase(it);

    auto *runner = primaryPlcRunner();
    if (!runner) {
        // The role went away between the failure and the retry. That is the recovery policy's
        // problem, not a write fault — see trackHandshakeWrite().
        return;
    }

    const quint64 newId = write.isBool
                              ? runner->requestWriteDigitalIo(write.tag, write.boolValue)
                              : runner->requestWriteWordIo(write.tag,
                                                           static_cast<qint16>(write.wordValue));
    trackHandshakeWrite(newId, write.signalName, write.tag, write.isBool, write.boolValue,
                        write.wordValue, write.attempts + 1);
}

/// Aborts the cycle with PlcWriteFailed, with the retry machinery held off for the duration.
void LocalizationRuntimeController::escalatePlcWriteFailure(const TrackedWrite &write,
                                                            const QString &reason)
{
    const QString detail =
        QStringLiteral("Handshake output \"%1\" could not be written to tag %2 after %3 attempts: "
                       "%4")
            .arg(write.signalName, write.tag)
            .arg(kPlcWriteRetryBudget)
            .arg(reason);

    LOG_USER_ERR << detail;

    // The guard is the whole recursion trap, closed in code rather than only in the test:
    // abortCycle() publishes bTaskFault and nFaultCode, both tracked signals, over the link that
    // just refused a write. Without this they would be tracked, fail, retry, and escalate again.
    m_plcWriteEscalating = true;
    abortCycle(LocalizationFaultCode::PlcWriteFailed, detail);
    m_plcWriteEscalating = false;

    // Said again through the two channels that do not need the PLC. If the link is genuinely
    // gone, the publishes inside abortCycle() reached nobody and this is the only report there is.
    emit runtimeFault(detail);
}

/// Forgets every outstanding write and cancels a pending retry.
void LocalizationRuntimeController::clearPendingWrites()
{
    m_plcWriteRetryTimer.stop();
    m_plcWriteRetryQueue.clear();
    m_pendingWrites.clear();
}

/// Publishes the PLC output signal set for a fresh "ready and idle" state: task/camera/
/// pattern valid, matching not busy/finished/detected/low-area, no fault, zero detected
/// count, and fault code None.
void LocalizationRuntimeController::publishInitialReadyOutputs()
{
    // Before bTaskReady, deliberately: a dashboard or a PLC program that reacts to the ready
    // edge must already be able to read which camera and group it is ready ON.
    publishNumberSignal(QStringLiteral("nActiveCameraStatus"), m_activeCameraNumber);
    publishNumberSignal(QStringLiteral("nActivePatternGroupStatus"), m_activePatternGroupNumber);
    publishBoolSignal(QStringLiteral("bTaskReady"), true);
    publishBoolSignal(QStringLiteral("bCameraValid"), true);
    publishBoolSignal(QStringLiteral("bPatternValid"), true);
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

/// Publishes the PLC output signal set for the start of a localization cycle: task no
/// longer ready, matching busy, finished/detected/low-area/fault cleared, and detected
/// count/fault code reset to 0/None.
void LocalizationRuntimeController::publishCycleStartOutputs()
{
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    publishBoolSignal(QStringLiteral("bMatchingBusy"), true);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

/// Publishes the PLC output signal set for a completed, non-faulted cycle: matching no
/// longer busy, finished, detected flag and low-area flag reflect `result`, no fault, and
/// nDetectedNumber/nFaultCode(None) reflect `result.detectedNumber`.
/// @param result the completed cycle's outcome to report
void LocalizationRuntimeController::publishCycleSuccessOutputs(const CycleResult &result)
{
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), true);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), result.detectedNumber > 0);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), result.lowArea);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), result.detectedNumber);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

/// Publishes the PLC output signal set for a faulted cycle: matching no longer busy,
/// finished, task not ready, detected/low-area cleared, task fault raised, detected count
/// reset to 0, and nFaultCode set to `code`.
/// @param code the fault code to publish
void LocalizationRuntimeController::publishCycleFaultOutputs(LocalizationFaultCode code)
{
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), true);
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), true);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"), localizationFaultCodeValue(code));
}

/// Builds a TaskLogEntry timestamped with the current local date/time and emits
/// taskLogAppended with it.
/// @param severity log level label (e.g. "INFO", "WARN", "ERROR")
/// @param message the log message text
void LocalizationRuntimeController::appendTaskLog(const QString &severity,
                                                  const QString &message)
{
    TaskLogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.severity = severity;
    entry.message = message;
    emit taskLogAppended(entry);
}

void LocalizationRuntimeController::reportSignalTypeMismatch(const QString &signalName,
                                                             const LocalizationSignalEvent &event)
{
    const QString message =
        QStringLiteral("%1 is mapped to %2, which did not read as a number. A number signal must "
                       "be mapped to a register area, not to a coil or discrete input.")
            .arg(signalName, event.tag);

    appendTaskLog(QStringLiteral("WARN"), message);
    LOG_USER_ERR << message;

    // Faulted like a bad index, and for the same reason: the runtime cannot know which camera or
    // pattern group the PLC meant, so continuing on the previous selection would run the cell on
    // an intent nobody expressed. The fault code names the subsystem; the message above is what
    // says the mapping, not the value, is wrong.
    const bool isCamera = signalName == QStringLiteral("nActiveCamera");
    // Latched and reported exactly like a bad index. It used to publish neither the domain flag
    // nor a latch, so this failure alone left bCameraValid/bPatternValid standing at true
    // underneath a fault — and the next re-arm cleared it without a valid value ever arriving.
    (isCamera ? m_activeCameraSelectionRejected
              : m_activePatternGroupSelectionRejected) = true;
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    publishBoolSignal(isCamera ? QStringLiteral("bCameraValid")
                               : QStringLiteral("bPatternValid"),
                      false);
    publishBoolSignal(QStringLiteral("bTaskFault"), true);
    // Same code as an index that names nothing, because the outcome is the same: no camera or
    // group is selected. A mis-bound tag carries no commanded number at all, so it can hardly have
    // named a registered one. The message above is what distinguishes the two causes.
    publishNumberSignal(
        QStringLiteral("nFaultCode"),
        localizationFaultCodeValue(isCamera ? LocalizationFaultCode::CameraNotRegistered
                                            : LocalizationFaultCode::PatternNotRegistered));
    m_cycleState = CycleState::Faulted;
    emit runtimeFault(message);
}

/// Checks that the active pattern group (falling back to the first available group if
/// none is set) exists and contains at least one pattern with a non-empty raw train
/// image.
bool LocalizationRuntimeController::validateActivePatternGroup(QStringList *errors) const
{
    int groupNumber = m_activePatternGroupNumber;
    if (groupNumber < 0 && !m_context.patternGroups.isEmpty()) {
        groupNumber = m_context.patternGroups.firstKey();
    }

    auto group = m_context.patternGroups.value(groupNumber);
    if (!group) {
        if (errors) errors->append(QStringLiteral("Active pattern group is missing."));
        return false;
    }

    for (const auto &pattern : group->patterns()) {
        if (pattern && !pattern->config().m_rawImage.empty()) {
            return true;
        }
    }

    if (errors) errors->append(QStringLiteral("Active pattern group has no train images."));
    return false;
}

/// Looks up the calibrator registered for the active camera number and checks that it is
/// calibrated.
bool LocalizationRuntimeController::validateActiveCameraCalibration(QStringList *errors) const
{
    const calib::Calibrator calibrator = m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated()) {
        if (errors) errors->append(QStringLiteral("Active camera calibration is invalid."));
        return false;
    }

    return true;
}

/// Resets m_pickingChecker and, if robot-kinematic checking is enabled in
/// m_context.robotCheckConfig and the active camera's calibrator is calibrated, rebuilds
/// it as a new RobotKinematicPickingChecker for that calibrator/config. Called once at
/// setup() and again whenever the active camera changes.
/// @return an empty string when the checker is in the state the config asks for; otherwise
///         the reason an ENABLED check has no usable checker, for setup() to refuse on.
QString LocalizationRuntimeController::rebuildPickingChecker()
{
    m_pickingChecker.reset();
    if (!m_context.robotCheckConfig.enabled)
        return QString();

    const calib::Calibrator calibrator =
        m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated()) {
        return QStringLiteral(
                   "Robot pick check is enabled, but camera %1 has no usable calibration, so no "
                   "pick pose can be converted to robot coordinates to check.")
            .arg(m_activeCameraNumber);
    }

    auto checker = std::make_shared<RobotKinematicPickingChecker>(
        calibrator, m_context.robotCheckConfig);
    // Installed even when the preset did not resolve. An unusable preset must keep failing
    // CLOSED — every pose unpickable — because the alternative, leaving the checker null,
    // means "matching not gated" and would send unreachable poses to the robot. setup()
    // refuses the runtime on the message below; this only decides which way it fails if
    // some other path ever reaches here without that refusal.
    m_pickingChecker = checker;
    if (!checker->isReady()) {
        return QStringLiteral(
                   "Robot pick check is enabled, but robot preset \"%1\" is not registered; every "
                   "pose would be reported unpickable.")
            .arg(m_context.robotCheckConfig.presetName);
    }
    return QString();
}

/// Returns the currently-active pattern group (falling back to the first available
/// group in m_context.patternGroups if none is explicitly set), for use as a stable
/// snapshot handed off to the matching request.
/// @return the active pattern group, or null if none is registered
std::shared_ptr<mtc::MatchGroup>
LocalizationRuntimeController::snapshotActivePatternGroup() const
{
    int groupNumber = m_activePatternGroupNumber;
    if (groupNumber < 0 && !m_context.patternGroups.isEmpty()) {
        groupNumber = m_context.patternGroups.firstKey();
    }

    return m_context.patternGroups.value(groupNumber);
}

/// Converts each matched object's image-space center/angle to a robot world-space
/// position via the active camera's calibrator (applying the camera-workspace crop
/// offset when in use), and builds the send list: an object is included (capped at 2
/// positions) only if it has no collision, is not outside the condition ROI, and is
/// reported pickable; skipped objects get a descriptive `status` string. Also fills
/// `rows` (if given) with one ResultRow per matched object for UI/logging, in original
/// match order.
QVector<vc::device::VisionOutputPosition>
LocalizationRuntimeController::buildVisionOutputPositions(
    const mtc::MatchResult &matchResult,
    QVector<ResultRow> *rows,
    LocalizationFaultCode *faultCode) const
{
    QVector<vc::device::VisionOutputPosition> positions;
    if (faultCode) {
        *faultCode = LocalizationFaultCode::None;
    }

    const calib::Calibrator calibrator = m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated()) {
        if (faultCode) *faultCode = LocalizationFaultCode::CalibrationInvalid;
        return positions;
    }

    int index = 1;
    for (const mtc::MatchedObject &object : matchResult.Objects) {
        ResultRow row;
        row.index = index++;
        row.patternName = QString::fromStdWString(object.pattern_name);
        row.score = object.matched_Score;
        row.imageX = object.point_Center.x;
        row.imageY = object.point_Center.y;
        row.imageR = object.point_angle;

        // image to real world position
        cv::Point2f pick_center = object.point_Center;
        if (m_context.activeCameraWorkspace.useWorkspace) {
            pick_center += matchResult.cropOffsetPoint;
        }

        const cv::Point3f pickPoint = calibrator.imageToRobot(pick_center);
        // Top-down pick rotation about Z, negated to match the robot's Z convention.
        // This is the axis the pre-Phase-5 4-axis contract called `r`; it is emitted as
        // `rz` now that the pattern can add its own Z rotation on top.
        const double worldYaw = -calibrator.rotateImageToRobot(object.point_angle);

        // Compose the pattern's 6-axis picking offset in the TOOL frame:
        //   world = pick * offset
        // This is the same composition RobotKinematicPickingChecker::isPickable uses,
        // so the advisory pickability verdict and the pose actually emitted cannot
        // drift apart. With a zero rotation offset the product reduces exactly to the
        // previous Rz-only Calibrator::translateWithZAxis() result (Rz rotates x/y and
        // passes z through), so existing patterns are unaffected.
        const RobotKinematics::Pose pickPose = RobotKinematics::Pose::fromXYZRPY_mm_deg(
            pickPoint.x, pickPoint.y, pickPoint.z, 0.0, 0.0, worldYaw);
        const RobotKinematics::Pose offsetPose = RobotKinematics::Pose::fromXYZRPY_mm_deg(
            object.point_offset.x, object.point_offset.y, object.point_offset.z,
            object.point_rotation_offset.x, object.point_rotation_offset.y,
            object.point_rotation_offset.z);

        const Eigen::Vector3d worldTranslation = (pickPose * offsetPose).translation_m();

        row.world.x = RobotKinematics::units::toMm(worldTranslation.x());
        row.world.y = RobotKinematics::units::toMm(worldTranslation.y());
        row.world.z = RobotKinematics::units::toMm(worldTranslation.z());

        // Orientation sent to the robot: the pattern's rotation offset, with the pick
        // yaw folded into the Z axis because both act about Z. A pattern with no
        // rotation offset therefore emits exactly the pre-Phase-5 pose (rx = ry = 0,
        // rz = the old `r`).
        row.world.rx = object.point_rotation_offset.x;
        row.world.ry = object.point_rotation_offset.y;
        row.world.rz = worldYaw + object.point_rotation_offset.z;

        // if (object.hasCollision() && object.isOutsideConditionRoi()) {
        //     row.status = QStringLiteral("Skipped: collision, outside");
        // } else if (object.hasCollision()) {
        //     row.status = QStringLiteral("Skipped: collision");
        // } else if (object.isOutsideConditionRoi()) {
        //     row.status = QStringLiteral("Skipped: outside");
        // }  else {
        //     row.status = QStringLiteral("Sent");
        //     positions.append(row.world);
        // }

        if ((!object.hasCollision()) && (!object.isOutsideConditionRoi()) && object.isPossibleToPick()) {
            if (positions.size() < 2) {
                row.status = QStringLiteral("Sent");
                positions.append(row.world);
            } else {
                row.status = QStringLiteral("Skipped");
            }
        } else {
            QString tempstr = object.hasCollision() ? tr("Collision") : "";
            tempstr += (object.isOutsideConditionRoi()
                                    ? ((tempstr.isEmpty() ? "" : ", ") + tr("Outside"))
                                    : "");
            tempstr += (!object.isPossibleToPick()
                                    ? ((tempstr.isEmpty() ? "" : ", ") + tr("Unpickable"))
                                    : "");
            row.status = QStringLiteral("Skipped: %1").arg(tempstr);
        }

        if (rows) {
            rows->append(row);
        }
    }

    return positions;
}

/// Returns the CameraRunner bound to the Camera role's recovery context.
/// @return the active camera runner, or null if no camera role is currently bound
vc::runtime::CameraRunner *LocalizationRuntimeController::activeCameraRunner() const
{
    const int key = roleKey(RunnerRole::Camera);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return qobject_cast<vc::runtime::CameraRunner *>(m_recoveryContexts.value(key).runner.data());
}

/// Returns the PlcRunner bound to the PrimaryPlc role's recovery context.
/// @return the primary PLC runner, or null if no primary-PLC role is currently bound
vc::runtime::PlcRunner *LocalizationRuntimeController::primaryPlcRunner() const
{
    const int key = roleKey(RunnerRole::PrimaryPlc);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return qobject_cast<vc::runtime::PlcRunner *>(m_recoveryContexts.value(key).runner.data());
}

/// Returns the runner bound to the VisionOutput role's recovery context.
/// @return the bound runner, or null if no vision-output role is currently bound
/// @note No downcast: the role is filled by capability, so the concrete runner type is not
///       knowable here and must not be assumed.
vc::runtime::IDeviceRunner *LocalizationRuntimeController::visionOutputRunner() const
{
    const int key = roleKey(RunnerRole::VisionOutput);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return m_recoveryContexts.value(key).runner.data();
}

/// Starts a new localization cycle: revalidates the pattern group, camera calibration,
/// and camera runner availability (aborting the cycle with the matching fault code on
/// failure); otherwise transitions to CycleState::Running, bumps m_activeCycleId,
/// resets m_pendingCycleResult, publishes the cycle-start outputs, emits
/// runtimeCycleStarted, wires the camera's grabFinished (single-shot)/commandFinished
/// connections, and requests a single-shot grab.
/// Elapsed milliseconds since the in-flight cycle's trigger-accept, from the one monotonic
/// clock (Phase 9 / F3).
/// @return the elapsed time, or 0.0 if no cycle has started this session
double LocalizationRuntimeController::cycleElapsedMs() const
{
    if (!m_cycleClock.isValid()) {
        return 0.0;
    }
    // nsecsElapsed(), not elapsed(): elapsed() truncates to whole milliseconds, and a grab plus a
    // match can both finish inside one of those on a fast cell — which would make the stages look
    // simultaneous and the breakdown useless exactly where it is most interesting.
    return static_cast<double>(m_cycleClock.nsecsElapsed()) / 1'000'000.0;
}

/// Renders a cycle's stage breakdown as one compact, readable clause for the task log.
/// Each stage is the time spent IN that stage (the difference between consecutive boundaries),
/// not the cumulative time, which is what a reader is actually trying to attribute.
/// @param timings the cycle's boundaries; unset ones are omitted rather than shown as 0
/// @return e.g. "cycle=142.3 ms (grab 98.1, match 39.7, send 4.2, publish 0.3)"
QString LocalizationRuntimeController::formatCycleTimings(const CycleTimings &timings)
{
    if (!timings.outputsPublishedMs) {
        return QStringLiteral("cycle=incomplete");
    }

    QStringList stages;
    // Each stage needs BOTH its own boundary and the one before it; a gap means the stage cannot
    // be attributed and is left out rather than guessed from a zero origin.
    auto stage = [&stages](const char *name, std::optional<double> from,
                           std::optional<double> to) {
        if (from && to) {
            stages << QStringLiteral("%1 %2")
                          .arg(QLatin1String(name))
                          .arg(*to - *from, 0, 'f', 1);
        }
    };
    stage("grab", std::optional<double>(0.0), timings.grabFinishedMs);
    stage("match", timings.grabFinishedMs, timings.matchingFinishedMs);
    stage("send", timings.matchingFinishedMs, timings.sendFinishedMs);
    stage("publish", timings.sendFinishedMs, timings.outputsPublishedMs);

    return QStringLiteral("cycle=%1 ms (%2)")
        .arg(*timings.outputsPublishedMs, 0, 'f', 1)
        .arg(stages.join(QStringLiteral(", ")));
}

void LocalizationRuntimeController::startCycle()
{
    if (m_cycleState != CycleState::ReadyForTrigger) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Cycle start ignored: runtime is not ready."));
        return;
    }

    if (!validateActivePatternGroup()) {
        abortCycle(LocalizationFaultCode::PatternNotRegistered,
                   QStringLiteral("Active pattern group is invalid."));
        return;
    }

    if (!validateActiveCameraCalibration()) {
        abortCycle(LocalizationFaultCode::CalibrationInvalid,
                   QStringLiteral("Active camera calibration is invalid."));
        return;
    }

    auto *cameraRunner = activeCameraRunner();
    if (!cameraRunner) {
        abortCycle(LocalizationFaultCode::CameraLost,
                   QStringLiteral("Active camera runner is not available."));
        return;
    }

    // A new cycle supersedes any fault still waiting to auto-clear: the runtime is
    // demonstrably armed again, and letting the old countdown fire mid-cycle would
    // publish a "recovered" state on top of a running one.
    cancelFaultAutoRecovery();

    m_cycleState = CycleState::Running;
    ++m_activeCycleId;
    m_pendingCycleResult = CycleResult();
    // Stage 0 — trigger accepted (Phase 9 / F3). Started before the outputs are published so the
    // handshake write itself is inside the measured cycle: on a slow PLC link that write is part
    // of what the customer experiences as cycle time, and excluding it would flatter the number.
    m_cycleClock.start();
    publishCycleStartOutputs();
    emit runtimeCycleStarted(QStringLiteral("Localization cycle started."));
    appendTaskLog(QStringLiteral("INFO"),
                  QStringLiteral("Trigger accepted. Localization cycle started."));

    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    m_cameraGrabConnection = connect(cameraRunner,
                                     &vc::runtime::CameraRunner::grabFinished,
                                     this,
                                     &LocalizationRuntimeController::onCameraGrabFinished,
                                     Qt::SingleShotConnection);
    m_cameraCommandConnection = connect(cameraRunner,
                                        &vc::runtime::CameraRunner::commandFinished,
                                        this,
                                        &LocalizationRuntimeController::onCameraCommandFinished,
                                        Qt::UniqueConnection);
    cameraRunner->requestSingleShot();
}

/// Aborts the in-progress cycle: disconnects the camera grab/command and vision-output
/// result connections, bumps m_activeCycleId (invalidating any pending async result),
/// marks m_pendingCycleResult as faulted with `code`, publishes the cycle-fault outputs,
/// emits cycleResultUpdated, logs an ERROR entry with `message`, transitions to
/// WaitingTriggerReset (if the execute trigger is still asserted) or Recovering
/// otherwise, and emits runtimeRecovering.
/// @param code the fault code to record and publish
/// @param message human-readable description of what failed, used in the log entry
void LocalizationRuntimeController::abortCycle(LocalizationFaultCode code,
                                               const QString &message)
{
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    disconnect(m_visionOutputResultConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();
    m_visionOutputResultConnection = QMetaObject::Connection();
    ++m_activeCycleId;

    // The cycle is over, so nothing still owed a completion belongs to it. Dropped BEFORE the
    // fault outputs are published, or the abort's own bTaskFault write could be matched against
    // a pending entry from the cycle it is ending.
    clearPendingWrites();

    m_pendingCycleResult.faulted = true;
    m_pendingCycleResult.faultCode = code;
    m_pendingCycleResult.detectedNumber = 0;
    m_pendingCycleResult.sentNumber = 0;

    publishCycleFaultOutputs(code);
    emit cycleResultUpdated(m_pendingCycleResult);
    appendTaskLog(QStringLiteral("ERROR"),
                  QStringLiteral("%1 fault=%2")
                      .arg(message, localizationFaultCodeName(code)));

    m_cycleState = m_lastExecuteTrigger ? CycleState::WaitingTriggerReset
                                        : CycleState::Recovering;

    // Arm the way out only once the fault has come to rest. While the PLC still holds
    // bExecuteTrigger high the runtime is in WaitingTriggerReset and must stay there —
    // re-arming underneath an asserted trigger would break the rising-edge handshake.
    // That case is armed later, on the falling edge in handlePlcValues().
    if (m_cycleState == CycleState::Recovering) {
        armFaultAutoRecovery();
    }

    emit runtimeRecovering(message);
}

/// Handles a rising edge of the bErrorReset PLC input. Cancels any pending automatic
/// recovery first — the operator got there ahead of the timer, and letting both run
/// would clear the fault twice and log it twice — then performs the shared recovery.
void LocalizationRuntimeController::acknowledgeFault()
{
    cancelFaultAutoRecovery();
    recoverFromFault(QStringLiteral("Fault acknowledged via bErrorReset."));
}

/// Shared body of both recovery paths (bErrorReset and the auto-recovery timeout).
/// Clears the latched fault outputs unconditionally, drops the pending cycle's faulted
/// flag, then asks markRuntimeReady() to re-arm — which it will refuse to do while a
/// role is unhealthy or the pattern group / calibration are invalid. Clearing without
/// re-arming is the correct outcome in that case: the operator (or the timer) has
/// acknowledged the fault, but nothing has repaired its cause.
/// @param reason human-readable cause, logged and reused as the runtime-ready message
void LocalizationRuntimeController::recoverFromFault(const QString &reason)
{
    appendTaskLog(QStringLiteral("INFO"), reason);

    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));

    m_pendingCycleResult.faulted = false;
    m_pendingCycleResult.faultCode = LocalizationFaultCode::None;

    if (m_cycleState == CycleState::Faulted ||
        m_cycleState == CycleState::Recovering) {
        markRuntimeReady(reason);
    }
}

/// Starts the fault auto-recovery countdown, unless one is already pending (re-arming
/// would extend the wait every time the state was touched).
void LocalizationRuntimeController::armFaultAutoRecovery()
{
    if (m_faultRecoverTimer.isActive()) {
        return;
    }

    LOG_DEV_INFO << "Fault auto-recovery armed."
                 << "delayMs=" << kFaultAutoRecoverMs;
    m_faultRecoverTimer.start();
}

/// Stops a pending fault auto-recovery countdown, if any.
void LocalizationRuntimeController::cancelFaultAutoRecovery()
{
    if (!m_faultRecoverTimer.isActive()) {
        return;
    }

    LOG_DEV_INFO << "Fault auto-recovery cancelled.";
    m_faultRecoverTimer.stop();
}

/// Central connection-status handler for all three device roles. On a healthy status,
/// clears the role's retry bookkeeping and, once every required role is healthy again,
/// calls markRuntimeReady (using a "recovered" message if this role had been retrying).
/// On an unhealthy status: if a cycle is running, aborts it with the role-specific fault
/// code; then consults decideRecoveryAction on the role's policy and schedules a reconnect
/// retry. Retrying is unbounded — an unreachable device is retried until the runtime ends
/// and never escalates to a task fault. Removes the role's context outright if its runner
/// pointer has gone null.
/// @param role which device role reported the status change
/// @param status the runner's new ConnectStatus
void LocalizationRuntimeController::handleRoleStatusChanged(RunnerRole role,
                                                            vc::device::ConnectStatus status)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        m_recoveryContexts.remove(key);
        return;
    }

    if (isHealthyStatus(status)) {
        const int retryHistory = context.retryCount;
        const QDateTime outageStartedAt = context.outageStartedAt;
        const bool wasRecovering = retryHistory > 0 || context.retryScheduled;
        context.retryCount = 0;
        context.retryScheduled = false;
        context.reportedStatus = vc::device::ConnectStatus::Connected;
        context.outageStartedAt = QDateTime();
        m_recoveryContexts.insert(key, context);

        // Log the reconnect itself, separately from markRuntimeReady() below. The
        // runtime may not be able to re-arm yet (another role still down, or a cycle in
        // flight), and "the camera came back" is worth showing the operator either way.
        if (wasRecovering) {
            appendTaskLog(QStringLiteral("INFO"),
                          buildRecoveryReadyMessage(context.policy,
                                                    context.deviceId,
                                                    retryHistory,
                                                    outageStartedAt));
        }

        if (m_cycleState == CycleState::Running ||
            m_cycleState == CycleState::WaitingTriggerReset) {
            return;
        }
        if (allRequiredRolesHealthy()) {
            // The reconnect detail was logged above; this line reports the separate
            // fact that the runtime actually re-armed, which only happens once every
            // role is healthy and the pattern/calibration still validate.
            markRuntimeReady(QStringLiteral("Runtime ready."));
        }
        return;
    }

    if (m_cycleState == CycleState::Running) {
        switch (role) {
        case RunnerRole::Camera:
            abortCycle(LocalizationFaultCode::CameraLost,
                       QStringLiteral("Active camera lost during localization cycle."));
            break;
        case RunnerRole::VisionOutput:
            abortCycle(LocalizationFaultCode::VisionOutputLost,
                       QStringLiteral("Vision output lost during localization cycle."));
            break;
        case RunnerRole::PrimaryPlc:
            abortCycle(LocalizationFaultCode::PlcLost,
                       QStringLiteral("Primary PLC lost during localization cycle."));
            break;
        }
    }

    const LocalizationRecoveryAction action = decideRecoveryAction(
        context.policy,
        status,
        context.retryScheduled);

    if (action != LocalizationRecoveryAction::RetryScheduled) {
        return;
    }

    // A new outage: stamp its start so the recovery message can report how long it
    // lasted, which is what an operator asks after the fact now that attempts are
    // no longer bounded by a known maximum.
    if (context.retryCount == 0) {
        context.outageStartedAt = QDateTime::currentDateTime();

        // Once per outage, on the operator's dashboard, not only in the app log. An
        // outage that shows up nowhere in the task log is indistinguishable from a
        // runtime that simply stopped being triggered.
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Connection lost: role=%1 deviceId=%2 status=%3. "
                                     "Reconnecting every %4 ms until it returns.")
                          .arg(context.policy.roleName,
                               context.deviceId,
                               connectStatusName(status),
                               QString::number(context.policy.retryIntervalMs)));

        // Withdraw readiness for the duration of the outage. Losing a role outside a
        // running cycle previously left CycleState::ReadyForTrigger and bTaskReady=true
        // standing: the escalation path published bTaskFault a minute later and covered
        // for it. With escalation removed there is no such backstop, so an unreachable
        // camera would advertise "ready, send me a trigger" indefinitely. The PLC
        // contract is that it triggers only while bTaskReady is true, so this is the
        // signal it needs. markRuntimeReady() restores both on reconnect.
        if (m_cycleState == CycleState::ReadyForTrigger) {
            m_cycleState = CycleState::Recovering;
            publishBoolSignal(QStringLiteral("bTaskReady"), false);
        }
    }

    // Report only when the outage begins or when its status actually changes. Without
    // this, an outage lasting minutes emits one identical line every retryIntervalMs
    // and drowns out everything else the operator needs to see.
    const bool statusIsNew = context.reportedStatus != status;

    context.retryScheduled = true;
    context.retryCount += 1;
    context.reportedStatus = status;
    m_recoveryContexts.insert(key, context);

    if (statusIsNew) {
        emit runtimeRecovering(buildRecoveryProgressMessage(
            context.policy,
            context.deviceId,
            status));
    }

    scheduleRoleReconnect(role);
}

/// Returns whether this reconnect attempt is one of the ones an operator should see:
/// the first of an outage, then one in every kQuietRetryLogStride. Everything else is
/// still written to the developer log by the callers.
bool LocalizationRuntimeController::shouldReportRetryToUser(const RoleRecoveryContext &context)
{
    return context.retryCount <= 1 ||
           (context.retryCount % kQuietRetryLogStride) == 0;
}

/// Logs the scheduled retry and, after the role's configured retryIntervalMs elapses,
/// clears the role's retryScheduled flag and calls requestRoleConnectNow for it (both
/// re-checked against the current recovery context in case the role was rebound/removed
/// in the meantime). No-op if the role has no recovery context or a null runner.
///
/// The retry itself is unbounded, so its logging is rate-limited instead: the first
/// attempt and every kQuietRetryLogStride-th attempt reach the user log, the rest go to
/// the developer log. Nothing is discarded — a flapping link is still fully reconstructable.
/// @param role which device role to schedule a reconnect attempt for
void LocalizationRuntimeController::scheduleRoleReconnect(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    const RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        return;
    }

    if (shouldReportRetryToUser(context)) {
        LOG_USER_WARN << "Recovery retry scheduled."
                      << "role=" << context.policy.roleName
                      << "deviceId=" << context.deviceId
                      << "attempt=" << context.retryCount
                      << "intervalMs=" << context.policy.retryIntervalMs;
    } else {
        LOG_DEV_INFO << "Recovery retry scheduled."
                     << "role=" << context.policy.roleName
                     << "deviceId=" << context.deviceId
                     << "attempt=" << context.retryCount;
    }

    // The `this` context argument is load-bearing, not incidental: retries are unbounded,
    // so a context-free QTimer::singleShot would keep firing into a controller that
    // endRuntime() has already destroyed. Binding to `this` makes the pending retry die
    // with the runtime session.
    QTimer::singleShot(context.policy.retryIntervalMs, this, [this, role]() {
        const int currentKey = roleKey(role);
        if (!m_recoveryContexts.contains(currentKey)) {
            return;
        }
        RoleRecoveryContext current = m_recoveryContexts.value(currentKey);
        current.retryScheduled = false;
        m_recoveryContexts.insert(currentKey, current);
        requestRoleConnectNow(role);
    });
}

/// Calls requestConnect() on the role's bound runner immediately, then logs the attempt.
/// Removes the role's recovery context if its runner pointer has gone null; no-op if the role
/// has no recovery context at all.
/// @param role which device role to request an immediate connect for
/// @note This used to switch on the role and downcast to the concrete runner type. That made
///       reconnection silently do nothing for any role bound to a runner of an unexpected type,
///       which the capability-based vision_output role makes reachable. requestConnect() is on
///       IDeviceRunner precisely so this function does not have to know.
void LocalizationRuntimeController::requestRoleConnectNow(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        m_recoveryContexts.remove(key);
        return;
    }

    context.runner->requestConnect();

    if (shouldReportRetryToUser(context)) {
        LOG_USER_WARN << "Recovery reconnect requested."
                      << "role=" << context.policy.roleName
                      << "deviceId=" << context.deviceId
                      << "attempt=" << context.retryCount;
    } else {
        LOG_DEV_INFO << "Recovery reconnect requested."
                     << "role=" << context.policy.roleName
                     << "deviceId=" << context.deviceId
                     << "attempt=" << context.retryCount;
    }
}

/// Slot for the active camera's grabFinished signal: disconnects the grab/command
/// connections (they were single-use for this cycle), ignores stale results if the cycle
/// is no longer Running, aborts the cycle with CameraGrabTimeout on a failed/empty grab,
/// aborts with PatternNotRegistered if the active pattern-group snapshot is unavailable, else
/// stores a clone of the grabbed frame in m_pendingCycleResult.rawImage and emits
/// runtimeMatchingRequested to hand the frame off to the matcher.
/// @param result the camera grab outcome (success flag + captured frame)
void LocalizationRuntimeController::onCameraGrabFinished(vc::device::GrabResult result)
{
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();

    if (m_cycleState != CycleState::Running) {
        return;
    }

    // Stamped on ARRIVAL, ahead of the success check: a grab that came back empty still took
    // however long it took, and that number is the point of the instrumentation. A camera that
    // never answers at all is aborted by onCameraCommandFinished() instead, and leaves this
    // boundary unset — which is the honest record of what happened.
    m_pendingCycleResult.timings.grabFinishedMs = cycleElapsedMs();

    if (!result.isGrabSuccess || result.frame.empty()) {
        abortCycle(LocalizationFaultCode::CameraGrabTimeout,
                   QStringLiteral("Camera grab failed or returned an empty frame."));
        return;
    }

    const auto group = snapshotActivePatternGroup();
    if (!group) {
        abortCycle(LocalizationFaultCode::PatternNotRegistered,
                   QStringLiteral("Active pattern group snapshot failed."));
        return;
    }

    // The robot-pickability checker is built once at runtime setup and rebuilt
    // only on active-camera change (rebuildPickingChecker), not per cycle.
    m_pendingCycleResult.rawImage = result.frame.clone();
    emit runtimeMatchingRequested(m_activeCycleId, group, m_activeCameraWorkspace,
                                  result.frame.clone(), m_pickingChecker);
}

/// Callback for the matcher's result, invoked with the cycle id that was passed to
/// runtimeMatchingRequested. Ignores stale/mismatched cycle ids or a cycle that is no
/// longer Running; converts `matchResult` to world-space positions via
/// buildVisionOutputPositions (aborting the cycle if that conversion faults), records
/// the outcome in m_pendingCycleResult, then requests the vision-output runner to send
/// the positions, wiring a single-shot connection to onVisionOutputResultFinished for
/// the actual send outcome. Aborts with VisionOutputLost if no vision-output runner is
/// bound.
void LocalizationRuntimeController::onRuntimeMatchingFinished(int cycleId,
                                                              mtc::MatchResult matchResult)
{
    if (cycleId != m_activeCycleId || m_cycleState != CycleState::Running) {
        return;
    }

    m_pendingCycleResult.timings.matchingFinishedMs = cycleElapsedMs();

    QVector<ResultRow> rows;
    LocalizationFaultCode conversionFault = LocalizationFaultCode::None;
    const QVector<vc::device::VisionOutputPosition> positions =
        buildVisionOutputPositions(matchResult, &rows, &conversionFault);
    if (conversionFault != LocalizationFaultCode::None) {
        abortCycle(conversionFault,
                   QStringLiteral("Failed to convert match result to world coordinates."));
        return;
    }

    m_pendingCycleResult.faulted = false;
    m_pendingCycleResult.faultCode = LocalizationFaultCode::None;
    m_pendingCycleResult.detectedNumber = matchResult.totalPossiblePicking;
    m_pendingCycleResult.sentNumber = positions.size();
    m_pendingCycleResult.matchingTimeMs = matchResult.ExecutionTime;
    m_pendingCycleResult.lowArea = matchResult.isAreaLessThanLimits;
    m_pendingCycleResult.displayImage = matchResult.Image.clone();
    m_pendingCycleResult.matchResult = matchResult;
    m_pendingCycleResult.rows = rows;

    auto *visionRunner = visionOutputRunner();
    if (!visionRunner || !visionRunner->supportsResultOutput()) {
        abortCycle(LocalizationFaultCode::VisionOutputLost,
                   QStringLiteral("Vision output runner is not available."));
        return;
    }

    disconnect(m_visionOutputResultConnection);
    m_visionOutputResultConnection = connect(
        visionRunner,
        &vc::runtime::IDeviceRunner::resultRequestFinished,
        this,
        &LocalizationRuntimeController::onVisionOutputResultFinished,
        Qt::SingleShotConnection);
    visionRunner->requestSendResult(positions);
}

/// Slot for the active camera's commandFinished signal: only acts when the cycle is
/// Running and the failed command is the CameraSingleShot request; aborts the cycle with
/// CameraGrabTimeout (using the runner's failure message) if that failure's code is
/// TimedOut. All other command results/kinds/codes are ignored here (the grab result
/// itself is handled by onCameraGrabFinished).
/// @param result the camera command's completion status/code/message
void LocalizationRuntimeController::onCameraCommandFinished(
    vc::runtime::DeviceCommandResult result)
{
    if (m_cycleState != CycleState::Running ||
        result.kind != vc::runtime::DeviceCommandKind::CameraSingleShot ||
        result.status != vc::runtime::DeviceCommandResultStatus::Failed) {
        return;
    }

    if (result.code == vc::runtime::DeviceCommandResultCode::TimedOut) {
        abortCycle(LocalizationFaultCode::CameraGrabTimeout, result.message);
    }
}

/// Slot for the vision-output runner's resultRequestFinished signal, completing the
/// current cycle: disconnects the single-shot connection, ignores the result if the
/// cycle is no longer Running, aborts with VisionOutputSendFailed on failure, else
/// publishes the cycle-success outputs, emits cycleResultUpdated, logs an INFO summary
/// (detected/sent counts and matching time), and transitions to WaitingTriggerReset (if
/// the execute trigger is still asserted, logging a WARN to wait for its reset) or back
/// to ReadyForTrigger via markRuntimeReady otherwise.
/// @param ok whether the vision-output device accepted the sent positions
/// @param message failure detail when `ok` is false; used as the abort message
void LocalizationRuntimeController::onVisionOutputResultFinished(bool ok,
                                                                 const QString &message)
{
    disconnect(m_visionOutputResultConnection);
    m_visionOutputResultConnection = QMetaObject::Connection();

    if (m_cycleState != CycleState::Running) {
        return;
    }

    // Same rule as the grab: stamped before the outcome is judged, so a FAILED send still records
    // how long the device took to refuse.
    m_pendingCycleResult.timings.sendFinishedMs = cycleElapsedMs();

    if (!ok) {
        abortCycle(LocalizationFaultCode::VisionOutputSendFailed, message);
        return;
    }

    // A cycle that completes end to end proves the previous faults were transient, so
    // the repeating-auto-recovery counter starts over. Counting since the last success
    // (rather than since startup) is what makes the kAutoRecoverWarnStride report mean
    // "this is failing repeatedly right now".
    m_consecutiveAutoRecoveries = 0;

    publishCycleSuccessOutputs(m_pendingCycleResult);
    // The last boundary, and the cycle's total. Taken AFTER the handshake outputs are published
    // for the same reason the clock started before them: the PLC write is part of the cycle.
    m_pendingCycleResult.timings.outputsPublishedMs = cycleElapsedMs();

    emit cycleResultUpdated(m_pendingCycleResult);
    // ONE line per cycle, not five (Phase 9 / F3). Five would be unreadable at cycle rate and
    // would bury the fault lines this log exists to carry. matchingTimeMs is still reported
    // separately: it is the matcher's own measurement, and the match stage below is the
    // controller's — they answer different questions and are not interchangeable.
    QString cycleInfoLogString = formatCycleTimings(m_pendingCycleResult.timings);
    appendTaskLog(QStringLiteral("INFO"),
                  QStringLiteral("Vision output sent. detected=%1 sent=%2 time=%3 ms. %4")
                      .arg(m_pendingCycleResult.detectedNumber)
                      .arg(m_pendingCycleResult.sentNumber)
                      .arg(m_pendingCycleResult.matchingTimeMs, 0, 'f', 1)
                      .arg(cycleInfoLogString));

    LOG_DEV_INFO << "Task localization:" << cycleInfoLogString;

    m_cycleState = m_lastExecuteTrigger ? CycleState::WaitingTriggerReset
                                        : CycleState::ReadyForTrigger;
    if (!m_lastExecuteTrigger) {
        markRuntimeReady(QStringLiteral("Localization cycle completed."));
    } else {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Waiting for execute trigger reset."));
    }
}

/// Forwards the primary PLC runner's connectStatusChanged signal to
/// handleRoleStatusChanged for the PrimaryPlc role.
void LocalizationRuntimeController::onPrimaryPlcStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::PrimaryPlc, status);
}

/// Forwards the vision-output runner's connectStatusChanged signal to
/// handleRoleStatusChanged for the VisionOutput role.
void LocalizationRuntimeController::onVisionOutputStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::VisionOutput, status);
}

/// Forwards the active camera runner's connectStatusChanged signal to
/// handleRoleStatusChanged for the Camera role.
void LocalizationRuntimeController::onCameraStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::Camera, status);
}

/// Reports a device runner's errorOccurred signal to both the app log and the task log,
/// suppressing an immediate repeat of the same message from the same role.
///
/// The de-duplication is not cosmetic. A dead PLC fails every queued I/O write, so the
/// same line arrives many times a second; forwarding each one to the dashboard would
/// bury exactly the events the operator opened it to read. A different message, or the
/// same message after a different one, is always reported.
/// @param role which device role raised the error
/// @param roleName human-readable role name used in the log lines
/// @param message the error text reported by the runner
void LocalizationRuntimeController::reportRoleError(RunnerRole role,
                                                     const QString &roleName,
                                                     const QString &message)
{
    const int key = roleKey(role);
    if (m_lastRoleError.value(key) == message) {
        LOG_DEV_INFO << roleName << "runtime error (repeat):" << message;
        return;
    }

    m_lastRoleError.insert(key, message);
    LOG_USER_WARN << roleName << "runtime error:" << message;
    appendTaskLog(QStringLiteral("ERROR"),
                  QStringLiteral("%1 error: %2").arg(roleName, message));
}

/// Reports the primary PLC runner's errorOccurred signal; does not affect
/// cycle/connection state (connection loss is handled separately via
/// onPrimaryPlcStatusChanged).
void LocalizationRuntimeController::onPrimaryPlcError(const QString &message)
{
    reportRoleError(RunnerRole::PrimaryPlc, QStringLiteral("primary_plc"), message);
}

/// Reports the vision-output runner's errorOccurred signal; does not affect
/// cycle/connection state (connection loss is handled separately via
/// onVisionOutputStatusChanged).
void LocalizationRuntimeController::onVisionOutputError(const QString &message)
{
    reportRoleError(RunnerRole::VisionOutput, QStringLiteral("vision_output"), message);
}

/// Reports the active camera runner's errorOccurred signal; does not affect
/// cycle/connection state (connection loss is handled separately via
/// onCameraStatusChanged).
void LocalizationRuntimeController::onCameraError(const QString &message)
{
    reportRoleError(RunnerRole::Camera, QStringLiteral("camera"), message);
}

} // namespace vc::model
