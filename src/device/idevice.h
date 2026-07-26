#ifndef IDEVICE_H
#define IDEVICE_H

#include <QObject>
#include <QMutex>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>
// #include <QUuid>

#include "irequest.h"
#include "core/logger/app_logger.h"
#include "idevice_config.h"

/// Device abstraction layer: the IDevice interface, connection status, and the Qt
/// property/JSON serialization contract shared by all concrete device implementations.
namespace vc::device {

class DeviceManager;

/// Connection lifecycle states reported by IDevice::connectStatus()/setConnectionStatus().
enum ConnectStatus {
    NoConnection,
    Disconnected,
    Connected,
    LostConnected,
    ConnectFailed,
    Connecting   ///< transport active but the link(s) are not (all) up yet
};

/// Abstract base for every device family (camera/PLC/vision-output/robot): owns the
/// device's id/name/task-assignment state, connection status, abstract IDeviceCfg pointer,
/// and JSON (de)serialization; concrete subclasses implement connect/disconnect/request
/// handling.
class IDevice : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_CLASSINFO("name_name", "Device name")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_CLASSINFO("id_name", "Device ID")

public:
    /// Constructs the device with its id/name; connection status starts at NoConnection
    /// and no config or manager is attached yet.
    explicit IDevice(QString id, QString name, QObject* parent = nullptr)
        : QObject(parent),
        m_id(id),
        m_name(name),
        m_connect_status(ConnectStatus::NoConnection),
        m_abstract_cfg(nullptr),
        m_manager(nullptr) {

    }

    virtual ~IDevice() = default;

    /// Establishes the device's connection (transport-specific in subclasses).
    /// @return true on success
    virtual bool deviceConnect()            = 0;
    /// Tears down the device's connection (transport-specific in subclasses).
    /// @return true on success
    virtual bool deviceDisconnect()         = 0;
    /// Reports whether the device is currently connected.
    virtual bool isDeviceConnected() const  = 0;
    /// Returns the concrete DeviceType (Camera/PLC/VisionOutput/Robot) of this device.
    virtual DeviceType  deviceType() const  = 0;

    /// Returns the last recorded status/error message (set via setConnectionStatus() on
    /// ConnectFailed, or overridden by subclasses to report other errors).
    virtual QString lastMsg()   const {
        return m_last_msg;
    }

    /// Returns the device's unique id.
    QString id() const {
        return m_id;
    }

    /// Returns the device's display name.
    QString name() const {
        return m_name;
    }

    /// Sets the display name and emits nameChanged(); no-op if `v` equals the current name.
    void setName(const QString& v) {
        if (m_name == v) return;
        m_name = v;
        emit nameChanged();
    }

    /// Records the id of the task this device is assigned to.
    void setAssignedTaskId(const QString& taskId) {
        m_assignedTaskId = taskId;
    }

    /// Returns the id of the task this device is assigned to (empty if unassigned).
    QString assignedTaskId() const {
        return m_assignedTaskId;
    }

    /// Returns the current connection lifecycle state.
    ConnectStatus connectStatus() const {
        return m_connect_status;
    }

    /// Updates the connection state and emits connectStatusChanged(); when `status` is
    /// ConnectFailed, also records `msg` as the last message and emits connectionFailed(msg).
    void setConnectionStatus(ConnectStatus status, QString msg = "") {
        // if (m_connect_status != status) {
        m_connect_status = status;
        emit connectStatusChanged(m_connect_status);

        if (status == ConnectStatus::ConnectFailed) {
            m_last_msg = msg;
            emit connectionFailed(msg);
        }
        // }
    }

    /// Attaches `cfg` as this device's abstract configuration and emits configChanged().
    /// @note stores a non-owning pointer — subclasses pass the address of their own owned
    /// config member; IDevice never deletes it
    virtual void setDeviceConfig(IDeviceCfg *cfg) {
        m_abstract_cfg = cfg;
        emit configChanged();
    }

    /// Returns a heap-allocated clone of the current device configuration (caller owns it).
    /// @note dereferences m_abstract_cfg without a null check; will crash if no config has
    /// been attached via setDeviceConfig()
    virtual IDeviceCfg* deviceConfig() {
        return m_abstract_cfg->clone();
    }

    /// Sets the DeviceManager that owns/tracks this device instance.
    virtual void setManager(DeviceManager* manager) {
        m_manager = manager;
    }

    /// Returns the DeviceManager that owns/tracks this device instance (nullptr if unset).
    virtual DeviceManager* deviceManager() {
        return m_manager;
    }

