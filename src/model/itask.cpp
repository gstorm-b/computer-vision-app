#include "itask.h"
#include "model/project.h"
#include "device/device_manager.h"

/// Model-layer task types (ITask and its derivatives): phase/state-machine transitions,
/// assigned-device resolution, and per-device runner synchronization.
namespace vc::model {

/// Attempts to move the task from its current state to `targetState`, validating the move
/// via canTransitionTaskState() and logging the outcome; on success updates m_taskState and
/// emits taskStateChanged(). Returns false (leaving the state unchanged, after logging a
/// warning) if canTransitionTaskState() rejects the transition.
bool ITask::transitionTaskState(TaskState targetState, const QString &reason)
{
    if (m_taskState == targetState) {
        return true;
    }

    if (!canTransitionTaskState(m_taskState, targetState)) {
        LOG_USER_WARN << buildInvalidTaskStateTransitionMessage(
            m_taskState, targetState, reason);
        return false;
    }

    LOG_USER_INFO << buildTaskStateTransitionMessage(
        m_taskState, targetState, reason);
    m_taskState = targetState;
    emit taskStateChanged(m_taskState);
    return true;
}

// ── Assigned-device helpers ───────────────────────────────────────────────────

/// Looks up a single assigned device by id via the project's DeviceManager.
std::shared_ptr<vc::device::IDevice>
ITask::getTaskDevice(const QString &deviceId) const {
    std::shared_ptr<vc::device::IDevice> result;
    if (!m_proj) return result;


    auto dm = m_proj->deviceManager();
    if (!dm) return result;

    result = m_proj->deviceManager()->deviceById(deviceId);
    return result;
}

/// Resolves m_assignedDeviceIds against the project's DeviceManager and returns only the
/// devices whose deviceType() matches `t`; ids that no longer resolve to a registered device
/// are skipped.
QList<std::shared_ptr<vc::device::IDevice>>
ITask::assignedDevicesOfType(vc::device::DeviceType t) const {
    QList<std::shared_ptr<vc::device::IDevice>> result;
    if (!m_proj) return result;

    auto dm = m_proj->deviceManager();
    if (!dm) return result;

    for (const QString &id : m_assignedDeviceIds) {
        auto dev = dm->deviceById(id);
        if (dev && dev->deviceType() == t)
            result.append(dev);
    }
    return result;
}

// ── Phase management ──────────────────────────────────────────────────────────

/// Enters the Commission phase: transitions to CommissionStarting, syncs per-device runners
/// with the currently assigned devices, has the TaskRunner enter commission (spinning up
/// per-device threads), then transitions to Commission and emits commissionStarted().
/// @note No-op if the CommissionStarting transition is rejected (e.g. wrong current state).
void ITask::beginCommission()
{
    if (!transitionTaskState(TaskState::CommissionStarting,
                             QStringLiteral("beginCommission"))) {
        return;
    }

    syncRunnersWithDevices();
    m_taskRunner->enterCommission();
    transitionTaskState(TaskState::Commission,
                        QStringLiteral("commission resources ready"));
    emit commissionStarted();
}

/// Ends the Commission phase: transitions to Stopping, has the TaskRunner enter idle
/// (stopping per-device threads), then transitions to Idle and emits commissionStopped().
/// @note No-op if the Stopping transition is rejected.
void ITask::endCommission()
{
    if (!transitionTaskState(TaskState::Stopping,
                             QStringLiteral("endCommission"))) {
        return;
    }

    m_taskRunner->enterIdle();
    transitionTaskState(TaskState::Idle,
                        QStringLiteral("commission stopped"));
    emit commissionStopped();
}

/// Enters the Runtime phase: transitions to RuntimeStarting, syncs per-device runners with
/// the currently assigned devices, has the TaskRunner enter runtime, then transitions to
/// Ready and emits runtimeStarted().
/// @note No-op if the RuntimeStarting transition is rejected.
void ITask::beginRuntime(bool mergeToTaskThread)
{
    if (!transitionTaskState(TaskState::RuntimeStarting,
                             QStringLiteral("beginRuntime"))) {
        return;
    }

    syncRunnersWithDevices();
    m_taskRunner->enterRuntime(mergeToTaskThread);
    transitionTaskState(TaskState::Ready,
                        QStringLiteral("runtime ready"));
    emit runtimeStarted();
}

/// Ends the Runtime phase: transitions to Stopping, has the TaskRunner enter idle, then
/// transitions to Idle and emits runtimeStopped().
/// @note No-op if the Stopping transition is rejected.
void ITask::endRuntime()
{
    if (!transitionTaskState(TaskState::Stopping,
                             QStringLiteral("endRuntime"))) {
        return;
    }

    m_taskRunner->enterIdle();
    transitionTaskState(TaskState::Idle,
                        QStringLiteral("runtime stopped"));
    emit runtimeStopped();
}

/// Stops all task activity regardless of the current phase: transitions to Stopping (unless
/// already Idle), has the TaskRunner enter idle, then transitions to Idle.
/// @note No-op if not already Idle and the Stopping transition is rejected.
void ITask::stopAll()
{
    if (m_taskState != TaskState::Idle &&
        !transitionTaskState(TaskState::Stopping,
                             QStringLiteral("stopAll"))) {
        return;
    }

    m_taskRunner->enterIdle();
    transitionTaskState(TaskState::Idle,
                        QStringLiteral("all task activity stopped"));
}

// ── Runner sync ───────────────────────────────────────────────────────────────

/// Reconciles the TaskRunner's registered runners with m_assignedDeviceIds: registers a
/// runner for each newly assigned device (auto-starting/attaching it if the task is already
/// past Idle) and unregisters runners for devices that are no longer assigned.
/// @note No-op if the task has no project set or the project has no DeviceManager.
void ITask::syncRunnersWithDevices()
{
    if (!m_proj) return;

    auto dm = m_proj->deviceManager();
    if (!dm) return;

    // 1. Register runners for newly assigned devices.
    //    TaskRunner::registerDevice() auto-starts + attaches the runner
    //    if we're already past Idle (Commission / Runtime), so the new
    //    device is immediately ready for the Widget to drive.
    for (const QString &id : m_assignedDeviceIds) {
        if (!m_taskRunner->hasRunner(id)) {
            auto device = dm->deviceById(id);
            if (device) {
                m_taskRunner->registerDevice(id, device);
            }
        }
    }

    // 2. Unregister runners for devices that are no longer assigned.
    //    (User clicked "remove device" → free its thread, drop the runner.)
    for (const QString &id : m_taskRunner->registeredDeviceIds()) {
        if (!m_assignedDeviceIds.contains(id)) {
            m_taskRunner->unregisterDevice(id);
        }
    }
}

} // namespace vc::model
