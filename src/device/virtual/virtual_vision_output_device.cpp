#include "device/virtual/virtual_vision_output_device.h"

#include "core/logger/app_logger.h"

namespace vc::device {

VirtualVisionOutputDevice::VirtualVisionOutputDevice(const QString &id, const QString &name,
                                                     QObject *parent)
    : VisionOutputDevice(id, name, parent)
{
    // Publish the address of the owned config to the base class, which keeps a NON-owning
    // pointer and serialises through it. Without this the device saves an empty DeviceConfig
    // and loses both its sub-type token and its kinematic-check settings on reload.
    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);
}

/// Reports Connected. No port is bound, so nothing can refuse.
bool VirtualVisionOutputDevice::deviceConnect()
{
    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

/// Reports Disconnected.
bool VirtualVisionOutputDevice::deviceDisconnect()
{
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

/// True while the connection status is Connected.
bool VirtualVisionOutputDevice::isDeviceConnected() const
{
    return connectStatus() == ConnectStatus::Connected;
}

/// Takes ownership of `cfg`, copies it into the owned config, and re-publishes that.
void VirtualVisionOutputDevice::setDeviceConfig(IDeviceCfg *cfg)
{
    if (auto *virtualCfg = dynamic_cast<VirtualVisionOutputCfg *>(cfg)) {
        m_config = *virtualCfg;
    } else if (cfg != nullptr) {
        LOG_DEV_ERR << "Virtual vision output was given a config it cannot use; ignored.";
    }
    delete cfg;
    // Re-publish the owned member, not the caller's object: the base holds a non-owning
    // pointer and the one it was given has just been deleted.
    IDevice::setDeviceConfig(&m_config);
}

/// Captures the request's positions, counts the push, and returns sendSucceeds.
///
/// The count is incremented for a rejected send too: "how many times did the runtime try to
/// send" and "how many succeeded" are different questions, and the failure path is exactly
/// where they diverge.
bool VirtualVisionOutputDevice::pushRequest(IRequest *request)
{
    auto *outputRequest = dynamic_cast<VisionOutputRequest *>(request);
    if (outputRequest) {
        capturedPositions = outputRequest->positions();
    }
    requestCount += 1;
    return sendSucceeds;
}

} // namespace vc::device
