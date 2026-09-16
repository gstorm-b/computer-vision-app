#ifndef LOCALIZATION_RUNTIME_CONTROLLER_H
#define LOCALIZATION_RUNTIME_CONTROLLER_H

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include <memory>
#include <optional>

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

/**
 * @file localization_runtime_controller.h
 * @brief LocalizationRuntimeController — runs one localization task at runtime: per-cycle state
 *        machine, device-role binding, and connection recovery.
 */

/// Forward declarations of runtime runner types, referenced here only via QPointer/raw pointer
/// so this header does not need to include the full runtime runner headers.
namespace vc::runtime {
class CameraRunner;
class IDeviceRunner;
class PlcRunner;
class VisionOutputRunner;
}

/// Held only by shared_ptr in RuntimeContext, so a forward declaration is enough here and the
/// PLC family header stays out of everything that includes the controller.
namespace vc::device {
class PlcValueMap;
}

namespace vc::model {

/**
 * @class LocalizationRuntimeController
 * @brief Runs one localization task at runtime.
 *
 * Binds the task's primary PLC, vision-output, and active-camera device runners into fixed
 * "roles", drives the per-cycle state machine (wait for trigger -> grab -> match -> convert to
 * world coordinates -> send to vision output), and monitors/recovers each role's connection
 * according to its LocalizationRecoveryPolicy. Task-relevant state changes (ready/fault/
 * recovering, signal values, cycle results, log lines) are published via Qt signals for the UI
 * and the PLC (through publishBoolSignal / publishNumberSignal).
 */
class LocalizationRuntimeController : public QObject {
    Q_OBJECT

public:
    /**
     * @struct RuntimeContext
     * @brief Snapshot of a task's device/pattern/calibration bindings, supplied to setup() and
     *        rebuilt by the owner whenever roles or the active camera/pattern-group selection
     *        change.
     */
    struct RuntimeContext {
        TaskLocalizeConfig config;                            ///< Localization task configuration used for this runtime session.
        QString primaryPlcDeviceId;                           ///< Device id bound to the "primary_plc" role.
        QString visionOutputDeviceId;                         ///< Device id bound to the "vision_output" role.
        QPointer<vc::runtime::PlcRunner> primaryPlcRunner;    ///< Runner for the primary PLC role; null if not available.
        /// Runner for the vision output role; null if not available. Held as the abstract
        /// IDeviceRunner, not VisionOutputRunner: the role is a capability
        /// (IDeviceRunner::supportsResultOutput()), so any family's runner can fill it.
        QPointer<vc::runtime::IDeviceRunner> visionOutputRunner;
        QMap<int, QString> cameraDeviceIds;                   ///< Camera device id keyed by configured camera number.
        QMap<int, QPointer<vc::runtime::CameraRunner>> cameraRunners; ///< Camera runner keyed by camera number.
        QMap<int, std::shared_ptr<mtc::MatchGroup>> patternGroups;    ///< Pattern match group keyed by pattern-group number.
        QMap<int, calib::Calibrator> cameraCalibrators;       ///< Calibrator keyed by camera number, used to map image points to robot/world coordinates.
        /// Robot pick-check settings, taken from the TASK — TaskLocalizeConfig::robotCheckConfig()
        /// — never from the device bound to vision_output (Phase 9 / F1). Drives the per-object
        /// robotPossiblePickingCheck. setup() refuses to start when the check is enabled but no
        /// usable checker can be built (see rebuildPickingChecker()).
        vc::device::RobotKinematicCheckConfig robotCheckConfig;
        CameraWorkspace activeCameraWorkspace;                ///< Workspace crop/offset settings for the active camera.
        int activeCameraNumber{-1};                           ///< Camera number to activate at setup; -1 selects the first available.
        int activePatternGroupNumber{-1};                     ///< Pattern group number to activate at setup; -1 selects the first available.
        /**
         * @brief What the primary PLC held when the runtime started, if it had been read yet.
         *
         * The two active-index signals are **inputs the PLC owns**. Resolving them from the
         * project file — which is what happened until Phase 9 / C6 — is wrong by construction,
         * and it is why a task reached Ready with the master's registers at 0 and ran cycles on
         * whichever camera sorted first (backlog item 58).
         *
         * Null is a meaningful value and means *"the PLC has not been read"*, which is treated
         * exactly like an unmapped signal: fall back to the project default. It must never be
         * read as "the PLC holds 0".
         */
        std::shared_ptr<vc::device::PlcValueMap> plcSnapshot;
    };

    /**
     * @struct ResultRow
     * @brief One matched object's localization result, as reported to the UI/log: image-space
     *        and world-space (robot) position, plus the disposition status assigned by
     *        buildVisionOutputPositions().
     */
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

