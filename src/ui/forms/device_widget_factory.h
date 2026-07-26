#ifndef DEVICE_WIDGET_FACTORY_H
#define DEVICE_WIDGET_FACTORY_H

#include <memory>

#include <QWidget>

#include "DockWidget.h"

namespace vc::device {
class IDevice;
}

namespace vc::runtime {
class IDeviceRunner;
}

/// Factory that maps a device (by its concrete device type/subtype) to the matching
/// device-specific control widget (e.g. Basler camera, Mitsubishi MC PLC, vision output).
class DeviceWidgetFactory {
public:
    /// Creates and returns the control widget appropriate for `device`'s runtime type/subtype,
    /// wiring it to `runner` and, if given, hosting it inside `dock`.
    /// @param device the device to create a widget for; dispatch is based on its deviceType()
    ///        and, where applicable, its subtype (camera type, PLC type, vision output type)
    /// @param runner the device runner to bind to the widget, cast to the subtype-specific
    ///        runner interface expected by the created widget
    /// @param dock optional dock widget host passed through to the created widget
    /// @param parent optional parent widget passed through to the created widget
    /// @return a newly allocated widget owned by the caller, or nullptr if `device` is null
    ///         or its type/subtype is not supported
    static QWidget *createDeviceWidget(
        const std::shared_ptr<vc::device::IDevice> &device,
        vc::runtime::IDeviceRunner *runner,
        ads::CDockWidget *dock = nullptr,
        QWidget *parent = nullptr);
};

#endif // DEVICE_WIDGET_FACTORY_H
