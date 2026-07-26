#include "vision_tcpip_client_device_widget.h"
#include "ui_vision_tcpip_client_device_widget.h"

#include <QDoubleSpinBox>
#include <QHeaderView>

/// Creates and configures a QtVariantProperty for a single Qt meta-property (enum properties get
/// their enumerator names translated via QCoreApplication::translate(); other types are added by
/// their QVariant user type). Also applies the `<prop>_name`/`<prop>_min`/`<prop>_max` classinfo
/// entries as the property's display name/minimum/maximum, and disables the property if `prop`
/// is not writable.
/// @param meta meta-object used to resolve enum names and `_name`/`_min`/`_max` classinfo for `prop`
/// @param prop the meta-property being converted into a browser property
/// @param value the property's current value (read from the owning object/gadget)
/// @param manager property manager used to create the QtVariantProperty
/// @param browser unused by this helper (kept for signature symmetry with the populate* helpers)
/// @return the newly created property, or nullptr for "objectName" or if the manager could not
/// create a property for the value's type
static QtVariantProperty* addPropertyToBrowser(const QMetaObject &meta, QMetaProperty &prop, QVariant &value,
                                               QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {

    QString propName = prop.name();
    if (propName == "objectName") {
        // ignore objectName
        return nullptr;
    }

    QtVariantProperty *variantProp = nullptr;
    // enum type Check
    if (prop.isEnumType()) {
        variantProp = manager->addProperty(QtVariantPropertyManager::enumTypeId(), propName);

        // get enum names
        QStringList enumNames;
        QMetaEnum metaEnum = prop.enumerator();
        for (int j = 0; j < metaEnum.keyCount(); ++j) {
            const char* key = metaEnum.key(j);
            QString translatedName = QCoreApplication::translate(meta.className(), key);
            enumNames << translatedName;
        }
        variantProp->setAttribute(QLatin1String("enumNames"), enumNames);
        // set enum value
        variantProp->setValue(value.toInt());

    } else {
        // normal data types handle (int, QString, bool, QColor, v.v.)
        int typeId = value.userType();
        variantProp = manager->addProperty(typeId, propName);
        if (variantProp) {
            variantProp->setValue(value);
        }
    }

    if (!variantProp) {
        return nullptr;
    }

    int displayNameIdx = meta.indexOfClassInfo(QString("%1_name").arg(propName).toUtf8());
    if (displayNameIdx != -1) {
        const QString displayName = meta.classInfo(displayNameIdx).value();
        if (!displayName.isEmpty()) {
            variantProp->setDisplayName(displayName);
        }
    }

    // --- set attributes---
    int minIdx = meta.indexOfClassInfo(QString("%1_min").arg(propName).toUtf8());
    if (minIdx != -1) {
        variantProp->setAttribute("minimum", QString(meta.classInfo(minIdx).value()).toInt());
    }

    int maxIdx = meta.indexOfClassInfo(QString("%1_max").arg(propName).toUtf8());
    if (maxIdx != -1) {
        variantProp->setAttribute("maximum", QString(meta.classInfo(maxIdx).value()).toInt());
    }

    if (!prop.isWritable()) {
        variantProp->setEnabled(false);
    }

    return variantProp;
}

/// Populates `browser` with a "Device Information" group listing every Q_PROPERTY exposed by
/// `gadget`'s meta-object (id, name, connection status, etc.), one row per property.
/// @param gadget the device instance whose properties are enumerated via QObject::property()
/// @param manager property manager used to create the group and child properties
/// @param browser tree browser the "Device Information" group is added to
static void populateBrowser_Device(vc::device::IDevice *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Device Information"));
    browser->addProperty(topItem);
    const QMetaObject *meta = gadget->metaObject();

    int propCount = meta->propertyCount();
    for (int i = 0; i < propCount; ++i) {
        QMetaProperty prop = meta->property(i);
        QVariant value = gadget->property(prop.name());
        QtVariantProperty *variantProp = addPropertyToBrowser(*meta, prop, value, manager, browser);
        if (variantProp) {
            topItem->addSubProperty(variantProp);
        }
    }
}

/// Populates `browser` with an "Output configuration" group listing every property of the
/// `VisionTcpipClientDeviceCfg` Q_GADGET, read via QMetaProperty::readOnGadget (server
/// address/ports/kinematic-check config).
/// @param gadget the client output configuration whose properties are enumerated
/// @param manager property manager used to create the group and child properties
/// @param browser tree browser the "Output configuration" group is added to
static void populateBrowser_VisionOutput(vc::device::VisionTcpipClientDeviceCfg *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Output configuration"));
    browser->addProperty(topItem);
    const QMetaObject &meta = gadget->getMetaObject();

    int propCount = meta.propertyCount();
    for (int i = 0; i < propCount; ++i) {
        QMetaProperty prop = meta.property(i);
        QVariant value = prop.readOnGadget(gadget);

        QtVariantProperty *variantProp = addPropertyToBrowser(meta, prop, value, manager, browser);
        if (variantProp) {
            topItem->addSubProperty(variantProp);
        }
    }
}

/// Generic property-browser populator for any Q_GADGET type: adds a group named `groupName` and
/// one row per meta-property (values read via QMetaProperty::readOnGadget). Every added property
/// is force-disabled, so these groups are always display-only regardless of whether the
/// underlying property is writable.
/// @param meta static meta-object of the gadget type (e.g. VisionTcpipRuntimeState::staticMetaObject)
/// @param gadget pointer to the gadget instance to read values from
/// @param groupName label shown for the top-level group item in the browser
/// @param manager property manager used to create the group and child properties
/// @param browser tree browser the group is added to
static void populateBrowser_Gadget(const QMetaObject &meta,
                                   const void *gadget,
                                   const QString &groupName,
                                   QtVariantPropertyManager *manager,
                                   QtTreePropertyBrowser *browser)
{
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(), groupName);
    browser->addProperty(topItem);

    const int propCount = meta.propertyCount();
    for (int i = 0; i < propCount; ++i) {
        QMetaProperty prop = meta.property(i);
        QVariant value = prop.readOnGadget(gadget);
        QtVariantProperty *variantProp = addPropertyToBrowser(meta, prop, value, manager, browser);
        if (!variantProp) {
            continue;
        }

        variantProp->setEnabled(false);
        topItem->addSubProperty(variantProp);
    }
}

