#ifndef PROJECT_H
#define PROJECT_H

#include <QObject>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include "model/itask.h"
#include "device/idevice.h"
#include "device/device_manager.h"
#include <memory>

/// Core domain-model namespace: Project, ITask, and their JSON (de)serialization support.
namespace vc::model {

/// Aggregate root of a picking-configuration project: owns the task map, the shared
/// DeviceManager, and project metadata (name/author/version/timestamps), and provides
/// the JSON (de)serialization used by ProjectRepository.
class Project : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name
                   READ name WRITE setName
                       NOTIFY nameChanged)
    Q_PROPERTY(QString version
                   READ version WRITE setVersion
                       NOTIFY versionChanged)
    Q_PROPERTY(QString createdAt
                   READ createdAt NOTIFY createdAtChanged)
    Q_PROPERTY(QString updatedAt
                   READ updatedAt NOTIFY updatedAtChanged)

public:
    /// Constructs an empty project: creates the owned DeviceManager and wires its
    /// devicesChanged/deviceModified signals, and this project's taskModified signal,
    /// through to projectModificationOccurred().
    explicit Project(QObject* parent = nullptr);
    /// Destroys the project. Owned tasks and the device manager are released via
    /// their shared_ptr/QObject ownership; no explicit teardown is performed here.
    ~Project();

    // ── Getters ────────────────────────────────────────
    /// Returns the project name.
    QString name()      const { return m_name;      }
    /// Returns the project author.
    QString author()    const { return m_author;    }
    /// Returns the project description.
    QString description() const { return m_description; }
    /// Returns the project version string.
    QString version()   const { return m_version;   }
    /// Returns the creation timestamp string previously set via setCreatedAt().
    QString createdAt() const { return m_createdAt; }
    /// Returns the last-updated timestamp string previously set via setUpdatedAt().
    QString updatedAt() const { return m_updatedAt; }

    // ── Setters ────────────────────────────────────────
    /// Sets the project name. No-op if unchanged; otherwise emits nameChanged().
    void setName(const QString& v);
    /// Sets the project description. Does not emit a change signal.
    void setDescription(const QString &v);
    /// Sets the project author. No-op if unchanged; otherwise emits authorChanged().
    void setAuthor(const QString& v);
    /// Sets the project version. No-op if unchanged; otherwise emits versionChanged().
    void setVersion(const QString& v);
    /// Sets the creation timestamp and unconditionally emits createdAtChanged().
    void setCreatedAt(const QString& v);
    /// Sets the last-updated timestamp and unconditionally emits updatedAtChanged().
    void setUpdatedAt(const QString& v);


    // ── Task management ────────────────────────────────
    /// Takes ownership of `task` (wrapped into a shared_ptr) and adds it to the
    /// project. Fails if `task` is null or its name is already occupied. On success,
    /// assigns this project onto the task, registers its name as occupied, connects
    /// the task's configChanged signal to emit taskModified(id) and
    /// projectModificationOccurred(), and emits tasksChanged() and taskCreated(id).
    /// @param task task instance to adopt; ownership transfers to the project on success
    /// @return true if the task was added, false if `task` is null or its name is taken
    bool addTask(ITask* task);
    /// Removes the task with the given id, if present, emitting tasksChanged() and
    /// taskDeleted(id) on success.
    /// @return true if a task with `id` existed and was removed
    bool removeTask(const QString& id);
    /// Assigns the given device to the given task: looks up both by id, then calls
    /// ITask::assignDevice() and IDevice::setAssignedTaskId().
    /// @return false if either the task or the device cannot be found
    bool assignDeviceToTask(const QString &deviceId, const QString &taskId);
    /// Unassigns the given device from the given task: looks up both by id, then
    /// calls ITask::unassignDevice() and clears the device's assigned task id.
    /// @return false if either the task or the device cannot be found
    bool unassignDeviceFromTask(const QString &deviceId, const QString &taskId);
    /// Looks up a task by id.
    /// @return the matching task, or nullptr if no task has this id
    std::shared_ptr<vc::model::ITask> taskById(const QString& id) const;
    /// Renames the task with the given id to `name`, unless `name` is already occupied.
    /// @note removes `task->id()` (not the task's previous name) from the occupied-names
    /// set, and always returns false regardless of whether the rename succeeded.
    bool changeTaskName(const QString& id, const QString &name);
    /// Checks whether a task name is already in use in this project.
    bool isTaskNameOccupied(const QString& name) const;
    /// Returns the project's tasks, keyed by task id.
    const QMap<QString, std::shared_ptr<vc::model::ITask>>& getCurrentTasks() const {
        return m_tasks;
    }

    /// Moves device ownership from one task to another: unassigns the device from
    /// `fromTaskId`, assigns it to `toTaskId`, updates the device's assigned-task id,
    /// and emits projectModificationOccurred().
    /// @return false if either task can't be found, the device isn't currently on
    /// `fromTaskId`, or `toTaskId` already has the device
    bool moveDeviceToTask(const QString &deviceId,
                          const QString &fromTaskId,
                          const QString &toTaskId);

    // ── Device management ──────────────────────────────
    /// Returns the project's shared DeviceManager, which owns and tracks all
    /// registered devices.
    std::shared_ptr<device::DeviceManager> deviceManager();
    /// Looks up a device by id via the DeviceManager.
    /// @return the matching device, or nullptr if no device has this id
    std::shared_ptr<vc::device::IDevice> deviceById(const QString& id);

    // ── Serialize ──────────────────────────────────────
    /// Serializes the project to JSON: name, version, all registered devices (via
    /// IDevice::toJson()), and all tasks (via ITask::toJson()).
    /// @note image blobs associated with tasks are persisted separately by
    /// ProjectRepository and are not included here.
    QJsonObject toJson() const;
    /// Restores project state from JSON produced by toJson(): sets name/version,
    /// reconstructs devices via DeviceFactory and reserves them on the DeviceManager,
    /// reconstructs tasks via TaskFactory and adds them via addTask(), then re-assigns
    /// each device to its recorded task (releasing any device with no assigned task).
    /// @param json JSON object as produced by toJson()
    /// @return false if `json` is empty; true otherwise, including when individual
    /// malformed device/task entries were skipped
    bool fromJson(const QJsonObject &json);
    // QMap<QString, QMap<QString, cv::Mat>> toImageMaps() const;
    // bool fromImageMaps(const  QMap<QString, QMap<QString, cv::Mat>> &mapping);

    /// Checks whether the project is populated enough to be considered valid: has a
    /// non-empty name and at least one task.
    bool isValid() const {
        return !m_name.isEmpty() && !m_tasks.isEmpty();
    }