    /// Enqueues `request` for processing by this device (handling is device-specific).
    /// @return true if the request was accepted
    virtual bool pushRequest(IRequest *request) = 0;

    /// Serializes the device's id, name, assigned task id, type, and (if set) config into a
    /// JSON object using the DEVICE_JSK_* keys.
    /// @return the populated JSON object
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj[DEVICE_JSK_ID] = m_id;
        obj[DEVICE_JSK_NAME] = m_name;
        obj[DEVICE_JSK_TASKID] = m_assignedTaskId;
        obj[DEVICE_JSK_TYPE] = DeviceTypeToString(deviceType());
        if (m_abstract_cfg) {
            obj[DEVICE_JSK_CONFIG] = m_abstract_cfg->toJson();
        } else {
            obj[DEVICE_JSK_CONFIG] = QJsonObject();
        }
        return obj;
    }

    /// Restores id/name/assigned-task-id from `obj` (the first step of loading a device),
    /// validating that the required keys are present and that the JSON device type matches
    /// this instance's deviceType(); forwards the nested DeviceConfig object to
    /// m_abstract_cfg->fromJson() if a config is attached.
    /// @param obj device JSON produced by toJson()
    /// @return true on success; false (with a logged error) if required keys are missing or
    /// the device type doesn't match
    virtual bool fromJson(const QJsonObject &obj) {
        if (!obj.contains(DEVICE_JSK_ID) ||
            !obj.contains(DEVICE_JSK_NAME) ||
            // !obj.contains(DEVICE_JSK_TASKID) ||
            !obj.contains(DEVICE_JSK_TYPE) ||
            !obj.contains(DEVICE_JSK_CONFIG)) {

            LOG_DEV_ERR << "Cannot convert device from json, wrong json format.";
            LOG_DEV_ERR << QJsonDocument(obj).toJson();
            return false;
        }

        DeviceType dv_type = DeviceTypeFromString(obj[DEVICE_JSK_TYPE].toString());
        if (dv_type != this->deviceType()) {
            LOG_DEV_ERR << "Cannot convert device from json, wrong device type.";
            LOG_DEV_ERR << obj[DEVICE_JSK_TYPE].toString();
            return false;
        }

        m_id = obj[DEVICE_JSK_ID].toString();
        m_name = obj[DEVICE_JSK_NAME].toString();
        m_assignedTaskId = obj[DEVICE_JSK_TASKID].toString("");
        if (m_abstract_cfg){
            m_abstract_cfg->fromJson(obj[DEVICE_JSK_CONFIG].toObject());
        }
        return true;
    }

public slots:
    /// Base no-op initialization hook; concrete devices override this to perform actual
    /// startup and return true on success.
    /// @return false in the base implementation (not implemented)
    virtual bool deviceInitialize() {
        return false;
    }

    /// Stops/tears down the device instance in preparation for destruction.
    virtual void deviceTerminate() = 0;

    /// Moves this device's QObject to thread `dest` and emits deviceThreadDetached().
    /// No-op if `dest` is null.
    virtual void deviceDetachThread(QThread *dest) {
        if (dest == nullptr) {
            return;
        }

        this->moveToThread(dest);
        emit deviceThreadDetached();
    }

signals:
    /// Emitted when the device's display name changes.
    void nameChanged();
    /// Emitted when the device's configuration is (re)attached via setDeviceConfig().
    void configChanged();
    /// Emitted whenever setConnectionStatus() updates the connection state.
    void connectStatusChanged(ConnectStatus status);
    /// Emitted when setConnectionStatus(ConnectFailed, msg) records a failed connection attempt.
    void connectionFailed(QString msg);
    /// Emitted by subclasses to report a device-level error.
    void errorOccurred(const QString& msg);
    /// Emitted after deviceDetachThread() moves this object to a new thread.
    void deviceThreadDetached();

protected:
    QMutex m_mutex;  ///< Available to subclasses for guarding device-specific state.
    QString m_last_msg;  ///< Last status/error message; returned by lastMsg().

private:
    QString m_id;  ///< Unique device id.
    QString m_name;  ///< Display name.
    QString m_assignedTaskId;  ///< Id of the task this device is assigned to (empty if unassigned).
    ConnectStatus m_connect_status;  ///< Current connection lifecycle state.
    IDeviceCfg *m_abstract_cfg{nullptr};  ///< Non-owning pointer to the concrete subclass's config member.
    DeviceManager* m_manager{nullptr};  ///< Non-owning pointer to the owning DeviceManager.
};

} // namespace vc::device

#endif // IDEVICE_H
