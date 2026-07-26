#ifndef DEVICE_RUNNER_H
#define DEVICE_RUNNER_H

#include "runtime/idevice_runner.h"
#include <QApplication>
#include <QEventLoop>

/// Runtime layer: device command/result value types and the queue/runner machinery that
/// dispatches commands to per-device worker threads.
namespace vc::runtime {

/// CRTP template base for per-device thread runners — eliminates the duplication that used
/// to exist between the old CameraWorker and McDeviceWorker. Concrete subclasses only need
/// to implement wireSignals() / unwireSignals() with device-specific connections.
/// @note Thread ownership: m_thread is parented to this runner (QThread(this)) and is
///       automatically cleaned up. The device pointer is NOT owned here; it is a raw
///       pointer into the shared_ptr held by DeviceManager.
/// @note attach() / detach() guard against double-calling via m_attached.
template<typename TDevice>
class DeviceRunner : public IDeviceRunner {

public:
    /// Wraps `device` (not owned) and creates (but does not start) its worker QThread,
    /// parented to this runner.
    /// @note `device` must not be null (enforced via Q_ASSERT).
    explicit DeviceRunner(TDevice *device, QObject *parent = nullptr)
        : IDeviceRunner(parent)
        , m_device(device)
    {
        Q_ASSERT(device != nullptr);
        m_thread = new QThread(this);
        // m_thread = new QThread();
    }

    /// Stops the worker thread (via stop()) before destruction.
    ~DeviceRunner() override {
        stop();
        // m_thread->deleteLater();
    }

    // ── IDeviceRunner ─────────────────────────────────────────────────────────

    /// Starts the worker thread at high priority, if not already running.
    void start() override {
        if (!m_thread->isRunning()) {
            m_thread->start(QThread::HighPriority);
            LOG_USER_INFO << QString("Device {%1} - ID %2, runner thread started.")
                                 .arg(m_device->name()).arg(m_device->id());
        }
    }

    /// Quits the worker thread and waits up to 3 s for it to finish, if running.
    void stop() override {
        if (m_thread->isRunning()) {
            m_thread->quit();
            m_thread->wait(3000);
        }
    }

    /// Moves the device onto the worker thread and calls wireSignals(); no-op if already attached.
    void attach() override {
        if (m_attached) return;
        m_device->moveToThread(m_thread);
        wireSignals();
        m_attached = true;
    }

    /// Calls unwireSignals() and moves the device off the worker thread onto `dest` (or the
    /// calling thread if null); no-op if not currently attached.
    void detach(QThread *dest = nullptr) override {
        if (!m_attached) return;
        unwireSignals();
        detachFromOutsideThread(dest ? dest : QThread::currentThread());
        m_attached = false;
    }

    /// Returns the worker QThread the device runs on.
    QThread             *workerThread() const override { return m_thread;  }
    /// Returns the wrapped device instance (not owned by this runner).
    vc::device::IDevice *device()       const override { return m_device; }

    /// Type-safe accessor for concrete subclasses: returns the wrapped device as `TDevice *`.
    TDevice *typedDevice() const { return m_device; }

protected:
    // ── Subclass implements ──────────────────────────────────────────────────
    /// Connects device-specific signals/slots using Qt::QueuedConnection; called by attach()
    /// after the device is moved to the worker thread.
    virtual void wireSignals()   = 0;
    /// Disconnects the connections made by wireSignals(); called by detach() before the device
    /// is moved off the worker thread.
    virtual void unwireSignals() = 0;

    TDevice *m_device{nullptr}; ///< Wrapped device instance; not owned (raw pointer into DeviceManager's shared_ptr).
    QThread *m_thread{nullptr}; ///< Worker thread the device is moved onto while attached; owned (parented to this runner).
};

} // namespace vc::runtime

#endif // DEVICE_RUNNER_H
