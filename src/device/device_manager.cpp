#include "device_manager.h"
#include "core/logger/app_logger.h"

#include "device/camera/camera_device.h"
#include "device/device_registry.h"
#include "device/plc/mc_protocol_device.h"
#include "device/output_device/vision_output_device.h"
#include "device/robot/robot_device.h"

#include <QThread>

#define CAMERA_NUM_LIMIT         4
#define PLC_NUM_LIMIT            2
#define MC_PTC_NUM_LIMIT         4
#define TCPIP_DV_NUM_LIMIT       4
#define VISION_OUTPUT_NUM_LIMIT  8
#define ROBOT_NUM_LIMIT          1

/// Device management: owns the live IDevice instances for a Project, allocates and
/// tracks their string IDs/names, enforces per-type creation limits, and re-emits
/// per-device signals (created/modified/deleted) for the UI.
namespace vc::device {

/// Constructs the manager for `proj`, seeding the per-type creation limits
/// (camera/PLC/vision-output/robot) and sub-device display-name lists from
/// DeviceRegistry, and populating the device-type string list used by the UI.
DeviceManager::DeviceManager(vc::model::Project* proj, QObject* parent)
    : QObject(parent), currentMaxId(1), parent_proj(proj) {

    // Register device type to map
    // Initialize counter
    // Add Device String type

    maxLimits[DeviceType::Camera] = CAMERA_NUM_LIMIT;
    currentCounts[DeviceType::Camera] = 0;

    maxLimits[DeviceType::PLC] = MC_PTC_NUM_LIMIT;
    currentCounts[DeviceType::PLC] = 0;

    maxLimits[DeviceType::VisionOutput] = VISION_OUTPUT_NUM_LIMIT;
    currentCounts[DeviceType::VisionOutput] = 0;

    // maxLimits[DeviceType::TCPIP_DEVICE] = TCPIP_DV_NUM_LIMIT;
    // currentCounts[DeviceType::TCPIP_DEVICE] = 0;

    maxLimits[DeviceType::Robot] = ROBOT_NUM_LIMIT;
    currentCounts[DeviceType::Robot] = 0;

    subDeviceTypeLists.insert(DeviceType::Camera,
                              DeviceRegistry::displayNamesFor(DeviceType::Camera));
    subDeviceTypeLists.insert(DeviceType::PLC,
                              DeviceRegistry::displayNamesFor(DeviceType::PLC));
    subDeviceTypeLists.insert(DeviceType::Robot,
                              DeviceRegistry::displayNamesFor(DeviceType::Robot));
    subDeviceTypeLists.insert(DeviceType::VisionOutput,
                              DeviceRegistry::displayNamesFor(DeviceType::VisionOutput));

    QMap<DeviceType, int>::const_iterator cnt_it = currentCounts.constBegin();
    while (cnt_it != currentCounts.cend()) {
        DeviceType device_t = cnt_it.key();
        dvTypeStrings.append(DeviceTypeToString(device_t));
        cnt_it++;
    }
}

/// Terminates every managed device (marshalling the call onto the device's own thread
/// via a blocking queued invoke when it differs from the calling thread) before
/// clearing the instance map.
DeviceManager::~DeviceManager() {
    if (deviceInstances.isEmpty()) return;

    for (std::shared_ptr<vc::device::IDevice> device : deviceInstances) {
        if (QThread::currentThread() != device->thread()) {
            QMetaObject::invokeMethod(device.get(), "deviceTerminate", Qt::BlockingQueuedConnection);
        } else {
            device->deviceTerminate();
        }
    }

    LOG_DEV_DEBUG << "Destroy all device instances";
    deviceInstances.clear();
}

/// Returns the owning Project, or nullptr if this manager was constructed without one.
vc::model::Project* DeviceManager::project() {
    return parent_proj;
}

/// Checks whether the number of currently created devices of `type` has reached the
/// configured maximum for that type.
bool DeviceManager::isLimitReached(DeviceType type) const {
    return currentCounts.value(type, 0) >= maxLimits.value(type, 0);
}

/// Registers an already-constructed `device` under `id`/`name` (used when loading a
/// project, where the id/name are already known), validating the id format
/// (numeric, 1-9999) and that both the id and name are not already in use.
/// @param id numeric device id as a string; must parse to an integer in [1, 9999]
/// @param name display name; must not already be occupied by another device
/// @param device the device instance to take ownership of (via shared_ptr)
/// @return true on success; false if the id is malformed or the id/name is a duplicate
/// @note advances currentMaxId/freedIds bookkeeping so subsequent generateId() calls
///       stay consistent with manually-reserved ids, and wires up connectConfigChanged().
bool DeviceManager::reserveDevice(const QString &id, const QString &name, std::shared_ptr<IDevice> device) {
    bool ok;
    int idNum = id.toInt(&ok);

    // check input id string format
    if (!ok || idNum <= 0 || idNum > 9999) {
        LOG_DEV_DEBUG << tr("Device factory warning: ID is not be correct format to reserve -").arg(id);
        return false;
    }

    QMutexLocker locker(&mutex);

    // check input id availibility
    // if (deviceMap.contains(id)) {
    if (deviceInstances.contains(id)) {
        LOG_DEV_DEBUG << tr("Device factory warning: Duplicated! ID already existed in system -").arg(id);
        return false;
    }

    if (occupiedNames.contains(name)) {
        LOG_DEV_DEBUG << tr("Device factory warning: Duplicated! Name already existed in system - %1").arg(name);
        return false;
    }

    // mark occupied id
    // deviceMap.insert(id, name);
    deviceInstances.insert(id, device);
    occupiedNames.insert(name);
    device->setManager(this);

    if (idNum >= currentMaxId) {
        // if currentMaxId id 1, and idNum is 4 -> 1, 2, 3 will be inserted into freedIds
        for (int i = currentMaxId; i < idNum; ++i) {
            freedIds.insert(i);
        }
        currentMaxId = idNum + 1;
    } else {
        // if idNum samller than currentMaxid, it obviously contains in freedIds
        // just remove it
        freedIds.erase(idNum);
    }

    connectConfigChanged(device);

    emit devicesChanged();
    emit deviceCreated(id);
    return true;
}

/// Forwards `device`'s configChanged signal to this manager's deviceModified(id).
/// Called by both creation paths (reserveDevice on project load, commitDevice via the
/// Add-Device wizard) so config edits mark the project modified consistently.
/// @note no-op if `device` is null.
void DeviceManager::connectConfigChanged(const std::shared_ptr<IDevice> &device) {
    if (!device) {
        return;
    }
    // Capture the device id by value, NOT the shared_ptr, so the connection does
    // not keep the device alive past releaseDevice(); it auto-disconnects when
    // the device (the sender) is destroyed.
    const QString deviceId = device->id();
    connect(device.get(), &vc::device::IDevice::configChanged, this, [this, deviceId]() {
        emit this->deviceModified(deviceId);
    });
}

/// Removes the device registered under `id`: frees its occupied name, decrements the
/// per-type count, and returns its numeric id to the freedIds pool for reuse. Emits
/// devicesChanged() (always, if `id` is occupied) and deviceDeleted(id) (unless `id`
/// fails to parse as a number).
/// @note no-op if `id` is not currently occupied, or if the stored device pointer is null.
void DeviceManager::releaseDevice(const QString &id) {
    // lock mutex before access to set
    QMutexLocker locker(&mutex);

    // check occupied id
    if (!deviceInstances.contains(id)) {
        return;
    }

    if (!deviceInstances[id]) {
        LOG_DEV_ERR << "Cannot release device because device pointer is null";
        return;
    }

    DeviceType type = deviceInstances[id]->deviceType();

    std::shared_ptr<IDevice> dv = deviceInstances.value(id);
    // QString nameToRemove = deviceMap.value(id);
    QString nameToRemove = dv->name();
    if (!nameToRemove.isEmpty()) {
        occupiedNames.remove(nameToRemove);
    }

    // deviceMap.remove(id);
    deviceInstances.remove(id);

    if (currentCounts[type] > 0) {
        currentCounts[type]--;
    }

    bool ok;
    int idNum = id.toInt(&ok);

    if (!ok) {
        LOG_DEV_DEBUG << tr("Device ID is wrong format: %1").arg(id);
        emit devicesChanged();
        return;
    }

    if (idNum < currentMaxId) {
        freedIds.insert(idNum);
    }

    emit devicesChanged();
    emit deviceDeleted(id);
}

/// Cancels a previously-allocated pending id (one reserved via allocatePendingId()
/// but never committed via commitDevice()): removes the placeholder entry from
/// deviceInstances and returns its numeric value to the freedIds pool.
/// @note no-op if `id` is not currently occupied.
void DeviceManager::releasePendingId(const QString &id) {
    QMutexLocker locker(&mutex);

    if (!deviceInstances.contains(id)) {
        return;
    }

    deviceInstances.remove(id);

    bool ok;
    int idNum = id.toInt(&ok);

    if (!ok) {
        LOG_DEV_DEBUG << tr("Device ID is wrong format: %1").arg(id);
        return;
    }

    if (idNum < currentMaxId) {
        freedIds.insert(idNum);
    }
}

/// Allocates a fresh device id and reserves it immediately with a null device pointer
/// (a "Pending" placeholder in deviceInstances) so the id cannot be handed out again
/// before the caller finishes constructing the actual device via commitDevice().
/// @return the newly generated id, or an empty string if generateId() could not
///         produce one (id space exhausted).
QString DeviceManager::allocatePendingId() {
    QMutexLocker locker(&mutex);

    QString newId = generateId();
    if (!newId.isEmpty()) {
        // Push ID into map with blank name for Pending status
        // deviceMap.insert(newId, "");
        deviceInstances.insert(newId, nullptr);
    }
    return newId;
}

/// Finalizes a previously-allocated pending `id` (see allocatePendingId()) by
/// attaching the constructed `device` to it under `name`: validates the per-type
/// limit, that `id` is a pending (null) entry, and that `name` is not already taken,
/// then stores the device, bumps the per-type count, and wires up connectConfigChanged().
/// @param id a pending id previously returned by allocatePendingId()
/// @param name display name; must not already be occupied by another device
/// @param device the constructed device instance to attach
/// @return true on success; false if `device` is null, the type limit is reached,
///         `id` is not a pending entry, or `name` is already taken
bool DeviceManager::commitDevice(const QString &id, const QString &name, std::shared_ptr<IDevice> device) {
    if (!device) {
        LOG_DEV_ERR << "Commit device failed, input abstract device is null";
        return false;
    }

    DeviceType type = device->deviceType();

    if (currentCounts[type] >= maxLimits[type]) {
        lastMsg = tr("Cannot create more %1 device, limit reach").arg(DeviceTypeToString(type));
        LOG_USER_ERR << lastMsg;
        return false;
    }

    QMutexLocker locker(&mutex);
    // ID pending check
    // if (!deviceMap.contains(id)) return false;
    if (!deviceInstances.contains(id)) {
        return false;
    }
    if (deviceInstances[id] != nullptr) {
        return false;
    }
    // Name duplicated check
    if (occupiedNames.contains(name)) {
        return false;
    }
    // update map and name set
    occupiedNames.insert(name);
    // deviceMap[id] = name;
    deviceInstances.insert(id, device);
    // increase device typecounter
    currentCounts[type]++;
    device->setManager(this);
    connectConfigChanged(device);

    emit devicesChanged();
    emit deviceCreated(id);
    return true;
}

/// Looks up the name of the device registered under `id`.
/// @return the device's name
/// @note as written, the ternary condition looks inverted: when `id` resolves to a
///       null shared_ptr this dereferences it via dv->name() (undefined behavior),
///       and when the device is actually found it returns an empty string instead.
QString DeviceManager::getDeviceName(const QString &id) {
    QMutexLocker locker(&mutex);
    // return deviceMap.value(id, QString());
    std::shared_ptr<IDevice> dv = deviceInstances.value(id, nullptr);
    return (!dv) ? dv->name() : "";
}

/// Checks whether `name` is already occupied by a registered device.
bool DeviceManager::isNameExists(const QString &name) {
    QMutexLocker locker(&mutex);
    return occupiedNames.contains(name);
}

/// Checks whether `id` currently has an entry in deviceInstances (either a live
/// device or a pending/null placeholder).
bool DeviceManager::isOccupied(const QString &id) {
    QMutexLocker locker(&mutex);
    // return deviceMap.contains(id);
    return deviceInstances.contains(id);
}

/// Renames the device registered under `id` to `new_name`, updating the
/// occupiedNames set accordingly and emitting deviceModified(id).
/// @return true if renamed (or already named `new_name`); false if `id` has no
///         device or `new_name` is already occupied by another device
/// @note unlike most other members here, this does not lock `mutex`.
bool DeviceManager::changeDeviceName(const QString &id, const QString new_name) {
    IDevice *device = deviceInstances[id].get();
    if (!device) {
        return false;
    }

    if (device->name() == new_name) {
        return true;
    }

    if (this->isNameExists(new_name)) {
        return false;
    }

    occupiedNames.remove(device->name());
    device->setName(new_name);
    occupiedNames.insert(new_name);
    emit deviceModified(id);
    return true;
}

/// Collects all registered PLC-type devices into an id-to-name map.
/// @return map of device id -> device name for every device with DeviceType::PLC
QMap<QString, QString> DeviceManager::commDevices() {
    QMap<QString, QString> devices_map;

    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::PLC) {
            devices_map.insert(it_device.value()->id(), it_device.value()->name());
        }
        it_device++;
    }

    return devices_map;
}

