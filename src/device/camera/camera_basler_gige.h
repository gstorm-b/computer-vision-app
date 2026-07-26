#ifndef CAMERA_BASLER_GIGE_H
#define CAMERA_BASLER_GIGE_H

// #include <pylon/PylonIncludes.h>
// #include <pylon/_BaslerUniversalCameraParams.h>

#include "device/idevice.h"
#include "device/camera/camera_device.h"
#include "device/camera/basler_define.h"

#include "core/qgadget_macro.h"

using namespace vc::device::basler;

/// Basler-specific camera device: config (BaslerGigeCfg) and CameraDevice implementation
/// (BaslerGigECamera) built on the Pylon SDK for GigE Vision cameras.
namespace vc::device {

/// Basler GigE camera configuration: exposure/gain/frame-rate/backlight parameters plus
/// camera identity (model/serial/IP), exposed as Q_GADGET properties for the property-browser
/// UI (each G_PROPERTY_* macro below declares a property with generated getter/setter backed
/// by the matching m_* field declared further down).
class BaslerGigeCfg : public CameraCfg {
    Q_GADGET

    G_PROPERTY_STRING_READ(QString, modelName, "Model name")                                     ///< Camera model name (read-only, from device info).
    G_PROPERTY_STRING_READ(QString, userDefinedName, "User-defined name")                         ///< User-defined camera name (read-only, from device info).
    G_PROPERTY_STRING_READ(QString, serialNumber, "Serial number")                                ///< Camera serial number (read-only, from device info).
    G_PROPERTY_STRING_READWRITE(QString, ipAddress, "IP Address")                                 ///< IP address used to locate the camera on connect.

    G_PROPERTY_ENUM_READWRITE(vc::device::basler::BaslerExposureMode, autoExposureMode, "Exposure Mode")  ///< Auto-exposure mode (Off/Once/Continuous).
    G_PROPERTY_NUMBER_READWRITE(double, paramsExposureTime, 0.0, 10000.0, "Exposure time")         ///< Configured exposure time.

    G_PROPERTY_NUMBER_READWRITE(double, paramsGain, 0.0, 10000.0, "Gain")                          ///< Configured gain.

    G_PROPERTY_BOOL_READWRITE(bool, enableAcquisitionFrameRate, "Enable acquisition rate")         ///< Enables acquisition frame-rate limiting.
    G_PROPERTY_NUMBER_READWRITE(double, paramsAcquisitionFrameRate, 1.0, 100.0, "Acquisition rate") ///< Configured acquisition frame rate.

    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightControl, "Enable auto backlight")                 ///< Enables automatic backlight output around a grab.
    G_PROPERTY_STRING_READWRITE(QString, autoBacklightLine, "Backlight line")                      ///< Digital output line driven for backlight control.
    G_PROPERTY_NUMBER_READWRITE(int, autoBacklightDelay, 0, 10000000, "Back light delay (us)")      ///< Delay (microseconds) after enabling backlight before grabbing.
    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightInvert, "Line invert")                            ///< Inverts the backlight output line's logic level.

