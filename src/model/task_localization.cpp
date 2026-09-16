#include "task_localization.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>

#include "device/device_capabilities.h"
#include "device/output_device/vision_output_config.h"
#include "device/virtual/virtual_device.h"
#include "matching/match_group.h"
#include "matching/match_pattern.h"
#include "model/project.h"
#include "runtime/vision_output_runner.h"

namespace vc::model {

namespace {

/// Image-blob key: builds the stable per-pattern key used in the project_images table.
/// Pattern *names* may be renamed by the user without going through a "rename in
/// storage" step, so this keys on the stable (groupNumber, patternNumber) pair instead.
inline QString imageKey(int groupNumber, int patternNumber) {
    return QStringLiteral("g%1_p%2").arg(groupNumber).arg(patternNumber);
}

/// Parses `key` back into its (groupNumber, patternNumber) components; the inverse of
/// imageKey().
/// @param key candidate key string, expected form "g{int}_p{int}"
/// @param groupNumber output; set to the parsed group number on success
/// @param patternNumber output; set to the parsed pattern number on success
/// @return true if `key` matched the expected form
inline bool parseImageKey(const QString &key, int &groupNumber, int &patternNumber) {
    // Expected form: "g{int}_p{int}"
    static const QRegularExpression re(QStringLiteral("^g(-?\\d+)_p(-?\\d+)$"));
    const auto m = re.match(key);
    if (!m.hasMatch()) return false;
    bool ok1 = false, ok2 = false;
    groupNumber   = m.captured(1).toInt(&ok1);
    patternNumber = m.captured(2).toInt(&ok2);
    return ok1 && ok2;
}

} // anonymous namespace



TaskLocalization::TaskLocalization(QString name, QString id, QObject* parent)
    : ITask(id, parent), m_config()
{
    this->setName(name);

    // Block signals while seeding the initial config so no listener observes a
    // transient, not-yet-fully-constructed task.
    this->blockSignals(true);
    this->setTaskConfig(&m_config);
    this->blockSignals(false);

    m_limitDeviceMap.insert(device::DeviceType::PLC, kLimitCommDevice);
    m_limitDeviceMap.insert(device::DeviceType::VisionOutput, kLimitVisionOutputDevice);
    m_limitDeviceMap.insert(device::DeviceType::Camera, kLimitNumCamera);

    m_patternManager = new mtc::PatternGroupManager(this);
    qRegisterMetaType<mtc::MatchResult>("mtc::MatchResult");
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<CameraWorkspace>("CameraWorkspace");
    qRegisterMetaType<CameraWorkspace>("vc::model::CameraWorkspace");
    qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
    qRegisterMetaType<std::shared_ptr<mtc::IRobotPickingChecker>>(
        "std::shared_ptr<mtc::IRobotPickingChecker>");

    matchingRunner = new QThread();
    matchingRunner->setObjectName(QStringLiteral("LocalizationMatchingThread"));
    wireMatchingWorkerSignals();
    matchingRunner->start();
    createRuntimeController();
}

/// Destroys the runtime controller and stops the matching-worker thread, waiting up to
/// 3 seconds for it to quit before deleting it (logging a warning if it times out).
TaskLocalization::~TaskLocalization()
{
    destroyRuntimeController();
    if (matchingRunner) {
        matchingRunner->quit();
        if (!matchingRunner->wait(3000)) {
            LOG_USER_WARN << "TaskLocalization matching worker did not stop within timeout.";
        }
        delete matchingRunner;
        matchingRunner = nullptr;
    }
}

/// Replaces the task's configuration and re-applies it to the running runtime
/// controller (queued onto its own thread if it has one).
/// @param cfg new localization configuration
void TaskLocalization::setTaskLocalizeConfig(const TaskLocalizeConfig &cfg)
{
    m_config = cfg;
    this->setTaskConfig(&m_config);
    queueConfigureRuntimeController();
}

/// Returns whether the task already has as many assigned devices of type `t` as the
/// corresponding entry in m_limitDeviceMap allows.
/// @param t device type to check
/// @return true if the limit is reached, or if the task has no project or the project
///   has no DeviceManager
bool TaskLocalization::isReachLimitOfDeviceType(vc::device::DeviceType t) const {

    QList<std::shared_ptr<vc::device::IDevice>> result;
    if (!m_proj) return true;

    auto dm = m_proj->deviceManager();
    if (!dm) return true;

    auto assigned_device_ids = this->assignedDeviceIds();
    int device_counter = 0;
    int limit = m_limitDeviceMap.value(t);

    for (const QString &id : assigned_device_ids) {
        auto dev = dm->deviceById(id);
        if (dev && dev->deviceType() == t) {
            device_counter++;
        }

        if (device_counter >= limit) {
            return true;
        }
    }

    return false;
}

/// Starts the runtime phase: transitions to RuntimeStarting, syncs per-device runners,
/// enters the runtime task-runner phase, (re)creates and moves the runtime controller
/// onto the runtime thread, then calls setupTask(). Transitions to Faulted (without
/// emitting runtimeStarted()) if setup leaves the controller missing or invalid.
/// Writes one user-log warning per virtual device this task is about to run against.
///
/// The third of the three risk-R8 markers, and the only one that survives nobody looking at
/// the screen. A station can be commissioned, handed over and run for weeks on a simulated
/// camera without anyone noticing: everything passes and nothing moves. When someone finally
/// asks why, the log is the first place they look, and this line is what answers them.
///
/// Warning level, not info: a virtual device in a runtime cycle is either a deliberate demo
/// or a mistake, and the log cannot tell which — so it says so and lets the reader decide.
void TaskLocalization::logVirtualDevicesInUse()
{
    Project *proj = project();
    if (proj == nullptr) {
        return;
    }
    auto dm = proj->deviceManager();
    if (!dm) {
        return;
    }

    for (const QString &id : assignedDeviceIds()) {
        auto device = dm->deviceById(id);
        if (device && vc::device::isVirtualDevice(device.get())) {
            LOG_USER_WARN << "Task is entering runtime against a VIRTUAL device — results "
                             "prove the software path, not the hardware."
                          << "task=" << name()
                          << "device=" << device->name()
                          << "id=" << id;
        }
    }
}

void TaskLocalization::beginRuntime(bool mergeToTaskThread)
{
    if (mergeToTaskThread) {
        LOG_USER_WARN << "Localization runtime keeps per-device threads; mergeToTaskThread ignored.";
    }

    if (!transitionTaskState(TaskState::RuntimeStarting,
                             QStringLiteral("beginRuntime"))) {
        return;
    }

    syncRunnersWithDevices();
    logVirtualDevicesInUse();
    taskRunner()->enterRuntime(false);

    // The PLC first, and the camera only after (Task C6). The two active-index signals are
    // inputs the PLC owns, so which camera to bind is a value only the PLC can supply — it
    // cannot be resolved before the PLC has spoken. Resolving it from the project file instead
    // is why a task reached Ready with the master's registers at 0 and ran cycles on whichever
    // camera sorted first (backlog item 58).
    awaitPrimaryPlcSnapshot();

    createRuntimeController();
    if (auto *thread = taskRunner()->runtimeThread()) {
        if (m_runtimeController->thread() != thread) {
            m_runtimeController->moveToThread(thread);
        }
    }

    setupTask();
    if (!m_runtimeController || !m_runtimeController->isValid()) {
        transitionTaskState(TaskState::Faulted,
                            QStringLiteral("Runtime start aborted: setupTask failed"));
        return;
    }

    emit runtimeStarted();
}

/// Connects the primary PLC and vision-output roles and waits, bounded, for the PLC's first
/// whole-register snapshot. See the header for the bound and why expiry is not a failure.
bool TaskLocalization::awaitPrimaryPlcSnapshot()
{
    m_plcSnapshot.reset();
    disconnect(m_plcSnapshotConnection);

    const QString plcId = m_config.d->m_deviceBindings.primaryPlcDeviceId();
    auto *plc = plcRunner(plcId);
    if (!plc) {
        return false;   // no PLC bound: setup() will report that on its own
    }

    // The output role too, so the pair that carries the handshake is up before the selection is
    // resolved; nothing waits on it, because no index depends on it.
    if (auto *output = taskRunner()
                           ? taskRunner()->runnerFor(
                                 m_config.d->m_deviceBindings.visionOutputDeviceId())
                           : nullptr) {
        output->requestConnect();
    }

    QEventLoop loop;
    m_plcSnapshotConnection = connect(
        plc, &vc::runtime::PlcRunner::pollingUpdate, this,
        [this, &loop](std::shared_ptr<vc::device::PlcValueMap> map) {
            if (!map) {
                return;
            }
            m_plcSnapshot = std::move(map);
            loop.quit();
        });

    plc->requestConnect();

    // A bounded nested wait, on the GUI thread, in a lifecycle call. It is the narrowest thing
    // that meets the contract "the task must not reach Ready on a stale selection": resolving
    // after setup() would let the task go Ready on the project default first and only then
    // fault, which is the symptom this task exists to remove.
    QTimer::singleShot(kPlcSnapshotWaitMs, &loop, &QEventLoop::quit);
    loop.exec();

    disconnect(m_plcSnapshotConnection);

    if (!m_plcSnapshot) {
        // Reported, never swallowed. Falling back to the project default is the honest reading
        // of "the PLC has not told us anything" — the same answer an unmapped signal gets — but
        // an operator has to know that is what happened.
        LOG_USER_WARN << "Primary PLC published no register snapshot within"
                      << kPlcSnapshotWaitMs
                      << "ms; the active camera and pattern group fall back to the project "
                         "defaults. A selection the master intended may not have been read.";
        return false;
    }
    return true;
}

/// Ends the runtime phase: destroys the current runtime controller, delegates to
/// ITask::endRuntime() to stop the task's threads, then immediately recreates a fresh
/// (idle) runtime controller so the task is ready for the next beginRuntime()/setupTask().
void TaskLocalization::endRuntime()
{
    destroyRuntimeController();
    ITask::endRuntime();
    createRuntimeController();
}

/// Stops everything regardless of current phase: destroys the runtime controller,
/// delegates to ITask::stopAll(), then recreates a fresh runtime controller.
void TaskLocalization::stopAll()
{
    destroyRuntimeController();
    ITask::stopAll();
    createRuntimeController();
}

// ── Typed runner helpers ──────────────────────────────────────────────────────

/// Returns the CameraRunner registered for `deviceId`, or nullptr if the task has no
/// TaskRunner yet or the runner registered for that id isn't a CameraRunner.
/// @param deviceId id of the assigned camera device
vc::runtime::CameraRunner *TaskLocalization::cameraRunner(const QString &deviceId) const
{
    if (!taskRunner()) return nullptr;
    return qobject_cast<vc::runtime::CameraRunner *>(
        taskRunner()->runnerFor(deviceId));
}

/// Returns the PlcRunner registered for `deviceId`, or nullptr if the task has no
/// TaskRunner yet or the runner registered for that id isn't a PlcRunner.
/// @param deviceId id of the assigned PLC device
vc::runtime::PlcRunner *TaskLocalization::plcRunner(const QString &deviceId) const
{
    if (!taskRunner()) return nullptr;
    return qobject_cast<vc::runtime::PlcRunner *>(
        taskRunner()->runnerFor(deviceId));
}

/// Returns the PlcDevice bound to m_plcDeviceId (the primary PLC resolved by
/// setupTask()/buildRuntimeContext()), or nullptr if no PLC id is set yet or its runner
/// isn't available.
vc::device::PlcDevice *TaskLocalization::plcDevice() const
{
    if (m_plcDeviceId.isEmpty()) return nullptr;
    auto *runner = plcRunner(m_plcDeviceId);
    return runner ? runner->typedDevice() : nullptr;
}

/// Serializes the base ITask fields plus the pattern library (m_patternManager) under
/// the "patternManager" key. Training images are not included here — they travel
/// separately through the project_images BLOB table (see getTaskImageMap()/
/// loadTaskImageMap()).
/// @return the serialized task
QJsonObject TaskLocalization::toJson() const {
    QJsonObject obj = ITask::toJson();

    // Pattern library — fully delegated to the manager.  Training images
    // are excluded from JSON and travel through the project_images BLOB
    // table (see getTaskImageMap() / loadTaskImageMap()).
    if (m_patternManager)
        obj["patternManager"] = m_patternManager->toJson();

    // Gripper presets travel with the project so they survive a move to another
    // machine; they are authoring aids only and are copied into a pattern on apply.
    obj["gripperPresets"] = m_gripperPresets.toJson();

    return obj;
}

/// Restores the base ITask fields via ITask::fromJson(), then rebuilds the pattern
/// library from the "patternManager" key. Pattern training images are not restored here
/// — they're injected later via loadTaskImageMap().
/// @param obj serialized task previously produced by toJson()
bool TaskLocalization::fromJson(const QJsonObject& obj) {
    bool isOk = ITask::fromJson(obj);
    if (!isOk) return false;

    // Rebuild the pattern library.  Pattern images are restored later
    // by loadTaskImageMap().
    if (m_patternManager) {
        if (!m_patternManager->fromJson(obj["patternManager"].toObject()))
            isOk = false;
    }

    // Absent in projects authored before Phase 5; an empty store is the correct
    // degraded state (no presets offered, patterns keep their own geometry).
    m_gripperPresets.fromJson(obj["gripperPresets"].toArray());

    return isOk;
}

// ── Image BLOB I/O ───────────────────────────────────────────────────────────

/// Collects every image blob associated with this task for storage: each pattern's raw
/// training image (keyed via imageKey(groupNumber, patternNumber), skipping patterns
/// with no training image) plus each camera workspace's reference image (key form
/// "ws_{cameraId}", from m_config.cameraWorkspaces()).
QMap<QString, cv::Mat> TaskLocalization::getTaskImageMap() {
    QMap<QString, cv::Mat> map;

    if (m_patternManager) {
        for (const auto &group : m_patternManager->groups()) {
            if (!group) continue;
            const int gn = group->number();
            for (const auto &pattern : group->patterns()) {
                if (!pattern) continue;
                const cv::Mat &img = pattern->config().m_rawImage;
                if (img.empty()) continue;   // skip patterns without a training image
                map.insert(imageKey(gn, pattern->number()), img);
            }
        }
    }

    // Camera workspace reference images (key form "ws_{cameraId}").
    const QMap<QString, cv::Mat> wsImages = m_config.cameraWorkspaces().getImageMap();
    for (auto it = wsImages.cbegin(); it != wsImages.cend(); ++it)
        map.insert(it.key(), it.value());

    return map;
}

/// Re-injects previously stored image blobs (produced by getTaskImageMap()) back into
/// their owning objects: camera workspace reference images by camera id, or pattern
/// training images by (groupNumber, patternNumber) resolved through m_patternManager.
/// Logs and continues past unresolvable keys/groups/patterns rather than aborting.
bool TaskLocalization::loadTaskImageMap(QMap<QString, cv::Mat> &mapping) {
    if (!m_patternManager) return false;
    if (mapping.isEmpty())  return true;   // nothing to inject

    bool allOk = true;
    for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
        // Camera workspace reference image (key form "ws_{cameraId}").
        QString workspaceCameraId;
        if (CameraWorkspaceMap::parseImageKey(it.key(), workspaceCameraId)) {
            m_config.d->m_cameraWorkspaces.setReferenceImage(workspaceCameraId, it.value());
            continue;
        }

        int gn = 0, pn = 0;
        if (!parseImageKey(it.key(), gn, pn)) {
            LOG_DEV_ERR << "TaskLocalization::loadTaskImageMap – bad key:" << it.key();
            allOk = false;
            continue;
        }

        auto group = m_patternManager->findGroupByNumber(gn);
        if (!group) {
            LOG_DEV_ERR << "TaskLocalization::loadTaskImageMap – group" << gn
                        << "not found for key" << it.key();
            allOk = false;
            continue;
        }

        auto *pattern = group->findPatternByNumber(pn);
        if (!pattern) {
            LOG_DEV_ERR << "TaskLocalization::loadTaskImageMap – pattern" << pn
                        << "not found in group" << gn;
            allOk = false;
            continue;
        }

        const QString groupName   = QString::fromStdWString(group->name());
        const QString patternName = QString::fromStdWString(pattern->name());
        if (auto r = m_patternManager->setPatternImage(groupName, patternName, it.value()); !r) {
            LOG_DEV_ERR << "TaskLocalization::loadTaskImageMap – setPatternImage failed:"
                        << QString::fromStdWString(r.error);
            allOk = false;
        }
    }
    return allOk;
}