/// Constructs the widget for `dv`, wiring it to `runner` for connect/disconnect/send requests
/// and to `dock` as its hosting dock widget; builds the generated `.ui` form and runs
/// initWidget() to finish setup.
/// @param dv the vision TCP/IP client device instance this widget controls/displays
/// @param runner runner that performs connect/disconnect/send requests on the widget's behalf
/// @param dock hosting dock widget, if any
/// @param parent parent widget
VisionTcpipClientDeviceWidget::VisionTcpipClientDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                                             vc::runtime::VisionOutputRunner *runner,
                                                             ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent),
    ui(new Ui::VisionTcpipClientDeviceWidget),
    m_device(dv),
    m_dock(dock),
    m_runner(runner)  {

    ui->setupUi(this);
    initWidget();
}

/// Destroys the widget, deleting the generated `ui` form.
VisionTcpipClientDeviceWidget::~VisionTcpipClientDeviceWidget()
{
    delete ui;
}

/// Returns the id of the device this widget represents (delegates to IDevice::id()).
QString VisionTcpipClientDeviceWidget::deviceId() {
    return m_device->id();
}

/// Pushes the widget's in-memory `m_config` to the device via
/// VisionTcpipClientDevice::setVisionTcpipClientConfig(); no-op if no output device is attached.
void VisionTcpipClientDeviceWidget::loadConfigToDevice() {
    if (!m_output_device) return;
    m_output_device->setVisionTcpipClientConfig(m_config);
}

