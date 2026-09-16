#include "device_widget_factory.h"

#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_device.h"
#include "device/plc/plc_device.h"
#include "device/virtual/virtual_device.h"
#include "ui/forms/camera/basler_camera_widget.h"
#include "ui/forms/camera/jai_camera_widget.h"
#include "ui/forms/virtual/virtual_device_widget.h"
#include "ui/forms/plc/mitsubishi_mc_device_widget.h"
#include "ui/forms/plc/modbus_device_widget.h"
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

    // Checked BEFORE the family switch, not inside it. Both family arms below now enumerate
    // their sub-types explicitly, but a virtual device still must not reach them: it needs the
    // virtual panel, whose whole job is to say out loud that this device is running on nothing.
    // (When those arms were equality rejections, a virtual device also fell straight through to
    // nullptr and the task page substituted "No configuration panel available for this device" —
    // a soft failure that read as a missing panel rather than a missing registration.)
    if (vc::device::isVirtualDevice(device.get())) {
        // The runner is passed through for the virtual PLC's input-driving panel, which needs it
        // to reach the device thread. The widget stays generic: it asks the runner whether the
        // device supports driven inputs and adds nothing when it does not.
        return new VirtualDeviceWidget(device, runner, parent);
    }

    switch (device->deviceType()) {
    case vc::device::DeviceType::Camera: {
        auto *camera = qobject_cast<vc::device::CameraDevice *>(device.get());
        if (!camera) {
            LOG_DEV_ERR << "DeviceWidgetFactory: camera device is not a CameraDevice"
                        << device->id();
            return nullptr;
        }
        auto *cameraRunner = qobject_cast<vc::runtime::CameraRunner *>(runner);
        // A switch, not an equality rejection — the same change the PLC arm needed below. The
        // old `cameraType() != BaslerGigE -> nullptr` meant every camera sub-type added after
        // Basler would silently get "No configuration panel available for this device", a soft
        // failure that reads as a missing panel rather than a missing registration.
        switch (camera->cameraType()) {
        case vc::device::CameraType::BaslerGigE:
            return new BaslerCameraWidget(device, cameraRunner, dock, parent);
        case vc::device::CameraType::JaiGigE:
            return new JaiCameraWidget(device, cameraRunner, dock, parent);
        case vc::device::CameraType::VirtualCamera:
            // Claimed by the isVirtualDevice() check above, so this switch never sees one;
            // listed so the enumeration stays exhaustive.
        case vc::device::CameraType::Realsense:
        case vc::device::CameraType::BaslerUSB:
        case vc::device::CameraType::CamType:
            break;
        }
        LOG_DEV_ERR << "DeviceWidgetFactory: unsupported camera subtype"
                    << vc::device::CameraTypeToString(camera->cameraType())
                    << device->id();
        return nullptr;
    }

    case vc::device::DeviceType::PLC: {
        auto *plc = qobject_cast<vc::device::PlcDevice *>(device.get());
        if (!plc) {
            LOG_DEV_ERR << "DeviceWidgetFactory: PLC device is not a PlcDevice"
                        << device->id();
            return nullptr;
        }
        auto *plcRunner = qobject_cast<vc::runtime::PlcRunner *>(runner);
        // A switch, not an equality rejection. This arm used to read
        // `plcType() != MitsubishiMc -> nullptr`, which meant every PLC sub-type added after
        // Mitsubishi would silently get "No configuration panel available for this device" — a
        // soft failure that reads as a missing panel rather than a missing registration.
        switch (plc->plcType()) {
        case vc::device::PlcType::MitsubishiMc:
            return new MitsubishiMcDeviceWidget(device, plcRunner, dock, parent);
        case vc::device::PlcType::ModbusTcpClient:
        case vc::device::PlcType::ModbusTcpServer:
            // One widget serves both: they differ only in the connection card.
            return new ModbusDeviceWidget(device, plcRunner, dock, parent);
        case vc::device::PlcType::VirtualPlc:
            // Handled by the isVirtualDevice() check above; reaching here means that check and
            // this enum disagree, which is worth a loud line rather than a silent fallthrough.
        case vc::device::PlcType::PlcTypeNone:
            break;
        }
        LOG_DEV_ERR << "DeviceWidgetFactory: unsupported PLC subtype"
                    << device->id()
                    << static_cast<int>(plc->plcType());
        return nullptr;
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