/// Collects all registered vision-output devices into an id-to-name map.
/// @return map of device id -> device name for every device with DeviceType::VisionOutput
QMap<QString, QString> DeviceManager::outputDevices() {
    QMap<QString, QString> devices_map;

    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::VisionOutput) {
            devices_map.insert(it_device.value()->id(), it_device.value()->name());
        }
        it_device++;
    }

    return devices_map;
}

/// Collects all registered camera devices into an id-to-name map.
/// @return map of device id -> device name for every device with DeviceType::Camera
QMap<QString, QString> DeviceManager::cameraDevices() {
    QMap<QString, QString> devices_map;

    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::Camera) {
            devices_map.insert(it_device.value()->id(), it_device.value()->name());
        }
        it_device++;
    }

    return devices_map;
}


/// Lists the display names of all registered PLC-type devices.
/// @return one entry per PLC device
/// @note the format string "%1" has only one placeholder, so only the device name is
///       substituted; the id argument passed to arg() is not actually inserted.
QStringList DeviceManager::commDevicesNameList() {
    QStringList devices_name;
    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::PLC) {
            devices_name << QString("%1").arg(it_device.value()->name(), it_device.value()->id());
        }
        it_device++;
    }
    return devices_name;
}

/// Lists the display names of all registered vision-output devices.
/// @return one entry per vision-output device
/// @note the format string "%1" has only one placeholder, so only the device name is
///       substituted; the id argument passed to arg() is not actually inserted.
QStringList DeviceManager::outputDevicesNameList() {
    QStringList devices_name;
    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::VisionOutput) {
            devices_name << QString("%1").arg(it_device.value()->name(), it_device.value()->id());
        }
        it_device++;
    }
    return devices_name;
}