    /**
     * @struct CycleTimings
     * @brief Where the time in one cycle went: milliseconds from trigger-accept to each boundary.
     *
     * Until Phase 9 / F3 nothing measured a cycle end to end — `CycleResult::matchingTimeMs` is
     * the matcher's own number and covers one stage of four. That left the standing criterion
     * *"revisit threading only on measured latency evidence"*
     * (`phase2_phase3_runtime_hardening.md`) impossible to evaluate, and a customer asking "what
     * is my cycle time" unanswerable.
     *
     * @note Every field is elapsed time from the SAME origin (trigger accepted) on ONE monotonic
     *       clock, never wall time: an NTP step can move the system clock backwards mid-cycle and
     *       would produce negative stage durations.
     * @note Unset means **the cycle never reached that boundary** — that is why these are
     *       `std::optional` and not zero-initialised doubles. A zero reads as "instant", and the
     *       slow stage before a timeout is the single most interesting number in the phase.
     */
    struct CycleTimings {
        std::optional<double> grabFinishedMs;       ///< Camera answered the single-shot request.
        std::optional<double> matchingFinishedMs;   ///< Matcher returned its result.
        std::optional<double> sendFinishedMs;       ///< Vision-output device reported the send complete.
        std::optional<double> outputsPublishedMs;   ///< Handshake outputs published; the cycle's total.
    };

    /**
     * @struct CycleResult
     * @brief Outcome of one localization cycle (grab + match + send), published via
     *        cycleResultUpdated() for UI display and task logging.
     */
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
        /// Stage boundaries for this cycle. Unset fields are boundaries the cycle never reached;
        /// see CycleTimings. `matchingTimeMs` above keeps its existing meaning and is NOT derived
        /// from these — it is the matcher's own measurement of its own work.
        CycleTimings timings;
    };

    /**
     * @struct TaskLogEntry
     * @brief One timestamped task-log line surfaced to the UI via taskLogAppended().
     */
    struct TaskLogEntry {
        QDateTime timestamp;  ///< Time the log line was appended.
        QString severity;     ///< Severity tag used by appendTaskLog(), e.g. "INFO", "WARN", "ERROR".
        QString message;      ///< Log message text.
    };

    /**
     * @struct SetupResult
     * @brief Outcome of setup(): whether the supplied RuntimeContext was valid to run, plus the
     *        resolved role device ids and any validation errors.
     */
    struct SetupResult {
        bool valid{false};                  ///< True if the context passed all validation checks.
        QString primaryPlcDeviceId;         ///< Device id resolved for the primary PLC role.
        QString visionOutputDeviceId;       ///< Device id resolved for the vision output role.
        QStringList errors;                 ///< Human-readable validation failures; empty when valid.
    };

    /// Delay after which a latched cycle fault clears itself when no bErrorReset rising
    /// edge arrives, in milliseconds. The runtime must never park on a transient fault
    /// waiting for an operator, so the acknowledge input is the fast way out of a fault
    /// rather than the only one.
    static constexpr int kFaultAutoRecoverMs = 2000;

    /// How many consecutive automatic recoveries pass before one is reported at USER
    /// level. A repeating auto-recovered fault is a real problem, but logging every
    /// occurrence would bury the event log the automatic path exists to keep readable.
    static constexpr int kAutoRecoverWarnStride = 5;

    /// How many reconnect attempts pass between USER-level retry reports during one
    /// outage. Attempt 1 is always reported; at the default 5000 ms retry interval this
    /// works out to roughly one line per minute thereafter. Every attempt is still
    /// recorded at DEV level, so a flapping link stays diagnosable after the fact.
    static constexpr int kQuietRetryLogStride = 12;

    /// How many ATTEMPTS a HANDSHAKE output write gets in total — the original plus re-issues —
    /// before the cycle is aborted with LocalizationFaultCode::PlcWriteFailed. Three attempts is
    /// one write and two re-issues: publishBoolSignal()/publishNumberSignal() track the first
    /// attempt as 1, and onPlcWriteFinished() re-issues only while attempts < this budget. (This
    /// comment used to say "re-issued" three times, which counted one too many.)
    ///
    /// Three, not more: a write that fails three times in a row on a link that is still reported
    /// connected is not a transient collision, and every further attempt delays the fault the PLC
    /// needs in order to stop. Only the five signals the PLC's own logic WAITS on are retried —
    /// see kHandshakeSignals. Retrying all fourteen would turn a degraded link into a write storm.
    static constexpr int kPlcWriteRetryBudget = 3;

