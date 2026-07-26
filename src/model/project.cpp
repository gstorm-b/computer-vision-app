#include "project.h"
#include "device/device_factory.h"
#include "model/task_factory.h"

#include <QTimer>
#include <QEventLoop>

/// Model classes for projects, tasks, and task configuration.
namespace vc::model {

/// Constructs the project: creates its owned DeviceManager and wires up change
/// propagation so that device changes, device modifications, and task
/// modifications all surface as a single projectModificationOccurred() signal.
/// @param parent optional QObject parent
Project::Project(QObject* parent)
    : QObject(parent) {

    m_deviceManager = std::make_shared<device::DeviceManager>(this);

    connect(m_deviceManager.get(), &device::DeviceManager::devicesChanged,
            this, &Project::projectModificationOccurred);
    connect(m_deviceManager.get(), &device::DeviceManager::deviceModified,
            this, [this]() {
        emit this->projectModificationOccurred();
    });

    connect(this, &vc::model::Project::taskModified,
            this, [this]() {
        emit this->projectModificationOccurred();
    });
}

/// Default-destructs the project; no explicit cleanup required.
Project::~Project() {

}

/// Sets the project name, emitting nameChanged() if it actually changed.
/// @param v new name
void Project::setName(const QString& v) {
    if (m_name == v) return;
    m_name = v; emit nameChanged();
}

/// Sets the project description. No change check and no change signal.
/// @param v new description
void Project::setDescription(const QString &v) {
    m_description = v;
}

/// Sets the project author, emitting authorChanged() if it actually changed.
/// @param v new author
void Project::setAuthor(const QString& v) {
    if (m_author == v) return;
    m_author = v; emit authorChanged();
}

/// Sets the project version, emitting versionChanged() if it actually changed.
/// @param v new version string
void Project::setVersion(const QString& v) {
    if (m_version == v) return;
    m_version = v; emit versionChanged();
}

/// Sets the creation timestamp, always emitting createdAtChanged() (no
/// no-op check for an unchanged value).
/// @param v new created-at timestamp
void Project::setCreatedAt(const QString& v) {
    m_createdAt = v; emit createdAtChanged();
}

/// Sets the last-updated timestamp, always emitting updatedAtChanged() (no
/// no-op check for an unchanged value).
/// @param v new updated-at timestamp
void Project::setUpdatedAt(const QString& v) {
    m_updatedAt = v; emit updatedAtChanged();
}

/// Takes ownership of `task` (wraps it in a shared_ptr), registers it under its
/// id, links it to this project (setProject()), and connects its configChanged()
/// signal to re-emit taskModified()/projectModificationOccurred(). Rejects the
/// task if its name is already occupied.
bool Project::addTask(ITask* task) {
    if (!task) {
        return false;
    }

    if (m_occupiedTaskNames.contains(task->name())){
        return false;
    }

    std::shared_ptr<vc::model::ITask> task_ptr;
    task_ptr.reset(task);
    task_ptr->setProject(this);
    m_tasks.insert(task_ptr->id(), task_ptr);
    m_occupiedTaskNames.insert(task_ptr->name());

    // connect signal — capture the task id by value, NOT the shared_ptr, so this
    // connection does not keep the task alive past removeTask(). The connection
    // auto-disconnects when the task (the sender) is destroyed.
    const QString taskId = task_ptr->id();
    connect(task_ptr.get(), &vc::model::ITask::configChanged, this, [this, taskId]() {
        emit this->taskModified(taskId);
    });

    connect(task_ptr.get(), &vc::model::ITask::configChanged,
            this, &Project::projectModificationOccurred);

    emit tasksChanged();
    emit taskCreated(task_ptr->id());
    return true;
}

/// Removes the task with the given id from the project, emitting tasksChanged()
/// and taskDeleted() on success.
/// @param id id of the task to remove
bool Project::removeTask(const QString& id) {
    if (m_tasks.contains(id)) {
        m_tasks.remove(id);
        emit tasksChanged();
        emit taskDeleted(id);
        return true;
    }
    return false;
}

/// Assigns a device to a task: adds the device id to the task's assigned-device
/// set and records the task id on the device (device::IDevice::setAssignedTaskId()).
/// @param deviceId id of the device to assign
/// @param taskId id of the task to assign it to
bool Project::assignDeviceToTask(const QString &deviceId, const QString &taskId) {
    std::shared_ptr<vc::model::ITask> task = this->taskById(taskId);
    if (!task) return false;

    std::shared_ptr<vc::device::IDevice> device = this->deviceById(deviceId);
    if (!device) return false;

    task->assignDevice(deviceId);
    device->setAssignedTaskId(taskId);
    return true;
}

/// Unassigns a device from a task: removes the device id from the task's
/// assigned-device set and clears the device's recorded assigned task id.
/// @param deviceId id of the device to unassign
/// @param taskId id of the task to unassign it from
bool Project::unassignDeviceFromTask(const QString &deviceId, const QString &taskId) {
    std::shared_ptr<vc::model::ITask> task = this->taskById(taskId);
    if (!task) return false;

    std::shared_ptr<vc::device::IDevice> device = this->deviceById(deviceId);
    if (!device) return false;

    task->unassignDevice(deviceId);
    device->setAssignedTaskId("");
    return true;
}

/// Looks up a task by id.
/// @param id task id to look up
std::shared_ptr<vc::model::ITask> Project::taskById(const QString& id) const {
    if (m_tasks.contains(id)) {
        return m_tasks.value(id, nullptr);
    }
    return nullptr;
}

/// Renames the task with the given id to `name` and emits taskModified(id).
/// @param id id of the task to rename
/// @param name new name; rejected if already occupied by another task
/// @return false if `name` is already occupied or `id` is not a known task;
///   also returns false unconditionally after a successful rename (the
///   `return false` below is reached even when the rename succeeded)
/// @note frees the *old* occupied-name entry using `task->id()` rather than the
///   task's previous name, so m_occupiedTaskNames is only cleared correctly when
///   the task's id happens to equal its prior name
bool Project::changeTaskName(const QString& id, const QString &name) {
    if (m_occupiedTaskNames.contains(name)) {
        return false;
    }

    if (!m_tasks.contains(id)) {
        return false;
    }

    std::shared_ptr<ITask> task = m_tasks.value(id);
    QString old_name = task->id();
    task->setName(name);
    m_occupiedTaskNames.remove(old_name);
    emit taskModified(id);
    return false;
}

/// Returns whether `name` is already in use by a registered task.
bool Project::isTaskNameOccupied(const QString& name) const {
    return m_occupiedTaskNames.contains(name);
}

/// Returns the project's owned DeviceManager.
std::shared_ptr<device::DeviceManager> Project::deviceManager() {
    return m_deviceManager;
}

/// Looks up a device by id via the project's DeviceManager.
/// @param id device id to look up
std::shared_ptr<vc::device::IDevice> Project::deviceById(const QString& id) {
    return m_deviceManager->getCurrentDevices().value(id, nullptr);
}

/// Serializes the project to JSON: name, version, all devices (via the
/// DeviceManager's current devices), and all tasks.
/// @return the serialized project
QJsonObject Project::toJson() const {
    // get device json data
    QJsonArray deviceArr;
    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = m_deviceManager->getCurrentDevices().cbegin();
    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device_end = m_deviceManager->getCurrentDevices().cend();
    while (it_device != it_device_end) {
        deviceArr.append(it_device.value()->toJson());
        it_device++;
    }

    // get task json data
    QJsonArray taskArr;
    QMap<QString, std::shared_ptr<vc::model::ITask>>::const_iterator it_task = m_tasks.cbegin();
    while (it_task != m_tasks.cend()) {
        taskArr.append(it_task.value()->toJson());
        it_task++;
    }

    // qDebug() << deviceArr;

    return QJsonObject {
                       { "name",    m_name    },
                       { "version", m_version },
                       { "devices", deviceArr },
                       { "tasks",   taskArr   },
                       };
}

/// Restores the project from JSON previously produced by toJson(): reads name
/// and version, reconstructs devices via device::DeviceFactory::fromJson() and
/// reserves them on the DeviceManager, reconstructs tasks via
/// model::TaskFactory::fromJson() and adds them, then re-links each device to
/// its assignedTaskId(). Devices with no assigned task are logged and released
/// (deleted from the DeviceManager) rather than kept orphaned.
bool Project::fromJson(const QJsonObject &json) {
    if (json.isEmpty()) {
        return false;
    }

    m_name = json["name"].toString();
    m_version = json["version"].toString();
    QJsonArray taskArr = json["tasks"].toArray();
    QJsonArray deviceArr = json["devices"].toArray();

    // convert devices from json
    for (int idx=0;idx<deviceArr.size();idx++) {
        if (!deviceArr[idx].isObject()) {
            continue;
        }

        QJsonObject obj = deviceArr[idx].toObject();
        std::shared_ptr<device::IDevice> dv(device::DeviceFactory::fromJson(obj));
        if (!dv) {
            continue;
        }

        QString id = obj[DEVICE_JSK_ID].toString();
        QString name = obj[DEVICE_JSK_NAME].toString();
        m_deviceManager->reserveDevice(id, name, dv);
        LOG_USER_INFO << "Device loaded from JSON. ID: " << id << ", Name: " << name;
    }

    // convert tasks from json
    for (int idx=0;idx<taskArr.size();idx++) {
        if (!taskArr[idx].isObject()) {
            continue;
        }

        QJsonObject obj = taskArr[idx].toObject();
        model::ITask* task = model::TaskFactory::fromJson(obj);
        if (!task) {
            continue;
        }

        this->addTask(task);
        LOG_USER_INFO << "Task loaded from JSON. ID: " << task->id() << ", Name: " << task->name();
    }

    // assign devices to tasks
    const QMap<QString, std::shared_ptr<vc::device::IDevice>>& devices = m_deviceManager->getCurrentDevices();
    QStringList releaseDeviceIds;
    for (const std::shared_ptr<vc::device::IDevice>& device : devices) {
        qDebug() << device->id() << device->assignedTaskId();
        QString assignedTaskId = device->assignedTaskId();
        if (assignedTaskId.isEmpty()) {
            releaseDeviceIds.append(device->id());
            LOG_USER_ERR << "Device " << device->id() << " has no assigned task, deleting device.";
            continue;
        }

        std::shared_ptr<vc::model::ITask> task = this->taskById(assignedTaskId);
        if (task) {
            task->assignDevice(device->id());
            LOG_USER_INFO << "Assigned device " << device->id() << " to task " << assignedTaskId;
        }
    }

    for (const QString &deviceId : releaseDeviceIds) {
        m_deviceManager->releaseDevice(deviceId);
    }

    LOG_USER_INFO << "Project loaded from JSON successfully. Name: " << m_name << ", Version: " << m_version
                 << ", Devices: " << devices.size() << ", Tasks: " << m_tasks.size();
    // convert succeesfully
    return true;
}

// QMap<QString, QMap<QString, cv::Mat>> Project::toImageMaps() const {
//     QMap<QString, QMap<QString, cv::Mat>> maps;
//     auto task_it = m_tasks.cbegin();
//     while (task_it != m_tasks.cend()) {
//         QMap<QString, cv::Mat> mapping = task_it.value()->getTaskImageMap();
//         maps.insert(task_it.key(), mapping);
//         task_it++;
//     }
//     return maps;
// }

// bool Project::fromImageMaps(const QMap<QString, QMap<QString, cv::Mat>> &mapping) {
//     auto map_it = mapping.cbegin();
//     while (map_it != mapping.cend()) {
//         std::shared_ptr<vc::model::ITask> task = m_tasks.value(map_it.key());
//         if (task) {
//             QMap<QString, cv::Mat> image_mat = map_it.value();
//             task->loadTaskImageMap(image_mat);
//         }
//         map_it++;
//     }
//     return true;
// }

/// Moves ownership of a device from one task to another: unassigns it from
/// `fromTaskId`, assigns it to `toTaskId`, updates the device's recorded
/// assigned-task id, and emits projectModificationOccurred().
/// @param deviceId id of the device to move
/// @param fromTaskId id of the task currently holding the device
/// @param toTaskId id of the task to move the device to
bool Project::moveDeviceToTask(const QString &deviceId,
                               const QString &fromTaskId,
                               const QString &toTaskId)
{
    auto fromTask = taskById(fromTaskId);
    auto toTask   = taskById(toTaskId);
    auto device   = deviceById(deviceId);
    if (!fromTask || !toTask) return false;
    if (!fromTask->hasDevice(deviceId)) return false;
    if (toTask->hasDevice(deviceId))   return false;

    fromTask->unassignDevice(deviceId);
    toTask->assignDevice(deviceId);
    device->setAssignedTaskId(toTaskId);
    emit projectModificationOccurred();
    return true;
}

}
