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

/// Model classes for projects, tasks, and task configuration.
namespace vc::model {

/// Forward declaration; ITask holds a raw back-pointer to its owning Project
/// (see m_proj) without needing the full Project definition here.
class Project;

/// Abstract base for a configurable, stateful vision task: owns a set of assigned
/// device ids, a per-task TaskRunner for commission/runtime thread management, and
/// JSON (de)serialization of its identity and ITaskConfig. Concrete subclasses supply
/// taskType(), isValid(), and device-limit checks.
class ITask : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_CLASSINFO("name_name", "Task name")

public:
    /// Constructs the task, assigning it a fixed id.
    /// @param init_id id to reuse (e.g. when reloading from JSON); if empty, a new
    ///   random UUID is generated instead. The id cannot be changed after construction.
    /// @param parent optional QObject parent
    explicit ITask(QString init_id = "", QObject* parent = nullptr)
        : QObject(parent) {

        // create id, cannot change id after construction
        if (init_id.isEmpty()) {
            m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        } else {
            m_id = init_id;
        }
    }

    /// Default-destructs the task; no explicit cleanup beyond QObject's.
    virtual ~ITask() = default;

    /// Returns the concrete task type this instance implements.
    virtual TaskType taskType() const = 0;
    /// Returns whether the task's current configuration/state is valid enough to run.
    virtual bool isValid() const = 0;
    /// Returns the current commission/runtime lifecycle state of the task.
    TaskState taskState() const { return m_taskState; }
    /// Returns the human-readable display name for the current taskState().
    QString taskStateName() const { return taskStateDisplayName(m_taskState); }

    /// Returns the task's fixed id (set once at construction; never changes).
    QString id() const {
        return m_id;
    }

    /// Returns the task's display name.
    QString name() const {
        return m_name;
    }

    /// Sets the task's display name, emitting nameChanged() if it actually changed.
    /// @param v new name
    void setName(const QString& v) {
        if (m_name == v) {
            return;
        }
        m_name = v;
        emit nameChanged();
    }

    /// Returns whether this task uses its own owned camera or a shared one.
    CameraSourceType cameraSourceType() const {
        return m_cameraSourceType;
    }

    /// Sets the camera source type, always emitting cameraSourceTypeChanged()
    /// (unlike setName(), there is no no-op check for an unchanged value).
    /// @param v new camera source type
    void setCameraSourceType(CameraSourceType v) {
        m_cameraSourceType = v;
        emit cameraSourceTypeChanged();
    }

    /// Returns true if this task's camera source is Source_Owned.
    bool isOwned()  const {
        return m_cameraSourceType == CameraSourceType::Source_Owned;
    }

    /// Returns true if this task's camera source is Source_Shared.
    bool isShared() const {
        return m_cameraSourceType == CameraSourceType::Source_Shared;
    }

    /// Returns the id of the camera this task owns (only meaningful when isOwned()).
    QString ownedCameraId() const {
        return m_ownedCameraId;
    }

    /// Sets the owned camera id. Does not check for a no-op value and does not
    /// emit any change signal.
    /// @param v new owned camera id
    void setOwnedCameraId(const QString& v) {
        m_ownedCameraId = v;
    }

    // ── Device ownership ───────────────────────────────
    /// Adds `deviceId` to the set of devices assigned to this task; no-op (and no
    /// signal) if already assigned. Emits devicesChanged() on success.
    /// @param deviceId id of the device to assign
    void assignDevice(const QString &deviceId) {
        if (m_assignedDeviceIds.contains(deviceId)) return;
        m_assignedDeviceIds.append(deviceId);
        emit devicesChanged();
    }

    /// Removes `deviceId` from the assigned-device set. Emits devicesChanged()
    /// only if the id was actually present and removed.
    /// @param deviceId id of the device to unassign
    void unassignDevice(const QString &deviceId) {
        if (m_assignedDeviceIds.removeOne(deviceId))
            emit devicesChanged();
    }

    /// Returns whether `deviceId` is currently assigned to this task.
    bool hasDevice(const QString &deviceId) const {
        return m_assignedDeviceIds.contains(deviceId);
    }

    /// Returns the ids of all devices currently assigned to this task.
    QStringList assignedDeviceIds() const {
        return m_assignedDeviceIds;
    }

    /// Looks up `deviceId` via the owning Project's DeviceManager.
    /// @param deviceId id of the device to resolve
    /// @return the shared device instance, or a null pointer if the task has no
    ///   project set, the project has no DeviceManager, or the id isn't registered
    std::shared_ptr<vc::device::IDevice> getTaskDevice(const QString &deviceId) const;

    /// Resolves m_assignedDeviceIds against the project's DeviceManager and
    /// returns only the devices matching `t`.  Skips ids whose device isn't
    /// currently registered.  Returns an empty list if the task has no
    /// project set yet.
    /// @param t device type to filter by
    /// @return assigned devices of type `t`
    QList<std::shared_ptr<vc::device::IDevice>>
        assignedDevicesOfType(vc::device::DeviceType t) const;

    /// Returns whether the task already has as many assigned devices of type `t`
    /// as its configuration allows (subclass-defined limit).
    virtual bool isReachLimitOfDeviceType(vc::device::DeviceType t) const = 0;

    /// Replaces the task's abstract configuration object and emits configChanged().
    /// @param cfg new configuration; ownership/lifetime is managed by the caller
    virtual void setTaskConfig(ITaskConfig *cfg) {
        m_abstract_cfg = cfg;
        emit configChanged();
    }

