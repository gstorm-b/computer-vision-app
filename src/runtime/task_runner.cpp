#include "task_runner.h"

#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/vision_output_runner.h"
#include "device/camera/camera_device.h"
#include "device/plc/plc_device.h"
#include "core/logger/app_logger.h"

#include <QEventLoop>

/// TaskRunner implementation: manages one QThread per registered device plus the task's own
/// runtime coordinator thread, and drives the Idle -> Commission -> Runtime phase transitions.
namespace vc::runtime {

// ── Construction / destruction ────────────────────────────────────────────────

/// Constructs the runner in the Idle phase; creates (but does not start) the task's runtime
/// coordinator thread.
TaskRunner::TaskRunner(QObject *parent)
    : QObject(parent)
{
    m_runtimeThread = new QThread(this);
    m_runtimeThread->setObjectName("TaskRuntimeThread");
}

/// Tears the task down via enterIdle() (moves devices back to the main thread and stops all
/// runners), deletes every registered runner, and stops m_runtimeThread if still running.
TaskRunner::~TaskRunner()
{
    // enterIdle() moves devices back to main thread and stops all runners
    enterIdle();

    for (IDeviceRunner *runner : m_runners) {
        runner->deleteLater();
    }

    // m_runtimeThread is parented → deleted automatically, but stop first
    if (m_runtimeThread->isRunning()) {
        m_runtimeThread->quit();
        m_runtimeThread->wait(3000);
    }
}

// ── Device registration ───────────────────────────────────────────────────────

/// Creates the IDeviceRunner for `deviceId` (via createRunner) and registers it. If the task has
/// already left Idle (Commission or Runtime), the new runner is immediately started and attached
/// so it can be used right away. No-op if `deviceId` is already registered, `device` is null, or
/// `device`'s type has no matching runner (logged as an error in both failure cases).
void TaskRunner::registerDevice(const QString &deviceId,
                                std::shared_ptr<vc::device::IDevice> device)
{
    if (m_runners.contains(deviceId)) return;

    if (!device) {
        LOG_DEV_ERR << "TaskRunner::registerDevice – null device for id:" << deviceId;
        return;
    }

    IDeviceRunner *runner = createRunner(device);
    if (!runner) {
        LOG_DEV_ERR << "TaskRunner::registerDevice – unsupported device type, id:"
                    << deviceId
                    << "type:" << static_cast<int>(device->deviceType());
        return;
    }

    m_runners.insert(deviceId, runner);
    LOG_DEV_DEBUG << "TaskRunner: registered device" << deviceId
                  << device->name();

    // If we're already past Idle, the new runner must catch up: start its
    // thread and attach so the Widget can immediately interact with it.
    if (m_phase == Phase::Commission) {
        runner->start();
        runner->attach();
    } else if (m_phase == Phase::Runtime) {
        // In runtime per-device mode we keep the per-device thread.
        // (Merged-mode runtime starts with all devices already moved; new
        // devices attached here will live on their own thread and be
        // controlled via the runtime thread coordinator the same way.)
        runner->start();
        runner->attach();
    }
}

/// Detaches, stops, and deletes the runner for `deviceId` via deleteLater(). No-op if no runner
/// is registered for that id.
void TaskRunner::unregisterDevice(const QString &deviceId)
{
    if (!m_runners.contains(deviceId)) return;

    IDeviceRunner *runner = m_runners.take(deviceId);

    if (runner->isAttached()) runner->detach(nullptr);
    if (runner->isRunning())  runner->stop();

    runner->deleteLater();
    LOG_DEV_DEBUG << "TaskRunner: unregistered device" << deviceId;
}

/// Returns the runner registered for `deviceId`, or nullptr if none exists.
IDeviceRunner *TaskRunner::runnerFor(const QString &deviceId) const
{
    return m_runners.value(deviceId, nullptr);
}

/// Returns whether a runner is currently registered for `deviceId`.
bool TaskRunner::hasRunner(const QString &deviceId) const
{
    return m_runners.contains(deviceId);
}

// ── Phase transitions ─────────────────────────────────────────────────────────

/// Moves from Idle to Commission: starts and attaches every registered device's own
/// HighPriority QThread. No-op if already in the Commission phase.
void TaskRunner::enterCommission()
{
    if (m_phase == Phase::Commission) return;
    m_phase = Phase::Commission;

    // Each device gets its own HighPriority thread
    for (IDeviceRunner *runner : m_runners) {
        runner->start();
        runner->attach();
    }

    LOG_DEV_DEBUG << "TaskRunner: → Commission phase (" << m_runners.size() << "devices)";
    emit phaseChanged(m_phase);
}

/// Moves to the Runtime phase and starts m_runtimeThread. No-op if already in the Runtime phase.
/// When `mergeToTaskThread` is true, detaches every device from its per-device thread into
/// m_runtimeThread (started at HighPriority) and stops the now-empty per-device threads; when
/// false, per-device threads are kept running (starting any not already running) and
/// m_runtimeThread starts at NormalPriority merely as the coordinating thread.
void TaskRunner::enterRuntime(bool mergeToTaskThread)
{
    if (m_phase == Phase::Runtime) return;

    if (mergeToTaskThread) {
        // Detach all devices from their per-device threads and move them into
        // the single runtime thread, then stop the now-empty per-device threads.
        for (IDeviceRunner *runner : m_runners) {
            runner->detach(m_runtimeThread);
            runner->stop();
        }
        m_runtimeThread->start(QThread::HighPriority);
        LOG_DEV_DEBUG << "TaskRunner: → Runtime phase (merged to task thread)";
    } else {
        // Keep per-device threads running.
        // Start any runner that isn't running yet (e.g. fresh Runtime without Commission).
        for (IDeviceRunner *runner : m_runners) {
            if (!runner->isRunning()) {
                runner->start();
                runner->attach();
            }
        }
        // Task runtime thread runs as coordinator alongside device threads.
        m_runtimeThread->start(QThread::NormalPriority);
        LOG_DEV_DEBUG << "TaskRunner: → Runtime phase (per-device threads)";
    }

    m_phase = Phase::Runtime;
    emit phaseChanged(m_phase);
}

/// Moves back to Idle: for each attached runner, synchronously disconnects the device on its own
/// thread (disconnectAndWait()) before detaching and stopping it, then stops m_runtimeThread.
/// No-op if already in the Idle phase.
void TaskRunner::enterIdle()
{
    if (m_phase == Phase::Idle) return;

    for (IDeviceRunner *runner : m_runners) {
        if (runner->isAttached()) {
            // Close the device connection on its own worker thread before
            // detaching/stopping. Moving a live socket across threads or
            // stopping the worker while the link is open leaks the connection
            // and can crash on the next phase entry. See
            // docs/task_localization for the lifecycle contract.
            runner->disconnectAndWait();
            runner->detach(nullptr);
        }

        if (runner->isRunning())  {
            runner->stop();
        }
    }

    if (m_runtimeThread->isRunning()) {
        m_runtimeThread->quit();
        m_runtimeThread->wait(3000);
    }

    m_phase = Phase::Idle;
    LOG_DEV_DEBUG << "TaskRunner: → Idle phase";
    emit phaseChanged(m_phase);
}

// ── Runner factory ────────────────────────────────────────────────────────────

/// Constructs the concrete IDeviceRunner matching `device`'s deviceType() (CameraRunner,
/// PlcRunner, or VisionOutputRunner).
/// @return the new runner (owned by `this`), or nullptr if `device`'s runtime type does not
/// match its declared deviceType(), or the type is unsupported
IDeviceRunner *TaskRunner::createRunner(std::shared_ptr<vc::device::IDevice> device)
{
    using namespace vc::device;

    switch (device->deviceType()) {
    case DeviceType::Camera:
        if (auto *cam = qobject_cast<CameraDevice *>(device.get()))
            return new CameraRunner(cam, this);
        break;

    case DeviceType::PLC:
        if (auto *plc = qobject_cast<PlcDevice *>(device.get()))
            return new PlcRunner(plc, this);
        break;

    case DeviceType::VisionOutput:
        if (auto *vision_output = qobject_cast<VisionOutputDevice *>(device.get()))
            return new VisionOutputRunner(vision_output, this);
        break;

    default:
        break;
    }
    return nullptr;
}

} // namespace vc::runtime
