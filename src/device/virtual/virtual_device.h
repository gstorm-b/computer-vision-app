#ifndef VIRTUAL_DEVICE_H
#define VIRTUAL_DEVICE_H

/**
 * @file virtual_device.h
 * @brief isVirtualDevice() — the single answer to "is this device simulated?".
 */

#include "device/camera/camera_device.h"
#include "device/idevice.h"
#include "device/output_device/vision_output_device.h"
#include "device/plc/plc_device.h"

namespace vc::device {

/**
 * @brief True when `device` is one of the hardware-free virtual sub-types.
 *
 * One predicate, deliberately, because two callers must never disagree about the answer:
 * DeviceWidgetFactory uses it to pick the widget, and the project tree / device badge / task
 * log use it to mark the device as simulated. A device that got a real panel but no marker —
 * or a marker but a real panel — is worse than either failure alone, because it makes the
 * marker untrustworthy.
 *
 * @param[in] device device to test; nullptr is not virtual
 * @return true if the device is a virtual camera, PLC or vision output
 */
inline bool isVirtualDevice(const IDevice *device)
{
    if (device == nullptr) {
        return false;
    }

    if (const auto *camera = qobject_cast<const CameraDevice *>(device)) {
        return camera->cameraType() == CameraType::VirtualCamera;
    }
    if (const auto *plc = qobject_cast<const PlcDevice *>(device)) {
        return plc->plcType() == PlcType::VirtualPlc;
    }
    if (const auto *output = qobject_cast<const VisionOutputDevice *>(device)) {
        return output->visionOutputType() == VisionOutputType::VirtualVisionOutput;
    }
    // Robots have no virtual sub-type. Phase 7 / D3 deliberately did not add one: nothing
    // exercises it, and an abstraction with no consumer is the thing AGENT.md warns against.
    return false;
}

} // namespace vc::device

#endif // VIRTUAL_DEVICE_H