// ── Task lifecycle ────────────────────────────────────────────────────────────

/// Builds the runtime context and applies it to the runtime controller via
/// setupRuntimeController(), caching the resolved primary PLC device id and the
/// resulting validity in m_plcDeviceId / m_isValid.
void TaskLocalization::setupTask()
{
    if (!m_runtimeController) {
        m_isValid = false;
        return;
    }

    const auto result = setupRuntimeController();
    m_plcDeviceId = result.primaryPlcDeviceId;
    m_isValid = result.valid;
}

/// Triggers one localization cycle: requires a valid runtime controller and the task to
/// currently be Ready or RunningCycle, then queues controller->execute() onto the
/// controller's own thread.
void TaskLocalization::executeLocalization()
{
    if (!m_runtimeController || !m_runtimeController->isValid()) {
        LOG_DEV_ERR << "TaskLocalization::executeLocalization – task not set up";
        return;
    }

    if (taskState() != TaskState::Ready &&
        taskState() != TaskState::RunningCycle) {
        LOG_USER_WARN << buildInvalidTaskStateTransitionMessage(
            taskState(),
            TaskState::RunningCycle,
            QStringLiteral("executeLocalization"));
        return;
    }

    auto *controller = m_runtimeController;
    if (controller) {
        QMetaObject::invokeMethod(controller, [controller]() {
            controller->execute();
        }, Qt::QueuedConnection);
    }
}

