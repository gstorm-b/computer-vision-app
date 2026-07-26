#include "vision_tcpip_device_widget.h"
#include "ui_vision_tcpip_device_widget.h"

#include <QDoubleSpinBox>
#include <QHeaderView>

/// Builds one QtVariantProperty for the property browser from a QMetaProperty/value pair:
/// creates an enum-typed property (with translated enum names) for enum properties or a
/// plain variant property otherwise, then applies the display name and min/maximum
/// attributes found via matching `<prop>_name`/`<prop>_min`/`<prop>_max` Q_CLASSINFO entries
/// and disables the property if the underlying meta-property is not writable.
/// @param meta meta-object the property belongs to (used to resolve Q_CLASSINFO/enum names)
/// @param prop the meta-property being mirrored into the browser
/// @param value the property's current value
/// @param manager manager used to create the new property
/// @param browser unused for creation itself, but kept alongside `manager` for a consistent call shape
/// @return the newly created property, or nullptr for the ignored "objectName" property or on creation failure
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

/// Adds a "Device Information" group to the property browser and fills it with every
/// Q_PROPERTY exposed by `gadget`'s meta-object (read via QObject::property()).
/// @param gadget the abstract device whose properties are mirrored
/// @param manager manager used to create the group and child properties
/// @param browser browser the group is added to
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

/// Adds an "Output cofiguration" group to the property browser and fills it with every
/// Q_PROPERTY exposed by the VisionTcpipDeviceCfg gadget's meta-object (read via
/// QMetaProperty::readOnGadget()).
/// @param gadget the output-config gadget whose properties are mirrored
/// @param manager manager used to create the group and child properties
/// @param browser browser the group is added to
static void populateBrowser_VisionOutput(vc::device::VisionTcpipDeviceCfg *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Output cofiguration"));
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

/// Adds a read-only property group named `groupName` to the browser, filled with every
/// Q_PROPERTY of `gadget` (read via QMetaProperty::readOnGadget() against `meta`); used for
/// display-only gadgets such as VisionTcpipRuntimeState and VisionTcpipDiagnostics. Every
/// created property is explicitly disabled so the browser cannot edit these values.
/// @param meta meta-object describing `gadget`'s properties
/// @param gadget the read-only gadget instance whose properties are displayed
/// @param groupName label shown for the top-level group node
/// @param manager manager used to create the group and child properties
/// @param browser browser the group is added to
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

/// Builds the widget for device `dv`: stores the device/runner/dock pointers, builds the
/// generated `ui` form, then delegates the rest of the setup to initWidget().
/// @param dv the device instance (expected to be a VisionTcpipDevice)
/// @param runner the runner that forwards device signals onto the GUI thread
/// @param dock the enclosing dock widget, if any
/// @param parent the parent widget, forwarded to IDeviceWidget
VisionTcpipDeviceWidget::VisionTcpipDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                               vc::runtime::VisionOutputRunner *runner,
                                               ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent),
    ui(new Ui::VisionTcpipDeviceWidget),
    m_device(dv),
    m_dock(dock),
    m_runner(runner)  {

    ui->setupUi(this);
    initWidget();
}

/// Destroys the widget and deletes the generated `ui` form.
VisionTcpipDeviceWidget::~VisionTcpipDeviceWidget()
{
    delete ui;
}

/// @return the id of the underlying device.
QString VisionTcpipDeviceWidget::deviceId() {
    return m_device->id();
}

/// Pushes the currently edited `m_config` to the underlying VisionTcpipDevice; a no-op if the
/// device did not resolve to a VisionTcpipDevice.
void VisionTcpipDeviceWidget::loadConfigToDevice() {
    if (!m_output_device) return;
    m_output_device->setVisionTcpipConfig(m_config);
}

/// Reloads `m_config` from the underlying VisionTcpipDevice and refreshes the listen-address/port
/// fields and kinematic-check sub-widget from it. Property-browser change notifications are
/// suppressed (m_populating_browser) for the duration to avoid feedback loops.
void VisionTcpipDeviceWidget::loadConfigToWidget() {
    if (!m_output_device) return;
    m_config = m_output_device->visionTcpipConfig();

    m_populating_browser = true;
    ui->ledit_ip->setText(m_config.m_listenAddress);
    ui->spb_port_data->setValue(m_config.m_mainPort);
    ui->spb_port_heartbeat->setValue(m_config.m_heartbeatPort);
    if (m_kcheckWidget) m_kcheckWidget->setConfig(m_config.m_kinematicCheck);
    m_populating_browser = false;
}

