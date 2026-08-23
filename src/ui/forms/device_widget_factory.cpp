#include "device_widget_factory.h"

#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_device.h"
#include "device/plc/plc_device.h"
#include "device/virtual/virtual_device.h"
#include "ui/forms/camera/basler_camera_widget.h"
#include "ui/forms/virtual/virtual_device_widget.h"
#include "ui/forms/plc/mitsubishi_mc_device_widget.h"
#include "ui/forms/vision_output/vision_tcpip_device_widget.h"
#include "ui/forms/vision_output/vision_tcpip_client_device_widget.h"
#include "core/logger/app_logger.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/vision_output_runner.h"

/// Dispatches on device->deviceType() (and the relevant subtype enum) to construct the
/// matching device widget; logs via LOG_DEV_ERR and returns nullptr for a null device or any
/// unsupported type/subtype combination.
QWidget *DeviceWidgetFactory::createDeviceWidget(
    const std::shared_ptr<vc::device::IDevice> &device,
    vc::runtime::IDeviceRunner *runner,
    ads::CDockWidget *dock,
    QWidget *parent)
{
    if (!device) {
        LOG_DEV_ERR << "DeviceWidgetFactory: null device";
        return nullptr;
    }

    // Checked BEFORE the family switch, not inside it. The Camera and PLC arms below are
    // written as equality rejections (`cameraType() != BaslerGigE`), so a virtual device
    // would fall straight through to nullptr and the task page would substitute
    // "No configuration panel available for this device" — a soft failure that reads as a
    // missing panel rather than a missing registration, and hides the one thing the operator
    // most needs to know about this device.
    if (vc::device::isVirtualDevice(device.get())) {
        return new VirtualDeviceWidget(device, parent);
    }

    switch (device->deviceType()) {
    case vc::device::DeviceType::Camera: {
        auto *camera = qobject_cast<vc::device::CameraDevice *>(device.get());
        if (!camera || camera->cameraType() != vc::device::CameraType::BaslerGigE) {
            LOG_DEV_ERR << "DeviceWidgetFactory: unsupported camera subtype"
                        << device->id();
            return nullptr;
        }
        auto *cameraRunner = qobject_cast<vc::runtime::CameraRunner *>(runner);
        return new BaslerCameraWidget(device, cameraRunner, dock, parent);
    }

    case vc::device::DeviceType::PLC: {
        auto *plc = qobject_cast<vc::device::PlcDevice *>(device.get());
        if (!plc || plc->plcType() != vc::device::PlcType::MitsubishiMc) {
            LOG_DEV_ERR << "DeviceWidgetFactory: unsupported PLC subtype"
                        << device->id();
            return nullptr;
        }
        auto *plcRunner = qobject_cast<vc::runtime::PlcRunner *>(runner);
        return new MitsubishiMcDeviceWidget(device, plcRunner, dock, parent);
    }

    case vc::device::DeviceType::VisionOutput: {
        auto *output = qobject_cast<vc::device::VisionOutputDevice *>(device.get());
        if (!output) {
            LOG_DEV_ERR << "DeviceWidgetFactory: unsupported vision output subtype"
                        << device->id();
            return nullptr;
        }
        auto *outputRunner = qobject_cast<vc::runtime::VisionOutputRunner *>(runner);
        switch (output->visionOutputType()) {
        case vc::device::VisionOutputType::VisionTCPIP:
            return new VisionTcpipDeviceWidget(device, outputRunner, dock, parent);
        case vc::device::VisionOutputType::VisionTcpipClient:
            return new VisionTcpipClientDeviceWidget(device, outputRunner, dock, parent);
        default:
            LOG_DEV_ERR << "DeviceWidgetFactory: unsupported vision output subtype"
                        << device->id();
            return nullptr;
        }
    }

    default:
        LOG_DEV_ERR << "DeviceWidgetFactory: unsupported device family"
                    << device->id()
                    << static_cast<int>(device->deviceType());
        return nullptr;
    }
}
