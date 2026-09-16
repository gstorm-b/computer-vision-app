#include "device/device_registry.h"

#include "device/camera/camera_basler_gige.h"
#include "device/camera/camera_device.h"
#include "device/camera/camera_jai_gige.h"
#include "device/output_device/vision_output_config.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/output_device/vision_tcpip_client_device.h"
#include "device/plc/mc_protocol_device.h"
#include "device/plc/modbus/modbus_tcp_client_device.h"
#include "device/plc/modbus/modbus_tcp_server_device.h"
#include "device/plc/plc_device.h"
#include "device/robot/kawasaki_robot_device.h"
#include "device/robot/nachi_robot_device.h"
#include "device/robot/robot_device.h"
#include "device/virtual/virtual_camera_device.h"
#include "device/virtual/virtual_plc_device.h"
#include "device/virtual/virtual_vision_output_device.h"

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

/// Factory for a JAI GigE camera device: requires a non-empty DeviceId in `obj`.
/// @return the new JaiGigECamera (parented to `parent`, config applied via fromJson() when a
/// DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createJaiGige(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new JaiGigECamera(deviceId,
                                     obj[DEVICE_JSK_NAME].toString(),
                                     parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a hardware-free virtual camera: requires a non-empty DeviceId in `obj`.
/// @return the new VirtualCameraDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createVirtualCamera(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new VirtualCameraDevice(deviceId,
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

/// Factory for a Modbus TCP client PLC device: requires a non-empty DeviceId in `obj`.
/// @return the new ModbusTcpClientDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createModbusTcpClient(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new ModbusTcpClientDevice(deviceId,
                                             obj[DEVICE_JSK_NAME].toString(),
                                             parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a Modbus TCP server PLC device: requires a non-empty DeviceId in `obj`.
/// @return the new ModbusTcpServerDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createModbusTcpServer(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new ModbusTcpServerDevice(deviceId,
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

/// Factory for a hardware-free virtual PLC: requires a non-empty DeviceId in `obj`.
/// @return the new VirtualPlcDevice (parented to `parent`, config applied via fromJson()
/// when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createVirtualPlc(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new VirtualPlcDevice(deviceId,
                                        obj[DEVICE_JSK_NAME].toString(),
                                        parent);
    if (obj.contains(DEVICE_JSK_CONFIG) && obj[DEVICE_JSK_CONFIG].isObject()) {
        device->fromJson(obj);
    }
    return device;
}

/// Factory for a hardware-free virtual vision output: requires a non-empty DeviceId in `obj`.
/// @return the new VirtualVisionOutputDevice (parented to `parent`, config applied via
/// fromJson() when a DeviceConfig object is present), or nullptr if DeviceId is missing
IDevice *createVirtualVisionOutput(const QJsonObject &obj, QObject *parent)
{
    const QString deviceId = obj[DEVICE_JSK_ID].toString();
    if (deviceId.isEmpty()) {
        return nullptr;
    }

    auto *device = new VirtualVisionOutputDevice(deviceId,
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
    { DeviceType::Camera,
      CameraTypeToString(CameraType::JaiGigE),
      QStringLiteral("JAI GigE"),
      QStringLiteral(DEVICE_JSK_CAM_TYPE),
      createJaiGige,
      true },
    // Virtual entries go LAST within their family, and that ordering is load-bearing.
    // displayNamesFor() preserves this order, the Add Device wizard's combo leaves index 0
    // current, and buildDeviceJson() writes back currentText() — so the first entry of a
    // family is what an operator creates when they never touch the combo. Put Virtual first
    // and the default camera silently becomes a simulated one.
    { DeviceType::Camera,
      CameraTypeToString(CameraType::VirtualCamera),
      QStringLiteral("Virtual Camera"),
      QStringLiteral(DEVICE_JSK_CAM_TYPE),
      createVirtualCamera,
      true },
    { DeviceType::PLC,
      PlcTypeToString(PlcType::MitsubishiMc),
      QStringLiteral("Mitsubishi MC"),
      QStringLiteral(DEVICE_JSK_PLC_TYPE),
      createMitsubishiMc,
      true },
    { DeviceType::PLC,
      PlcTypeToString(PlcType::ModbusTcpClient),
      QStringLiteral("Modbus TCP Client"),
      QStringLiteral(DEVICE_JSK_PLC_TYPE),
      createModbusTcpClient,
      true },
    { DeviceType::PLC,
      PlcTypeToString(PlcType::ModbusTcpServer),
      QStringLiteral("Modbus TCP Server"),
      QStringLiteral(DEVICE_JSK_PLC_TYPE),
      createModbusTcpServer,
      true },
    // Virtual stays last within the family: the ordering is load-bearing, not cosmetic.
    { DeviceType::PLC,
      PlcTypeToString(PlcType::VirtualPlc),
      QStringLiteral("Virtual PLC"),
      QStringLiteral(DEVICE_JSK_PLC_TYPE),
      createVirtualPlc,
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
    { DeviceType::VisionOutput,
      VisionOutputTypeToString(VisionOutputType::VirtualVisionOutput),
      QStringLiteral("Virtual Vision Output"),
      QStringLiteral(DEVICE_JSK_VOUT_TYPE),
      createVirtualVisionOutput,
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