    /// Returns a fresh copy of the task's current configuration.
    /// @return a newly allocated ITaskConfig::copy() of m_abstract_cfg; caller owns it
    virtual ITaskConfig* taskConfig() {
        return m_abstract_cfg->copy();
    }

    /// Serializes id, name, taskType, cameraSourceType, ownedCameraId,
    /// assignedDeviceIds, and (if set) the taskConfig into a JSON object.
    /// @return the serialized task
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


    /// Restores id, name, assignedDeviceIds, and (if a config object is already set)
    /// the taskConfig from a JSON object previously produced by toJson().
    /// @param obj serialized task; must contain "id", "name", "taskType", and
    ///   "taskConfig" with a "taskType" matching this instance's taskType()
    /// @return false (logging the mismatch) if required keys are missing or the
    ///   task type doesn't match; true otherwise
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

    /// Base implementation returns an empty map; subclasses that cache per-device
    /// result/reference images override this to expose them.
    /// @return empty map (base class has no images to expose)
    virtual QMap<QString, cv::Mat> getTaskImageMap() {
        return QMap<QString, cv::Mat>();
    }

    /// Base implementation is a no-op that always fails; subclasses that support
    /// restoring cached images from a mapping override this.
    /// @param mapping candidate images keyed by identifier
    /// @return false (base class does not load anything)
    virtual bool loadTaskImageMap(QMap<QString, cv::Mat> &mapping) {
        return false;
    }

    /// Sets the owning Project back-pointer (non-owning).
    /// @param proj owning project, or nullptr
    virtual void setProject(Project *proj) {
        m_proj = proj;
    }

    /// Returns the owning Project, or nullptr if none has been set yet.
    virtual Project* project() {
        return m_proj;
    }

    // ── Thread / phase management ──────────────────────────────────────────
    /// Returns the task's owned TaskRunner, which manages per-device threads
    /// across commission/runtime phases.
    vc::runtime::TaskRunner *taskRunner() const { return m_taskRunner; }

    /// Commission phase: each assigned device gets its own HighPriority thread.
    /// Widget calls this before showing the device config page.
    virtual void beginCommission();

    /// End commission, stop per-device threads (→ Idle).
    virtual void endCommission();

    /// Runtime phase: start production execution.
    /// @param mergeToTaskThread false (default): keep per-device threads, runtime
    ///   thread acts as coordinator; true: move all devices to the single runtime thread.
    virtual void beginRuntime(bool mergeToTaskThread = false);

    /// End runtime, stop all threads (→ Idle).
    virtual void endRuntime();

    /// Convenience: stop everything regardless of current phase.
    virtual void stopAll();

    /// Re-evaluate runners against m_assignedDeviceIds.  Call after the user
    /// adds or removes a device while the task is running — TaskRunner will
    /// auto-start any new runner when in Commission/Runtime phase.
    void resyncDevices() { syncRunnersWithDevices(); }

signals:
    void nameChanged();               ///< Emitted after setName() actually changes the name.
    void configChanged();              ///< Emitted after setTaskConfig() replaces the configuration.
    void cameraSourceTypeChanged();     ///< Emitted after setCameraSourceType() is called.
    void patternsChanged();             ///< Emitted when subclass-specific patterns change.
    void devicesChanged();               ///< Emitted after a device is assigned or unassigned.
    void taskStateChanged(vc::model::TaskState newState); ///< Emitted after a successful state transition.

    void signalChanged(QString name, QVariant value); ///< Generic named-value change notification.

    void commissionStarted(); ///< Emitted at the end of beginCommission() once resources are ready.
    void commissionStopped(); ///< Emitted at the end of endCommission().
    void runtimeStarted();    ///< Emitted at the end of beginRuntime() once resources are ready.
    void runtimeStopped();    ///< Emitted at the end of endRuntime().

protected:
    /// Attempts to move the task's lifecycle state to `targetState`, validating the
    /// transition, logging the outcome, and emitting taskStateChanged() on success.
    /// @param targetState desired new state
    /// @param reason free-text context for the transition, used in log messages
    /// @return true if the transition was valid and applied (or was already a no-op)
    bool transitionTaskState(TaskState targetState, const QString &reason = QString());

    // Syncs registered runners with m_assignedDeviceIds.
    // Must be called from beginCommission() / beginRuntime().
    // Implemented in itask.cpp (needs full Project definition).
    /// Registers TaskRunner runners for newly assigned devices and unregisters
    /// runners for devices no longer assigned, per m_assignedDeviceIds.
    void syncRunnersWithDevices();

protected:
    Project *m_proj{nullptr}; ///< Non-owning back-pointer to the owning Project, if any.

private:
    QString m_id;   ///< Fixed task id, generated or supplied at construction; immutable afterward.
    QString m_name; ///< Display name shown to the user.
    CameraSourceType m_cameraSourceType = CameraSourceType::Source_Owned; ///< Owned vs. shared camera source.
    QString m_ownedCameraId;         ///< Id of the owned camera, when cameraSourceType() is Source_Owned.
    QStringList m_assignedDeviceIds; ///< Ids of all devices currently assigned to this task.
    ITaskConfig *m_abstract_cfg{nullptr}; ///< Current task configuration; not owned/deleted here.

    // Owned thread/runner manager — created once, lives with the task.
    vc::runtime::TaskRunner *m_taskRunner{new vc::runtime::TaskRunner(this)}; ///< Owned per-task thread/runner manager (QObject-parented to `this`).
    TaskState m_taskState{TaskState::Idle}; ///< Current commission/runtime lifecycle state.
};

} // namespace vc::model
#endif // ITASK_H
