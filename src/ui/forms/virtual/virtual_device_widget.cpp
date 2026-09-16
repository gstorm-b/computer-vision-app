#include "ui/forms/virtual/virtual_device_widget.h"
#include "ui_virtual_device_widget.h"

#include <QMetaProperty>
#include <QVBoxLayout>

#include "core/logger/app_logger.h"
#include "core/qgadget_macro.h"
#include "device/device_capabilities.h"
#include "device/device_manager.h"
#include "runtime/plc_runner.h"
#include "ui/forms/virtual/virtual_plc_input_panel.h"

namespace {

/// Creates one browser row for `prop`, labelled with the gadget's registered display name.
/// @return the new property, or nullptr for `objectName` (Qt's own, never a device setting)
QtVariantProperty *addRow(const QMetaObject &meta, const QMetaProperty &prop,
                          const QVariant &value, QtVariantPropertyManager *manager)
{
    const QString name = QString::fromUtf8(prop.name());
    if (name == QLatin1String("objectName")) {
        return nullptr;
    }

    QtVariantProperty *row = nullptr;
    if (prop.isEnumType()) {
        row = manager->addProperty(QtVariantPropertyManager::enumTypeId(), name);
        QStringList names;
        const QMetaEnum metaEnum = prop.enumerator();
        for (int i = 0; i < metaEnum.keyCount(); ++i) {
            names << QString::fromUtf8(metaEnum.key(i));
        }
        if (row != nullptr) {
            row->setAttribute(QLatin1String("enumNames"), names);
            row->setValue(value.toInt());
        }
    } else {
        row = manager->addProperty(value.userType(), name);
        if (row != nullptr) {
            row->setValue(value);
        }
    }

    if (row != nullptr) {
        row->setPropertyName(vc::gadget_meta::displayName(meta, prop.name()));
    }
    return row;
}

} // namespace

/// Builds the panel, the banner and the property browser for `device`.
VirtualDeviceWidget::VirtualDeviceWidget(std::shared_ptr<vc::device::IDevice> device,
                                         vc::runtime::IDeviceRunner *runner,
                                         QWidget *parent)
    : IDeviceWidget(parent)
    , ui(new Ui::VirtualDeviceWidget)
    , m_device(std::move(device))
{
    ui->setupUi(this);

    initPropertyBrowser();
    // The form declares the banner and the labels; the browser is created at runtime and is
    // dropped into the placeholder the form reserved for it.
    ui->layout_browser_host->addWidget(m_propBrowser);

    if (m_device) {
        m_config.reset(m_device->deviceConfig());
    }

    connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
            this, &VirtualDeviceWidget::onPropertyValueChanged);

    buildPropertyBrowser();
    addInputPanelIfSupported(runner);
    loadConfigToWidget();
}

/// Adds the input-driving panel when `device` is a PLC whose inputs can be driven.
void VirtualDeviceWidget::addInputPanelIfSupported(vc::runtime::IDeviceRunner *runner)
{
    auto *plcRunner = dynamic_cast<vc::runtime::PlcRunner *>(runner);
    // Asked of the runner's capability, not of the device's sub-type. A device that would ignore
    // the controls must not be offered them, and the capability is the only thing that answers
    // that per device rather than per family.
    if (!plcRunner || !plcRunner->supportsInputSimulation()) {
        return;
    }

    auto *tagProvider = dynamic_cast<vc::device::IPlcTagProvider *>(m_device.get());
    ui->layout_extra_host->addWidget(new VirtualPlcInputPanel(
        plcRunner,
        tagProvider ? tagProvider->availableDigitalIoNames() : QStringList(),
        tagProvider ? tagProvider->availableWordIoNames() : QStringList(),
        this));
}

VirtualDeviceWidget::~VirtualDeviceWidget()
{
    delete ui;
}

/// Returns the id of the underlying device, or an empty string if it is gone.
QString VirtualDeviceWidget::deviceId()
{
    return m_device ? m_device->id() : QString();
}

