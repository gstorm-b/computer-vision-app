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

namespace vc::device {

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

vc::model::Project* DeviceManager::project() {
    return parent_proj;
}

bool DeviceManager::isLimitReached(DeviceType type) const {
    return currentCounts.value(type, 0) >= maxLimits.value(type, 0);
}

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

QString DeviceManager::getDeviceName(const QString &id) {
    QMutexLocker locker(&mutex);
    // return deviceMap.value(id, QString());
    std::shared_ptr<IDevice> dv = deviceInstances.value(id, nullptr);
    return (!dv) ? dv->name() : "";
}

bool DeviceManager::isNameExists(const QString &name) {
    QMutexLocker locker(&mutex);
    return occupiedNames.contains(name);
}

bool DeviceManager::isOccupied(const QString &id) {
    QMutexLocker locker(&mutex);
    // return deviceMap.contains(id);
    return deviceInstances.contains(id);
}

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

QStringList& DeviceManager::getDeviceTypeList() {
    return dvTypeStrings;
}

QStringList& DeviceManager::getSubDeviceTypeList(DeviceType type) {
    return subDeviceTypeLists[type];
}

// helper function
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
