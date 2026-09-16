#ifndef VISION_OUTPUT_DEVICE_H
#define VISION_OUTPUT_DEVICE_H

/**
 * @file vision_output_device.h
 * @brief Abstract device classes for the vision-output device family (the software side that
 *        streams matching results / raw bytes out to an external system).
 */

#include "device/idevice.h"
#include "device/device_capabilities.h"
#include "device/output_device/vision_output_config.h"
#include "device/output_device/vision_output_request.h"

#include <memory>

#define VISION_OUTPUT_TYPE_TCPIP    "VisionTCPIP"
#define VISION_OUTPUT_TYPE_SERIAL   "VisionSerial"

namespace vc::device {

/**
 * @class VisionOutputDevice
 * @brief Abstract base for the vision-output device family. Concrete vendors
 *        (VisionTcpipDevice, future VisionSerialDevice, …) inherit from this base. The base
 *        only carries the family-level dispatch (visionOutputType()) and the family JSON
 *        header; transport-specific surface (TCP servers / serial port / heartbeat) lives
 *        entirely on the concrete subclass.
 */
class VisionOutputDevice : public IDevice, public IResultOutputDevice {
    Q_OBJECT

public:
    /// Constructs the device with the given id/name; connection and transport state are
    /// owned entirely by the concrete subclass.
    explicit VisionOutputDevice(QString id, QString name, QObject* parent = nullptr)
        : IDevice(id, name, parent) {}

    /// Returns DeviceType::VisionOutput.
    DeviceType deviceType() const override {
        return DeviceType::VisionOutput;
    }

    /// Returns the concrete vision-output sub-type (server/client/serial/…) this device
    /// implements; used both for factory dispatch and for the JSON type-tag round-trip.
    virtual VisionOutputType visionOutputType() const = 0;

    // ── IResultOutputDevice ───────────────────────────────────────────────────

    /// Wraps `positions` in a VisionOutputRequest and pushes it through the concrete
    /// transport's pushRequest(). This is the whole of the family's result path: the request
    /// is built from the same positions and pushed the same way it was when VisionOutputRunner
    /// did it inline, so the bytes on the wire are unchanged.
    /// @param[in]  positions the cycle's result positions, in send order
    /// @param[out] message   success/failure detail; may be null
    /// @return true if the transport accepted the request
    bool sendVisionResult(const QVector<VisionOutputPosition> &positions,
                          QString *message) override {
        VisionOutputRequest request(positions);
        const bool ok = pushRequest(&request);
        if (message) {
            *message = ok ? QStringLiteral("Vision output result sent.")
                          : QStringLiteral("Vision output result send failed.");
        }
        return ok;
    }

    /// Returns the pick-check settings from this device's config. The family guarantees its
    /// config is a VisionOutputDeviceCfg, so the cast here cannot be the silent-default hazard
    /// it was when callers outside the family made it.
    /// @return the commissioned settings, or a default (check disabled) if no config is attached
    /// @note deviceConfig() returns an owned clone and is non-const, hence the const_cast; the
    ///       clone is released at the end of the call.
    RobotKinematicCheckConfig robotKinematicCheckConfig() const override {
        auto *self = const_cast<VisionOutputDevice *>(this);
        std::unique_ptr<IDeviceCfg> cfg(self->deviceConfig());
        if (auto *voutCfg = dynamic_cast<VisionOutputDeviceCfg *>(cfg.get())) {
            return voutCfg->m_kinematicCheck;
        }
        return {};
    }

    /// Serializes the base IDevice fields plus the family-level VisionOutputType tag to
    /// JSON. Concrete subclasses override to add their own transport-specific fields.
    QJsonObject toJson() const override {
        QJsonObject obj = IDevice::toJson();
        obj.insert(DEVICE_JSK_VOUT_TYPE,
                   VisionOutputTypeToString(this->visionOutputType()));
        return obj;
    }
};

} // namespace vc::device

#endif // VISION_OUTPUT_DEVICE_H
