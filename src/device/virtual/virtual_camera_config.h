#ifndef VIRTUAL_CAMERA_CONFIG_H
#define VIRTUAL_CAMERA_CONFIG_H

/**
 * @file virtual_camera_config.h
 * @brief VirtualCameraCfg — configuration for the hardware-free VirtualCameraDevice.
 */

#include "core/qgadget_macro.h"
#include "device/camera/camera_device.h"

namespace vc::device {

/**
 * @class VirtualCameraCfg
 * @brief Configuration for a camera with no hardware behind it.
 *
 * Two groups of fields, and it is worth knowing which is which:
 *
 * - **The frame description means something.** `imagePath` replays a still image from disk;
 *   when it is empty or unreadable the device falls back to a flat frame of `frameWidth` ×
 *   `frameHeight` at `frameGreyLevel`. A flat frame cannot produce a match, so a demo that
 *   has to *find* something needs the image.
 * - **Exposure, gain, acquisition rate and backlight do not.** There is no sensor to apply
 *   them to. They exist because CameraCfg's contract is not optional and the property browser
 *   needs something to show, and they round-trip so a save/load stays lossless — but nothing
 *   reads them back out.
 *
 * @note The calibration fields inherited from CameraCfg are **not** decoration: the runtime
 *       refuses to run an uncalibrated camera, so the calibrator is the one base field a
 *       virtual camera genuinely needs. CameraCfg::toJson()/fromJson() already carry it.
 * @note This class exists rather than reusing BaslerGigeCfg. Sharing it would have written
 *       `"Basler_GigE"` into the nested DeviceConfig while the top level said `"Virtual"` —
 *       both load correctly today, but the moment the top-level key is dropped or hand-edited
 *       the device comes back as a real Basler. That is exactly the "a virtual device passes
 *       for a real one" failure this phase is supposed to prevent.
 */
class VirtualCameraCfg : public CameraCfg {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, imagePath, "Still image path")                            ///< Image replayed by every grab; empty = generated flat frame.
    G_PROPERTY_NUMBER_READWRITE(int, frameWidth, 1, 8192, "Frame width (px)")                      ///< Width of the generated frame.
    G_PROPERTY_NUMBER_READWRITE(int, frameHeight, 1, 8192, "Frame height (px)")                    ///< Height of the generated frame.
    G_PROPERTY_NUMBER_READWRITE(int, frameGreyLevel, 0, 255, "Frame grey level")                   ///< Grey value filling the generated frame.

    G_PROPERTY_NUMBER_READWRITE(double, millimetresPerPixel, 0.0, 1000.0, "Millimetres per pixel") ///< Declared scale of the synthetic calibration; 0 disables it.
    G_PROPERTY_NUMBER_READWRITE(double, originXMm, -100000.0, 100000.0, "Origin X (mm)")           ///< Robot-frame X that image pixel (0,0) maps to.
    G_PROPERTY_NUMBER_READWRITE(double, originYMm, -100000.0, 100000.0, "Origin Y (mm)")           ///< Robot-frame Y that image pixel (0,0) maps to.
    G_PROPERTY_NUMBER_READWRITE(double, workPlaneZMm, -100000.0, 100000.0, "Work plane Z (mm)")    ///< Height of the assumed flat work plane.