/// Lists the display names of all registered camera devices.
/// @return one entry per camera device
/// @note the format string "%1" has only one placeholder, so only the device name is
///       substituted; the id argument passed to arg() is not actually inserted.
QStringList DeviceManager::cameraDevicesNameList() {
    QStringList devices_name;
    QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_device = deviceInstances.cbegin();
    while (it_device != deviceInstances.cend()) {
        if (it_device.value()->deviceType() == DeviceType::Camera) {
            devices_name << QString("%1").arg(it_device.value()->name(), it_device.value()->id());
        }
        it_device++;
    }
    return devices_name;
}

/// Looks up the device registered under `id`.
/// @return the device, or nullptr if `id` has no entry (or is a pending placeholder)
std::shared_ptr<IDevice> DeviceManager::deviceById(const QString &id) {
    return deviceInstances.value(id, nullptr);
    // QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_start = deviceInstances.cbegin();
    // QMap<QString, std::shared_ptr<vc::device::IDevice>>::const_iterator it_end = deviceInstances.cend();
    // while (it_start != it_end) {
    //     if (it_start.key() == id) {
    //         return it_start.value();
    //     }
    //     it_start++;
    // }
    // return std::shared_ptr<vc::device::IDevice>(nullptr);
}

/// Returns the reference to the list of top-level device type display strings
/// (one per registered DeviceType), built once in the constructor.
QStringList& DeviceManager::getDeviceTypeList() {
    return dvTypeStrings;
}

/// Returns the reference to the sub-device display-name list for `type`, as
/// populated from DeviceRegistry::displayNamesFor() in the constructor.
QStringList& DeviceManager::getSubDeviceTypeList(DeviceType type) {
    return subDeviceTypeLists[type];
}

/// Allocates the next available device id: reuses the smallest freed id if one is
/// pooled in freedIds, otherwise takes currentMaxId and advances it.
/// @return a 2+-digit zero-padded numeric id string, or an empty string once the id
///         space is exhausted (nextIdNum > 9999)
QString DeviceManager::generateId() {
    // QMutexLocker locker(&mutex);
    int nextIdNum = -1;
    if (!freedIds.empty()) {
        // retrieve ID from pool
        auto it = freedIds.begin();
        nextIdNum = *it;
        freedIds.erase(it);
    } else {
        // allocate new ID
        nextIdNum = currentMaxId;
        currentMaxId++;
    }

    if (nextIdNum > 9999) {
        LOG_DEV_DEBUG << tr("Device factory warning: Device ID already reached max number 9999");
        return QString();
    }

    // occupiedIds.insert(nextIdNum);
    return QString("%1").arg(nextIdNum, 2, 10, QChar('0'));
}

} // namespace vc::device
