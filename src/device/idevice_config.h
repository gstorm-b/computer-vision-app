#ifndef IDEVICE_CONFIG_H
#define IDEVICE_CONFIG_H

#include <QJsonObject>

/// Human-readable DeviceType names, used by DeviceTypeToString()/DeviceTypeFromString() for
/// the DEVICE_JSK_TYPE field.
#define DEVICE_TYPE_NAME_CAMERA          "Camera"
#define DEVICE_TYPE_NAME_PLC             "PLC"
#define DEVICE_TYPE_NAME_TCP_IP_DV       "TCP/IP Device"
#define DEVICE_TYPE_NAME_VISION_OUTPUT   "Vision Output Device"
#define DEVICE_TYPE_NAME_ROBOT           "Robot"

/// JSON keys used by IDevice::toJson()/fromJson() and camera/calibrator configs.
#define DEVICE_JSK_ID               "DeviceId"
#define DEVICE_JSK_NAME             "DeviceName"
#define DEVICE_JSK_TASKID           "AssignedTaskId"
#define DEVICE_JSK_TYPE             "DeviceType"
#define DEVICE_JSK_CONFIG           "DeviceConfig"
#define DEVICE_JSK_CAM_TYPE             "CameraType"
#define DEVICE_JSK_CALIBRATOR           "CameraCalibrator"
#define DEVICE_JSK_CALIB_BOARD_PRESET   "CalibBoardPreset"
#define DEVICE_JSK_CALIB_THRESHOLD      "CalibBinarizeThreshold"

/// JSON key identifying the robot sub-type (e.g. Kawasaki/Nachi) inside a robot device's config.
#define DEVICE_JSK_ROBOT_TYPE           "RobotType"

/// JSON keys for the Mitsubishi MC-protocol PLC device's sub-type/frame/context config.
#define DEVICE_JSK_PLC_TYPE         "PlcType"
#define DEVICE_JSK_MC_FRAME         "McFrameType"
#define DEVICE_JSK_MC_CONTEXT       "McContext"

/// JSON keys for the vision-output device family's connection/timing config.
#define DEVICE_JSK_VOUT_TYPE               "VisionOutputType"
#define DEVICE_JSK_VOUT_LISTEN_ADDR        "ListenAddress"
#define DEVICE_JSK_VOUT_SERVER_ADDR        "ServerAddress"
#define DEVICE_JSK_VOUT_MAIN_PORT          "MainPort"
#define DEVICE_JSK_VOUT_HB_PORT            "HeartbeatPort"
#define DEVICE_JSK_VOUT_HB_INTERVAL        "HeartbeatIntervalMs"
#define DEVICE_JSK_VOUT_HB_TIMEOUT         "HeartbeatTimeoutMs"
#define DEVICE_JSK_VOUT_RECONNECT          "ReconnectIntervalMs"
#define DEVICE_JSK_VOUT_KCHECK             "RobotKinematicCheck"
// #define DEVICE_JSK_PLC_BRAND        "PlcBrand"
// #define DEVICE_JSK_PLC_TYPE         "PlcType"

/// Device abstraction layer: device-family/type identification and the IDeviceCfg contract
/// shared by every device's configuration object.
namespace vc::device {

/// Top-level device family identifying which IDevice/IDeviceCfg subclass hierarchy applies.
enum DeviceType {
    UserType,
    Camera,
    PLC,
    VisionOutput,
    Robot
};

/// Converts a DeviceType to its DEVICE_JSK_TYPE JSON string (DEVICE_TYPE_NAME_* tokens).
/// @return the matching name string, or an empty string for UserType/unrecognized values
[[maybe_unused]] static QString DeviceTypeToString(DeviceType t) {
    switch(t) {
    case vc::device::DeviceType::UserType:
        return "";
    case vc::device::DeviceType::Camera:
        return DEVICE_TYPE_NAME_CAMERA;
    case vc::device::DeviceType::PLC:
        return DEVICE_TYPE_NAME_PLC;
    case vc::device::DeviceType::VisionOutput:
        return DEVICE_TYPE_NAME_VISION_OUTPUT;
    case vc::device::DeviceType::Robot:
        return DEVICE_TYPE_NAME_ROBOT;
    default:
        return "";
    }
    return "";
}

/// Parses a DeviceType back from its DEVICE_JSK_TYPE JSON string.
/// @return the matching DeviceType, or UserType if `t` matches none of the known names
[[maybe_unused]] static DeviceType DeviceTypeFromString(QString t) {
    if (t == DEVICE_TYPE_NAME_CAMERA) {
        return DeviceType::Camera;
    } else if (t == DEVICE_TYPE_NAME_PLC) {
        return DeviceType::PLC;
    } else if (t == DEVICE_TYPE_NAME_VISION_OUTPUT) {
        return DeviceType::VisionOutput;
    } else if (t == DEVICE_TYPE_NAME_ROBOT) {
        return DeviceType::Robot;
    }
    return UserType;
}

/// Abstract configuration contract shared by every device family: JSON (de)serialization,
/// its owning DeviceType, Qt meta-object access (for property-browser reflection), and cloning.
class IDeviceCfg {
public:
    /// Default virtual destructor; concrete configs own no extra resources here.
    virtual ~IDeviceCfg() = default;

    /// Returns the DeviceType of the device family this config belongs to.
    virtual DeviceType deviceType() const = 0;

    /// Serializes this config's parameters to JSON for saving.
    virtual QJsonObject toJson() const = 0;

    /// Returns the Q_GADGET/Q_OBJECT meta-object for this config, used for property-browser
    /// reflection of its parameters.
    virtual const QMetaObject &getMetaObject() const = 0;

    /// Restores this config's parameters from `obj`.
    /// @param obj JSON object produced by toJson()
    virtual bool fromJson(const QJsonObject &obj) = 0;

    /// Allocates and returns a heap copy of this config (caller owns the returned instance).
    virtual IDeviceCfg* clone() = 0;
};

}



#endif // IDEVICE_CONFIG_H
