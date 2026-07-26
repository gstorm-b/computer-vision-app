#ifndef TASK_LOCALIZATION_H
#define TASK_LOCALIZATION_H

#include "model/itask.h"
#include "device/camera/camera_device.h"
#include "device/plc/mc_protocol_device.h"
#include "model/localization_pipeline.h"
#include "model/localization_runtime_controller.h"
#include "task_localization_config.h"
#include "matching/pattern_group_manager.h"
#include <opencv2/imgcodecs.hpp>

// Runtime runners (for typed access inside executeLocalization)
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"

/// Task model layer: TaskLocalization, the localization ITask implementation that owns
/// the pattern library, runtime controller, and matching worker thread.
namespace vc::model {

/// Localization ITask implementation. Binds a PLC, one or more cameras, and a vision
/// output device; drives a LocalizationRuntimeController through commissioning and
/// runtime cycles on a per-device runner model; and persists its pattern library
/// (via PatternGroupManager) and config (TaskLocalizeConfig) to/from JSON.
class TaskLocalization : public ITask {
    Q_OBJECT

public:
    /// Constructs the task: registers the cross-thread meta types used by its signals
    /// (MatchResult, cv::Mat, shared_ptr<MatchGroup>, shared_ptr<IRobotPickingChecker>),
    /// creates the pattern manager, starts the dedicated matching worker thread, and
    /// creates the initial runtime controller.
    /// @param name display name of the task
    /// @param id stable task id; ITask auto-generates one when left empty
    explicit TaskLocalization(QString name, QString id = "", QObject* parent = nullptr);

    /// Destroys the runtime controller and stops the matching worker thread, waiting
    /// up to 3000 ms for it to quit (logs a warning if it does not stop in time).
    ~TaskLocalization() override;

    /// Returns TaskType::LocalizationTask.
    TaskType taskType()  const override {
        return TaskType::LocalizationTask;
    }

    /// Returns whether setupTask() has last confirmed the assigned devices and
    /// runtime controller are usable.
    bool isValid() const override {
        return m_isValid;
    }

    /// Returns true once the number of already-assigned devices of type `t` has
    /// reached the per-type cap in m_limitDeviceMap (kLimitCommDevice / 1,
    /// kLimitVisionOutputDevice / 1, kLimitNumCamera / 16). Also returns true
    /// (fail-safe) if the project or its device manager isn't available.
    bool isReachLimitOfDeviceType(vc::device::DeviceType t) const override;

    /// Starts localization runtime: syncs runners with assigned devices, enters the
    /// runtime phase, (re)creates and moves the runtime controller to the runtime
    /// thread, then calls setupTask(). Transitions to Faulted if setup fails,
    /// otherwise emits runtimeStarted().
    /// @param mergeToTaskThread ignored — localization always keeps per-device
    ///        runner threads; a warning is logged if this is true
    void beginRuntime(bool mergeToTaskThread = false) override;

    /// Tears down and recreates the runtime controller around ITask::endRuntime().
    void endRuntime() override;

    /// Tears down and recreates the runtime controller around ITask::stopAll().
    void stopAll() override;

    /// Replaces the task's config, re-applies it via ITask::setTaskConfig(), and
    /// queues a reconfigure of the runtime controller with the new config.
    void setTaskLocalizeConfig(const TaskLocalizeConfig &cfg);

    /// Returns a copy of the current localization config (cheap: TaskLocalizeConfig
    /// is copy-on-write via QSharedData).
    TaskLocalizeConfig taskLocalizeConfig() const {
        return m_config;
    }