    /// Delay between re-issues of a failed handshake write, in milliseconds.
    ///
    /// Short on purpose. The PLC is blocked waiting for this value; the delay exists only so a
    /// momentary collision has time to clear, not to pace a recovery. kPlcWriteRetryBudget
    /// attempts at this interval bound the whole escalation at roughly 120 ms, well inside the
    /// kFaultAutoRecoverMs window that follows.
    static constexpr int kPlcWriteRetryDelayMs = 40;

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
    /**
     * @brief Replaces the per-role (camera/PLC/vision-output) reconnect policies used by the
     *        connection-recovery state machine.
     *
     * @warning **No production caller exists.** The only caller in the repository is the
     *          architecture contract test, which shortens the retry interval so it can cover many
     *          attempts. The product never sets policies, so every runtime runs on
     *          defaultCameraRecoveryPolicy() / defaultPlcRecoveryPolicy() /
     *          defaultVisionOutputRecoveryPolicy() — those defaults ARE the shipped behaviour.
     *          Persisting per-task values is deferred (Phase 9 / D5).
     *
     * @note Policies are copied into a role's recovery context when that role is BOUND — by
     *       setup(), and for the camera role also by an active-camera change. Recovery reads that
     *       copy, so a call made after binding changes nothing until the next bind. When a
     *       production caller is added, the injection point is
     *       TaskLocalization::setupRuntimeController(), immediately before `controller->setup(context)`
     *       and inside the same thread hop — the controller already lives on the runtime thread.
     */
    void setRecoveryPolicies(const LocalizationRecoveryPolicy &cameraPolicy,
                             const LocalizationRecoveryPolicy &plcPolicy,
                             const LocalizationRecoveryPolicy &visionOutputPolicy);

    /**
     * @brief Binds `context`'s role runners/cameras/patterns/calibration, validates them, and,
     *        if valid, requests a connection for every role and marks the runtime ready once
     *        all roles are healthy.
     * @param[in] context device/pattern/calibration bindings for this task run
     * @return validation outcome; SetupResult::valid is false if a required role, the active
     *         pattern group, or the active camera calibration is missing/invalid
     */
    SetupResult setup(const RuntimeContext &context);
    /// Manually starts a localization cycle, as if triggered by the PLC. No-op (logged) if the
    /// runtime is not valid or is not currently ReadyForTrigger.
    void execute();
    /// Returns whether the last setup() call produced a usable (fully validated) configuration.
    bool isValid() const { return m_valid; }

    /// The five signals a runtime cannot work without. Everything else is optional and only warns.
    ///
    /// Without `bExecuteTrigger` nothing can start a cycle, without `bTaskReady` the master never
    /// learns it may trigger, and without `bMatchingFinished`/`bTaskFault`/`nFaultCode` the result
    /// of a cycle reaches nobody. A project missing any of them starts and then sits silent, which
    /// is indistinguishable from a PLC that stopped talking.
    ///
    /// Public because the commissioning UI classifies orphaned rows against exactly this list
    /// before offering to purge them. One definition: a second copy in the widget would drift, and
    /// the drift would let the editor purge a signal the runtime still demands.
    static QStringList requiredSignalNames();

