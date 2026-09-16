#ifndef VIRTUAL_CAMERA_DEVICE_H
#define VIRTUAL_CAMERA_DEVICE_H

/**
 * @file virtual_camera_device.h
 * @brief VirtualCameraDevice — a camera that needs no hardware.
 */

#include <opencv2/opencv.hpp>

#include "device/camera/camera_device.h"
#include "device/virtual/virtual_camera_config.h"

namespace vc::device {

/**
 * @class VirtualCameraDevice
 * @brief A CameraDevice whose frames come from a still image or a generated pattern.
 *
 * Exists so the product can be demonstrated, and the runtime exercised, on a machine with no
 * camera attached. It is shipped code, not a test double: the contract test drives the same
 * class the application does, which is the only way the two stay honest about each other.
 *
 * The two public flags are **features, not test hooks**. A camera that cannot connect and a
 * grab that times out are the two failures the runtime's recovery path exists for, and on a
 * virtual device the only way to produce them is to ask.
 *
 * @note Includes no vendor SDK. That is the point — a device meant to remove the hardware
 *       requirement must not drag in the hardware's driver headers.
 */
class VirtualCameraDevice : public CameraDevice {
public:
    /// Creates the device and builds its first frame from the default config.
    explicit VirtualCameraDevice(const QString &id, const QString &name,
                                 QObject *parent = nullptr);

    /// Reports Connected, or ConnectFailed when connectSucceeds is false.
    bool deviceConnect() override;
    /// Reports Disconnected.
    bool deviceDisconnect() override;
    /// True while the connection status is Connected.
    bool isDeviceConnected() const override;

    void deviceTerminate() override {}
    CameraType cameraType() const override { return CameraType::VirtualCamera; }
    bool hasIOPort() const override { return false; }
    bool isRgbCamera() const override { return false; }
    void setGrabTimeout(int) override {}
    bool setExposure(double) override { return true; }
    bool setGain(double) override { return true; }
    void setBacklightControl(bool) override {}
    bool readIO(QString) override { return false; }
    bool writeIO(QString, bool) override { return false; }
    /// Emits parametersApplied(true) and reports success.
    bool applyParametersChange() override;

    /// Returns a clone of the current frame, or a failed result when grabSucceeds is false.
    GrabResult grabSingleShot() override;

    bool startAutoContinuousShot() override { return true; }
    void stopAutoContinousShot() override {}

    /// Starts "streaming": tracks the state and reports it, without producing frames.
    ///
    /// @warning Tracks state rather than returning a bare `true`, which is what it used to do. That lie
    /// was harmless only while nothing called it; now CameraRunner resolves its continuous
    /// commands from continuousStateChanged(), and a device that never reports leaves every such
    /// command hanging until its watchdog fires — including in the contract test.
    ///
    /// Frames are deliberately not generated: the runner contract under test is about command
    /// lifecycle, and a fake frame pump would test the fake.
    bool startContinuousShot() override
    {
        m_continuousActive = true;
        emit continuousStateChanged(true);
        return true;
    }
    /// Stops "streaming" and reports the state, whether or not it was running.
    void stopContinuousShot() override
    {
        m_continuousActive = false;
        emit continuousStateChanged(false);
    }
    bool isContinuousActive() const override { return m_continuousActive; }
    /// Same as grabSingleShot(): there is no hardware trigger to distinguish.
    GrabResult softwareTriggerShot() override { return grabSingleShot(); }

    /// Takes ownership of `cfg`, adopts it if it is a VirtualCameraCfg, and rebuilds the frame.
    void setDeviceConfig(IDeviceCfg *cfg) override;

    /**
     * @brief Restores the device from `obj`, then rebuilds the frame from the loaded config.
     *
     * The rebuild is the whole reason this override exists. `IDevice::fromJson()` writes
     * straight into the config through its stored pointer and never calls setDeviceConfig(),
     * so a device has no hook to react to being loaded. Without this, a project saved with a
     * still-image path reopened showing that path in the property browser while every grab
     * returned the default flat grey frame — the config was right and the frame was stale.
     */
    bool fromJson(const QJsonObject &obj) override;
    /// Returns a heap copy of the current config; the caller owns it.
    IDeviceCfg *deviceConfig() override;

    /**
     * @brief Sets the calibrator directly, without going through a config object.
     * @note Overwritten by the synthetic calibration on the next config change. Set
     *       `millimetresPerPixel` to 0 first if an externally supplied calibration must stand.
     */
    void setCalibrator(const calib::Calibrator &calibrator);

    /**
     * @brief Queues a connection-status change.
     *
     * Queued rather than immediate so the change is delivered through the event loop, the way
     * a real device's status arrives from its own thread — a direct call would let a caller
     * observe an ordering the hardware can never produce.
     */
    void forceConnectionStatus(ConnectStatus status);

    bool grabSucceeds{true};     ///< When false, grabSingleShot() reports a timeout.
    bool connectSucceeds{true};  ///< When false, deviceConnect() reports ConnectFailed.

private:
    /// Loads the configured still image, or generates a flat frame when there is none.
    void rebuildFrame();

    /**
     * @brief Fits a calibration for a declared flat work plane, from the config's scale.
     *
     * The runtime refuses to run a camera whose Calibrator is not calibrated, and the real
     * calibration workflow (board setup, corner detection) lives inside the Basler widget —
     * so without this a virtual camera can never run at all.
     *
     * **The homography is real; the geometry is asserted.** Correspondences are generated
     * across the frame at `millimetresPerPixel`, then `Calibrator::calibrate()` fits them the
     * same way it fits a real board. Nothing about the fit is faked. What IS an assertion is
     * the scene: that it is flat, axis-aligned, at the configured scale and origin. Positions
     * this camera reports are therefore in millimetres of a plane somebody declared, not of
     * anything measured — which is why the scale is an explicit, visible property rather than
     * a hidden constant, and why the device carries the VIRT markers it does.
     *
     * A `millimetresPerPixel` of 0 skips this and leaves the camera uncalibrated, so the
     * runtime refuses it exactly as it refuses an uncalibrated real one.
     */
    void rebuildCalibration();

    VirtualCameraCfg m_config;
    cv::Mat m_frame;
    bool m_continuousActive{false};   ///< Whether startContinuousShot() is in effect.
};

} // namespace vc::device

#endif // VIRTUAL_CAMERA_DEVICE_H