signals:
    /// Emitted when the project name changes.
    void nameChanged();
    /// Emitted when the project author changes.
    void authorChanged();
    /// Emitted when the project version changes.
    void versionChanged();
    /// Emitted when the creation timestamp is set.
    void createdAtChanged();
    /// Emitted when the last-updated timestamp is set.
    void updatedAtChanged();
    /// Emitted whenever the project's persisted state changes: name/author edits, a
    /// task's config, or the DeviceManager's device list/state.
    void projectModificationOccurred();

    /// Emitted after a task has been added to the project.
    /// @param id id of the newly added task
    void taskCreated(QString id);
    /// Emitted after a task has been removed from the project.
    /// @param id id of the removed task
    void taskDeleted(QString id);
    /// Emitted when a task's configuration changes.
    /// @param id id of the modified task
    void taskModified(QString id);
    /// Emitted whenever the task list changes (add or remove).
    void tasksChanged();

private:
    QString m_name;                     ///< Project name.
    QString m_author;                   ///< Project author.
    QString m_description;              ///< Project description.
    QString m_version   = "0.1";        ///< Project/schema version string; defaults to "0.1".
    QString m_createdAt;                ///< Creation timestamp, as set by setCreatedAt().
    QString m_updatedAt;                ///< Last-updated timestamp, as set by setUpdatedAt().

    std::shared_ptr<device::DeviceManager> m_deviceManager;   ///< Owned manager for all registered devices.

    QMap<QString, std::shared_ptr<vc::model::ITask>> m_tasks; ///< Tasks owned by this project, keyed by task id.

    QSet<QString> m_occupiedTaskNames; ///< Task names currently in use in this project.
};

} // namespace vc::model

#endif // PROJECT_H