/// Pulls the current configuration from the device into `m_config` and reflects it in the UI
/// fields (server address, data/heartbeat ports, kinematic-check widget), guarding the update
/// with `m_populating_browser` so the field-changed handlers don't write it straight back.
/// No-op if no output device is attached.
void VisionTcpipClientDeviceWidget::loadConfigToWidget() {
    if (!m_output_device) return;
    m_config = m_output_device->visionTcpipClientConfig();

    m_populating_browser = true;
    ui->ledit_ip->setText(m_config.m_serverAddress);
    ui->spb_port_data->setValue(m_config.m_mainPort);
    ui->spb_port_heartbeat->setValue(m_config.m_heartbeatPort);
    if (m_kcheckWidget) m_kcheckWidget->setConfig(m_config.m_kinematicCheck);
    m_populating_browser = false;
}

/// One-time widget setup run from the constructor: builds the property browser, loads the
/// dark/light theme stylesheets, wires the runner/device signals (connection state, main-link
/// state) and UI controls (connect button, config fields, position table buttons, kinematic
/// check widget), seeds the position table with one row, and sets the initial connection visual
/// from the device's current connection state.
/// @note connections to `m_output_device`/`m_runner` are only made if `m_device`/`m_runner` are
/// non-null; without a runner an error is logged and connect/disconnect controls stay inert.
void VisionTcpipClientDeviceWidget::initWidget() {
    initPropertyBrowser();
    setupThemeReload(QStringLiteral(":/styles/vision_tcpip_client_device_widget_dark.qss"),
                    QStringLiteral(":/styles/vision_tcpip_client_device_widget_light.qss"));

    if (m_device) {
        m_output_device = static_cast<vc::device::VisionTcpipClientDevice*>(m_device.get());

        // ── Wire to runner (NOT to device directly) ──────────────────────────
        // The runner forwards device signals onto the GUI thread via
        // QueuedConnection, so it is safe to update widgets from these slots.
        // Widget never owns a QThread.
        if (m_runner) {
            connect(m_runner, &vc::runtime::VisionOutputRunner::connectStatusChanged,
                    this,     &VisionTcpipClientDeviceWidget::onConnectionStateChanged);
        } else {
            LOG_DEV_ERR << "VisionTcpipClientDeviceWidget: no runner provided - control disabled";
        }

        loadConfigToWidget();
        populateBrowser();

        connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                this, &VisionTcpipClientDeviceWidget::onPropertyValueChanged);

        connect(m_output_device, &vc::device::VisionTcpipClientDevice::mainClientStateChanged,
                this, &VisionTcpipClientDeviceWidget::onMainClientStateChanged,
                Qt::QueuedConnection);
    }

    connect(ui->btn_connect, &QPushButton::clicked,
            this, &VisionTcpipClientDeviceWidget::onConnectClicked);

    // ── Config input fields ───────────────────────────────────────────────
    connect(ui->ledit_ip,          &QLineEdit::editingFinished,
            this, &VisionTcpipClientDeviceWidget::onFieldConfigChanged);
    connect(ui->spb_port_data,     qOverload<int>(&QSpinBox::valueChanged),
            this, &VisionTcpipClientDeviceWidget::onFieldConfigChanged);
    connect(ui->spb_port_heartbeat,qOverload<int>(&QSpinBox::valueChanged),
            this, &VisionTcpipClientDeviceWidget::onFieldConfigChanged);

    // ── Send Result section ────────────────────────────────────────────────
    ui->tbl_positions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tbl_positions->verticalHeader()->setDefaultSectionSize(28);
    addPositionRow();

    connect(ui->btn_add_row,    &QPushButton::clicked, this, &VisionTcpipClientDeviceWidget::onAddRow);
    connect(ui->btn_remove_row, &QPushButton::clicked, this, &VisionTcpipClientDeviceWidget::onRemoveRow);
    connect(ui->btn_send_result,&QPushButton::clicked, this, &VisionTcpipClientDeviceWidget::onSendResult);

    m_kcheckWidget = ui->widget_kinematic_check;
    m_kcheckWidget->setConfig(m_config.m_kinematicCheck);
    connect(m_kcheckWidget, &RobotKinematicCheckWidget::configChanged, this, [this]() {
        m_config.m_kinematicCheck = m_kcheckWidget->config();
        saveConfig();
    });
    connect(m_kcheckWidget, &RobotKinematicCheckWidget::testerWidgetVisibleChanged, this, [this]() {
        this->parentWidget()->setMinimumSize(this->minimumSizeHint());
    });

    updateConnectionVisual(m_device && m_device->isDeviceConnected()
                               ? vc::device::ConnectStatus::Connected
                               : vc::device::ConnectStatus::Disconnected);
}

