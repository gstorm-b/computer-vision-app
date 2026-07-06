#ifndef ITASK_H
#define ITASK_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include <QStringList>

#include <opencv2/opencv.hpp>

#include "task_define.h"
#include "task_state_machine.h"
#include "itask_config.h"
#include "core/logger/app_logger.h"
#include "runtime/task_runner.h"
#include "device/idevice.h"

namespace vc::model {

class Project;

class ITask : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_CLASSINFO("name_name", "Task name")

public:
    explicit ITask(QString init_id = "", QObject* parent = nullptr)
        : QObject(parent) {

        // create id, cannot change id after construction
        if (init_id.isEmpty()) {
            m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        } else {
            m_id = init_id;
        }
    }

    virtual ~ITask() = default;

    virtual TaskType taskType() const = 0;
    virtual bool isValid() const = 0;
    TaskState taskState() const { return m_taskState; }
    QString taskStateName() const { return taskStateDisplayName(m_taskState); }

    QString id() const {
        return m_id;
    }

    QString name() const {
        return m_name;
    }

    void setName(const QString& v) {
        if (m_name == v) {
            return;
        }
        m_name = v;
        emit nameChanged();
    }

    CameraSourceType cameraSourceType() const {
        return m_cameraSourceType;
    }

    void setCameraSourceType(CameraSourceType v) {
        m_cameraSourceType = v;
        emit cameraSourceTypeChanged();
    }

    bool isOwned()  const {
        return m_cameraSourceType == CameraSourceType::Source_Owned;
    }

    bool isShared() const {
        return m_cameraSourceType == CameraSourceType::Source_Shared;
    }

    QString ownedCameraId() const {
        return m_ownedCameraId;
    }

    void setOwnedCameraId(const QString& v) {
        m_ownedCameraId = v;
    }

    // ── Device ownership ───────────────────────────────
    void assignDevice(const QString &deviceId) {
        if (m_assignedDeviceIds.contains(deviceId)) return;
        m_assignedDeviceIds.append(deviceId);
        emit devicesChanged();
    }

    void unassignDevice(const QString &deviceId) {
        if (m_assignedDeviceIds.removeOne(deviceId))
            emit devicesChanged();
    }

    bool hasDevice(const QString &deviceId) const {
        return m_assignedDeviceIds.contains(deviceId);
    }

    QStringList assignedDeviceIds() const {
        return m_assignedDeviceIds;
    }

    std::shared_ptr<vc::device::IDevice> getTaskDevice(const QString &deviceId) const;

    // Resolves m_assignedDeviceIds against the project's DeviceManager and
    // returns only the devices matching `t`.  Skips ids whose device isn't
    // currently registered.  Returns an empty list if the task has no
    // project set yet.
    QList<std::shared_ptr<vc::device::IDevice>>
        assignedDevicesOfType(vc::device::DeviceType t) const;

    virtual bool isReachLimitOfDeviceType(vc::device::DeviceType t) const = 0;

    virtual void setTaskConfig(ITaskConfig *cfg) {
        m_abstract_cfg = cfg;
        emit configChanged();
    }

    virtual ITaskConfig* taskConfig() {
        return m_abstract_cfg->copy();
    }

    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = m_id;
        obj["name"] = m_name;
        obj["taskType"] = qenumToString(taskType());
        obj["cameraSourceType"] = qenumToString(m_cameraSourceType);
        obj["ownedCameraId"] = m_ownedCameraId;

        QJsonArray devIds;
        for (const QString &id : m_assignedDeviceIds)
            devIds.append(id);
        obj["assignedDeviceIds"] = devIds;

        if (m_abstract_cfg) {
            obj["taskConfig"] = m_abstract_cfg->toJson();
        }
        return obj;
    }


    virtual bool fromJson(const QJsonObject& obj) {
        if (!obj.contains("id") ||
            !obj.contains("name") ||
            !obj.contains("taskType") ||
            !obj.contains("taskConfig")) {
            LOG_DEV_ERR << "Cannot convert task from json, wrong json format.";
            LOG_DEV_ERR << QJsonDocument(obj).toJson();
            return false;
        }

        TaskType task_type = stringToQEnum(obj["taskType"].toString(), TaskType::UndefineTask);
        if (task_type != this->taskType()) {
            LOG_DEV_ERR << "Cannot convert task from json, wrong task type.";
            LOG_DEV_ERR << obj["taskType"].toString();
            return false;
        }

        m_id = obj["id"].toString();
        m_name = obj["name"].toString();

        if (obj.contains("assignedDeviceIds")) {
            m_assignedDeviceIds.clear();
            for (const QJsonValue &v : obj["assignedDeviceIds"].toArray())
                m_assignedDeviceIds.append(v.toString());
        }

        if (m_abstract_cfg) {
            return m_abstract_cfg->fromJson(obj["taskConfig"].toObject());
        }
        return true;
    }

    virtual QMap<QString, cv::Mat> getTaskImageMap() {
        return QMap<QString, cv::Mat>();
    }

    virtual bool loadTaskImageMap(QMap<QString, cv::Mat> &mapping) {
        return false;
    }

    virtual void setProject(Project *proj) {
        m_proj = proj;
    }

    virtual Project* project() {
        return m_proj;
    }

    // ── Thread / phase management ──────────────────────────────────────────
    vc::runtime::TaskRunner *taskRunner() const { return m_taskRunner; }

    // Commission phase: each assigned device gets its own HighPriority thread.
    // Widget calls this before showing the device config page.
    virtual void beginCommission();

    // End commission, stop per-device threads (→ Idle).
    virtual void endCommission();

    // Runtime phase: start production execution.
    // mergeToTaskThread=false (default): keep per-device threads, runtime
    //   thread acts as coordinator.
    // mergeToTaskThread=true: move all devices to the single runtime thread.
    virtual void beginRuntime(bool mergeToTaskThread = false);

    // End runtime, stop all threads (→ Idle).
    virtual void endRuntime();

    // Convenience: stop everything regardless of current phase.
    virtual void stopAll();

    // Re-evaluate runners against m_assignedDeviceIds.  Call after the user
    // adds or removes a device while the task is running — TaskRunner will
    // auto-start any new runner when in Commission/Runtime phase.
    void resyncDevices() { syncRunnersWithDevices(); }

signals:
    void nameChanged();
    void configChanged();
    void cameraSourceTypeChanged();
    void patternsChanged();
    void devicesChanged();
    void taskStateChanged(vc::model::TaskState newState);

    void signalChanged(QString name, QVariant value);

    void commissionStarted();
    void commissionStopped();
    void runtimeStarted();
    void runtimeStopped();

protected:
    bool transitionTaskState(TaskState targetState, const QString &reason = QString());

    // Syncs registered runners with m_assignedDeviceIds.
    // Must be called from beginCommission() / beginRuntime().
    // Implemented in itask.cpp (needs full Project definition).
    void syncRunnersWithDevices();

protected:
    Project *m_proj{nullptr};

private:
    QString m_id;
    QString m_name;
    CameraSourceType m_cameraSourceType = CameraSourceType::Source_Owned;
    QString m_ownedCameraId;
    QStringList m_assignedDeviceIds;
    ITaskConfig *m_abstract_cfg{nullptr};

    // Owned thread/runner manager — created once, lives with the task.
    vc::runtime::TaskRunner *m_taskRunner{new vc::runtime::TaskRunner(this)};
    TaskState m_taskState{TaskState::Idle};
};

} // namespace vc::model
#endif // ITASK_H
