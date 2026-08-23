#ifndef VIRTUAL_VISION_OUTPUT_CONFIG_H
#define VIRTUAL_VISION_OUTPUT_CONFIG_H

/**
 * @file virtual_vision_output_config.h
 * @brief VirtualVisionOutputCfg — configuration for VirtualVisionOutputDevice.
 */

#include "core/qgadget_macro.h"
#include "device/output_device/vision_output_config.h"

namespace vc::device {

/**
 * @class VirtualVisionOutputCfg
 * @brief Configuration for a vision-output device that transmits nothing.
 *
 * Adds no transport fields of its own — there is no address, no port and no heartbeat to
 * configure. What it does inherit matters: `VisionOutputDeviceCfg` carries the **robot
 * kinematic check**, which is a property of the picking geometry rather than of the wire, so
 * a virtual output still validates reachability exactly as a real one does. Turning that off
 * here would make a hardware-free run pass poses a real robot could never reach.
 */
class VirtualVisionOutputCfg : public VisionOutputDeviceCfg {
    Q_GADGET

public:
    /// Constructs the config; the inherited kinematic-check settings carry the state.
    explicit VirtualVisionOutputCfg() = default;

    /// Returns this class's Qt meta-object, used by the gadget property-browser machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::VirtualVisionOutputCfg::staticMetaObject;
    }

    /// Always VisionOutputType::VirtualVisionOutput.
    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VirtualVisionOutput;
    }

    /// Allocates and returns a heap copy; the caller owns it.
    IDeviceCfg *clone() override {
        return new VirtualVisionOutputCfg(*this);
    }
};

} // namespace vc::device

#endif // VIRTUAL_VISION_OUTPUT_CONFIG_H