    G_PROPERTY_NUMBER_READWRITE(double, paramsExposureTime, 0.0, 10000.0, "Exposure time")         ///< Stored, never applied — there is no sensor.
    G_PROPERTY_NUMBER_READWRITE(double, paramsGain, 0.0, 10000.0, "Gain")                          ///< Stored, never applied.
    G_PROPERTY_BOOL_READWRITE(bool, enableAcquisitionFrameRate, "Enable acquisition rate")         ///< Stored, never applied.
    G_PROPERTY_NUMBER_READWRITE(double, paramsAcquisitionFrameRate, 1.0, 100.0, "Acquisition rate")///< Stored, never applied.
    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightControl, "Enable auto backlight")                 ///< Stored, never applied.

    /// Translation markers for the display names above. Not read by any code: lupdate cannot
    /// see Q_CLASSINFO, so without this table these labels never enter the .ts. The context
    /// must be this class's className(). See TaskLocalizeConfig::kDisplayNameSources for the
    /// full reasoning; the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Still image path"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Frame width (px)"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Frame height (px)"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Frame grey level"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Millimetres per pixel"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Origin X (mm)"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Origin Y (mm)"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Work plane Z (mm)"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Exposure time"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Gain"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Enable acquisition rate"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Acquisition rate"),
        QT_TRANSLATE_NOOP("vc::device::VirtualCameraCfg", "Enable auto backlight"),
    };