/// Handles edits made in the property browser: routes a changed value back to either the device
/// (currently only the "name" property, renamed via DeviceManager::changeDeviceName(), reverting
/// the displayed value and warning the user if the new name is already taken) or to the output
/// config gadget (`m_config`, written via QMetaProperty::writeOnGadget then persisted through
/// saveConfig()). Ignored while `m_populating_browser` is true to avoid feedback loops.
/// @param property the browser property whose value changed
/// @param variant the new value
void VisionTcpipClientDeviceWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant) {
    if (m_populating_browser) {
        return;
    }

    QString propName = property->propertyName();

    const QMetaObject *idevice_meta = m_device->metaObject();
    const QMetaObject &meta_cfg = m_config.getMetaObject();

    // abstract device
    int index = idevice_meta->indexOfProperty(propName.toUtf8());
    if (index != -1) {
        if (propName == "name") {
            QString new_name = variant.toString();
            // avoid loop made by signal valueChanged
            if (m_device->name() == new_name) {
                return;
            }

            if (!m_device->deviceManager()->changeDeviceName(m_device->id(), new_name)) {
                LOG_USER_WARN << tr("Cannot rename device to \"%1\": the name is "
                                    "already in use.").arg(new_name);
                m_variantManager->setValue(property, m_device->name());
            }
        }
        return;
    }

    // output config
    index = meta_cfg.indexOfProperty(propName.toUtf8());
    if (index != -1) {
        QMetaProperty mProp = meta_cfg.property(index);
        mProp.writeOnGadget(&m_config, variant);
        this->saveConfig();
        return;
    }
}

/// Persists `m_config` to the device via VisionTcpipClientDevice::setVisionTcpipClientConfig().
void VisionTcpipClientDeviceWidget::saveConfig() {
    m_output_device->setVisionTcpipClientConfig(m_config);
}

/// Placeholder hook for refreshing the config from the device; currently only guards against a
/// null `m_device` and otherwise does nothing.
void VisionTcpipClientDeviceWidget::refreshConfig() {
    if (!m_device) {
        return;
    }
}

/// Slot for the IP/port field editing-finished/valueChanged signals: copies the trimmed IP and
/// both port spin-box values into `m_config`, persists them via saveConfig(), and repopulates
/// the property browser to reflect the change. Ignored while `m_populating_browser` is true.
void VisionTcpipClientDeviceWidget::onFieldConfigChanged() {
    if (m_populating_browser) return;
    m_config.m_serverAddress  = ui->ledit_ip->text().trimmed();
    m_config.m_mainPort       = ui->spb_port_data->value();
    m_config.m_heartbeatPort  = ui->spb_port_heartbeat->value();
    saveConfig();
    populateBrowser();
}

/// Slot for the connect/disconnect button: requests a disconnect via the runner if the device
/// is currently connected, connecting, or recovering (LostConnected); otherwise requests a new
/// connection. No-op if no runner is attached.
void VisionTcpipClientDeviceWidget::onConnectClicked() {
    if (!m_runner) return;
    // The client is "active" (dialing / connected / recovering) for any status
    // other than the idle/failed ones, so the button can cancel an in-progress
    // dial as well as disconnect an established link.
    const vc::device::ConnectStatus st =
        m_device ? m_device->connectStatus() : vc::device::ConnectStatus::Disconnected;
    const bool active = (st == vc::device::ConnectStatus::Connected
                         || st == vc::device::ConnectStatus::Connecting
                         || st == vc::device::ConnectStatus::LostConnected);
    if (active) {
        m_runner->requestDisconnect();
    } else {
        m_runner->requestConnect();
    }
}

