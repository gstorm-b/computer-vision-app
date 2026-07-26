#ifndef CAMERA_DEVICE_H
#define CAMERA_DEVICE_H

#include "device/idevice.h"
#include "device/device_capabilities.h"
#include "calibration/calibrator.h"
#include <opencv2/opencv.hpp>

/// String tokens used to (de)serialize CameraType to/from JSON; kept in sync with the
/// CameraType enum by CameraTypeToString()/CameraTypeFromString().
#define CAM_TYPE_REALSENSE       "Realsense"
#define CAM_TYPE_BASLER_GIGE     "Basler_GigE"
#define CAM_TYPE_BASLER_USB      "Basler_USB"

/// Camera device abstractions: camera hardware-family identification, the shared
/// CameraCfg configuration contract, grab results, and the CameraDevice base class.
namespace vc::device {

/// Identifies the concrete camera implementation/hardware family behind a CameraDevice.
enum CameraType {
    CamType,
    Realsense,
    BaslerGigE,
    BaslerUSB,
};

/// Converts a CameraType to its JSON/config string token.
/// @return one of the CAM_TYPE_* tokens, or an empty string for CamType (unset/unknown).
[[maybe_unused]] static QString CameraTypeToString(CameraType t) {
    switch(t) {
    case vc::device::CameraType::Realsense:
        return CAM_TYPE_REALSENSE;
    case vc::device::CameraType::BaslerGigE:
        return CAM_TYPE_BASLER_GIGE;
    case vc::device::CameraType::BaslerUSB:
        return CAM_TYPE_BASLER_USB;
    case CamType:
        return "";
    }
    return "";
};

/// Parses a CameraType back from its JSON/config string token.
/// @return the matching CameraType, or CamType if `t` matches none of the known tokens.
[[maybe_unused]] static CameraType CameraTypeFromString(QString t) {
    if (t == CAM_TYPE_REALSENSE) {
        return CameraType::Realsense;
    } else if (t == CAM_TYPE_BASLER_GIGE) {
        return CameraType::BaslerGigE;
    } else if (t == CAM_TYPE_BASLER_USB) {
        return CameraType::BaslerUSB;
    }
    return CameraType::CamType;
}

/// Base configuration contract for a camera device: exposure/gain/frame-rate parameters,
/// backlight control, calibration data and JSON (de)serialization. Concrete cameras (e.g.
/// BaslerGigeCfg) subclass this and add their own hardware-specific properties.
class CameraCfg : public IDeviceCfg {
public:
    /// Enables or disables the camera's auto-backlight control feature.
    virtual void enableBlackLightControl(bool ena) = 0;

    /// Returns the allowed exposure-time range via the output parameters.
    /// @param min output; minimum exposure time
    /// @param max output; maximum exposure time
    virtual void exposureTimeLimit(double &min, double &max) = 0;

    /// Sets the allowed exposure-time range.
    virtual void setExposureTimeLimit(double min, double max) = 0;

    /// Returns the currently configured exposure time.
    virtual double exposureTime() = 0;

    /// Sets the exposure time (implementations typically clamp/reject values outside the
    /// configured min/max range).
    virtual void setExposureTime(double value) = 0;

    /// Returns the allowed gain range via the output parameters.
    /// @param min output; minimum gain
    /// @param max output; maximum gain
    virtual void gainLimit(int &min, int &max) = 0;

    /// Sets the allowed gain range.
    virtual void setGainLimit(int min, int max) = 0;

    /// Returns the currently configured gain.
    virtual int gain() = 0;

    /// Sets the gain (implementations typically clamp/reject values outside the configured
    /// min/max range).
    virtual void setGain(int value) = 0;

    /// Returns whether acquisition frame-rate limiting is enabled and its current value.
    /// @param enable output; true if frame-rate limiting is enabled
    /// @param rate output; the configured frame rate
    virtual void acquisitionFrameRate(bool &enable, double &rate) = 0;

    /// Enables/disables acquisition frame-rate limiting and sets its value.
    virtual void setAcquisitionFrameRate(bool enable, double rate) = 0;