    /**
     * @brief Maps raw PLC tag values to named signal events via the signal mapper, and reacts to
     *        active-camera / active-pattern-group changes, rising/falling edges of the
     *        bExecuteTrigger signal (starts a cycle on rising edge; on falling edge, resets the
     *        matching-finished output and either recovers a pending fault or marks the runtime
     *        ready), and the rising edge of bErrorReset (acknowledges a latched fault).
     * @param[in] values raw values keyed by PLC signal name, as received from the primary PLC
     *            runner
     */
    void handlePlcValues(const QMap<QString, QVariant> &values);
    /**
     * @brief Receives the completed match result for `cycleId`, converts it to world-space
     *        positions, and forwards them to the vision output device. Stale results (cycleId
     *        mismatch, or the cycle is no longer running) are silently dropped.
     * @param[in] cycleId     cycle id the result belongs to
     * @param[in] matchResult matcher output for the grabbed frame
     */
    void onRuntimeMatchingFinished(int cycleId, mtc::MatchResult matchResult);

signals:
    /// Emitted whenever a named PLC-mapped signal value is published (mirrors every
    /// publishBoolSignal / publishNumberSignal call for UI/logging).
    void signalChanged(QString name, QVariant value);
    // Phase 9 / C5 removed cameraNumberChanged() / patternNumberChanged(). They were emitted from
    // handlePlcValues() and connected by nothing — checked across src/, app/, runtime_app/ and
    // tests/, not assumed. Both indices already reach every consumer through signalChanged(), and
    // since C3 the adopted value also arrives on nActiveCameraStatus / nActivePatternGroupStatus.
    /// Emitted with the final result of each cycle, whether it succeeded or faulted.
    void cycleResultUpdated(vc::model::LocalizationRuntimeController::CycleResult result);
    /// Emitted for every task log line appended via appendTaskLog().
    void taskLogAppended(vc::model::LocalizationRuntimeController::TaskLogEntry entry);
    /// Emitted when a localization cycle begins (trigger accepted).
    void runtimeCycleStarted(QString message);
    /// Emitted when a role starts recovering, or when its unhealthy status changes during
    /// an outage — NOT on every retry. Reconnect is unbounded, so an outage lasting
    /// minutes would otherwise emit an identical line every retryIntervalMs and bury the
    /// operator's event log.
    void runtimeRecovering(QString message);
    /// Emitted when the runtime (re)enters the ReadyForTrigger state.
    void runtimeReady(QString message);
    /// Emitted when the runtime enters the Faulted state: an invalid setup, a refused
    /// active-index selection (written at runtime, or commanded by the PLC at startup), a
    /// non-numeric value on an index signal, or a handshake write that exhausted
    /// kPlcWriteRetryBudget (escalatePlcWriteFailure()). A lost device connection does NOT reach
    /// here: role recovery retries indefinitely instead of escalating.
    void runtimeFault(QString message);
    /**
     * @brief Emitted after a successful camera grab to hand the frame off for matching (on
     *        whatever external thread/service listens for this signal).
     * @param[in] cycleId        id of the cycle the frame belongs to, echoed back via
     *            onRuntimeMatchingFinished()
     * @param[in] group          pattern group snapshot to match against
     * @param[in] workspace      active camera workspace (crop/offset) settings
     * @param[in] image          grabbed frame to match
     * @param[in] pickingChecker robot-pickability checker to gate matches with, or null if disabled
     */
    void runtimeMatchingRequested(int cycleId,
                                  std::shared_ptr<mtc::MatchGroup> group,
                                  CameraWorkspace workspace,
                                  cv::Mat image,
                                  std::shared_ptr<mtc::IRobotPickingChecker> pickingChecker);

private:
    /**
     * @struct RoleRecoveryContext
     * @brief Per-role bookkeeping for the connection-recovery state machine: the runner/device
     *        bound to a role, its recovery policy, and the current retry/fault progress against
     *        that policy.
     */
    struct RoleRecoveryContext {
        QString roleName;                              ///< Role name from the bound LocalizationRecoveryPolicy (for logging/messages).
        QString deviceId;                               ///< Device id bound to this role.
        QPointer<vc::runtime::IDeviceRunner> runner;    ///< Runner bound to this role.
        LocalizationRecoveryPolicy policy;               ///< Reconnect policy in effect for this role.
        int retryCount{0};                               ///< Number of reconnect attempts made since the last healthy connection; unbounded.
        bool retryScheduled{false};                      ///< True while a scheduleRoleReconnect() timer is pending.
        /// Unhealthy status already reported for the current outage. Repeats of the same
        /// status are retried silently at USER level: an outage that lasts minutes would
        /// otherwise emit one identical "recovering" line every retryIntervalMs and bury
        /// everything else in the operator's event log.
        vc::device::ConnectStatus reportedStatus{vc::device::ConnectStatus::Connected};
        QDateTime outageStartedAt;                       ///< When the current outage began; reported as elapsed time on recovery.
        /// Handle for this role's connectStatusChanged connection, so clearRoleContext() can drop
        /// exactly it. A blanket disconnect would also take connections another role — or the PLC
        /// value stream — made on the same runner; see clearRoleContext().
        QMetaObject::Connection statusConnection;
        /// Handle for the PrimaryPlc role's writeFinished connection (Phase 9 / E4). Unused by
        /// the other two roles, which issue no tracked writes.
        QMetaObject::Connection writeConnection;
        /// Handle for this role's errorOccurred connection; same reasoning as statusConnection.
        QMetaObject::Connection errorConnection;
    };

    /**
     * @enum RunnerRole
     * @brief The three device roles the controller coordinates.
     */
    enum class RunnerRole {
        Camera,
        PrimaryPlc,
        VisionOutput
    };

    /**
     * @enum CycleState
     * @brief Lifecycle state of the runtime / current localization cycle.
     */
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
    /// Resolves the active camera's workspace (crop ROI + condition ROI) from the task config into
    /// both m_context.activeCameraWorkspace and m_activeCameraWorkspace, or clears both when no
    /// camera runner is bound.
    ///
    /// Called from setup() **and** from setActiveCameraNumber(). It exists as a helper because
    /// only the setter used to do it: a runtime that started and was never commanded to change
    /// camera ran its entire session on a default workspace, which silently disables both the
    /// commissioned crop and the condition-ROI filter.
    void applyActiveCameraWorkspace();

