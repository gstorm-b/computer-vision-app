#ifndef CAMERA_BASLER_GIGE_H
#define CAMERA_BASLER_GIGE_H

// #include <pylon/PylonIncludes.h>
// #include <pylon/_BaslerUniversalCameraParams.h>

#include "device/idevice.h"
#include "device/camera/camera_device.h"
#include "device/camera/basler_define.h"

#include "core/qgadget_macro.h"

using namespace vc::device::basler;

namespace vc::device {

class BaslerGigeCfg : public CameraCfg {
    Q_GADGET

    G_PROPERTY_STRING_READ(QString, modelName, "Model name")
    G_PROPERTY_STRING_READ(QString, userDefinedName, "User-defined name")
    G_PROPERTY_STRING_READ(QString, serialNumber, "Serial number")
    G_PROPERTY_STRING_READWRITE(QString, ipAddress, "IP Address")

    G_PROPERTY_ENUM_READWRITE(vc::device::basler::BaslerExposureMode, autoExposureMode, "Exposure Mode")
    G_PROPERTY_NUMBER_READWRITE(double, paramsExposureTime, 0.0, 10000.0, "Exposure time")

    G_PROPERTY_NUMBER_READWRITE(double, paramsGain, 0.0, 10000.0, "Gain")

    G_PROPERTY_BOOL_READWRITE(bool, enableAcquisitionFrameRate, "Enable acquisition rate")
    G_PROPERTY_NUMBER_READWRITE(double, paramsAcquisitionFrameRate, 1.0, 100.0, "Acquisition rate")

    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightControl, "Enable auto backlight")
    G_PROPERTY_STRING_READWRITE(QString, autoBacklightLine, "Backlight line")
    G_PROPERTY_NUMBER_READWRITE(int, autoBacklightDelay, 0, 10000000, "Back light delay (us)")
    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightInvert, "Line invert")

public:
    explicit BaslerGigeCfg()
        : CameraCfg() {}

    const QMetaObject &getMetaObject() const override {
        return vc::device::BaslerGigeCfg::staticMetaObject;
    }

    DeviceType deviceType() const override {
        return DeviceType::Camera;
    }

    CameraType cameraType() const override {
        return CameraType::BaslerGigE;
    }

    void enableBlackLightControl(bool ena) override {
        m_autoBacklightControl = ena;
    }

    void exposureTimeLimit(double &min, double &max) override {
        min = m_paramsExposureMin;
        max = m_paramsExposureMax;
    }

    void setExposureTimeLimit(double min, double max) override {
        m_paramsExposureMin = min;
        m_paramsExposureMax = max;
    }

    void setExposureTime(double value) override {
        if ((value >= m_paramsExposureMin) && (value <= m_paramsExposureMax)) {
            m_paramsExposureTime = value;
        }
    }

    double exposureTime() override {
        return m_paramsExposureTime;
    }

    void setAutoExposure(vc::device::basler::BaslerExposureMode mode) {
        m_autoExposureMode = mode;
    }

    void setAutoExposure(const QString &mode) {
        m_autoExposureMode = BaslerExposureTypeFromString(mode);
    }

    QString autoExposure() {
        return BaslerExposureTypeToString(m_autoExposureMode);
    }

    void gainLimit(int &min, int &max) override {
        min = m_paramsGainMin;
        max = m_paramsGainMax;
    }

    void setGainLimit(int min, int max) override {
        m_paramsGainMin = min;
        m_paramsGainMax = max;
    }

    void setGain(int value) override {
        if ((value >= m_paramsGainMin) && (value <= m_paramsGainMax)) {
            m_paramsGain = value;
        }
    }

    int gain() override {
        return m_paramsGain;
    }

    void setAcquisitionFrameRate(bool enable, double rate) override {
        m_enableAcquisitionFrameRate = enable;
        m_paramsAcquisitionFrameRate = rate;
    }

    void acquisitionFrameRate(bool &enable, double &rate) override {
        enable = m_enableAcquisitionFrameRate;
        rate = m_paramsAcquisitionFrameRate;
    }

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject &obj) override;

    IDeviceCfg* clone() override {
        return new BaslerGigeCfg(*this);
    }

public:
    QString m_modelName{"Unknown"};
    QString m_userDefinedName;
    QString m_serialNumber = "";
    QString m_ipAddress = "";

    double m_paramsExposureMin{0.0};
    double m_paramsExposureMax{0.0};
    double m_paramsExposureTime{0.0};
    BaslerExposureMode m_autoExposureMode;

    int m_paramsGainMin{0};
    int m_paramsGainMax{0};
    int m_paramsGain{0};

    bool m_enableAcquisitionFrameRate{false};
    double m_paramsAcquisitionFrameRate{0.0};

    bool m_autoBacklightControl{false};
    QString m_autoBacklightLine;
    bool m_autoBacklightInvert{false};
    int m_autoBacklightDelay{0};

    QList<BaslerIOLine> m_ioCapabilities;
    Pylon::CDeviceInfo m_deviceInfo;

};

class BaslerGigECamera : public vc::device::CameraDevice {
    Q_OBJECT

public:
    explicit BaslerGigECamera(QString id, QString name, QObject* parent = nullptr);

    bool deviceConnect() override;
    bool deviceDisconnect() override;
    bool isDeviceConnected() const override {
        return connectStatus() == device::ConnectStatus::Connected;
    }

    CameraType cameraType() const override {
        return CameraType::BaslerGigE;
    }

    bool hasIOPort() const override {
        return true;
    }

    bool isRgbCamera() const override {
        return true;
    }

    void setDeviceConfig(IDeviceCfg *cfg) override;
    void setBaslerGigeConfig(BaslerGigeCfg& cfg);
    BaslerGigeCfg baslerGigeConfig() const;

    void setGrabTimeout(int ms) override;
    bool setExposure(double exposure) override;
    bool setGain(double gain) override;
    void setBacklightControl(bool enable) override;
    bool readIO(QString name) override;
    bool writeIO(QString name, bool value) override;
    bool applyParametersChange() override;

    GrabResult grabSingleShot() override;
    bool startAutoContinuousShot() override;
    void stopAutoContinousShot() override;
    bool startContinuousShot() override;
    void stopContinuousShot() override;
    GrabResult softwareTriggerShot() override;

    virtual bool fromJson(const QJsonObject &obj) override {
        if (!CameraDevice::fromJson(obj)) {
            return false;
        }
        return IDevice::fromJson(obj);
    }


public slots:
    void deviceTerminate() override;

private:

    /**
     * @brief initializeIOPort: read IO configuration from camera
     */
    void initializeIOPort();

    void readCameraSetting();
    void loadSettingToCamera();

    void setOutputLineState(QString line, bool value);
    BaslerGigeCfg BaslerConfig();
    void SetBaslerConfig(BaslerGigeCfg cfg);

    void pylon_image_to_mat(Pylon::CInstantCamera::GrabResultPtr_t &ptrGrabResult, cv::Mat &mat);

private:
    int m_grab_timeout{5000};
    BaslerGigeCfg m_config;
    Pylon::CDeviceInfo m_camera_info;
    QString m_camera_ip_address;

    std::shared_ptr<Pylon::CInstantCamera> m_camera_instance;
    QString m_str_camera_id;

    bool m_continuous_shot_ready{false};
    bool m_software_trigger_shot_ready{false};

    QList<BaslerIOLine> m_io_lines;
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::BaslerGigeCfg)

#endif // CAMERA_BASLER_GIGE_H
