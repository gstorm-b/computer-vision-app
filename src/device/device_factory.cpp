#include "device_factory.h"

#include <QJsonDocument>

namespace vc::device {

/// Internal helpers for DeviceFactory, not exposed outside this translation unit.
namespace {

/// Looks up the registry entry matching `obj`'s config key within `deviceType` and
/// invokes its creator function if the entry is available.
/// @param obj JSON object describing the device (used to match the sub-type and passed to the creator)
/// @param deviceType device family to search within
/// @param parent QObject parent passed through to the created device
/// @return the newly created IDevice, or nullptr if no matching/available entry was found
IDevice *createFromRegistryEntry(const QJsonObject &obj,
                                 DeviceType deviceType,
                                 QObject *parent)
{
    const DeviceRegistryEntry *entry = DeviceRegistry::find(obj, deviceType);
    if (!entry) {
        LOG_DEV_INFO << "DeviceFactory: unsupported device sub-type for family"
                     << DeviceTypeToString(deviceType);
        return nullptr;
    }

    if (!entry->available || !entry->creator) {
        LOG_DEV_INFO << "DeviceFactory: sub-type is unavailable"
                     << entry->subTypeValue;
        return nullptr;
    }

    return entry->creator(obj, parent);
}

} // namespace

/// Builds an IDevice from a JSON object, validating that the required top-level keys
/// (id, name, type, config) are present before dispatching to create().
IDevice *DeviceFactory::fromJson(const QJsonObject &obj, QObject *parent)
{
    if (!obj.contains(DEVICE_JSK_ID) ||
        !obj.contains(DEVICE_JSK_NAME) ||
        !obj.contains(DEVICE_JSK_TYPE) ||
        !obj.contains(DEVICE_JSK_CONFIG)) {
        LOG_DEV_INFO << "Cannot convert device from json, wrong json format.";
        LOG_DEV_INFO << QJsonDocument(obj).toJson();
        return nullptr;
    }

    return create(DeviceTypeFromString(obj[DEVICE_JSK_TYPE].toString()), obj, parent);
}

/// Dispatches device creation to the type-specific create* helper matching `type`.
IDevice *DeviceFactory::create(const DeviceType &type,
                               const QJsonObject &obj,
                               QObject *parent)
{
    switch (type) {
    case DeviceType::Camera:
        return createCamera(obj, parent);
    case DeviceType::PLC:
        return createPlcDevice(obj, parent);
    case DeviceType::VisionOutput:
        return createVisionOutputDevice(obj, parent);
    case DeviceType::Robot:
        return createRobotDevice(obj, parent);
    case DeviceType::UserType:
    default:
        return nullptr;
    }
}

/// Creates a camera device from the registry entry matching `obj`'s sub-type.
/// @param obj JSON object describing the camera device
/// @param parent QObject parent passed through to the created device
IDevice *DeviceFactory::createCamera(const QJsonObject &obj, QObject *parent)
{
    return createFromRegistryEntry(obj, DeviceType::Camera, parent);
}

/// Creates a PLC device from the registry entry matching `obj`'s sub-type.
/// @param obj JSON object describing the PLC device
/// @param parent QObject parent passed through to the created device
IDevice *DeviceFactory::createPlcDevice(const QJsonObject &obj, QObject *parent)
{
    return createFromRegistryEntry(obj, DeviceType::PLC, parent);
}

/// Creates a vision-output device from the registry entry matching `obj`'s sub-type.
/// @param obj JSON object describing the vision-output device
/// @param parent QObject parent passed through to the created device
IDevice *DeviceFactory::createVisionOutputDevice(const QJsonObject &obj,
                                                 QObject *parent)
{
    return createFromRegistryEntry(obj, DeviceType::VisionOutput, parent);
}

/// Creates a robot device from the registry entry matching `obj`'s sub-type.
/// @param obj JSON object describing the robot device
/// @param parent QObject parent passed through to the created device
IDevice *DeviceFactory::createRobotDevice(const QJsonObject &obj, QObject *parent)
{
    return createFromRegistryEntry(obj, DeviceType::Robot, parent);
}

} // namespace vc::device