public:
    /// Constructs a default-initialized config (see the field initializers below).
    explicit VirtualCameraCfg()
        : CameraCfg() {}

    /// Returns this class's Qt meta-object, used by the gadget property-browser machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::VirtualCameraCfg::staticMetaObject;
    }

    /// Always DeviceType::Camera.
    DeviceType deviceType() const override {
        return DeviceType::Camera;
    }

    /// Always CameraType::VirtualCamera.
    CameraType cameraType() const override {
        return CameraType::VirtualCamera;
    }

    /// Stores the flag; there is no backlight line to drive.
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

    /// Returns the currently configured exposure time.
    double exposureTime() override {
        return m_paramsExposureTime;
    }

    /// Sets the exposure time; silently ignored if `value` is outside [min, max] — the same
    /// clamping contract the hardware configs use, so the property browser behaves identically.
    void setExposureTime(double value) override {
        if ((value >= m_paramsExposureMin) && (value <= m_paramsExposureMax)) {
            m_paramsExposureTime = value;
        }
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

    /// Returns the currently configured gain.
    int gain() override {
        return static_cast<int>(m_paramsGain);
    }

    /// Sets the gain; silently ignored if `value` is outside [min, max].
    void setGain(int value) override {
        if ((value >= m_paramsGainMin) && (value <= m_paramsGainMax)) {
            m_paramsGain = value;
        }
    }

    /// Returns whether acquisition frame-rate limiting is enabled and its current value.
    void acquisitionFrameRate(bool &enable, double &rate) override {
        enable = m_enableAcquisitionFrameRate;
        rate = m_paramsAcquisitionFrameRate;
    }

    /// Enables/disables acquisition frame-rate limiting and sets its value.
    void setAcquisitionFrameRate(bool enable, double rate) override {
        m_enableAcquisitionFrameRate = enable;
        m_paramsAcquisitionFrameRate = rate;
    }

    /// Serializes the frame description and the inert camera parameters on top of the
    /// CameraCfg base fields, which carry the camera-type token and the calibrator.
    QJsonObject toJson() const override {
        QJsonObject obj = CameraCfg::toJson();
        obj["ImagePath"]                  = m_imagePath;
        obj["FrameWidth"]                 = m_frameWidth;
        obj["FrameHeight"]                = m_frameHeight;
        obj["FrameGreyLevel"]             = m_frameGreyLevel;
        obj["MillimetresPerPixel"]        = m_millimetresPerPixel;
        obj["OriginXMm"]                  = m_originXMm;
        obj["OriginYMm"]                  = m_originYMm;
        obj["WorkPlaneZMm"]               = m_workPlaneZMm;
        obj["ExposureLimitMin"]           = m_paramsExposureMin;
        obj["ExposureLimitMax"]           = m_paramsExposureMax;
        obj["Exposure"]                   = m_paramsExposureTime;
        obj["GainLimitMin"]               = m_paramsGainMin;
        obj["GainLimitMax"]               = m_paramsGainMax;
        obj["Gain"]                       = m_paramsGain;
        obj["EnableAcquisitionFrameRate"] = m_enableAcquisitionFrameRate;
        obj["AcquisitionFrameRate"]       = m_paramsAcquisitionFrameRate;
        obj["AutoBacklightControl"]       = m_autoBacklightControl;
        return obj;
    }

    /// Restores everything toJson() wrote.
    ///
    /// CameraCfg::fromJson() runs first: it restores the calibrator and then **rejects a
    /// mismatched camera type**, which is what stops a Basler config being loaded into this
    /// class and the other way round.
    bool fromJson(const QJsonObject &obj) override {
        if (!CameraCfg::fromJson(obj)) {
            return false;
        }

        m_imagePath                  = obj["ImagePath"].toString();
        m_frameWidth                 = obj["FrameWidth"].toInt(32);
        m_frameHeight                = obj["FrameHeight"].toInt(32);
        m_frameGreyLevel             = obj["FrameGreyLevel"].toInt(128);
        m_millimetresPerPixel        = obj["MillimetresPerPixel"].toDouble(1.0);
        m_originXMm                  = obj["OriginXMm"].toDouble(0.0);
        m_originYMm                  = obj["OriginYMm"].toDouble(0.0);
        m_workPlaneZMm               = obj["WorkPlaneZMm"].toDouble(0.0);

        m_paramsExposureMin          = obj["ExposureLimitMin"].toDouble(0.0);
        m_paramsExposureMax          = obj["ExposureLimitMax"].toDouble(10000.0);
        m_paramsExposureTime         = obj["Exposure"].toDouble(0.0);
        m_paramsGainMin              = obj["GainLimitMin"].toInt(0);
        m_paramsGainMax              = obj["GainLimitMax"].toInt(10000);
        m_paramsGain                 = obj["Gain"].toDouble(0.0);
        m_enableAcquisitionFrameRate = obj["EnableAcquisitionFrameRate"].toBool(false);
        m_paramsAcquisitionFrameRate = obj["AcquisitionFrameRate"].toDouble(1.0);
        m_autoBacklightControl       = obj["AutoBacklightControl"].toBool(false);
        return true;
    }

    /// Allocates and returns a heap copy; the caller owns it.
    IDeviceCfg *clone() override {
        return new VirtualCameraCfg(*this);
    }

private:
    QString m_imagePath;              ///< Still image replayed by every grab; empty = generated frame.
    int m_frameWidth{32};             ///< Width of the generated frame.
    int m_frameHeight{32};            ///< Height of the generated frame.
    int m_frameGreyLevel{128};        ///< Grey value filling the generated frame.

    /// Declared scale of the synthetic calibration. 1 mm/px by default so a freshly created
    /// virtual camera is calibrated and runnable immediately; set it to 0 to leave the camera
    /// uncalibrated and have the runtime refuse it, exactly as it refuses a real one.
    double m_millimetresPerPixel{1.0};
    double m_originXMm{0.0};          ///< Robot-frame X that image pixel (0,0) maps to.
    double m_originYMm{0.0};          ///< Robot-frame Y that image pixel (0,0) maps to.
    double m_workPlaneZMm{0.0};       ///< Height of the assumed flat work plane.

    double m_paramsExposureMin{0.0};       ///< Lower bound offered to the property browser.
    double m_paramsExposureMax{10000.0};   ///< Upper bound offered to the property browser.
    double m_paramsExposureTime{0.0};      ///< Stored exposure; nothing applies it.
    int m_paramsGainMin{0};                ///< Lower gain bound offered to the property browser.
    int m_paramsGainMax{10000};            ///< Upper gain bound offered to the property browser.
    double m_paramsGain{0.0};              ///< Stored gain; nothing applies it.
    bool m_enableAcquisitionFrameRate{false};   ///< Stored flag; nothing applies it.
    double m_paramsAcquisitionFrameRate{1.0};   ///< Stored rate; nothing applies it.
    bool m_autoBacklightControl{false};         ///< Stored flag; there is no backlight line.
};

} // namespace vc::device

#endif // VIRTUAL_CAMERA_CONFIG_H
