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

class DeviceWidgetFactory {
public:
    static QWidget *createDeviceWidget(
        const std::shared_ptr<vc::device::IDevice> &device,
        vc::runtime::IDeviceRunner *runner,
        ads::CDockWidget *dock = nullptr,
        QWidget *parent = nullptr);
};

#endif // DEVICE_WIDGET_FACTORY_H
