#ifndef VIRTUAL_DEVICE_WIDGET_H
#define VIRTUAL_DEVICE_WIDGET_H

/**
 * @file virtual_device_widget.h
 * @brief VirtualDeviceWidget — the device panel shown for every virtual device.
 */

#include <memory>

#include "device/idevice.h"
#include "device/idevice_config.h"
#include "ui/forms/device_widget.h"

class QtProperty;

QT_BEGIN_NAMESPACE
namespace Ui { class VirtualDeviceWidget; }
QT_END_NAMESPACE

/**
 * @class VirtualDeviceWidget
 * @brief Panel for a virtual camera, PLC or vision-output device.
 *
 * **One widget for all three families, on purpose.** The editable parameters come from the
 * device's own config gadget through its Q_PROPERTY metadata, so this class needs to know
 * nothing about any family: a virtual camera shows its frame settings, a virtual PLC shows
 * nothing because it genuinely has nothing to set, and a fourth virtual family would work on
 * the day it is written without touching this file.
 *
 * The banner above the properties is one of the three risk-R8 markers. All three use the same
 * vc::device::isVirtualDevice() predicate, so they cannot disagree about what the device is.
 *
 * @note An earlier version of this widget deliberately had no property browser, on the
 *       reasoning that a virtual device has nothing worth configuring. That was wrong twice
 *       over: the virtual camera's `imagePath` is what makes a hardware-free localization
 *       cycle able to find anything at all, and reaching it without a UI meant hand-editing
 *       the project file. It also crashed the host, which assumed every IDeviceWidget owns a
 *       browser.
 */
class VirtualDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    /// Builds the panel, the banner and the property browser for `device`.
    explicit VirtualDeviceWidget(std::shared_ptr<vc::device::IDevice> device,
                                 QWidget *parent = nullptr);
    ~VirtualDeviceWidget() override;

    /// Returns the id of the underlying device, or an empty string if it is gone.
    QString deviceId() override;
    /// Pushes the edited config down onto the device.
    void loadConfigToDevice() override;
    /// Refreshes the banner and every property value from the device.
    void loadConfigToWidget() override;

private slots:
    /// Applies one edited property to the device name or to the config gadget.
    void onPropertyValueChanged(QtProperty *property, const QVariant &value);

private:
    /// Fills the browser with the device's own properties and its config gadget's.
    void buildPropertyBrowser();

    Ui::VirtualDeviceWidget *ui;
    std::shared_ptr<vc::device::IDevice> m_device;
    /// Working copy of the device's config; edits are applied here then pushed down.
    std::unique_ptr<vc::device::IDeviceCfg> m_config;
    /// True while the browser is being populated, so programmatic writes are not treated
    /// as user edits.
    bool m_populating{false};
};

#endif // VIRTUAL_DEVICE_WIDGET_H