/// Initializes the property browser and theme stylesheets, resolves `m_device` to a
/// VisionTcpipDevice (disabling the whole widget on type mismatch), wires the runner's
/// connectStatusChanged signal and the device's mainClientStateChanged signal (queued, since it
/// may fire off the GUI thread), loads the config, populates the browser, wires up the connect
/// button/config fields/send-result table controls, and syncs the initial connection visuals.
void VisionTcpipDeviceWidget::initWidget() {
    initPropertyBrowser();
    setupThemeReload(QStringLiteral(":/styles/vision_tcpip_device_widget_dark.qss"),
                    QStringLiteral(":/styles/vision_tcpip_device_widget_light.qss"));

    if (m_device) {
        m_output_device = qobject_cast<vc::device::VisionTcpipDevice*>(m_device.get());
        if (!m_output_device) {
            LOG_DEV_ERR << "VisionTcpipDeviceWidget: expected VisionTcpipDevice but got"
                        << m_device->id();
            setEnabled(false);
        } else {
            // ── Wire to runner (NOT to device directly) ──────────────────────
            // The runner forwards device signals onto the GUI thread via
            // QueuedConnection, so it is safe to update widgets from these slots.
            // Widget never owns a QThread.
            if (m_runner) {
                connect(m_runner, &vc::runtime::VisionOutputRunner::connectStatusChanged,
                        this,     &VisionTcpipDeviceWidget::onConnectionStateChanged);
            } else {
                LOG_DEV_ERR << "VisionTcpipDeviceWidget: no runner provided - control disabled";
            }

            loadConfigToWidget();
            populateBrowser();

            connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                    this, &VisionTcpipDeviceWidget::onPropertyValueChanged);

            connect(m_output_device, &vc::device::VisionTcpipDevice::mainClientStateChanged,
                    this, &VisionTcpipDeviceWidget::onMainClientStateChanged,
                    Qt::QueuedConnection);
        }
    }

    connect(ui->btn_connect, &QPushButton::clicked,
            this, &VisionTcpipDeviceWidget::onConnectClicked);

    // ── Config input fields ───────────────────────────────────────────────
    connect(ui->ledit_ip,          &QLineEdit::editingFinished,
            this, &VisionTcpipDeviceWidget::onFieldConfigChanged);
    connect(ui->spb_port_data,     qOverload<int>(&QSpinBox::valueChanged),
            this, &VisionTcpipDeviceWidget::onFieldConfigChanged);
    connect(ui->spb_port_heartbeat,qOverload<int>(&QSpinBox::valueChanged),
            this, &VisionTcpipDeviceWidget::onFieldConfigChanged);

    // ── Send Result section ────────────────────────────────────────────────
    ui->tbl_positions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tbl_positions->verticalHeader()->setDefaultSectionSize(28);
    addPositionRow();

    connect(ui->btn_add_row,    &QPushButton::clicked, this, &VisionTcpipDeviceWidget::onAddRow);
    connect(ui->btn_remove_row, &QPushButton::clicked, this, &VisionTcpipDeviceWidget::onRemoveRow);
    connect(ui->btn_send_result,&QPushButton::clicked, this, &VisionTcpipDeviceWidget::onSendResult);

    // ── Robot kinematic check (Phase 2) ─────────────────────────────────────
    m_kcheckWidget = ui->widget_kinematic_check;
    m_kcheckWidget->setConfig(m_config.m_kinematicCheck);
    connect(m_kcheckWidget, &RobotKinematicCheckWidget::configChanged, this, [this]() {
        m_config.m_kinematicCheck = m_kcheckWidget->config();
        saveConfig();
    });

    updateConnectionVisual(m_device && m_device->isDeviceConnected()
                               ? vc::device::ConnectStatus::Connected
                               : vc::device::ConnectStatus::Disconnected);
}

/// Handles an edit made in the property browser: ignored while m_populating_browser is true (to
/// avoid feedback loops). Otherwise checks whether the edited property belongs to the abstract
/// device's meta-object — a "name" edit is routed through the device manager's renaming (rejecting
/// duplicate names and reverting the displayed value on failure) — and, if not, dispatches the
/// edit to the output-config gadget via vc::gadget_meta::writeProperty(), persisting through
/// saveConfig() on a successful write.
/// @param property the edited property (its name identifies the target field)
/// @param variant the new value entered in the browser
void VisionTcpipDeviceWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant) {
    if (m_populating_browser) {
        return;
    }

    // vc::device::McContext *context = this->m_config.context();
    // vc::device::McMsgItf *cfg = context->msgConfig();

    QString propName = property->propertyName();

    const QMetaObject *idevice_meta = m_device->metaObject();
    // const QMetaObject &meta_context = context->getMetaObject();
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

    // output config — dispatch the edit through the shared gadget_meta helper.
    if (vc::gadget_meta::writeProperty(meta_cfg, &m_config, propName, variant)) {
        this->saveConfig();
        return;
    }
}

/// Writes `m_config` back to the device, logging a user-facing warning if the device rejects the
/// save (e.g. because it is currently connected — config is locked while a link is live).
void VisionTcpipDeviceWidget::saveConfig() {
    if (!m_output_device) return;
    if (!m_output_device->setVisionTcpipConfig(m_config)) {
        LOG_USER_WARN << tr("Cannot save Vision Output settings while the device "
                            "is connected. Disconnect first, then retry.");
    }
}

/// Placeholder refresh hook; currently a no-op beyond the null-device guard.
void VisionTcpipDeviceWidget::refreshConfig() {
    if (!m_device) {
        return;
    }
}