/// Slot invoked when the runner reports a connection state change: updates the connect
/// button/status visuals, repopulates the property browser, and refreshes the send-result
/// section's enabled state from the device's current main-link connection flag.
/// @param state the new connection status reported by the runner
void VisionTcpipClientDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state) {
    updateConnectionVisual(state);
    populateBrowser();
    const bool mainConnected = m_output_device
                               && m_output_device->runtimeState().mainClientConnected;
    updateSendSection(mainConnected);
}

/// Rebuilds the entire property browser from scratch: clears the variant manager, then adds the
/// "Device Information", "Output configuration", and (if an output device is attached) read-only
/// "Runtime State"/"Diagnostics" groups from fresh snapshots of the device's state. Editor
/// signals are blocked and `m_populating_browser` is set for the duration so this rebuild
/// doesn't trigger onPropertyValueChanged().
void VisionTcpipClientDeviceWidget::populateBrowser() {
    m_variantEditor->blockSignals(true);
    m_populating_browser = true;

    m_variantManager->clear();

    populateBrowser_Device(m_device.get(), m_variantManager, m_variantEditor);
    populateBrowser_VisionOutput(&m_config, m_variantManager, m_variantEditor);
    if (m_output_device) {
        const vc::device::VisionTcpipRuntimeState runtimeState = m_output_device->runtimeState();
        const vc::device::VisionTcpipDiagnostics diagnostics = m_output_device->diagnostics();
        populateBrowser_Gadget(vc::device::VisionTcpipRuntimeState::staticMetaObject,
                               &runtimeState,
                               QLatin1String("Runtime State"),
                               m_variantManager,
                               m_variantEditor);
        populateBrowser_Gadget(vc::device::VisionTcpipDiagnostics::staticMetaObject,
                               &diagnostics,
                               QLatin1String("Diagnostics"),
                               m_variantManager,
                               m_variantEditor);
    }

    m_populating_browser = false;
    m_variantEditor->blockSignals(false);
}

/// Slot for VisionTcpipClientDevice::mainClientStateChanged(): forwards the new main-link
/// connection state to updateSendSection().
/// @param connected whether the device's main data link is currently connected
void VisionTcpipClientDeviceWidget::onMainClientStateChanged(bool connected) {
    updateSendSection(connected);
}

/// Enables/disables the "Send Result" button and updates the hint label's text and `sendState`
/// dynamic property (repolishing the label) to reflect whether the main data link is connected.
/// @param mainLinkConnected whether the device's main data link is currently connected
void VisionTcpipClientDeviceWidget::updateSendSection(bool mainLinkConnected) {
    ui->btn_send_result->setEnabled(mainLinkConnected);
    ui->lbl_send_hint->setText(mainLinkConnected
                               ? tr("Connected — ready to send")
                               : tr("Not connected"));
    const QByteArray state = mainLinkConnected ? "ready" : "idle";
    ui->lbl_send_hint->setProperty("sendState", state);
    ui->lbl_send_hint->style()->unpolish(ui->lbl_send_hint);
    ui->lbl_send_hint->style()->polish(ui->lbl_send_hint);
}

/// Appends a new row to the position table, populating it with one borderless, button-less
/// QDoubleSpinBox per column (x, y, z, r) preset to `x`/`y`/`z`/`r`, with a ±99999 range and 3
/// decimal places.
/// @param x initial value for the x column's spin box
/// @param y initial value for the y column's spin box
/// @param z initial value for the z column's spin box
/// @param r initial value for the r column's spin box
void VisionTcpipClientDeviceWidget::addPositionRow(double x, double y, double z, double r) {
    auto *tbl = ui->tbl_positions;
    const int row = tbl->rowCount();
    tbl->insertRow(row);

    const double vals[4] = {x, y, z, r};
    for (int col = 0; col < 4; ++col) {
        auto *spb = new QDoubleSpinBox(tbl);
        spb->setRange(-99999.0, 99999.0);
        spb->setDecimals(3);
        spb->setValue(vals[col]);
        spb->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spb->setFrame(false);
        tbl->setCellWidget(row, col, spb);
    }
}