    /// Kicks off commissioning (single-shot, GUI-triggered) matching on the matching
    /// worker thread via startCommissionMatchingRequest.
    /// @param group pattern group to match against; deep-copied on this (GUI) thread
    ///        before crossing to the worker thread (see snapshotPatternGroup()) since
    ///        MatchGroup is non-QObject/unlocked and would otherwise race concurrent
    ///        pattern edits (addPattern/removePattern/setPatternImage)
    /// @param image source image to match; cloned before crossing threads
    /// @param workspace camera workspace (ROI) the match runs within
    void startCommissionMatching(
        std::shared_ptr<mtc::MatchGroup> group, cv::Mat image, vc::model::CameraWorkspace workspace) {

        // Deep-copy the live group on the caller's (GUI) thread before handing
        // it to the matching worker thread. MatchGroup is non-QObject and
        // unlocked, so the worker must read an isolated snapshot — otherwise it
        // races concurrent pattern edits (addPattern/removePattern/
        // setPatternImage). See snapshotPatternGroup().
        emit startCommissionMatchingRequest(
            snapshotPatternGroup(group), image.clone(), workspace);
    }

    /// Returns the task's pattern-group manager (owns all pattern groups/patterns;
    /// created as a QObject child of this task in the constructor).
    mtc::PatternGroupManager* patternManager() const {
        return m_patternManager;
    }

    /// Returns the image BLOBs owned by this task, paired with the JSON written by
    /// toJson(). ProjectRepository stores these in the project_images table and
    /// re-injects them on load via loadTaskImageMap().
    /// @return map keyed "g{groupNumber}_p{patternNumber}" for each pattern's raw
    ///         training image (patterns without one are skipped) plus
    ///         "ws_{cameraId}" for each camera workspace's reference image
    QMap<QString, cv::Mat> getTaskImageMap() override;

    /// Re-injects image BLOBs previously produced by getTaskImageMap(), routing each
    /// entry by its key: "ws_{cameraId}" sets a camera workspace reference image,
    /// "g{groupNumber}_p{patternNumber}" sets the matching pattern's training image.
    /// Unparseable keys or unresolved group/pattern references are logged and skipped
    /// rather than aborting the whole load.
    /// @param mapping image map as returned by getTaskImageMap()
    /// @return true if every entry was parsed and applied successfully
    bool                   loadTaskImageMap(QMap<QString, cv::Mat> &mapping) override;

    /// Serializes base ITask state plus the pattern library, written as the nested
    /// "patternManager" object (delegates to PatternGroupManager::toJson()).
    QJsonObject toJson() const override;

    /// Restores base ITask state via ITask::fromJson(), then rebuilds the pattern
    /// library from the nested "patternManager" object. Pattern training images are
    /// restored separately, later, by loadTaskImageMap().
    /// @return false if either the base restore or the pattern manager restore fails
    bool fromJson(const QJsonObject& obj) override;

    /// Family-level typed access to the assigned PLC device via the runner.
    /// Vendor-specific access (e.g. Mitsubishi frame type) is reached by
    /// qobject_cast<McProtocolDevice *>(plcDevice()) at the call site.
    /// @return nullptr if no PLC device id is bound yet or its runner isn't found
    vc::device::PlcDevice *plcDevice() const;

public slots:
    /// Runs LocalizationRuntimeController::setup() for the current device bindings
    /// (via setupRuntimeController()), caches the resolved primary PLC device id, and
    /// sets m_isValid from the result.
    void setupTask();

    /// Queues a single localization cycle (LocalizationRuntimeController::execute())
    /// on the controller's thread. No-op (with a log message) unless the runtime
    /// controller is valid and the task state is Ready or RunningCycle.
    void executeLocalization();

    /// Switches the active camera by its configured number, after validating the
    /// task is in the runtime phase, the number resolves to an assigned camera
    /// device id, and that device is actually a Camera. Queues the change onto the
    /// runtime controller; logs a warning and does nothing on any validation failure.
    void setCameraNumber(int number);

    /// Queues activation of the given pattern-group number on the runtime controller.
    void setPatternNumber(int number);

private slots:
    // ── Signals value change method ───────────────────────────────────────
    /// Forwards a batch of changed PLC signal values to the runtime controller
    /// (queued via queueHandlePlcValues()).
    void onCommDeviceValueChanged(QMap<QString, QVariant> values);

    /// Parses `value` as an int and calls setCameraNumber(); logs a warning if the
    /// value isn't convertible.
    void onSignalChangeCameraNumber(QVariant value);

