#include "device/device_registry.h"

#include "device/camera/camera_basler_gige.h"
#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_config.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/output_device/vision_tcpip_client_device.h"
#include "device/plc/mc_protocol_device.h"
#include "device/plc/plc_device.h"
#include "device/robot/kawasaki_robot_device.h"
#include "device/robot/nachi_robot_device.h"
#include "device/robot/robot_device.h"

/// Device abstraction layer: concrete device family registration and lookup (see DeviceRegistry).
namespace vc::device {

// Translation-unit-local factory functions and the static registry table they populate.
namespace {

/// Factory for a Basler GigE camera device: requires a non-empty DeviceId in `obj`.
/// @return the new BaslerGigECamera (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createBaslerGige(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new BaslerGigECamera(deviceId,
                                        obj[DEVICE_JSK_NAME].toString(),
                                        parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Mitsubishi MC-protocol PLC device: requires a non-empty DeviceId in `obj`.
/// @return the new McProtocolDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createMitsubishiMc(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new McProtocolDevice(deviceId,
                                        obj[DEVICE_JSK_NAME].toString(),
                                        parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Vision TCP/IP server output device: requires a non-empty DeviceId in `obj`.
/// @return the new VisionTcpipDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createVisionTcpip(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new VisionTcpipDevice(deviceId,
                                         obj[DEVICE_JSK_NAME].toString(),
                                         parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Vision TCP/IP client output device: requires a non-empty DeviceId in `obj`.
/// @return the new VisionTcpipClientDevice (parented to `parent`, config applied via
/// fromJson() when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createVisionTcpipClient(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new VisionTcpipClientDevice(deviceId,
                                               obj[DEVICE_JSK_NAME].toString(),
                                               parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Kawasaki robot device: requires a non-empty DeviceId in `obj`.
/// @return the new KawasakiRobotDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createKawasaki(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new KawasakiRobotDevice(deviceId,
                                           obj[DEVICE_JSK_NAME].toString(),
                                           parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Nachi robot device: requires a non-empty DeviceId in `obj`.
/// @return the new NachiRobotDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createNachi(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new NachiRobotDevice(deviceId,
                                        obj[DEVICE_JSK_NAME].toString(),
                                        parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Static table of every registered device sub-type, in lookup/listing order; backs
/// DeviceRegistry::entries()/find()/displayNamesFor().
const QList<DeviceRegistryEntry> kEntries = {
    { DeviceType::Camera,
      CameraTypeToString(CameraType::BaslerGigE),
      QStringLiteral("Basler GigE"),
      QStringLiteral(DEVICE_JSK_CAM_TYPE),
      createBaslerGige,
      true },
    { DeviceType::PLC,
      PlcTypeToString(PlcType::MitsubishiMc),
      QStringLiteral("Mitsubishi MC"),
      QStringLiteral(DEVICE_JSK_PLC_TYPE),
      createMitsubishiMc,
      true },
    { DeviceType::VisionOutput,
      VisionOutputTypeToString(VisionOutputType::VisionTCPIP),
      QStringLiteral("Vision TCP/IP Server"),
      QStringLiteral(DEVICE_JSK_VOUT_TYPE),
      createVisionTcpip,
      true },
    { DeviceType::VisionOutput,
      VisionOutputTypeToString(VisionOutputType::VisionTcpipClient),
      QStringLiteral("Vision TCP/IP Client"),
      QStringLiteral(DEVICE_JSK_VOUT_TYPE),
      createVisionTcpipClient,
      true },
    { DeviceType::Robot,
      RobotTypeToString(RobotType::Kawasaki),
      QStringLiteral("Kawasaki"),
      QStringLiteral(DEVICE_JSK_ROBOT_TYPE),
      createKawasaki,
      true },
    { DeviceType::Robot,
      RobotTypeToString(RobotType::Nachi),
      QStringLiteral("Nachi"),
      QStringLiteral(DEVICE_JSK_ROBOT_TYPE),
      createNachi,
      true }
};

/// Reads the sub-type token identified by `configJsonKey` from `obj`, checking the
/// top-level key first and falling back to the nested DeviceConfig object.
/// @return the token value, or an empty string if configJsonKey is empty or not found
QString subTypeValueFrom(const QJsonObject &obj, const QString &configJsonKey)
{
    if (configJsonKey.isEmpty()) {
        return QString();
    }

    if (obj.contains(configJsonKey)) {
        return obj[configJsonKey].toString();
    }

    const QJsonObject cfg = obj.value(DEVICE_JSK_CONFIG).toObject();
    return cfg.value(configJsonKey).toString();
}

} // namespace

const QList<DeviceRegistryEntry> &DeviceRegistry::entries()
{
    return kEntries;
}

const DeviceRegistryEntry *DeviceRegistry::find(DeviceType deviceType,
                                                const QString &subTypeValue)
{
    for (const DeviceRegistryEntry &entry : kEntries) {
        if (entry.deviceType == deviceType && entry.subTypeValue == subTypeValue) {
            return &entry;
        }
    }
    return nullptr;
}

const DeviceRegistryEntry *DeviceRegistry::find(const QJsonObject &obj,
                                                DeviceType deviceType)
{
    for (const DeviceRegistryEntry &entry : kEntries) {
        if (entry.deviceType != deviceType) {
            continue;
        }

        if (subTypeValueFrom(obj, entry.configJsonKey) == entry.subTypeValue) {
            return &entry;
        }
    }
    return nullptr;
}

QStringList DeviceRegistry::displayNamesFor(DeviceType deviceType,
                                            bool includeUnavailable)
{
    QStringList names;
    for (const DeviceRegistryEntry &entry : kEntries) {
        if (entry.deviceType != deviceType) {
            continue;
        }
        if (!includeUnavailable && !entry.available) {
            continue;
        }
        names.append(entry.subTypeValue);
    }
    return names;
}

} // namespace vc::device