    /// Why a commanded active-index value was refused — or that it was accepted.
    ///
    /// The two reasons are kept apart because they send a commissioning engineer to different
    /// screens: `OutOfRange` is a number that can never name a slot, `NotRegistered` is a number
    /// that could but is not bound in this project.
    enum class IndexVerdict {
        Accepted,       ///< Names a registered camera / a pattern group in range.
        OutOfRange,     ///< 0, negative, or past the last slot the product defines.
        NotRegistered   ///< In range, but nothing is bound to it in this project.
    };

    /// A verdict plus the message that names the number and the reason it was refused.
    struct IndexValidation {
        IndexVerdict verdict{IndexVerdict::Accepted};
        QString message;   ///< Empty when accepted.
        bool accepted() const { return verdict == IndexVerdict::Accepted; }
    };

    /// Validates a commanded camera number against the product's slot range and this project's
    /// bindings.
    ///
    /// One definition, three callers — both setters and setup(). They used to carry three
    /// duplicated inline blocks, which is how setup() ended up not checking at all: an
    /// unregistered active camera reached validateActiveCameraCalibration() and surfaced as
    /// *"Active camera calibration is invalid."*, which is the wrong screen (backlog item 55).
    IndexValidation validateCameraNumber(int cameraNumber) const;
    /// Validates a commanded pattern-group number against MatchGroup's own range. See
    /// validateCameraNumber() for why this is a function rather than an inline block.
    IndexValidation validatePatternGroupNumber(int patternGroupNumber) const;

    /**
     * @brief Reads the index the PLC currently holds for `signalName`, from the setup snapshot.
     * @param[out] number       the value, when one was found and it is numeric.
     * @param[out] typeMismatch set when a value was found but is not a number — the tag is bound
     *                          to a bit area, which no index range check can diagnose.
     * @return true only when the PLC actually supplied a number for this signal.
     *
     * Family-independent by construction: it goes through PlcValueMap, so MC, both Modbus roles
     * and the virtual PLC are served by one path and the controller never casts to a device.
     *
     * **Three outcomes, and only one of them is "the PLC said something":** the signal is
     * unmapped, or the PLC has not been read / holds nothing for the tag — both fall back to the
     * project default — or a value came back. Collapsing the first two into "0" is the defect.
     */
    bool commandedIndexFromPlc(const QString &signalName, int *number, bool *typeMismatch) const;
    /// Writes the one-line startup summary naming the active camera and pattern group, their
    /// source (commanded vs project default), the camera's device id and the workspace state.
    void logStartupSelectionSummary();

    /// Validates the configured signal map before the runtime is allowed to start: required
    /// signals must be mapped, optional ones only warn, no two signals may share a tag, and every
    /// mapped tag must exist on the bound PLC — checked per kind (bit signals against the digital
    /// list, word signals against the word list), never against the union of the two.
    /// @param[in,out] result setup result whose `errors` list receives every hard failure
    void validateSignalMap(SetupResult *result);

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
    /// Core recovery state machine driven by a role's connection-status change: on
    /// reconnect, clears retry state and re-evaluates runtime readiness; on loss, aborts a
    /// running cycle (with a role-specific fault code) and then, via decideRecoveryAction(),
    /// schedules a retry. Retrying is unbounded — there is no escalation to a fault.
    void handleRoleStatusChanged(RunnerRole role, vc::device::ConnectStatus status);
    /// Schedules a one-shot reconnect attempt for `role` after its policy's retry interval.
    void scheduleRoleReconnect(RunnerRole role);
    /// Issues an immediate requestConnect() on the role's runner (dispatched by concrete runner
    /// type).
    void requestRoleConnectNow(RunnerRole role);
    /// Returns true when a reconnect attempt for `context` should be reported at USER level:
    /// the first attempt of an outage, then one in every kQuietRetryLogStride. Everything
    /// else goes to the developer log only, so a long outage stays one readable line per
    /// minute instead of one per retry.
    static bool shouldReportRetryToUser(const RoleRecoveryContext &context);
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

    // ── Handshake write policy (Phase 9 / E4, decision D3) ───────────────────────────────
    /// One handshake output write awaiting its completion, so it can be re-issued.
    struct TrackedWrite {
        QString signalName;   ///< Logical signal name, for the log line and the abort message.
        QString tag;          ///< PLC tag it was written to.
        bool isBool{true};    ///< Which runner entry point re-issues it.
        bool boolValue{false};
        int wordValue{0};
        int attempts{1};      ///< Issues so far, including the first.
    };