public:
    /// Constructs a default-initialized config (see field initializers below for defaults).
    explicit BaslerGigeCfg()
        : CameraCfg() {}

    /// Returns this class's Qt meta-object, used by the gadget property-browser machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::BaslerGigeCfg::staticMetaObject;
    }

    /// Always DeviceType::Camera.
    DeviceType deviceType() const override {
        return DeviceType::Camera;
    }

    /// Always CameraType::BaslerGigE.
    CameraType cameraType() const override {
        return CameraType::BaslerGigE;
    }

    /// Enables/disables the auto-backlight control feature.
    void enableBlackLightControl(bool ena) override {
        m_autoBacklightControl = ena;
    }

    /// Returns the configured exposure-time range via the output parameters.
    void exposureTimeLimit(double &min, double &max) override {
        min = m_paramsExposureMin;
        max = m_paramsExposureMax;
    }

    /// Sets the allowed exposure-time range.
    void setExposureTimeLimit(double min, double max) override {
        m_paramsExposureMin = min;
        m_paramsExposureMax = max;
    }

    /// Sets the exposure time; silently ignored if `value` is outside [min, max].
    void setExposureTime(double value) override {
        if ((value >= m_paramsExposureMin) && (value <= m_paramsExposureMax)) {
            m_paramsExposureTime = value;
        }
    }

    /// Returns the currently configured exposure time.
    double exposureTime() override {
        return m_paramsExposureTime;
    }

    /// Sets the auto-exposure mode directly from the enum value.
    void setAutoExposure(vc::device::basler::BaslerExposureMode mode) {
        m_autoExposureMode = mode;
    }

    /// Sets the auto-exposure mode by parsing its string name (via
    /// BaslerExposureTypeFromString()).
    void setAutoExposure(const QString &mode) {
        m_autoExposureMode = BaslerExposureTypeFromString(mode);
    }

    /// Returns the current auto-exposure mode as its string name.
    QString autoExposure() {
        return BaslerExposureTypeToString(m_autoExposureMode);
    }

    /// Returns the configured gain range via the output parameters.
    void gainLimit(int &min, int &max) override {
        min = m_paramsGainMin;
        max = m_paramsGainMax;
    }

    /// Sets the allowed gain range.
    void setGainLimit(int min, int max) override {
        m_paramsGainMin = min;
        m_paramsGainMax = max;
    }

    /// Sets the gain; silently ignored if `value` is outside [min, max].
    void setGain(int value) override {
        if ((value >= m_paramsGainMin) && (value <= m_paramsGainMax)) {
            m_paramsGain = value;
        }
    }

    /// Returns the currently configured gain.
    int gain() override {
        return m_paramsGain;
    }

    /// Enables/disables acquisition frame-rate limiting and sets its value.
    void setAcquisitionFrameRate(bool enable, double rate) override {
        m_enableAcquisitionFrameRate = enable;
        m_paramsAcquisitionFrameRate = rate;
    }

    /// Returns whether acquisition frame-rate limiting is enabled and its current value.
    void acquisitionFrameRate(bool &enable, double &rate) override {
        enable = m_enableAcquisitionFrameRate;
        rate = m_paramsAcquisitionFrameRate;
    }

    /// Serializes all Basler-specific fields (identity, exposure/gain/frame-rate, backlight,
    /// IO-line capabilities) plus the CameraCfg base fields to JSON. Implemented in the .cpp.
    QJsonObject toJson() const override;
    /// Restores all Basler-specific fields from JSON produced by toJson(); missing keys fall
    /// back to type-appropriate defaults. Implemented in the .cpp.
    bool fromJson(const QJsonObject &obj) override;

    /// Returns a heap-allocated copy of this config; caller takes ownership.
    IDeviceCfg* clone() override {
        return new BaslerGigeCfg(*this);
    }

public:
    QString m_modelName{"Unknown"};        ///< Camera model name, populated from device info on connect.
    QString m_userDefinedName;             ///< User-defined camera name, populated from device info on connect.
    QString m_serialNumber = "";           ///< Camera serial number, populated from device info on connect.
    QString m_ipAddress = "";              ///< IP address used to locate the camera on connect.

    double m_paramsExposureMin{0.0};       ///< Minimum allowed exposure time.
    double m_paramsExposureMax{0.0};       ///< Maximum allowed exposure time.
    double m_paramsExposureTime{0.0};      ///< Configured exposure time.
    BaslerExposureMode m_autoExposureMode; ///< Auto-exposure mode (Off/Once/Continuous).

    int m_paramsGainMin{0};                ///< Minimum allowed gain.
    int m_paramsGainMax{0};                ///< Maximum allowed gain.
    int m_paramsGain{0};                   ///< Configured gain.

    bool m_enableAcquisitionFrameRate{false};      ///< Enables acquisition frame-rate limiting.
    double m_paramsAcquisitionFrameRate{0.0};      ///< Configured acquisition frame rate.

    bool m_autoBacklightControl{false};    ///< Enables automatic backlight output around a grab.
    QString m_autoBacklightLine;           ///< Digital output line driven for backlight control.
    bool m_autoBacklightInvert{false};     ///< Inverts the backlight output line's logic level.
    int m_autoBacklightDelay{0};           ///< Delay (microseconds) after enabling backlight before grabbing.

    QList<BaslerIOLine> m_ioCapabilities;  ///< Digital I/O lines and their input/output capabilities, read from the camera.
    Pylon::CDeviceInfo m_deviceInfo;       ///< Pylon device-info record for the currently associated camera.

};

