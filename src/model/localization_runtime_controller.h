#ifndef LOCALIZATION_RUNTIME_CONTROLLER_H
#define LOCALIZATION_RUNTIME_CONTROLLER_H

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <memory>

#include <opencv2/core/mat.hpp>

#include "device/idevice_config.h"
#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_config.h"
#include "device/output_device/vision_output_request.h"
#include "matching/image_matcher.h"
#include "matching/robot_picking_checker.h"
#include "model/localization_fault_code.h"
#include "model/localization_recovery_policy.h"
#include "model/localization_signal_mapper.h"
#include "model/task_localization_config.h"
#include "runtime/device_command.h"

/// Forward declarations of runtime runner types, referenced here only via QPointer/raw pointer
/// so this header does not need to include the full runtime runner headers.
namespace vc::runtime {
class CameraRunner;
class IDeviceRunner;
class PlcRunner;
class VisionOutputRunner;
}

namespace vc::model {

/// Runs one localization task at runtime: binds the task's primary PLC, vision-output, and
/// active-camera device runners into fixed "roles", drives the per-cycle state machine
/// (wait for trigger -> grab -> match -> convert to world coordinates -> send to vision output),
/// and monitors/recovers each role's connection according to its LocalizationRecoveryPolicy.
/// Task-relevant state changes (ready/fault/recovering, signal values, cycle results, log lines)
/// are published via Qt signals for the UI and the PLC (through publishBoolSignal /
/// publishNumberSignal).
class LocalizationRuntimeController : public QObject {
    Q_OBJECT

public:
    /// Snapshot of a task's device/pattern/calibration bindings, supplied to setup() and rebuilt
    /// by the owner whenever roles or the active camera/pattern-group selection change.
    struct RuntimeContext {
        TaskLocalizeConfig config;                            ///< Localization task configuration used for this runtime session.
        QString primaryPlcDeviceId;                           ///< Device id bound to the "primary_plc" role.
        QString visionOutputDeviceId;                         ///< Device id bound to the "vision_output" role.
        QPointer<vc::runtime::PlcRunner> primaryPlcRunner;    ///< Runner for the primary PLC role; null if not available.
        QPointer<vc::runtime::VisionOutputRunner> visionOutputRunner; ///< Runner for the vision output role; null if not available.
        QMap<int, QString> cameraDeviceIds;                   ///< Camera device id keyed by configured camera number.
        QMap<int, QPointer<vc::runtime::CameraRunner>> cameraRunners; ///< Camera runner keyed by camera number.
        QMap<int, std::shared_ptr<mtc::MatchGroup>> patternGroups;    ///< Pattern match group keyed by pattern-group number.
        QMap<int, calib::Calibrator> cameraCalibrators;       ///< Calibrator keyed by camera number, used to map image points to robot/world coordinates.
        /// Robot kinematic check settings snapshotted from the assigned vision output device;
        /// drives the per-object robotPossiblePickingCheck.
        vc::device::RobotKinematicCheckConfig robotCheckConfig;
        CameraWorkspace activeCameraWorkspace;                ///< Workspace crop/offset settings for the active camera.
        int activeCameraNumber{-1};                           ///< Camera number to activate at setup; -1 selects the first available.
        int activePatternGroupNumber{-1};                     ///< Pattern group number to activate at setup; -1 selects the first available.
    };

    /// One matched object's localization result, as reported to the UI/log: image-space and
    /// world-space (robot) position, plus the disposition status assigned by
    /// buildVisionOutputPositions().
    struct ResultRow {
        int index{0};                       ///< 1-based row index within the cycle's result set.
        QString patternName;                ///< Name of the matched pattern.
        double score{0.0};                  ///< Match confidence score reported by the matcher.
        double imageX{0.0};                 ///< Image-space X of the matched object's center.
        double imageY{0.0};                 ///< Image-space Y of the matched object's center.
        double imageR{0.0};                 ///< Image-space rotation angle of the match.
        vc::device::VisionOutputPosition world; ///< Robot/world-space position and rotation computed via camera calibration.
        QString status;                     ///< Human-readable disposition, e.g. "Sent" or "Skipped: <reason>".
    };