    /// @return true if `signalName` is one of the five outputs the PLC's own logic WAITS on, and
    ///         therefore the only ones whose write failure is retried and escalated.
    ///
    /// A lost `bMatchingFinished` hangs the PLC forever; a lost `bTaskFault` is worse, because
    /// the PLC believes the cycle succeeded and picks on results that were never valid. Every
    /// other output is advisory and stays log-only, exactly as before — retrying all fourteen
    /// would turn a degraded link into a write storm.
    static bool isHandshakeSignal(const QString &signalName);
    /// Records a handshake write so its completion can be matched and re-issued.
    void trackHandshakeWrite(quint64 id, const QString &signalName, const QString &tag,
                             bool isBool, bool boolValue, int wordValue, int attempts);
    /// Matches a PlcRunner::writeFinished against m_pendingWrites: forgets it on success,
    /// re-issues it while the budget lasts, escalates when the budget is gone.
    void onPlcWriteFinished(quint64 id, bool ok, const QString &message);
    /// Re-issues the write recorded under `id`, carrying its attempt count onto the new id.
    void reissueTrackedWrite(quint64 id);
    /// Aborts the cycle with PlcWriteFailed without letting the abort's own publishes re-enter
    /// the retry machinery.
    void escalatePlcWriteFailure(const TrackedWrite &write, const QString &reason);
    /// Drops every pending write and cancels a retry in flight, so a stale one cannot fire into
    /// the next cycle.
    void clearPendingWrites();

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

    /**
     * @brief Reports a number signal whose PLC value is not numeric, and faults the runtime.
     *
     * @param[in] signalName the logical signal name, e.g. "nActiveCamera"
     * @param[in] event the mapped event, whose tag names the register actually read
     *
     * Kept separate from the range and registration checks because it is a different mistake with
     * a different fix: the value is not a bad index, it is not an index at all. In practice this
     * means the signal was mapped to a bit area — a coil or discrete input — so no index check
     * could ever diagnose it, and naming the tag is what lets the integrator find the mapping.
     */
    void reportSignalTypeMismatch(const QString &signalName,
                                  const LocalizationSignalEvent &event);
    /// Reports a runner error to the app log and the task log, suppressing an immediate
    /// repeat of the same message from the same role (a dead PLC fails every queued write,
    /// so the same line would otherwise arrive many times a second).
    void reportRoleError(RunnerRole role, const QString &roleName, const QString &message);
    /**
     * @brief Checks that the active (or first available) pattern group exists and has at least
     *        one pattern with a non-empty train image.
     * @param[out] errors optional; a failure message is appended on validation failure
     * @return true if the active pattern group is usable for matching
     */
    bool validateActivePatternGroup(QStringList *errors = nullptr) const;
    /**
     * @brief Checks that the calibrator for the active camera is calibrated.
     * @param[out] errors optional; a failure message is appended on validation failure
     * @return true if the active camera has a valid calibration
     */
    bool validateActiveCameraCalibration(QStringList *errors = nullptr) const;
    /// Elapsed milliseconds since the in-flight cycle's trigger-accept, off m_cycleClock.
    /// @return the elapsed time, or 0.0 if no cycle has started this session
    double cycleElapsedMs() const;
    /// Renders a cycle's stage breakdown as one compact clause for the task log, reporting time
    /// spent IN each stage rather than cumulative time. Unset boundaries are omitted, never
    /// shown as zero.
    static QString formatCycleTimings(const CycleTimings &timings);