/// Basler GigE Vision camera device, implemented on top of the Pylon SDK
/// (Pylon::CInstantCamera). Handles connect/disconnect, exposure/gain/backlight parameter
/// application, digital I/O lines, and single/continuous/software-triggered grabs, converting
/// captured Pylon images to cv::Mat.
class BaslerGigECamera : public vc::device::CameraDevice {
    Q_OBJECT

public:
    /// Constructs the camera with the given id/name and registers m_config as this device's
    /// IDeviceCfg (signal-blocked, so no configChanged() fires during construction).
    explicit BaslerGigECamera(QString id, QString name, QObject* parent = nullptr);

    /// Looks up the camera by m_config's configured IP address among enumerated GigE devices,
    /// opens it, reads its IO-line capabilities and current on-camera settings.
    /// @return true if the camera was found and opened successfully.
    bool deviceConnect() override;
    /// Stops any in-progress grab and closes the camera connection.
    /// @return true if a connection existed to close (false if already disconnected).
    bool deviceDisconnect() override;
    /// Returns true if connectStatus() is ConnectStatus::Connected.
    bool isDeviceConnected() const override {
        return connectStatus() == device::ConnectStatus::Connected;
    }

    /// Always CameraType::BaslerGigE.
    CameraType cameraType() const override {
        return CameraType::BaslerGigE;
    }

    /// Always true: Basler GigE cameras expose digital I/O lines.
    bool hasIOPort() const override {
        return true;
    }

    /// Always true: Basler GigE cameras used here capture color frames.
    bool isRgbCamera() const override {
        return true;
    }

    /// Copies the Basler-specific fields from `cfg` (cast from IDeviceCfg) into m_config,
    /// under m_mutex, and re-applies it via IDevice::setDeviceConfig().
    void setDeviceConfig(IDeviceCfg *cfg) override;
    /// Replaces m_config wholesale with `cfg`, under m_mutex, and re-applies it via
    /// IDevice::setDeviceConfig().
    void setBaslerGigeConfig(BaslerGigeCfg& cfg);
    /// Returns a copy of the current Basler configuration.
    BaslerGigeCfg baslerGigeConfig() const;

    /// Sets the timeout (milliseconds) used for grab calls, under m_mutex.
    void setGrabTimeout(int ms) override;
    /// Sets the exposure time if within the configured limits and emits exposureChanged().
    /// @return true if the value was within limits and applied.
    bool setExposure(double exposure) override;
    /// Sets the gain if within the configured limits and emits gainChanged().
    /// @return true if the value was within limits and applied.
    bool setGain(double gain) override;
    /// Enables/disables auto-backlight control and emits backlightControlChanged().
    void setBacklightControl(bool enable) override;
    /// Not implemented: always returns false without querying the camera.
    bool readIO(QString name) override;
    /// Drives the named digital output line's LineSelector/LineSource/UserOutput nodes to
    /// `value` via the camera's GenICam node map.
    /// @return true on success; false if the camera is not connected or the line is unknown.
    bool writeIO(QString name, bool value) override;
    /// Pushes exposure mode/time, gain and acquisition-frame-rate settings from m_config to the
    /// camera's GenICam node map and emits parametersApplied(true).
    /// @return false if the camera instance is null; true otherwise.
    bool applyParametersChange() override;

