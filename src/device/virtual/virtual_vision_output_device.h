#ifndef VIRTUAL_VISION_OUTPUT_DEVICE_H
#define VIRTUAL_VISION_OUTPUT_DEVICE_H

/**
 * @file virtual_vision_output_device.h
 * @brief VirtualVisionOutputDevice — a vision-output sink that binds no port.
 */

#include <QVector>

#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"
#include "device/virtual/virtual_vision_output_config.h"

namespace vc::device {

/**
 * @class VirtualVisionOutputDevice
 * @brief A VisionOutputDevice that captures the positions it is sent instead of transmitting.
 *
 * Exists so a full localization cycle can run with nothing listening on the other end. The
 * last request's positions stay in capturedPositions and every push is counted, so a caller
 * can check what the pipeline actually produced rather than that it merely finished.
 *
 * `sendSucceeds` is a feature: a send that fails is a case the runtime has to handle, and on
 * a device with no socket the only way to produce one is to ask for it.
 *
 * @note The robot kinematic check inherited from VisionOutputDeviceCfg still runs. It is a
 *       property of the picking geometry, not of the transport, so a hardware-free run must
 *       not be allowed to pass poses a real robot could never reach.
 */
class VirtualVisionOutputDevice : public VisionOutputDevice {
public:
    explicit VirtualVisionOutputDevice(const QString &id, const QString &name,
                                       QObject *parent = nullptr);

    /// Reports Connected. No port is bound.
    bool deviceConnect() override;
    /// Reports Disconnected.
    bool deviceDisconnect() override;
    /// True while the connection status is Connected.
    bool isDeviceConnected() const override;

    void deviceTerminate() override {}
    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VirtualVisionOutput;
    }

    /// Takes ownership of `cfg`, copies it into the owned config, and re-publishes that.
    ///
    /// Overridden rather than inherited on purpose: IDevice::setDeviceConfig() stores a
    /// **non-owning** pointer, so handing the base a heap config would leave it pointing at
    /// an object nobody owns while this device's own member goes unused.
    void setDeviceConfig(IDeviceCfg *cfg) override;

    /// Captures the request's positions, counts the push, and returns sendSucceeds.
    bool pushRequest(IRequest *request) override;

    bool sendSucceeds{true};   ///< When false, pushRequest() reports a failed send.
    int requestCount{0};       ///< How many pushes have been made, accepted or not.
    QVector<VisionOutputPosition> capturedPositions;  ///< Positions from the most recent request.

private:
    VirtualVisionOutputCfg m_config;
};

} // namespace vc::device

#endif // VIRTUAL_VISION_OUTPUT_DEVICE_H