    /// Returns the concrete camera hardware family this config belongs to.
    virtual CameraType cameraType() const = 0;

    /// Serializes the camera type, calibrator, calibration-board preset and threshold to JSON.
    /// @return a QJsonObject containing this config's base fields (base for subclass toJson()).
    virtual QJsonObject toJson() const override {
        QJsonObject obj;
        obj[DEVICE_JSK_CAM_TYPE] = CameraTypeToString(this->cameraType());
        obj[DEVICE_JSK_CALIBRATOR] = QString::fromStdString(m_calibrator.toJson());
        obj[DEVICE_JSK_CALIB_BOARD_PRESET] = m_calibBoardPreset;
        obj[DEVICE_JSK_CALIB_THRESHOLD] = m_calibThreshold;
        return obj;
    }

    /// Restores calibrator, calibration-board preset and threshold from JSON, then validates
    /// that `obj`'s recorded camera type matches this config's cameraType().
    /// @return true on success; false if the recorded camera type does not match (missing
    /// calibrator data is tolerated and only logged as a warning).
    virtual bool fromJson(const QJsonObject &obj) override {
        if (!obj.contains(DEVICE_JSK_CALIBRATOR)) {
            LOG_USER_WARN << "Camera load calibrator failed.";
        } else {
            std::string calib_str = obj[DEVICE_JSK_CALIBRATOR].toString().toStdString();
            m_calibrator.fromJson(calib_str);
        }

        if (obj.contains(DEVICE_JSK_CALIB_BOARD_PRESET)) {
            m_calibBoardPreset = obj[DEVICE_JSK_CALIB_BOARD_PRESET].toString();
        }

        if (obj.contains(DEVICE_JSK_CALIB_THRESHOLD)) {
            m_calibThreshold = obj[DEVICE_JSK_CALIB_THRESHOLD].toInt(-1);
        }

        if (cameraType() != CameraTypeFromString(obj[DEVICE_JSK_CAM_TYPE].toString())) {
            LOG_DEV_ERR << "Cannot convert camera parameters, wrong camera type";
            LOG_DEV_ERR << obj[DEVICE_JSK_CAM_TYPE].toString();
            return false;
        }
        return true;
    }

    /// Returns the calibrator holding this camera's calibration state (intrinsics/board data).
    virtual calib::Calibrator calibrator() const {
        return m_calibrator;
    }

    /// Replaces the calibrator with `calib` (copied).
    virtual void setCalibrator(calib::Calibrator calib)  {
        m_calibrator = calib;
    }

    /// Returns the name of the calibration-board preset resolved by
    /// calib::CalibrationBoardFactory (e.g. dot-grid spacing/layout).
    virtual QString calibBoardPreset() const {
        return m_calibBoardPreset;
    }

    /// Sets the calibration-board preset name.
    virtual void setCalibBoardPreset(const QString &preset) {
        m_calibBoardPreset = preset;
    }

    /// Binarization threshold for calibration-board detection:
    /// -1 => auto (Otsu), 0..255 => fixed manual threshold.
    virtual int calibThreshold() const {
        return m_calibThreshold;
    }

    /// Sets the binarization threshold, clamped to [-1, 255] (-1 = auto/Otsu, 0..255 = manual).
    virtual void setCalibThreshold(int threshold) {
        m_calibThreshold = (threshold < 0) ? -1
                           : (threshold > 255 ? 255 : threshold);
    }

protected:
    calib::Calibrator m_calibrator;  ///< Calibration state (intrinsics/board data) for this camera.
    /// Stored preset name (resolved by calib::CalibrationBoardFactory). Default
    /// matches iRVision 22.5 mm dot grid; UI lets the user change it.
    QString m_calibBoardPreset{"iRvision-22.5mm"};
    /// Detection binarization threshold; -1 = auto (Otsu).
    int m_calibThreshold{-1};
};

/// Result of a single camera grab: the captured frame plus success/error status.
struct GrabResult {
    cv::Mat frame;        ///< Captured image (BGR8 for color cameras, Mono8 for mono); empty on failure.
    bool isGrabSuccess;   ///< True if the grab completed and `frame` holds valid image data.
    QString msg;          ///< Human-readable status/error message for the grab attempt.
};

/// Abstract base for all camera devices: adds exposure/gain/backlight/IO control and
/// single-shot, software-triggered and continuous grab operations on top of IDevice, and
/// marks the device as an IImageSourceDevice.
class CameraDevice : public IDevice, public IImageSourceDevice {
    Q_OBJECT

public:
    /// Constructs the camera device with the given id/name; connects to hardware only on
    /// deviceConnect(), not here.
    explicit CameraDevice(QString id, QString name, QObject* parent = nullptr)
        : IDevice(id, name, parent) {}

