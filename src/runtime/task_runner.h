#ifndef TASK_RUNNER_H
#define TASK_RUNNER_H

#include <QObject>
#include <QMap>
#include <QThread>
#include <memory>

#include "runtime/idevice_runner.h"
#include "device/device_manager.h"

/// Runtime device-thread management: TaskRunner and the per-device
/// IDeviceRunner implementations that drive commissioning and runtime
/// execution off the GUI thread.
namespace vc::runtime {

/// Owned by ITask. Central manager for all device threads belonging to one task.
/// Drives the three-phase lifecycle Idle -> Commission -> Runtime:
/// @note Idle -- no threads running; devices live on the main (GUI) thread.
/// @note Commission -- each registered device gets its own HighPriority QThread;
/// the Widget obtains the typed runner via runnerFor(deviceId) and calls
/// requestConnect() / requestSingleShot() etc.; results arrive back on the GUI
/// thread via QueuedConnection.
/// @note Runtime (mergeToTaskThread = false, recommended) -- per-device threads
/// are kept; m_runtimeThread starts as coordinator and runs the task's
/// processing logic, coordinating camera grabs and PLC I/O across the device
/// threads via signals.
/// @note Runtime (mergeToTaskThread = true) -- all devices are detached from
/// their per-device threads and moved into m_runtimeThread; simpler but
/// sequential, suitable only when devices do not overlap IO.
/// @note The Widget NEVER creates or destroys QThread objects; it only calls
/// runner->requestXxx() and listens to runner signals.
class TaskRunner : public QObject {
    Q_OBJECT

public:
    /// Lifecycle phase of the task's device threads; see the class-level notes
    /// above for what each phase does to the underlying QThreads.
    enum class Phase { Idle, Commission, Runtime };
    Q_ENUM(Phase)

    /// Constructs the runner in the Idle phase (no device threads running yet).
    /// @param parent optional QObject owner
    explicit TaskRunner(QObject *parent = nullptr);
    /// Tears the task down via enterIdle(), deletes all registered runners, and
    /// stops m_runtimeThread if it is still running.
    ~TaskRunner() override;

    // ── Device registration ───────────────────────────────────────────────────
    /// Creates the IDeviceRunner for `deviceId` (via createRunner) and, if the
    /// task has already left Idle, immediately starts and attaches it so the
    /// Widget can interact with it right away. Ignored if `deviceId` is already
    /// registered, `device` is null, or the device type has no matching runner.
    /// Called by ITask::syncRunnersWithDevices() whenever devicesChanged fires.
    /// @param deviceId unique id of the device within the task
    /// @param device device instance to wrap; ownership stays with the shared_ptr holder
    void registerDevice(const QString &deviceId,
                        std::shared_ptr<vc::device::IDevice> device);
    /// Detaches, stops, and deletes the runner for `deviceId`. No-op if no
    /// runner is registered for that id. Called by
    /// ITask::syncRunnersWithDevices() whenever devicesChanged fires.
    void unregisterDevice(const QString &deviceId);

    /// Returns the runner registered for `deviceId`, or nullptr if none exists.
    IDeviceRunner *runnerFor(const QString &deviceId) const;
    /// Returns whether a runner is currently registered for `deviceId`.
    bool           hasRunner(const QString &deviceId) const;
    /// Returns the ids of all currently registered device runners.
    QStringList    registeredDeviceIds() const { return m_runners.keys(); }

    // ── Phase transitions ─────────────────────────────────────────────────────
    /// Moves from Idle to Commission: starts each registered device's own
    /// HighPriority QThread and attaches the device to it. No-op if already
    /// in the Commission phase.
    void enterCommission();
    /// Moves to the Runtime phase and starts m_runtimeThread.
    /// @param mergeToTaskThread when true, detaches every device from its
    /// per-device thread, moves it into m_runtimeThread, and stops the now-empty
    /// per-device threads (simpler, sequential execution); when false
    /// (recommended), per-device threads are kept running and m_runtimeThread
    /// starts merely as the coordinating thread for the task's processing logic.
    void enterRuntime(bool mergeToTaskThread = false);
    /// Moves back to Idle: for each attached runner, synchronously disconnects
    /// the device on its own thread before detaching and stopping it, then
    /// stops m_runtimeThread. No-op if already in the Idle phase.
    void enterIdle();

    /// Returns the task's current lifecycle phase.
    Phase   currentPhase()   const { return m_phase;          }
    /// Returns the task's coordinator thread (started in enterRuntime()).
    QThread *runtimeThread() const { return m_runtimeThread;  }

signals:
    /// Emitted to ask a device (on its own thread) to detach and move itself
    /// to `dest`; used internally to marshal thread-affine moves.
    void requestDetach(QThread *dest);
    /// Emitted after currentPhase() has changed, carrying the new phase.
    void phaseChanged(TaskRunner::Phase newPhase);

private:
    /// Constructs the concrete IDeviceRunner matching `device`'s deviceType()
    /// (CameraRunner, PlcRunner, or VisionOutputRunner). Returns nullptr if the
    /// device's runtime type does not match its declared deviceType(), or the
    /// type is unsupported.
    IDeviceRunner *createRunner(std::shared_ptr<vc::device::IDevice> device);

    Phase   m_phase{Phase::Idle};
    QThread *m_runtimeThread{nullptr};   ///< Task coordinator thread, started in enterRuntime().


    QMap<QString, IDeviceRunner *> m_runners; ///< Registered device runners, keyed by deviceId.
};

} // namespace vc::runtime

#endif // TASK_RUNNER_H