    /// Outcome of one localization cycle (grab + match + send), published via
    /// cycleResultUpdated() for UI display and task logging.
    struct CycleResult {
        bool faulted{false};                ///< True if the cycle ended in a fault instead of completing normally.
        LocalizationFaultCode faultCode{LocalizationFaultCode::None}; ///< Fault code recorded when faulted is true.
        int detectedNumber{0};               ///< Number of objects the matcher reported as possible to pick.
        int sentNumber{0};                   ///< Number of positions actually sent to the vision output device.
        double matchingTimeMs{0.0};          ///< Matching execution time reported by the matcher, in milliseconds.
        bool lowArea{false};                 ///< True if the matcher flagged the matched area as below its configured limit.
        cv::Mat rawImage;                    ///< Raw camera frame grabbed for this cycle.
        cv::Mat displayImage;                ///< Annotated/display image returned by the matcher.
        mtc::MatchResult matchResult;        ///< Full match result from the matcher.
        QVector<ResultRow> rows;             ///< Per-object result rows built from matchResult.
    };

    /// One timestamped task-log line surfaced to the UI via taskLogAppended().
    struct TaskLogEntry {
        QDateTime timestamp;  ///< Time the log line was appended.
        QString severity;     ///< Severity tag used by appendTaskLog(), e.g. "INFO", "WARN", "ERROR".
        QString message;      ///< Log message text.
    };

    /// Outcome of setup(): whether the supplied RuntimeContext was valid to run, plus the
    /// resolved role device ids and any validation errors.
    struct SetupResult {
        bool valid{false};                  ///< True if the context passed all validation checks.
        QString primaryPlcDeviceId;         ///< Device id resolved for the primary PLC role.
        QString visionOutputDeviceId;       ///< Device id resolved for the vision output role.
        QStringList errors;                 ///< Human-readable validation failures; empty when valid.
    };

    /// Registers the Qt meta-types used by this class's queued signals (CycleResult,
    /// TaskLogEntry, LocalizationFaultCode, CameraWorkspace, shared_ptr<IRobotPickingChecker>).
    explicit LocalizationRuntimeController(QObject *parent = nullptr);

    /// Stores `config` and reconfigures the PLC signal mapper from it.
    void configure(const TaskLocalizeConfig &config);
    /// Switches the active camera role to `cameraNumber`: rebinds the Camera role context,
    /// disconnects the previous camera runner, revalidates calibration, rebuilds the
    /// robot-picking checker, and reconnects the new camera runner. Ignored while a cycle is
    /// running.
    void setActiveCameraNumber(int cameraNumber);
    /// Switches the active pattern group used for matching and republishes the pattern-valid
    /// status. Ignored while a cycle is running.
    void setActivePatternGroupNumber(int patternGroupNumber);
    /// Replaces the per-role (camera/PLC/vision-output) reconnect/retry policies used by the
    /// connection-recovery state machine.
    void setRecoveryPolicies(const LocalizationRecoveryPolicy &cameraPolicy,
                             const LocalizationRecoveryPolicy &plcPolicy,
                             const LocalizationRecoveryPolicy &visionOutputPolicy);

    /// Binds `context`'s role runners/cameras/patterns/calibration, validates them, and, if
    /// valid, requests a connection for every role and marks the runtime ready once all roles
    /// are healthy.
    /// @param context device/pattern/calibration bindings for this task run
    /// @return validation outcome; SetupResult::valid is false if a required role, the active
    /// pattern group, or the active camera calibration is missing/invalid
    SetupResult setup(const RuntimeContext &context);
    /// Manually starts a localization cycle, as if triggered by the PLC. No-op (logged) if the
    /// runtime is not valid or is not currently ReadyForTrigger.
    void execute();
    /// Returns whether the last setup() call produced a usable (fully validated) configuration.
    bool isValid() const { return m_valid; }