    /// (Re)build the robot-pickability checker for the active camera. Called once at runtime
    /// setup and again on active-camera change (calibrator differs per camera). Leaves
    /// m_pickingChecker null when the kinematic check is disabled or no valid calibration.
    /// @return empty when the checker matches what the config asked for; otherwise the reason
    ///         an enabled check has no usable checker (setup() turns that into a refusal).
    QString rebuildPickingChecker();
    /// Returns the pattern group for the active (or first available) pattern-group number, or
    /// null if none is available.
    std::shared_ptr<mtc::MatchGroup> snapshotActivePatternGroup() const;
    /**
     * @brief Converts a match result's objects into world-space positions and per-object result
     *        rows, applying the active camera workspace's crop offset, image-to-robot
     *        calibration, and collision/ROI/pickability filtering; at most 2 positions are
     *        accepted for sending, the rest are marked "Skipped".
     * @param[in]  matchResult matcher output for the cycle
     * @param[out] rows        optional; appended with one ResultRow per matched object
     * @param[out] faultCode   optional; set to CalibrationInvalid if the active camera has no
     *             valid calibration, otherwise None
     * @return world-space positions accepted for sending (at most 2)
     */
    QVector<vc::device::VisionOutputPosition> buildVisionOutputPositions(
        const mtc::MatchResult &matchResult,
        QVector<ResultRow> *rows,
        LocalizationFaultCode *faultCode) const;
    /// Returns the runner bound to the Camera role, or nullptr if none is bound.
    vc::runtime::CameraRunner *activeCameraRunner() const;
    /// Returns the runner bound to the PrimaryPlc role, or nullptr if none is bound.
    vc::runtime::PlcRunner *primaryPlcRunner() const;
    /// Returns the runner bound to the VisionOutput role, or nullptr if none is bound.
    /// Abstract type on purpose: the result path goes through the runner's capability, so it
    /// must not care which family the bound runner comes from.
    vc::runtime::IDeviceRunner *visionOutputRunner() const;
    /// Validates readiness (pattern group, calibration, camera runner availability), then
    /// transitions to Running, publishes cycle-start outputs, and requests a single-shot grab
    /// from the active camera.
    void startCycle();
    /// Tears down any in-flight grab/matching/send connections, records `code` as the cycle
    /// fault, publishes fault outputs, logs `message`, and transitions to WaitingTriggerReset or
    /// Recovering depending on whether the execute trigger is still asserted. Arms the
    /// fault auto-recovery timer when the fault comes to rest in Recovering.
    void abortCycle(LocalizationFaultCode code, const QString &message);