/// Copies the IP/port fields into `m_config`, persists via saveConfig(), and repopulates the
/// property browser to reflect the new values. No-op while m_populating_browser is true.
void VisionTcpipDeviceWidget::onFieldConfigChanged() {
    if (m_populating_browser) return;
    m_config.m_listenAddress  = ui->ledit_ip->text().trimmed();
    m_config.m_mainPort       = ui->spb_port_data->value();
    m_config.m_heartbeatPort  = ui->spb_port_heartbeat->value();
    saveConfig();
    populateBrowser();
}

/// Requests connect or disconnect via the runner, depending on whether the device currently
/// reports itself connected; a no-op if no runner was provided.
void VisionTcpipDeviceWidget::onConnectClicked() {
    if (!m_runner) return;
    if (m_device && m_device->isDeviceConnected()) {
        m_runner->requestDisconnect();
    } else {
        m_runner->requestConnect();
    }
}

/// Updates the connection visuals, repopulates the property browser (to reflect any runtime-state
/// change), and refreshes the send section's enabled state based on the current main-client link
/// state, in response to a runner-reported connection-state change.
void VisionTcpipDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state) {
    updateConnectionVisual(state);
    populateBrowser();
    const bool mainConnected = m_output_device
                               && m_output_device->runtimeState().mainClientConnected;
    updateSendSection(mainConnected);
}

/// Clears and rebuilds the property browser with the device, output-config, and (when the device
/// resolved to a VisionTcpipDevice) runtime-state and diagnostics property groups. Editor signals
/// are blocked and m_populating_browser is held true for the duration to suppress change feedback.
void VisionTcpipDeviceWidget::populateBrowser() {
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

/// Updates the send-result section's enabled state when the main client connects/disconnects.
void VisionTcpipDeviceWidget::onMainClientStateChanged(bool connected) {
    updateSendSection(connected);
}

/// Enables/disables the send-result button and updates its hint label text and "sendState" style
/// property ("ready"/"idle") to reflect whether a main client is currently connected; the style
/// property drives QSS re-polish so the hint's visual state stays in sync.
void VisionTcpipDeviceWidget::updateSendSection(bool mainClientConnected) {
    ui->btn_send_result->setEnabled(mainClientConnected);
    ui->lbl_send_hint->setText(mainClientConnected
                               ? tr("Client connected — ready to send")
                               : tr("No client connected"));
    const QByteArray state = mainClientConnected ? "ready" : "idle";
    ui->lbl_send_hint->setProperty("sendState", state);
    ui->lbl_send_hint->style()->unpolish(ui->lbl_send_hint);
    ui->lbl_send_hint->style()->polish(ui->lbl_send_hint);
}

/// Appends a row of four QDoubleSpinBox cell widgets (x, y, z, r) to the position table,
/// pre-filled with the given values; each spin box is frameless, has no step buttons, allows
/// ±99999 with 3 decimals.
void VisionTcpipDeviceWidget::addPositionRow(double x, double y, double z, double r) {
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

/// Appends a blank position row to the send table, capped at 20 rows.
void VisionTcpipDeviceWidget::onAddRow() {
    if (ui->tbl_positions->rowCount() < 20) {
        addPositionRow();
    }
}

/// Removes the currently selected position row, or the last row if none is selected (and the
/// table is non-empty).
void VisionTcpipDeviceWidget::onRemoveRow() {
    const int row = ui->tbl_positions->currentRow();
    if (row >= 0) {
        ui->tbl_positions->removeRow(row);
    } else if (ui->tbl_positions->rowCount() > 0) {
        ui->tbl_positions->removeRow(ui->tbl_positions->rowCount() - 1);
    }
}

/// Reads every row of the position table (x, y, z, r spin boxes) into a VisionOutputPosition list
/// and asks the runner to send it; a no-op unless a runner, an output device, and a connected main
/// client all exist.
void VisionTcpipDeviceWidget::onSendResult() {
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

/// Updates the connect button/label/status-dot text ("LISTENING"/"STOPPED", "Stop"/"Start
/// Listening") and "connectionState" style property ("connected"/"disconnected") for `status`,
/// and disables the listen-address/port fields while connected (config is locked while listening).
void VisionTcpipDeviceWidget::updateConnectionVisual(vc::device::ConnectStatus status) {
    const bool connected = status == vc::device::ConnectStatus::Connected;
    const QByteArray state = connected ? "connected" : "disconnected";

    ui->lbl_conn_state->setText(connected ? tr("LISTENING") : tr("STOPPED"));
    ui->btn_connect->setText(connected ? tr("Stop") : tr("Start Listening"));

    ui->ledit_ip->setEnabled(!connected);
    ui->spb_port_data->setEnabled(!connected);
    ui->spb_port_heartbeat->setEnabled(!connected);

    for (QWidget *w : {static_cast<QWidget*>(ui->lbl_conn_dot),
                       static_cast<QWidget*>(ui->lbl_conn_state),
                       static_cast<QWidget*>(ui->btn_connect)}) {
        w->setProperty("connectionState", state);
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}