/// Forwards a batch of changed PLC signal values to the runtime controller.
/// @param values changed signal name/value pairs, as reported by the PLC comm device
void TaskLocalization::onCommDeviceValueChanged(QMap<QString, QVariant> values) {
    queueHandlePlcValues(values);
}

/// Slot: transitions the task to RunningCycle in response to the runtime controller's
/// runtimeCycleStarted signal.
/// @param message context string passed through as the transition reason
void TaskLocalization::onRuntimeCycleStarted(const QString &message)
{
    transitionTaskState(TaskState::RunningCycle, message);
}

/// Slot: transitions the task to Recovering in response to the runtime controller's
/// runtimeRecovering signal.
/// @param message context string passed through as the transition reason
void TaskLocalization::onRuntimeRecovering(const QString &message)
{
    transitionTaskState(TaskState::Recovering, message);
}

/// Slot: transitions the task to Ready in response to the runtime controller's
/// runtimeReady signal, unless the task has already moved to Stopping or Idle (in which
/// case a stale "ready" notification is ignored). A Faulted task IS allowed through, so
/// an acknowledged or auto-cleared fault can return the task to service.
/// @param message context string passed through as the transition reason
void TaskLocalization::onRuntimeReady(const QString &message)
{
    // Faulted is deliberately NOT in this guard: a fault acknowledged over bErrorReset,
    // or auto-cleared by the runtime controller, reaches the task as a ready event and
    // must be able to leave the Faulted state. Stopping and Idle stay guarded — a ready
    // event must never resurrect a task that is being torn down.
    if (taskState() == TaskState::Stopping ||
        taskState() == TaskState::Idle) {
        return;
    }

    transitionTaskState(TaskState::Ready, message);
}