    /// Parses `value` as an int and calls setPatternNumber(); logs a warning if the
    /// value isn't convertible.
    void onSignalChangePatternNumber(QVariant value);

    /// Transitions the task state to RunningCycle.
    void onRuntimeCycleStarted(const QString &message);

    /// Transitions the task state to Recovering.
    void onRuntimeRecovering(const QString &message);

    /// Transitions the task state to Ready, unless the task is currently Faulted,
    /// Stopping, or Idle (those states are not overridden by a runtime-ready event).
    void onRuntimeReady(const QString &message);

    /// Transitions the task state to Faulted.
    void onRuntimeFault(const QString &message);

private:
    /// Lazily allocates m_runtimeController (no-op if one already exists), configures
    /// it with the current m_config, and wires its signals to this task.
    void createRuntimeController();

    /// Disconnects and deletes m_runtimeController. If it lives on a different,
    /// still-running thread, deletion is deferred via deleteLater() posted to that
    /// thread instead of a direct delete.
    void destroyRuntimeController();

    /// Connects LocalizationRuntimeController signals (signalChanged,
    /// cycleResultUpdated, taskLogAppended, runtime state signals) to this task's
    /// slots, and — if the matching worker exists — connects
    /// runtimeMatchingRequested to a lambda run on the matching worker thread that
    /// (re)builds the persistent matcher when the active pattern group changes and
    /// reports the match result back to the controller.
    void wireRuntimeControllerSignals();

    /// Creates m_matchingWorker, moves it to the matchingRunner thread, and wires the
    /// commission-matching request/finished plumbing (startCommissionMatchingRequest
    /// -> ImageMatcher run -> commissionMatchingFinished).
    void wireMatchingWorkerSignals();

    /// Assembles a RuntimeContext snapshot for LocalizationRuntimeController::setup():
    /// current config, resolved PLC/vision-output/camera runners, per-camera
    /// calibrators, the robot kinematic-check config from the assigned vision output
    /// device, and deep-copied pattern-group snapshots (see snapshotPatternGroup()).
    LocalizationRuntimeController::RuntimeContext buildRuntimeContext() const;

    /// Deep-copies a pattern group (group config + per-pattern config incl. the
    /// raw training image) into an independent MatchGroup. Worker threads read
    /// this snapshot instead of the live PatternGroupManager group, so GUI-thread
    /// mutations cannot race the matcher. Used by both the runtime context build
    /// and the commission matching path.
    /// @return nullptr for a null source
    static std::shared_ptr<mtc::MatchGroup>
    snapshotPatternGroup(const std::shared_ptr<mtc::MatchGroup> &source);

    /// Builds the runtime context (buildRuntimeContext()) and calls
    /// LocalizationRuntimeController::setup() with it, blocking (via
    /// BlockingQueuedConnection) if the controller lives on another thread.
    LocalizationRuntimeController::SetupResult setupRuntimeController();

    /// Queues (or, if already on the controller's thread, runs inline)
    /// LocalizationRuntimeController::configure() with a copy of the current config.
    void queueConfigureRuntimeController();

    /// Queues LocalizationRuntimeController::setActiveCameraNumber(number).
    void queueSetActiveCameraNumber(int number);

    /// Queues LocalizationRuntimeController::setActivePatternGroupNumber(number).
    void queueSetActivePatternGroupNumber(int number);

    /// Queues LocalizationRuntimeController::handlePlcValues() with a copy of
    /// `values` captured by the invoked lambda.
    void queueHandlePlcValues(const QMap<QString, QVariant> &values);

signals:
    /// Declared for a PLC I/O cycle start notification; not currently emitted
    /// anywhere in this class (PLC-related updates are instead routed through the
    /// generic ITask::signalChanged() forwarded from the runtime controller).
    void startPLCRequest();

    /// Declared for a camera capture start notification; not currently emitted
    /// anywhere in this class (see startPLCRequest()).
    void startCameraRequest();

    /// Emitted on the GUI thread when a commission match started via
    /// startCommissionMatching() completes, carrying the match result.
    void commissionMatchingFinished(mtc::MatchResult result);