    /// Maps raw PLC tag values to named signal events via the signal mapper, and reacts to
    /// active-camera / active-pattern-group changes and rising/falling edges of the
    /// bExecuteTrigger signal (starts a cycle on rising edge; on falling edge, resets the
    /// matching-finished output and either recovers a pending fault or marks the runtime ready).
    /// @param values raw values keyed by PLC signal name, as received from the primary PLC runner
    void handlePlcValues(const QMap<QString, QVariant> &values);
    /// Receives the completed match result for `cycleId`, converts it to world-space positions,
    /// and forwards them to the vision output device. Stale results (cycleId mismatch, or the
    /// cycle is no longer running) are silently dropped.
    /// @param cycleId cycle id the result belongs to
    /// @param matchResult matcher output for the grabbed frame
    void onRuntimeMatchingFinished(int cycleId, mtc::MatchResult matchResult);

signals:
    /// Emitted whenever a named PLC-mapped signal value is published (mirrors every
    /// publishBoolSignal / publishNumberSignal call for UI/logging).
    void signalChanged(QString name, QVariant value);
    /// Emitted when the active camera number changes (from PLC input or setActiveCameraNumber()).
    void cameraNumberChanged(QVariant value);
    /// Emitted when the active pattern group number changes.
    void patternNumberChanged(QVariant value);
    /// Emitted with the final result of each cycle, whether it succeeded or faulted.
    void cycleResultUpdated(vc::model::LocalizationRuntimeController::CycleResult result);
    /// Emitted for every task log line appended via appendTaskLog().
    void taskLogAppended(vc::model::LocalizationRuntimeController::TaskLogEntry entry);
    /// Emitted when a localization cycle begins (trigger accepted).
    void runtimeCycleStarted(QString message);
    /// Emitted while a role is retrying its connection after a loss.
    void runtimeRecovering(QString message);
    /// Emitted when the runtime (re)enters the ReadyForTrigger state.
    void runtimeReady(QString message);
    /// Emitted when the runtime enters the Faulted state (unrecoverable role loss or invalid
    /// setup).
    void runtimeFault(QString message);
    /// Emitted after a successful camera grab to hand the frame off for matching (on whatever
    /// external thread/service listens for this signal).
    /// @param cycleId id of the cycle the frame belongs to, echoed back via onRuntimeMatchingFinished()
    /// @param group pattern group snapshot to match against
    /// @param workspace active camera workspace (crop/offset) settings
    /// @param image grabbed frame to match
    /// @param pickingChecker robot-pickability checker to gate matches with, or null if disabled
    void runtimeMatchingRequested(int cycleId,
                                  std::shared_ptr<mtc::MatchGroup> group,
                                  CameraWorkspace workspace,
                                  cv::Mat image,
                                  std::shared_ptr<mtc::IRobotPickingChecker> pickingChecker);

private:
    /// Per-role bookkeeping for the connection-recovery state machine: the runner/device bound
    /// to a role, its recovery policy, and the current retry/fault progress against that policy.
    struct RoleRecoveryContext {
        QString roleName;                              ///< Role name from the bound LocalizationRecoveryPolicy (for logging/messages).
        QString deviceId;                               ///< Device id bound to this role.
        QPointer<vc::runtime::IDeviceRunner> runner;    ///< Runner bound to this role.
        LocalizationRecoveryPolicy policy;               ///< Reconnect/retry policy in effect for this role.
        int retryCount{0};                               ///< Number of reconnect attempts made since the last healthy connection.
        bool retryScheduled{false};                      ///< True while a scheduleRoleReconnect() timer is pending.
        bool faultRaised{false};                         ///< True once raiseRoleFault() has been called for the current outage.
    };