/// Slot: transitions the task to Faulted in response to the runtime controller's
/// runtimeFault signal.
/// @param message context string passed through as the transition reason
void TaskLocalization::onRuntimeFault(const QString &message)
{
    transitionTaskState(TaskState::Faulted, message);
}

/// Lazily creates the runtime controller (no-op if one already exists), configures it
/// with the current m_config, and wires its signals to this task.
void TaskLocalization::createRuntimeController()
{
    if (m_runtimeController) {
        return;
    }

    m_runtimeController = new LocalizationRuntimeController();
    m_runtimeController->configure(m_config);
    wireRuntimeControllerSignals();
}

/// Detaches and destroys the current runtime controller (no-op if none exists):
/// disconnects all signal/slot links between it and this task, then deletes it — via a
/// queued deleteLater() on its own thread if that thread is different and running, or
/// immediately/synchronously otherwise.
void TaskLocalization::destroyRuntimeController()
{
    if (!m_runtimeController) {
        return;
    }

    auto *controller = m_runtimeController;
    m_runtimeController = nullptr;
    controller->disconnect(this);
    disconnect(controller, nullptr, this, nullptr);

    if (controller->thread() && controller->thread() != QThread::currentThread()
        && controller->thread()->isRunning()) {
        QMetaObject::invokeMethod(controller, [controller]() {
            controller->deleteLater();
        }, Qt::QueuedConnection);
    } else {
        delete controller;
    }
}