    /// Forwarded from LocalizationRuntimeController::cycleResultUpdated: emitted
    /// when a runtime localization cycle finishes.
    void cycleResultUpdated(vc::model::LocalizationRuntimeController::CycleResult result);

    /// Forwarded from LocalizationRuntimeController::taskLogAppended: emitted when a
    /// new runtime log entry is produced.
    void taskLogAppended(vc::model::LocalizationRuntimeController::TaskLogEntry entry);

    /// Declared for an active-camera-changed notification; not currently emitted
    /// anywhere in this class.
    void cameraChanged(QString name);

    /// Declared for an active-pattern-changed notification; not currently emitted
    /// anywhere in this class.
    void patternChanged(QString name);

private:
    signals:
        /// Internal signal used to hand a commission-matching request off to the
        /// matching worker thread; see startCommissionMatching().
        void startCommissionMatchingRequest(std::shared_ptr<mtc::MatchGroup> group, cv::Mat image, CameraWorkspace workspace);

private:
    // ── Helpers for typed runner access ───────────────────────────────────────
    /// Looks up the runner bound to `deviceId` and casts it to CameraRunner.
    /// @return nullptr if the device isn't registered or is the wrong type
    vc::runtime::CameraRunner *cameraRunner(const QString &deviceId) const;

    /// Looks up the runner bound to `deviceId` and casts it to PlcRunner.
    /// @return nullptr if the device isn't registered or is the wrong type
    vc::runtime::PlcRunner    *plcRunner(const QString &deviceId)    const;

    QThread *matchingRunner{nullptr};      ///< Dedicated worker thread ("LocalizationMatchingThread") that runs matching off the GUI thread.
    QObject *m_matchingWorker{nullptr};    ///< Worker object moved to matchingRunner; executes commission and runtime matching.

public:
    static constexpr int kLimitCommDevice = 1;          ///< Max number of PLC devices this task accepts.
    static constexpr int kLimitVisionOutputDevice = 1;   ///< Max number of vision-output devices this task accepts.
    static constexpr int kLimitNumCamera = 16;           ///< Max number of camera devices this task accepts.

private:
    QMap<device::DeviceType, int> m_limitDeviceMap;   ///< Per-device-type assignment caps, checked by isReachLimitOfDeviceType().

    bool m_isValid{false};   ///< Result of the last setupTask() call; see isValid().

    TaskLocalizeConfig m_config;   ///< Task's persisted configuration (signal name bindings, device bindings, camera workspaces).

    /// Device objects are retrieved from DeviceManager via taskRunner().
    /// Typed cached pointers below are populated in setupTask() after
    /// commission has confirmed which deviceId plays each role.
    QString m_plcDeviceId;

    LocalizationPipeline m_pipeline;   ///< Loads/runs the matcher model used for both commission and runtime matching.
    LocalizationRuntimeController *m_runtimeController{nullptr};   ///< Owns the per-cycle runtime state machine; recreated by createRuntimeController()/destroyRuntimeController().
    mtc::PatternGroupManager *m_patternManager;   ///< Owns the task's pattern groups/patterns; QObject child of this task.

    /// Persistent runtime matcher — built once and reused across cycles (touched
    /// only on the matchingRunner thread). The learned model is reloaded only
    /// when the active pattern group changes (m_loadedRuntimeGroup identity).
    std::unique_ptr<mtc::ImageMatcher> m_runtimeMatcher;
    std::shared_ptr<mtc::MatchGroup> m_loadedRuntimeGroup;   ///< Pattern group currently loaded into m_runtimeMatcher, or nullptr if none/stale.
};


}


/// Registers these types with Qt's meta-type system so they can be carried across
/// threads by queued signal/slot connections (runtime controller and matching
/// worker signals).
Q_DECLARE_METATYPE(mtc::MatchResult)
Q_DECLARE_METATYPE(cv::Mat)
Q_DECLARE_METATYPE(std::shared_ptr<mtc::MatchGroup>)

#endif // TASK_LOCALIZATION_H