    /// The three device roles the controller coordinates.
    enum class RunnerRole {
        Camera,
        PrimaryPlc,
        VisionOutput
    };

    /// Lifecycle state of the runtime / current localization cycle.
    enum class CycleState {
        NotReady,            ///< Not yet set up, or a required role/pattern/calibration is invalid.
        ReadyForTrigger,     ///< All roles healthy and validated; waiting for the next execute trigger.
        Running,             ///< A cycle (grab/match/send) is in progress.
        WaitingTriggerReset, ///< A cycle finished (or faulted) while the PLC trigger is still asserted; waiting for its falling edge.
        Recovering,          ///< A role connection was lost; retrying per its recovery policy.
        Faulted              ///< An unrecoverable role loss or setup failure; requires external intervention.
    };

    /// Disconnects all per-role signal connections and clears every recovery context; called at
    /// the start of setup().
    void resetRuntimeBindings();
    /// Binds the PrimaryPlc and VisionOutput role contexts from the current RuntimeContext
    /// (these two roles are fixed for the lifetime of the runtime session).
    void bindFixedRoleRunners();
    /// Binds (or clears, if unavailable) the Camera role context to the runner for `cameraNumber`.
    void bindActiveCameraRole(int cameraNumber);
    /// Replaces the recovery context for `role` and connects its runner's connectStatusChanged /
    /// errorOccurred signals to the matching per-role handler slots.
    void bindRoleContext(RunnerRole role,
                         vc::runtime::IDeviceRunner *runner,
                         const QString &deviceId,
                         const LocalizationRecoveryPolicy &policy);
    /// Disconnects and removes the recovery context for `role`, if any is bound.
    void clearRoleContext(RunnerRole role);
    /// Maps a role to the integer key used in m_recoveryContexts.
    static int roleKey(RunnerRole role) { return static_cast<int>(role); }
    /// Core recovery/fault state machine driven by a role's connection-status change: on
    /// reconnect, clears retry state and re-evaluates runtime readiness; on loss, aborts a
    /// running cycle (with a role-specific fault code) and then, via decideRecoveryAction(),
    /// either schedules a retry or escalates to a fault.
    void handleRoleStatusChanged(RunnerRole role, vc::device::ConnectStatus status);
    /// Schedules a one-shot reconnect attempt for `role` after its policy's retry interval.
    void scheduleRoleReconnect(RunnerRole role);
    /// Issues an immediate requestConnect() on the role's runner (dispatched by concrete runner
    /// type).
    void requestRoleConnectNow(RunnerRole role);
    /// Publishes a task fault (bTaskFault / nFaultCode) for `role`, transitions to Faulted, and
    /// emits runtimeFault() with a role-specific message.
    void raiseRoleFault(RunnerRole role, vc::device::ConnectStatus status);
    /// Returns true only if the PrimaryPlc, VisionOutput, and Camera roles are all bound and
    /// their devices report Connected.
    bool allRequiredRolesHealthy() const;
    /// Transitions to ReadyForTrigger and publishes the initial ready outputs, provided the
    /// runtime is valid, all roles are healthy, and the active pattern group / camera
    /// calibration are valid. No-op while a cycle is running.
    void markRuntimeReady(const QString &message);
    /// Emits signalChanged() for `name` and, if it maps to a PLC tag, writes `value` to the
    /// primary PLC via requestWriteDigitalIo().
    void publishBoolSignal(const QString &name, bool value);
    /// Emits signalChanged() for `name` and, if it maps to a PLC tag, writes `value` to the
    /// primary PLC via requestWriteWordIo().
    void publishNumberSignal(const QString &name, int value);
    /// Publishes the full set of "ready" status outputs (bTaskReady, bCameraValid, etc.) once
    /// the runtime becomes ready.
    void publishInitialReadyOutputs();
    /// Publishes outputs marking the start of a cycle (busy=true, prior results cleared).
    void publishCycleStartOutputs();
    /// Publishes outputs for a cycle that completed without fault.
    void publishCycleSuccessOutputs(const CycleResult &result);
    /// Publishes outputs for a cycle (or setup) that ended in a fault with `code`.
    void publishCycleFaultOutputs(LocalizationFaultCode code);
    /// Builds a timestamped TaskLogEntry from `severity`/`message` and emits taskLogAppended().
    void appendTaskLog(const QString &severity, const QString &message);
    /// Checks that the active (or first available) pattern group exists and has at least one
    /// pattern with a non-empty train image.
    /// @param errors optional; a failure message is appended on validation failure
    /// @return true if the active pattern group is usable for matching
    bool validateActivePatternGroup(QStringList *errors = nullptr) const;
    /// Checks that the calibrator for the active camera is calibrated.
    /// @param errors optional; a failure message is appended on validation failure
    /// @return true if the active camera has a valid calibration
    bool validateActiveCameraCalibration(QStringList *errors = nullptr) const;
    /// (Re)build the robot-pickability checker for the active camera. Called once at runtime
    /// setup and again on active-camera change (calibrator differs per camera). Leaves
    /// m_pickingChecker null when the kinematic check is disabled or no valid calibration.
    void rebuildPickingChecker();
    /// Returns the pattern group for the active (or first available) pattern-group number, or
    /// null if none is available.
    std::shared_ptr<mtc::MatchGroup> snapshotActivePatternGroup() const;
    /// Converts a match result's objects into world-space positions and per-object result rows,
    /// applying the active camera workspace's crop offset, image-to-robot calibration, and
    /// collision/ROI/pickability filtering; at most 2 positions are accepted for sending, the
    /// rest are marked "Skipped".
    /// @param matchResult matcher output for the cycle
    /// @param rows optional; appended with one ResultRow per matched object
    /// @param faultCode optional; set to CalibrationInvalid if the active camera has no valid
    /// calibration, otherwise None
    /// @return world-space positions accepted for sending (at most 2)
    QVector<vc::device::VisionOutputPosition> buildVisionOutputPositions(
        const mtc::MatchResult &matchResult,
        QVector<ResultRow> *rows,
        LocalizationFaultCode *faultCode) const;
    /// Returns the runner bound to the Camera role, or nullptr if none is bound.
    vc::runtime::CameraRunner *activeCameraRunner() const;
    /// Returns the runner bound to the PrimaryPlc role, or nullptr if none is bound.
    vc::runtime::PlcRunner *primaryPlcRunner() const;
    /// Returns the runner bound to the VisionOutput role, or nullptr if none is bound.
    vc::runtime::VisionOutputRunner *visionOutputRunner() const;
    /// Validates readiness (pattern group, calibration, camera runner availability), then
    /// transitions to Running, publishes cycle-start outputs, and requests a single-shot grab
    /// from the active camera.
    void startCycle();
    /// Tears down any in-flight grab/matching/send connections, records `code` as the cycle
    /// fault, publishes fault outputs, logs `message`, and transitions to WaitingTriggerReset or
    /// Recovering depending on whether the execute trigger is still asserted.
    void abortCycle(LocalizationFaultCode code, const QString &message);

private slots:
    /// Handles the single-shot grab result: on failure, aborts the cycle with
    /// CameraGrabTimeout; on success, snapshots the active pattern group and emits
    /// runtimeMatchingRequested() to hand the frame off for matching.
    void onCameraGrabFinished(vc::device::GrabResult result);
    /// Watches for a failed/timed-out CameraSingleShot command while a cycle is running and
    /// aborts the cycle with CameraGrabTimeout on a TimedOut result.
    void onCameraCommandFinished(vc::runtime::DeviceCommandResult result);
    /// Completes the cycle: on failure, aborts with VisionOutputSendFailed; on success,
    /// publishes success outputs, logs the result, and returns the runtime to ReadyForTrigger
    /// (or WaitingTriggerReset if the execute trigger is still asserted).
    void onVisionOutputResultFinished(bool ok, const QString &message);
    /// Forwards the primary PLC's connection-status change to handleRoleStatusChanged().
    void onPrimaryPlcStatusChanged(vc::device::ConnectStatus status);
    /// Forwards the vision output device's connection-status change to handleRoleStatusChanged().
    void onVisionOutputStatusChanged(vc::device::ConnectStatus status);
    /// Forwards the active camera's connection-status change to handleRoleStatusChanged().
    void onCameraStatusChanged(vc::device::ConnectStatus status);
    /// Logs a primary-PLC runtime error via LOG_USER_WARN.
    void onPrimaryPlcError(const QString &message);
    /// Logs a vision-output runtime error via LOG_USER_WARN.
    void onVisionOutputError(const QString &message);
    /// Logs a camera runtime error via LOG_USER_WARN.
    void onCameraError(const QString &message);

private:
    RuntimeContext m_context;                        ///< Runtime bindings captured by the last setup() call.
    TaskLocalizeConfig m_config;                     ///< Task configuration snapshot (mirrors m_context.config).
    LocalizationSignalMapper m_signalMapper;         ///< Maps PLC tag values to/from named signals for this task.
    QMetaObject::Connection m_plcValueConnection;    ///< Connection from the primary PLC runner's valueChanged() to handlePlcValues().
    QMetaObject::Connection m_cameraGrabConnection;  ///< Connection from the active camera runner's grabFinished() for the in-flight cycle.
    QMetaObject::Connection m_cameraCommandConnection; ///< Connection from the active camera runner's commandFinished() for the in-flight cycle.
    QMetaObject::Connection m_visionOutputResultConnection; ///< Connection from the vision output runner's resultRequestFinished() for the in-flight cycle.
    bool m_valid{false};                              ///< True once setup() has validated the context successfully.
    int m_activeCameraNumber{-1};                     ///< Camera number currently bound to the Camera role; -1 if unset.
    /// Robot-pickability checker for the active camera (built at setup / camera change). Passed
    /// to the matcher each cycle; null = matching not gated.
    std::shared_ptr<mtc::IRobotPickingChecker> m_pickingChecker;
    CameraWorkspace m_activeCameraWorkspace;          ///< Workspace settings for the active camera (mirrors m_context.activeCameraWorkspace).
    int m_activePatternGroupNumber{-1};               ///< Pattern group number currently used for matching; -1 if unset.
    int m_activeCycleId{0};                           ///< Monotonically incremented id for the in-flight/most-recent cycle; used to discard stale async results.
    bool m_lastExecuteTrigger{false};                 ///< Last observed value of the PLC bExecuteTrigger signal, used for edge detection.
    CycleState m_cycleState{CycleState::NotReady};    ///< Current lifecycle state of the runtime/cycle.
    CycleResult m_pendingCycleResult;                 ///< Result being assembled for the in-flight cycle.
    LocalizationRecoveryPolicy m_cameraRecoveryPolicy{defaultCameraRecoveryPolicy()}; ///< Reconnect/retry policy for the Camera role.
    LocalizationRecoveryPolicy m_plcRecoveryPolicy{defaultPlcRecoveryPolicy()}; ///< Reconnect/retry policy for the PrimaryPlc role.
    LocalizationRecoveryPolicy m_visionOutputRecoveryPolicy{defaultVisionOutputRecoveryPolicy()}; ///< Reconnect/retry policy for the VisionOutput role.
    QHash<int, RoleRecoveryContext> m_recoveryContexts; ///< Per-role recovery bookkeeping, keyed by roleKey(role).
};

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::ResultRow)
Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::CycleResult)
Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::TaskLogEntry)
Q_DECLARE_METATYPE(std::shared_ptr<mtc::IRobotPickingChecker>)

#endif // LOCALIZATION_RUNTIME_CONTROLLER_H