/// Fills the browser with the device's own properties and its config gadget's.
///
/// Both groups are built from Q_PROPERTY metadata rather than a hand-written field list, so
/// a new setting on a virtual config appears here the moment it is declared. The config group
/// is created even when the gadget has no properties: an empty "Configuration" heading says
/// "there is nothing to set", whereas no heading at all reads as "something failed to load".
void VirtualDeviceWidget::buildPropertyBrowser()
{
    if (!m_device || m_variantManager == nullptr || m_variantEditor == nullptr) {
        return;
    }

    m_populating = true;

    QtProperty *deviceGroup =
        m_variantManager->addProperty(QtVariantPropertyManager::groupTypeId(), tr("Device"));
    m_variantEditor->addProperty(deviceGroup);

    const QMetaObject *deviceMeta = m_device->metaObject();
    for (int i = 0; i < deviceMeta->propertyCount(); ++i) {
        QMetaProperty prop = deviceMeta->property(i);
        QtVariantProperty *row = addRow(*deviceMeta, prop, prop.read(m_device.get()),
                                        m_variantManager);
        if (row != nullptr) {
            deviceGroup->addSubProperty(row);
        }
    }

    QtProperty *configGroup = m_variantManager->addProperty(
        QtVariantPropertyManager::groupTypeId(), tr("Configuration"));
    m_variantEditor->addProperty(configGroup);

    if (m_config) {
        const QMetaObject &configMeta = m_config->getMetaObject();
        for (int i = 0; i < configMeta.propertyCount(); ++i) {
            QMetaProperty prop = configMeta.property(i);
            QtVariantProperty *row = addRow(configMeta, prop,
                                            prop.readOnGadget(m_config.get()),
                                            m_variantManager);
            if (row != nullptr) {
                configGroup->addSubProperty(row);
            }
        }
    }

    m_populating = false;
}

/// Applies one edited property to the device name or to the config gadget.
///
/// The device's own properties are tried first and the config second, matching the order the
/// browser shows them. A rejected rename is put back rather than left showing a value the
/// device did not accept.
void VirtualDeviceWidget::onPropertyValueChanged(QtProperty *property, const QVariant &value)
{
    if (m_populating || !m_device || property == nullptr) {
        return;
    }

    const QString propName = property->propertyName();

    if (propName == vc::gadget_meta::displayName(*m_device->metaObject(), "name")
        || propName == QLatin1String("name")) {
        const QString newName = value.toString();
        if (m_device->name() == newName) {
            return;
        }
        auto *manager = m_device->deviceManager();
        if (manager != nullptr && !manager->changeDeviceName(m_device->id(), newName)) {
            LOG_USER_WARN << tr("Cannot rename device to \"%1\": the name is already in use.")
                                 .arg(newName);
            loadConfigToWidget();
        }
        return;
    }

    if (!m_config) {
        return;
    }

    // The browser row is labelled with the display name, so the property has to be found by
    // matching that back to the underlying key — the gadget knows the key, the row does not.
    const QMetaObject &configMeta = m_config->getMetaObject();
    for (int i = 0; i < configMeta.propertyCount(); ++i) {
        const QMetaProperty prop = configMeta.property(i);
        if (propName != vc::gadget_meta::displayName(configMeta, prop.name())
            && propName != QString::fromUtf8(prop.name())) {
            continue;
        }
        if (prop.writeOnGadget(m_config.get(), value)) {
            loadConfigToDevice();
        }
        return;
    }
}

/// Pushes the edited config down onto the device.
///
/// A fresh clone is handed over each time: the virtual devices take ownership of what they
/// are given and copy it into their own member, so passing m_config itself would leave this
/// widget holding a deleted pointer.
void VirtualDeviceWidget::loadConfigToDevice()
{
    if (!m_device || !m_config) {
        return;
    }
    m_device->setDeviceConfig(m_config->clone());
}

/// Refreshes the banner and every property value from the device.
void VirtualDeviceWidget::loadConfigToWidget()
{
    if (!m_device) {
        ui->lbl_summary->setText(QString());
        return;
    }

    // Names the device family AND the operator's own device name: a banner that does not say
    // WHICH device is simulated leaves them checking each one by hand.
    ui->lbl_summary->setText(
        tr("%1  —  %2")
            .arg(m_device->name(),
                 vc::device::DeviceTypeToString(m_device->deviceType())));
}