    /// Always DeviceType::Camera for cameras.
    virtual DeviceType deviceType() const override {
        return DeviceType::Camera;
    }

    /// Returns the concrete camera hardware family (Basler GigE/USB, Realsense, ...).
    virtual CameraType cameraType() const = 0;
    /// Returns true if this camera exposes digital I/O lines (readIO/writeIO usable).
    virtual bool hasIOPort() const = 0;
    /// Returns true if this camera captures color (RGB/BGR) frames rather than mono.
    virtual bool isRgbCamera() const = 0;
    /// Sets the timeout (in milliseconds) used when waiting for a grab to complete.
    virtual void setGrabTimeout(int ms) = 0;
    /// Sets the exposure time; returns false if `exposure` is outside the configured limits.
    virtual bool setExposure(double exposure) = 0;
    /// Sets the gain; returns false if `gain` is outside the configured limits.
    virtual bool setGain(double gain) = 0;
    /// Enables/disables the auto-backlight control feature.
    virtual void setBacklightControl(bool enable) = 0;
    /// Reads the current state of the named digital I/O line.
    virtual bool readIO(QString name) = 0;
    /// Writes `value` to the named digital I/O line.
    /// @return true if the write succeeded.
    virtual bool writeIO(QString name, bool value) = 0;
    /// Pushes the currently configured exposure/gain/frame-rate parameters down to the
    /// physical camera hardware.
    /// @return true if the parameters were applied successfully.
    virtual bool applyParametersChange() = 0;

    /// Performs a single blocking grab and returns its result.
    virtual GrabResult grabSingleShot() = 0;
    /// Starts free-running (hardware/software-timed) continuous acquisition with automatic
    /// frame delivery via the grabFinished() signal.
    /// @return true if continuous acquisition was started successfully.
    virtual bool startAutoContinuousShot() = 0;
    /// Stops the auto-continuous acquisition started by startAutoContinuousShot().
    virtual void stopAutoContinousShot() = 0;
    /// Starts continuous acquisition driven by explicit grab calls rather than free-running.
    /// @return true if continuous acquisition was started successfully.
    virtual bool startContinuousShot() = 0;
    /// Stops the continuous acquisition started by startContinuousShot().
    virtual void stopContinuousShot() = 0;
    /// Arms/performs a software-triggered grab and returns its result.
    virtual GrabResult softwareTriggerShot() = 0;

    /// Cameras do not accept generic IRequest objects; always returns false.
    virtual bool pushRequest(IRequest *request) override {
        Q_UNUSED(request);
        return false;
    }

    /// Serializes the base IDevice fields plus this camera's CameraType.
    virtual QJsonObject toJson() const override {
        QJsonObject obj = IDevice::toJson();
        obj.insert(DEVICE_JSK_CAM_TYPE, CameraTypeToString(this->cameraType()));
        return obj;
    }

signals:
    void grabFinished(vc::device::GrabResult result);      ///< Emitted when a grab (single-shot or continuous) completes with `result`.
    void exposureChanged(double value);                     ///< Emitted after the exposure time is changed to `value`.
    void gainChanged(double value);                         ///< Emitted after the gain is changed to `value`.
    void backlightControlChanged(bool ena);                 ///< Emitted after auto-backlight control is enabled/disabled.
    void parametersApplied(bool isOk);                      ///< Emitted after applyParametersChange() runs, with its outcome.
};



} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::GrabResult)

#endif // CAMERA_DEVICE_H
