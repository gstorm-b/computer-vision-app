#include "device_widget_factory.h"

#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_device.h"
#include "device/plc/plc_device.h"
#include "form/camera/basler_camera_widget.h"
#include "form/plc/mitsubishi_mc_device_widget.h"
#include "form/vision_output/vision_tcpip_device_widget.h"
#include "form/vision_output/vision_tcpip_client_device_widget.h"
#include "core/logger/app_logger.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/vision_output_runner.h"

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
