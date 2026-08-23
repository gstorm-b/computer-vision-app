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

/**
 * @file device_widget_factory.h
 * @brief DeviceWidgetFactory — maps a device to its matching device-specific control widget.
 */

/**
 * @class DeviceWidgetFactory
 * @brief Factory that maps a device (by its concrete device type/subtype) to the matching
 *        device-specific control widget (e.g. Basler camera, Mitsubishi MC PLC, vision output).
 */
class DeviceWidgetFactory {
public:
    /**
     * @brief Creates and returns the control widget appropriate for @p device's runtime
     *        type/subtype, wiring it to @p runner and, if given, hosting it inside @p dock.
     *
     * @param[in] device the device to create a widget for; dispatch is based on its deviceType()
     *        and, where applicable, its subtype (camera type, PLC type, vision output type)
     * @param[in] runner the device runner to bind to the widget, cast to the subtype-specific
     *        runner interface expected by the created widget
     * @param[in] dock optional dock widget host passed through to the created widget
     * @param[in] parent optional parent widget passed through to the created widget
     * @return a newly allocated widget owned by the caller, or nullptr if @p device is null
     *         or its type/subtype is not supported
     */
    static QWidget *createDeviceWidget(
        const std::shared_ptr<vc::device::IDevice> &device,
        vc::runtime::IDeviceRunner *runner,
        ads::CDockWidget *dock = nullptr,
        QWidget *parent = nullptr);
};

#endif // DEVICE_WIDGET_FACTORY_H
