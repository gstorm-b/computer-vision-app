#ifndef IDEVICE_RUNNER_H
#define IDEVICE_RUNNER_H

#include <QObject>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include "device/idevice.h"
#include "runtime/device_command.h"

/// Runtime layer: per-device thread controllers that own and drive a device on its
/// own worker QThread.
namespace vc::runtime {

/// Abstract interface for per-device thread controllers. Concrete implementations
/// (CameraRunner, PlcRunner, VisionOutputRunner) inherit from the DeviceRunner<T>
/// template base which provides the boilerplate.
///
/// Lifecycle:
///   - start()  starts the internal QThread
///   - attach() moves the device onto the worker thread and wires its signals
///   - detach() unwires the device's signals and moves it to the destination thread
///   - stop()   quits the worker thread and waits (up to 3 s) for it to finish
///
/// @note All signals forwarded from the device are emitted as Qt::QueuedConnection
/// and can be safely received on the GUI thread.
class IDeviceRunner : public QObject {
    Q_OBJECT

public:
    /// Constructs the runner; the worker thread and device are created by the
    /// concrete subclass, not here.
    explicit IDeviceRunner(QObject *parent = nullptr) : QObject(parent) {}
    /// Defaulted destructor; concrete subclasses are responsible for stopping
    /// their worker thread before destruction.
    ~IDeviceRunner() override = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    /// Starts the internal worker QThread that will host the device.
    virtual void start()                         = 0;
    /// Quits the worker thread and waits (up to 3 s) for it to finish.
    virtual void stop()                          = 0;
    /// Moves the device onto the worker thread and wires its signals for
    /// cross-thread forwarding.
    virtual void attach()                        = 0;
    /// Unwires the device's signals and moves it off the worker thread onto
    /// `dest` (or the caller's thread if null).
    /// @param dest destination thread to move the device to
    virtual void detach(QThread *dest = nullptr) = 0;

    // ── State ─────────────────────────────────────────────────────────────────
    /// Returns the internal worker QThread the device runs on.
    virtual QThread             *workerThread()   const = 0;
    /// Returns the concrete device instance owned/managed by this runner.
    virtual vc::device::IDevice *device()         const = 0;

    /// Default command handler: rejects any command as unsupported. Concrete
    /// runners that support DeviceCommand dispatch override this.
    /// @param command the command that was submitted
    /// @return an UnsupportedCommand rejection result; also emitted via commandFinished()
    virtual DeviceCommandResult submitCommand(const DeviceCommand &command)
    {
        auto result = DeviceCommandResult::rejected(
            command,
            DeviceCommandResultCode::UnsupportedCommand,
            QStringLiteral("Runner does not support standard commands yet."));
        emit commandFinished(result);
        return result;
    }

    /// Returns whether attach() has run and detach() has not yet undone it.
    bool isAttached() const { return m_attached; }
    /// Returns whether the worker thread exists and is currently running.
    bool isRunning()  const { return workerThread() && workerThread()->isRunning(); }

    /// Requests the device detach on its own worker thread and blocks the calling
    /// thread (via a local QEventLoop) until the device confirms it via
    /// deviceThreadDetached().
    /// @param dest destination thread to move the device to once detached
    void detachFromOutsideThread(QThread *dest = nullptr) {
        QEventLoop loop;
        connect(this, &IDeviceRunner::requestDetach,
                device(), &device::IDevice::deviceDetachThread, Qt::SingleShotConnection);
        connect(device(), &device::IDevice::deviceThreadDetached,
                &loop, &QEventLoop::quit, Qt::SingleShotConnection);
        emit requestDetach(dest);
        loop.exec();
    }

    /// Synchronously closes the device connection on its own worker thread.
    ///
    /// Must be called BEFORE detach()/stop() during a phase teardown: the device
    /// owns thread-affined transport objects (e.g. a QTcpSocket created on the
    /// worker thread). Moving the device across threads or stopping the worker
    /// while the link is still open leaks the connection and can crash on
    /// re-entry. Triggering deviceDisconnect() here lets the device close its
    /// transport on the correct thread.
    ///
    /// No-op when the device is not connected. Bounded by timeoutMs so a
    /// misbehaving device cannot stall teardown. Requires the runner to still be
    /// attached with its worker thread running (the normal pre-detach state).
    /// @param timeoutMs maximum time to wait for the device to report disconnected
    void disconnectAndWait(int timeoutMs = 3000) {
        vc::device::IDevice *dev = device();
        if (!dev) return;

        const vc::device::ConnectStatus status = dev->connectStatus();
        if (status != vc::device::ConnectStatus::Connected &&
            status != vc::device::ConnectStatus::Connecting &&
            status != vc::device::ConnectStatus::LostConnected) {
            return;   // nothing to tear down
        }

        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);

        const QMetaObject::Connection statusConn = connect(
            this, &IDeviceRunner::connectStatusChanged, &loop,
            [&loop](vc::device::ConnectStatus s) {
                if (s == vc::device::ConnectStatus::Disconnected ||
                    s == vc::device::ConnectStatus::NoConnection) {
                    loop.quit();
                }
            });
        connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(this, &IDeviceRunner::requestDeviceDisconnect,
                dev, &vc::device::IDevice::deviceDisconnect, Qt::SingleShotConnection);

        guard.start(timeoutMs);
        emit requestDeviceDisconnect();
        loop.exec();
        disconnect(statusConn);
    }

signals:
    // Forwarded from the concrete device (cross-thread safe)
    /// Internal request (used by detachFromOutsideThread()) asking the device to
    /// detach on its own thread and move to `dest`.
    void requestDetach(QThread *dest);
    /// Internal request (used by disconnectAndWait()) asking the device to close
    /// its connection on its own worker thread.
    void requestDeviceDisconnect();
    // void detachFinished();
    /// Forwarded from the device via Qt::QueuedConnection: notifies of a connection
    /// state change; safe to receive on the GUI thread.
    void connectStatusChanged(vc::device::ConnectStatus status);
    /// Forwarded from the device via Qt::QueuedConnection: reports an error message;
    /// safe to receive on the GUI thread.
    void errorOccurred(const QString &msg);
    /// Emitted with the outcome of a submitCommand() call.
    void commandFinished(vc::runtime::DeviceCommandResult result);

protected:
    bool m_attached{false};  ///< True once attach() has run and detach() has not yet undone it.
};

} // namespace vc::runtime

#endif // IDEVICE_RUNNER_H