/// Connects the runtime controller's signals (state notifications, cycle results, log
/// entries) to this task's slots/signals, and — if the matching worker exists — wires
/// runtimeMatchingRequested to run image matching on the matchingRunner thread and post
/// the result back to the controller via onRuntimeMatchingFinished().
/// @note All connections use Qt::QueuedConnection. The matching-worker lambda guards the
///   controller with a QPointer since it runs on a different thread and the controller
///   may be destroyed/recreated concurrently.
void TaskLocalization::wireRuntimeControllerSignals()
{
    if (!m_runtimeController) {
        return;
    }

    connect(m_runtimeController, &LocalizationRuntimeController::signalChanged,
            this, &ITask::signalChanged, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::cycleResultUpdated,
            this, &TaskLocalization::cycleResultUpdated, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::taskLogAppended,
            this, &TaskLocalization::taskLogAppended, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::runtimeCycleStarted,
            this, &TaskLocalization::onRuntimeCycleStarted, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::runtimeRecovering,
            this, &TaskLocalization::onRuntimeRecovering, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::runtimeReady,
            this, &TaskLocalization::onRuntimeReady, Qt::QueuedConnection);
    connect(m_runtimeController, &LocalizationRuntimeController::runtimeFault,
            this, &TaskLocalization::onRuntimeFault, Qt::QueuedConnection);

    if (m_matchingWorker) {
        QPointer<LocalizationRuntimeController> controller(m_runtimeController);
        connect(m_runtimeController,
                &LocalizationRuntimeController::runtimeMatchingRequested,
                m_matchingWorker,
                [this, controller](int cycleId,
                                   std::shared_ptr<mtc::MatchGroup> group,
                                   CameraWorkspace workspace,
                                   cv::Mat image,
                                   std::shared_ptr<mtc::IRobotPickingChecker> pickingChecker) {

            // Persistent matcher: reused across cycles (runs on the
            // matchingRunner thread). The matcher + learned model are rebuilt
            // only when the active pattern group changes, never per cycle.
            if (!m_runtimeMatcher || m_loadedRuntimeGroup != group) {
                m_runtimeMatcher = std::make_unique<mtc::ImageMatcher>();
                m_loadedRuntimeGroup =
                    m_pipeline.loadModel(*m_runtimeMatcher, group) ? group : nullptr;
            }

            mtc::MatchResult result;
            if (m_loadedRuntimeGroup) {
                result = m_pipeline.runMatchOn(*m_runtimeMatcher, workspace, image,
                                               pickingChecker.get(),
                                               LocalizationPipeline::kRuntimeMaxObjects);
            }
            if (!controller) {
                return;
            }
            QMetaObject::invokeMethod(controller.data(),
                                      [controller, cycleId, result]() {
                if (controller) {
                    controller->onRuntimeMatchingFinished(cycleId, result);
                }
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }
}

/// Deep-copies `source` into an independent MatchGroup (own MatchGroupConfig, including
/// cloned typeConfig, plus copies of each pattern's config with its raw training image).
/// Worker threads operate on this snapshot instead of the live PatternGroupManager
/// group, so GUI-thread pattern edits cannot race the matcher.
/// @param source pattern group to copy; may be null
std::shared_ptr<mtc::MatchGroup>
TaskLocalization::snapshotPatternGroup(const std::shared_ptr<mtc::MatchGroup> &source)
{
    if (!source) {
        return nullptr;
    }

    // Independent deep copy: own MatchGroupConfig (with cloned typeConfig) plus
    // an own pattern vector whose configs carry the training image (m_rawImage).
    // The worker can learn/match from this snapshot without touching — or racing
    // — the live PatternGroupManager group on the GUI thread.
    auto snapshot = std::make_shared<mtc::MatchGroup>();
    source->cloneConfigTo(*snapshot);
    for (const auto &pattern : source->patterns()) {
        if (pattern) {
            snapshot->addPattern(pattern->config());
        }
    }
    return snapshot;
}

/// Assembles a RuntimeContext snapshot for the runtime controller: resolves the primary
/// PLC and vision-output runners/ids from m_config's device bindings, copies the TASK's robot
/// pick-check settings (m_config.robotCheckConfig(), Phase 9 / F1), resolves each bound camera's
/// runner and calibrator, takes a deep-copy snapshot of every pattern group (see
/// snapshotPatternGroup()), and attaches the PLC snapshot read at startup. The active camera and
/// pattern group are deliberately left unresolved (-1) for setup() — see the note in the body.
/// @return the populated runtime context, ready for setupRuntimeController()/setup()
LocalizationRuntimeController::RuntimeContext
TaskLocalization::buildRuntimeContext() const
{
    LocalizationRuntimeController::RuntimeContext context;
    context.config = m_config;
    context.primaryPlcDeviceId = m_config.d->m_deviceBindings.primaryPlcDeviceId();
    context.visionOutputDeviceId = m_config.d->m_deviceBindings.visionOutputDeviceId();
    context.primaryPlcRunner = plcRunner(context.primaryPlcDeviceId);
    context.visionOutputRunner =
        taskRunner() ? taskRunner()->runnerFor(context.visionOutputDeviceId) : nullptr;

    // The robot pick check is a property of the TASK, not of whatever transport carries the
    // result out (Phase 9 / F1, backlog item 57). It used to be read off the bound device through
    // IResultOutputDevice::robotKinematicCheckConfig(); reaching it through a capability rather
    // than a cast fixed *which* families could answer, but not the deeper problem — a device that
    // has no check commissioned answers "disabled", and that answer is indistinguishable from
    // "this cell does not want the check". Binding a PLC to vision_output therefore un-commissioned
    // the gate silently. Reading the task's own setting removes the device from the question.
    //
    // IResultOutputDevice::robotKinematicCheckConfig() still exists and both Modbus devices still
    // implement it; the localization task simply no longer reads it. See the note there.
    context.robotCheckConfig = m_config.robotCheckConfig();

    context.cameraDeviceIds = m_config.d->m_deviceBindings.cameraNumberMap();

    for (auto it = context.cameraDeviceIds.cbegin(); it != context.cameraDeviceIds.cend(); ++it) {
        context.cameraRunners.insert(it.key(), cameraRunner(it.value()));

        auto device = getTaskDevice(it.value());
        auto camera = std::dynamic_pointer_cast<device::CameraDevice>(device);
        if (!camera) {
            continue;
        }
        std::unique_ptr<device::IDeviceCfg> cfg(camera->deviceConfig());
        auto *cameraCfg = dynamic_cast<device::CameraCfg *>(cfg.get());
        if (cameraCfg) {
            context.cameraCalibrators.insert(it.key(), cameraCfg->calibrator());
        }
    }

    // Deliberately NOT pre-resolved to firstKey() here. Leaving the sentinel lets setup() own the
    // resolution, which is the only place that can tell "the project's first binding" from "a
    // number somebody commanded" — and the startup summary reports that distinction. Pre-resolving
    // made the summary say "commanded" for every run, which is false in every run today
    // (field-confirmed 2026-09-08). setup() applies the identical firstKey() fallback, so the
    // resolved value is unchanged; only the provenance survives now.
    //
    // It is also the precondition C6 needs: a selection read from the live PLC has to be able to
    // beat the project default, and it cannot if the default has already been written in.

    if (m_patternManager) {
        const auto groups = m_patternManager->groups();
        for (const auto &source : groups) {
            if (auto snapshot = snapshotPatternGroup(source)) {
                context.patternGroups.insert(snapshot->number(), snapshot);
            }
        }
        // Sentinel kept, same reasoning as the camera above.
    }

    // What the PLC held when the runtime started, if it had been read by then. Null means "not
    // read", which setup() treats exactly like an unmapped signal — never as a register holding 0.
    context.plcSnapshot = m_plcSnapshot;

    return context;
}

/// Builds the runtime context and calls the runtime controller's setup() with it —
/// directly if already on the controller's thread, otherwise via a blocking queued
/// invocation so the caller still gets the SetupResult synchronously.
/// @return the controller's setup result, or a result with a single "Runtime controller
///   is null." error if there is no runtime controller
/// @note Blocks the calling thread until setup() completes when called cross-thread.
LocalizationRuntimeController::SetupResult TaskLocalization::setupRuntimeController()
{
    LocalizationRuntimeController::SetupResult result;
    auto *controller = m_runtimeController;
    if (!controller) {
        result.errors.append(QStringLiteral("Runtime controller is null."));
        return result;
    }

    const auto context = buildRuntimeContext();
    if (controller->thread() == QThread::currentThread()) {
        return controller->setup(context);
    }

    QMetaObject::invokeMethod(controller, [controller, context, &result]() {
        result = controller->setup(context);
    }, Qt::BlockingQueuedConnection);
    return result;
}

/// Applies a copy of the current m_config to the runtime controller's configure(),
/// directly if already on its thread, otherwise queued onto it. No-op if there is no
/// runtime controller.
void TaskLocalization::queueConfigureRuntimeController()
{
    auto *controller = m_runtimeController;
    if (!controller) {
        return;
    }
    const TaskLocalizeConfig cfg = m_config;
    if (controller->thread() == QThread::currentThread()) {
        controller->configure(cfg);
        return;
    }
    QMetaObject::invokeMethod(controller, [controller, cfg]() {
        controller->configure(cfg);
    }, Qt::QueuedConnection);
}

/// Queues a call to the runtime controller's setActiveCameraNumber(number) onto its own
/// thread; no-op if there is no runtime controller.
/// @param number camera number to activate
void TaskLocalization::queueSetActiveCameraNumber(int number)
{
    auto *controller = m_runtimeController;
    if (!controller) {
        return;
    }
    QMetaObject::invokeMethod(controller, [controller, number]() {
        controller->setActiveCameraNumber(number);
    }, Qt::QueuedConnection);
}

/// Queues a call to the runtime controller's setActivePatternGroupNumber(number) onto
/// its own thread; no-op if there is no runtime controller.
/// @param number pattern group number to activate
void TaskLocalization::queueSetActivePatternGroupNumber(int number)
{
    auto *controller = m_runtimeController;
    if (!controller) {
        return;
    }
    QMetaObject::invokeMethod(controller, [controller, number]() {
        controller->setActivePatternGroupNumber(number);
    }, Qt::QueuedConnection);
}

/// Queues a copy of `values` to the runtime controller's handlePlcValues() onto its own
/// thread; no-op if there is no runtime controller.
/// @param values changed PLC signal name/value pairs to forward
void TaskLocalization::queueHandlePlcValues(const QMap<QString, QVariant> &values)
{
    auto *controller = m_runtimeController;
    if (!controller) {
        return;
    }
    const auto copiedValues = values;
    QMetaObject::invokeMethod(controller, [controller, copiedValues]() {
        controller->handlePlcValues(copiedValues);
    }, Qt::QueuedConnection);
}

/// Creates the matching-worker QObject on matchingRunner (deleted when the thread
/// finishes) and connects startCommissionMatchingRequest to run a commission match on
/// the worker thread, emitting commissionMatchingFinished() with the result.
/// @note The connected lambda runs on matchingRunner's thread even though it captures
///   `this`; it only reads m_pipeline (used solely for matching) and emits a signal, so
///   no additional locking is taken.
void TaskLocalization::wireMatchingWorkerSignals() {
    m_matchingWorker = new QObject();
    m_matchingWorker->moveToThread(matchingRunner);

    connect(matchingRunner, &QThread::finished, m_matchingWorker, &QObject::deleteLater);

    // commission matching handle, working on matching worker thread
    connect(this, &TaskLocalization::startCommissionMatchingRequest,
            m_matchingWorker, [this](std::shared_ptr<mtc::MatchGroup> group, cv::Mat image, CameraWorkspace workspace) {

        emit this->commissionMatchingFinished(m_pipeline.runMatchCommision(group, workspace, image));
    });
}

} // namespace vc::model