/// Slot for the "Add row" button: appends a new (blank) position row, capped at 20 rows total.
void VisionTcpipClientDeviceWidget::onAddRow() {
    if (ui->tbl_positions->rowCount() < 20) {
        addPositionRow();
    }
}

/// Slot for the "Remove row" button: removes the currently selected row, or the last row if
/// none is selected and the table isn't empty.
void VisionTcpipClientDeviceWidget::onRemoveRow() {
    const int row = ui->tbl_positions->currentRow();
    if (row >= 0) {
        ui->tbl_positions->removeRow(row);
    } else if (ui->tbl_positions->rowCount() > 0) {
        ui->tbl_positions->removeRow(ui->tbl_positions->rowCount() - 1);
    }
}

/// Slot for the "Send Result" button: reads every row of the position table into a
/// VisionOutputPosition (skipping any cell whose widget doesn't cast to QDoubleSpinBox) and
/// hands the resulting list to the runner via requestSendResult(). No-op unless a runner and
/// output device are present and the device's main link is connected.
void VisionTcpipClientDeviceWidget::onSendResult() {
    if (!m_runner || !m_output_device || !m_output_device->isMainClientConnected()) {
        return;
    }
    QVector<vc::device::VisionOutputPosition> positions;
    auto *tbl = ui->tbl_positions;
    for (int row = 0; row < tbl->rowCount(); ++row) {
        vc::device::VisionOutputPosition pos;
        auto *sx = qobject_cast<QDoubleSpinBox*>(tbl->cellWidget(row, 0));
        auto *sy = qobject_cast<QDoubleSpinBox*>(tbl->cellWidget(row, 1));
        auto *sz = qobject_cast<QDoubleSpinBox*>(tbl->cellWidget(row, 2));
        auto *sr = qobject_cast<QDoubleSpinBox*>(tbl->cellWidget(row, 3));
        if (sx) pos.x = sx->value();
        if (sy) pos.y = sy->value();
        if (sz) pos.z = sz->value();
        if (sr) pos.r = sr->value();
        positions.append(pos);
    }
    m_runner->requestSendResult(positions);
}

/// Refreshes the connection indicator dot, status label text, connect button label, and enabled
/// state of the IP/port config fields (locked while the client is active) to match `status`.
/// Also updates the `connectionState` dynamic property (and repolishes) on the indicator dot,
/// status label, and connect button so the stylesheet can react to it.
/// @param status the connection status to reflect in the UI
void VisionTcpipClientDeviceWidget::updateConnectionVisual(vc::device::ConnectStatus status) {
    using vc::device::ConnectStatus;
    const bool connected = status == ConnectStatus::Connected;
    // "Active" = dialing / connected / recovering. Connecting and LostConnected
    // both mean the client is running but not (fully) linked yet.
    const bool active = connected
                        || status == ConnectStatus::Connecting
                        || status == ConnectStatus::LostConnected;

    QByteArray state;
    QString stateText;
    if (connected) {
        state = "connected";
        stateText = tr("CONNECTED");
    } else if (status == ConnectStatus::LostConnected) {
        state = "connecting";
        stateText = tr("RECONNECTING…");
    } else if (status == ConnectStatus::Connecting) {
        state = "connecting";
        stateText = tr("CONNECTING…");
    } else {
        state = "disconnected";
        stateText = tr("DISCONNECTED");
    }

    ui->lbl_conn_state->setText(stateText);
    ui->btn_connect->setText(connected ? tr("Disconnect")
                                       : active ? tr("Cancel")
                                                : tr("Connect"));

    // Config is locked while the client is active.
    ui->ledit_ip->setEnabled(!active);
    ui->spb_port_data->setEnabled(!active);
    ui->spb_port_heartbeat->setEnabled(!active);

    for (QWidget *w : {static_cast<QWidget*>(ui->lbl_conn_dot),
                       static_cast<QWidget*>(ui->lbl_conn_state),
                       static_cast<QWidget*>(ui->btn_connect)}) {
        w->setProperty("connectionState", state);
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}
