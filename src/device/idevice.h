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

namespace vc::device {

class DeviceManager;

enum ConnectStatus {
    NoConnection,
    Disconnected,
    Connected,
    LostConnected,
    ConnectFailed,
    Connecting   // transport active but the link(s) are not (all) up yet
};

class IDevice : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_CLASSINFO("name_name", "Device name")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_CLASSINFO("id_name", "Device ID")

public:
    explicit IDevice(QString id, QString name, QObject* parent = nullptr)
        : QObject(parent),
        m_id(id),
        m_name(name),
        m_connect_status(ConnectStatus::NoConnection),
        m_abstract_cfg(nullptr),
        m_manager(nullptr) {

    }

    virtual ~IDevice() = default;

    virtual bool deviceConnect()            = 0;
    virtual bool deviceDisconnect()         = 0;
    virtual bool isDeviceConnected() const  = 0;
    virtual DeviceType  deviceType() const  = 0;

    virtual QString lastMsg()   const {
        return m_last_msg;
    }

    QString id() const {
        return m_id;
    }

    QString name() const {
        return m_name;
    }

    void setName(const QString& v) {
        if (m_name == v) return;
        m_name = v;
        emit nameChanged();
    }

    void setAssignedTaskId(const QString& taskId) {
        m_assignedTaskId = taskId;
    }
    
    QString assignedTaskId() const {
        return m_assignedTaskId;
    }

    ConnectStatus connectStatus() const {
        return m_connect_status;
    }

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

    virtual void setDeviceConfig(IDeviceCfg *cfg) {
        m_abstract_cfg = cfg;
        emit configChanged();
    }

    virtual IDeviceCfg* deviceConfig() {
        return m_abstract_cfg->clone();
    }

    virtual void setManager(DeviceManager* manager) {
        m_manager = manager;
    }

    virtual DeviceManager* deviceManager() {
        return m_manager;
    }

    virtual bool pushRequest(IRequest *request) = 0;

    /**
     * @brief toJson
     * @return
     */
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

    /**
     * @brief fromJson: convert form json for the first step
     * @param obj
     * @return
     */
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
    virtual bool deviceInitialize() {
        return false;
    }

    virtual void deviceTerminate() = 0;

    virtual void deviceDetachThread(QThread *dest) {
        if (dest == nullptr) {
            return;
        }

        this->moveToThread(dest);
        emit deviceThreadDetached();
    }

signals:
    void nameChanged();
    void configChanged();
    void connectStatusChanged(ConnectStatus status);
    void connectionFailed(QString msg);
    void errorOccurred(const QString& msg);
    void deviceThreadDetached();

protected:
    QMutex m_mutex;
    QString m_last_msg;

private:
    QString m_id;
    QString m_name;
    QString m_assignedTaskId;
    ConnectStatus m_connect_status;
    IDeviceCfg *m_abstract_cfg{nullptr};
    DeviceManager* m_manager{nullptr};
};

} // namespace vc::device

#endif // IDEVICE_H