    // ── Fault acknowledge / automatic recovery ────────────────────────────────
    /// Handles a rising edge of the bErrorReset PLC input: cancels any pending automatic
    /// recovery (the operator got there first) and clears the fault via recoverFromFault().
    void acknowledgeFault();
    /// Clears the latched fault outputs (bTaskFault / nFaultCode) unconditionally, then
    /// attempts to re-arm the runtime via markRuntimeReady(). Shared by the acknowledge
    /// input and the automatic timeout so the two paths cannot drift apart; `reason` is
    /// the only thing that differs between them, and it is what the event log records.
    /// @param reason human-readable cause, logged and passed on as the ready message
    /// @note Clearing is unconditional but re-arming is not: markRuntimeReady() still
    ///       requires healthy roles and a valid pattern group / calibration. This is an
    ///       acknowledge, not a repair — if the cause persists, the next cycle re-faults.
    void recoverFromFault(const QString &reason);
    /// Starts the single-shot fault auto-recovery timer, unless one is already pending.
    /// Called only where a cycle fault comes to rest, never at the moment of failure:
    /// re-arming while the PLC still holds bExecuteTrigger high would break the
    /// rising-edge handshake.
    void armFaultAutoRecovery();
    /// Stops any pending fault auto-recovery timer.
    void cancelFaultAutoRecovery();

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
    QMetaObject::Connection m_visionOutputResultConnection; ///< Connection from the bound vision-output runner's IDeviceRunner::resultRequestFinished() for the in-flight cycle.
    bool m_valid{false};                              ///< True once setup() has validated the context successfully.
    int m_activeCameraNumber{-1};                     ///< Camera number currently bound to the Camera role; -1 if unset.
    /// Robot-pickability checker for the active camera (built at setup / camera change). Passed
    /// to the matcher each cycle; null = matching not gated.
    std::shared_ptr<mtc::IRobotPickingChecker> m_pickingChecker;
    CameraWorkspace m_activeCameraWorkspace;          ///< Workspace settings for the active camera (mirrors m_context.activeCameraWorkspace).
    int m_activePatternGroupNumber{-1};               ///< Pattern group number currently used for matching; -1 if unset.
    /// True while the last nActiveCamera value the master commanded was refused (out of range,
    /// not registered, or not a number at all).
    ///
    /// A refused number is deliberately never adopted — m_activeCameraNumber keeps pointing at
    /// the last good camera so the runtime stays usable — which means the refusal leaves no
    /// trace any of markRuntimeReady()'s other checks can see. Without this latch, ANY unrelated
    /// re-arm (a valid pattern-group write, a role reconnecting, a bErrorReset) republished
    /// bTaskReady and bCameraValid as true while the master's own command register still held
    /// the rejected number. The task would then run cycles on a camera nobody had selected.
    ///
    /// Cleared only when a value for THIS signal is accepted. An acknowledge does not clear it:
    /// bErrorReset is an acknowledge, not a repair.
    bool m_activeCameraSelectionRejected{false};
    /// True while the last nActivePatternGroup value the master commanded was refused. Same
    /// contract as m_activeCameraSelectionRejected, for the other index signal.
    bool m_activePatternGroupSelectionRejected{false};
    /// True when setup() resolved the active camera from the project's first binding rather than
    /// from a commanded value. Reported in the startup summary, because "the runtime is on camera
    /// 1" and "the runtime was told to use camera 1" are different facts and only one of them
    /// means the master and the task agree.
    bool m_activeCameraFromProjectDefault{false};
    /// Same, for the pattern group. See m_activeCameraFromProjectDefault.
    bool m_activePatternGroupFromProjectDefault{false};
    /// The message for an index the PLC was already holding when the runtime started and that
    /// setup() refused. Empty when the startup selection was accepted.
    ///
    /// Carried rather than turned into a SetupResult error on purpose: an index the master can
    /// change at any moment is a **recoverable** fault, not an unusable configuration. Reporting
    /// it as a setup error left `m_valid` false, and `markRuntimeReady()` gates on `m_valid` — so
    /// the runtime could never re-arm, and no valid write the master made afterwards had any
    /// effect. Field-confirmed 2026-09-08: camera and pattern both re-selected and valid, the
    /// camera reconnected, and `bTaskReady` stayed off until the task was stopped.
    QString m_startupSelectionFault;
    int m_activeCycleId{0};                           ///< Monotonically incremented id for the in-flight/most-recent cycle; used to discard stale async results.
    bool m_lastExecuteTrigger{false};                 ///< Last observed value of the PLC bExecuteTrigger signal, used for edge detection.
    bool m_lastErrorReset{false};                     ///< Last observed value of the PLC bErrorReset signal, used for edge detection.
    CycleState m_cycleState{CycleState::NotReady};    ///< Current lifecycle state of the runtime/cycle.
    CycleResult m_pendingCycleResult;                 ///< Result being assembled for the in-flight cycle.
    /// Monotonic clock for the in-flight cycle, restarted at trigger-accept. QElapsedTimer, not
    /// QDateTime: on Windows this is QueryPerformanceCounter, which no clock adjustment can move.
    /// Every CycleTimings field is an elapsed() read off this one timer, so the stages cannot
    /// disagree with each other about when the cycle began.
    QElapsedTimer m_cycleClock;
    /// Single-shot timer that clears a latched cycle fault after kFaultAutoRecoverMs when
    /// no bErrorReset arrives. A cancellable member rather than QTimer::singleShot
    /// precisely because an acknowledge must be able to win the race.
    QTimer m_faultRecoverTimer;
    int m_consecutiveAutoRecoveries{0};               ///< Automatic recoveries since the last successful cycle; drives the kAutoRecoverWarnStride report.
    LocalizationRecoveryPolicy m_cameraRecoveryPolicy{defaultCameraRecoveryPolicy()}; ///< Reconnect/retry policy for the Camera role.
    LocalizationRecoveryPolicy m_plcRecoveryPolicy{defaultPlcRecoveryPolicy()}; ///< Reconnect/retry policy for the PrimaryPlc role.
    LocalizationRecoveryPolicy m_visionOutputRecoveryPolicy{defaultVisionOutputRecoveryPolicy()}; ///< Reconnect/retry policy for the VisionOutput role.
    QHash<int, RoleRecoveryContext> m_recoveryContexts; ///< Per-role recovery bookkeeping, keyed by roleKey(role).
    QHash<int, QString> m_lastRoleError;                ///< Last error message reported per role, keyed by roleKey(role); used to suppress immediate repeats.

    // ── Handshake write tracking (Phase 9 / E4, decision D3) ─────────────────────────────
    /// Writes awaiting a completion, keyed by the id PlcRunner handed out.
    QHash<quint64, TrackedWrite> m_pendingWrites;
    /// True while the escalation path is publishing bTaskFault/nFaultCode. Two of the five
    /// tracked signals are published BY the abort, over the link that just failed — tracking
    /// those would re-enter the retry machinery and, on a dead link, never terminate.
    bool m_plcWriteEscalating{false};
    /// Cancellable so a retry in flight dies with the cycle rather than firing into the next one.
    QTimer m_plcWriteRetryTimer;
    /// Writes whose retry delay is pending, in issue order.
    QList<quint64> m_plcWriteRetryQueue;
};

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::ResultRow)
Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::CycleResult)
Q_DECLARE_METATYPE(vc::model::LocalizationRuntimeController::TaskLogEntry)
Q_DECLARE_METATYPE(std::shared_ptr<mtc::IRobotPickingChecker>)

#endif // LOCALIZATION_RUNTIME_CONTROLLER_H
