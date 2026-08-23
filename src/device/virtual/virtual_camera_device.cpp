#include "device/virtual/virtual_camera_device.h"

#include <QFileInfo>

#include "core/logger/app_logger.h"

namespace vc::device {

/// Builds the first frame from the default config.
VirtualCameraDevice::VirtualCameraDevice(const QString &id, const QString &name,
                                         QObject *parent)
    : CameraDevice(id, name, parent)
{
    // Publish the address of the owned config to the base class. IDevice keeps a NON-owning
    // pointer and serialises through it, so without this line toJson() writes an empty
    // DeviceConfig and fromJson() restores nothing — the device would save and reload as a
    // virtual camera with every setting reset. Same call, same reason, as
    // camera_basler_gige.cpp:103; signals are blocked because a constructor has no listeners
    // and configChanged() from one is noise.
    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);

    rebuildFrame();
    rebuildCalibration();
}

/// Loads the configured still image, or generates a flat frame when there is none.
///
/// The fallback is not a failure path: a generated frame is enough to exercise grab, connect
/// and recovery. It is only *matching* that needs a real image, which is why an unreadable
/// path is logged rather than treated as a device fault — the device still works, it just
/// will not find anything.
void VirtualCameraDevice::rebuildFrame()
{
    const QString path = m_config.imagePath();
    if (!path.isEmpty()) {
        if (QFileInfo::exists(path)) {
            const cv::Mat loaded = cv::imread(path.toStdString(), cv::IMREAD_GRAYSCALE);
            if (!loaded.empty()) {
                m_frame = loaded;
                return;
            }
            LOG_USER_WARN << "Virtual camera could not decode its still image." << path;
        } else {
            LOG_USER_WARN << "Virtual camera still image is missing." << path;
        }
    }

    m_frame = cv::Mat(m_config.frameHeight(),
                      m_config.frameWidth(),
                      CV_8UC1,
                      cv::Scalar(m_config.frameGreyLevel()));
}

/// Fits a calibration for a declared flat work plane. See the header for what is fitted and
/// what is merely asserted.
void VirtualCameraDevice::rebuildCalibration()
{
    const double mmPerPixel = m_config.millimetresPerPixel();
    if (mmPerPixel <= 0.0 || m_frame.empty()) {
        return;  // Deliberately leaves the camera uncalibrated; the runtime will refuse it.
    }

    const double originX = m_config.originXMm();
    const double originY = m_config.originYMm();
    const double planeZ = static_cast<float>(m_config.workPlaneZMm());

    // A 3x3 grid rather than the four corners: calibrate() needs at least 4 points, and a
    // spread-out set keeps both the homography fit and the work-plane fit well conditioned
    // instead of resting on the minimum.
    std::vector<cv::Point2f> imagePts;
    std::vector<cv::Point3f> robotPts;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float px = static_cast<float>(col) * (m_frame.cols - 1) / 2.0f;
            const float py = static_cast<float>(row) * (m_frame.rows - 1) / 2.0f;
            imagePts.emplace_back(px, py);
            robotPts.emplace_back(static_cast<float>(originX + px * mmPerPixel),
                                  static_cast<float>(originY + py * mmPerPixel),
                                  static_cast<float>(planeZ));
        }
    }

    calib::Calibrator calibrator;
    calibrator.addCorrespondences(imagePts, robotPts);
    if (!calibrator.calibrate()) {
        LOG_DEV_ERR << "Virtual camera could not fit its synthetic calibration.";
        return;
    }
    m_config.setCalibrator(calibrator);
}

/// Reports Connected, or ConnectFailed when connectSucceeds is false.
///
/// The failure path is the point: it lets a caller hold the camera unreachable across many
/// reconnect attempts, which is what an unbounded retry policy has to be exercised against.
bool VirtualCameraDevice::deviceConnect()
{
    if (!connectSucceeds) {
        setConnectionStatus(ConnectStatus::ConnectFailed);
        return false;
    }
    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

/// Reports Disconnected. There is nothing to close.
bool VirtualCameraDevice::deviceDisconnect()
{
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

/// True while the connection status is Connected.
bool VirtualCameraDevice::isDeviceConnected() const
{
    return connectStatus() == ConnectStatus::Connected;
}

/// Emits parametersApplied(true) and reports success: every parameter this device accepts is
/// accepted unconditionally, so there is nothing that can fail to apply.
bool VirtualCameraDevice::applyParametersChange()
{
    emit parametersApplied(true);
    return true;
}

/// Returns a clone of the current frame, or a failed result when grabSucceeds is false.
///
/// The frame is cloned rather than shared because cv::Mat is reference-counted: handing out
/// the member would let a consumer write through it into every later grab.
GrabResult VirtualCameraDevice::grabSingleShot()
{
    GrabResult result;
    result.isGrabSuccess = grabSucceeds;
    result.msg = grabSucceeds ? QStringLiteral("virtual grab ok")
                              : QStringLiteral("virtual grab timeout");
    if (grabSucceeds) {
        result.frame = m_frame.clone();
    }
    emit grabFinished(result);
    return result;
}

/// Takes ownership of `cfg`, adopts it if it is a VirtualCameraCfg, and rebuilds the frame.
///
/// A config of any other camera type is rejected rather than partially absorbed: taking only
/// the calibrator out of a Basler config would leave this device claiming settings it never
/// received.
void VirtualCameraDevice::setDeviceConfig(IDeviceCfg *cfg)
{
    if (auto *virtualCfg = dynamic_cast<VirtualCameraCfg *>(cfg)) {
        m_config = *virtualCfg;
        rebuildFrame();
        rebuildCalibration();
    } else if (cfg != nullptr) {
        LOG_DEV_ERR << "Virtual camera was given a config it cannot use; ignored.";
    }
    delete cfg;
    // Re-publish the owned member, not the caller's object: the base holds a non-owning
    // pointer and the one it was given has just been deleted.
    IDevice::setDeviceConfig(&m_config);
}

/// Restores the device from `obj`, then rebuilds the frame from the loaded config.
/// See the header for why this override is necessary rather than tidy.
bool VirtualCameraDevice::fromJson(const QJsonObject &obj)
{
    if (!CameraDevice::fromJson(obj)) {
        return false;
    }
    rebuildFrame();
    rebuildCalibration();
    return true;
}

/// Returns a heap copy of the current config; the caller owns it.
IDeviceCfg *VirtualCameraDevice::deviceConfig()
{
    return new VirtualCameraCfg(m_config);
}

/// Sets the calibrator directly, without going through a config object.
void VirtualCameraDevice::setCalibrator(const calib::Calibrator &calibrator)
{
    m_config.setCalibrator(calibrator);
}

/// Queues a connection-status change through the event loop. See the header for why it is
/// queued rather than applied in place.
void VirtualCameraDevice::forceConnectionStatus(ConnectStatus status)
{
    QMetaObject::invokeMethod(this, [this, status]() {
        setConnectionStatus(status);
    }, Qt::QueuedConnection);
}

} // namespace vc::device