    /// Performs a blocking single grab (optionally toggling the backlight line around it),
    /// converts the result to cv::Mat and emits grabFinished().
    GrabResult grabSingleShot() override;
    /// Not implemented: always returns false.
    bool startAutoContinuousShot() override;
    /// Not implemented: no-op.
    void stopAutoContinousShot() override;
    /// Not implemented: always returns false.
    bool startContinuousShot() override;
    /// Not implemented: no-op.
    void stopContinuousShot() override;
    /// Not implemented: always returns a default (failed) GrabResult.
    GrabResult softwareTriggerShot() override;

    /// Validates and applies device-level JSON fields: runs CameraDevice::fromJson() (resolves
    /// to the inherited IDevice::fromJson()) as a pre-check, then applies IDevice::fromJson()
    /// again to populate this device's id/name/config.
    /// @return false if the base validation fails.
    virtual bool fromJson(const QJsonObject &obj) override {
        if (!CameraDevice::fromJson(obj)) {
            return false;
        }
        return IDevice::fromJson(obj);
    }


public slots:
    /// Disconnects the camera if still connected and logs device shutdown.
    void deviceTerminate() override;

private:

    /// Reads IO configuration from camera: enumerates LineSelector entries and, for each,
    /// records whether it can be Input/Output (and whether its mode is writable) into
    /// m_io_lines.
    void initializeIOPort();

    /// Reads exposure/gain limits, current exposure/gain/frame-rate and auto-exposure mode
    /// from the camera's GenICam node map into m_config (clamping stale config values that
    /// fall outside the camera-reported limits).
    void readCameraSetting();
    /// Pushes auto-exposure mode, exposure time, gain and acquisition frame rate from
    /// m_config to the camera's GenICam node map.
    void loadSettingToCamera();

    /// Drives the named digital output line's LineSelector/LineSource/UserOutput nodes to
    /// `value` via the camera's GenICam node map; no-op (logged) if the camera is not
    /// connected or the line is unknown.
    void setOutputLineState(QString line, bool value);
    /// Returns a copy of the current Basler configuration.
    BaslerGigeCfg BaslerConfig();
    /// Replaces the current Basler configuration with `cfg`.
    void SetBaslerConfig(BaslerGigeCfg cfg);

    /// Converts a Pylon grab result to an OpenCV cv::Mat, choosing BGR8 for color images and
    /// Mono8 otherwise; leaves `mat` empty and logs an error for unsupported pixel formats.
    void pylon_image_to_mat(Pylon::CInstantCamera::GrabResultPtr_t &ptrGrabResult, cv::Mat &mat);

private:
    int m_grab_timeout{5000};              ///< Timeout (milliseconds) for grab calls.
    BaslerGigeCfg m_config;                ///< Current Basler configuration; registered as this device's IDeviceCfg.
    Pylon::CDeviceInfo m_camera_info;      ///< Pylon device-info record for the connected camera.
    QString m_camera_ip_address;           ///< IP address used to find the camera on the last deviceConnect() call.

    std::shared_ptr<Pylon::CInstantCamera> m_camera_instance;  ///< Owning handle to the opened Pylon camera instance; null when disconnected.
    QString m_str_camera_id;               ///< Camera serial number, cached from device info after connect.

    bool m_continuous_shot_ready{false};           ///< Reset to false on disconnect; not yet set true (continuous-shot start/stop are unimplemented).
    bool m_software_trigger_shot_ready{false};     ///< Reset to false on disconnect; not yet set true (software-trigger shot is unimplemented).

    QList<BaslerIOLine> m_io_lines;        ///< Digital I/O lines and their input/output capabilities, populated by initializeIOPort().
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::BaslerGigeCfg)

#endif // CAMERA_BASLER_GIGE_H
