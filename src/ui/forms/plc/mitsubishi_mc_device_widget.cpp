#include "mitsubishi_mc_device_widget.h"
#include "ui_mitsubishi_mc_device_widget.h"

#include "device/plc/mc_request.h"
#include "device/plc/mc_msg_tcp_client.h"
#include "core/logger/app_logger.h"
#include "core/utils/theme_manager.h"

#include <QFile>
#include <QSignalBlocker>

/// Creates and configures a single QtVariantProperty mirroring meta-property `prop` (read from
/// `value`): enum-typed properties are mapped to QtVariantPropertyManager::enumTypeId() with
/// translated enum-key names, other types use the property's own QVariant user type; the
/// display name and "minimum"/"maximum" attributes are pulled from `<propName>_name`,
/// `<propName>_min`, `<propName>_max` class-info entries on `meta` when present, and the
/// property is disabled when `prop` is not writable. Returns nullptr for the "objectName"
/// property or if the manager could not create one. `browser` is accepted for signature
/// symmetry with the populate_* callers but is not used directly in this function.
/// @param meta the meta-object owning `prop` (used for class-info lookups and enum translation)
/// @param prop the meta-property to mirror into the browser
/// @param value the current value of `prop`
/// @param manager the variant property manager used to create the property
/// @return the newly created property, or nullptr (see above)
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

/// Adds a "Device Information" group to `browser` and fills it with one property per Qt
/// property exposed on `gadget`'s meta-object (e.g. IDevice's id/name/type), via
/// addPropertyToBrowser.
/// @param gadget the device instance whose properties are mirrored
/// @param manager variant property manager backing the new properties
/// @param browser tree browser the group and properties are added to
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

/// Adds an "Interface" group to `browser` and fills it with one property per Qt property
/// exposed on the message-interface config gadget's meta-object, via addPropertyToBrowser.
/// @param gadget the message interface config (e.g. Ethernet TCP settings) whose properties are mirrored
/// @param manager variant property manager backing the new properties
/// @param browser tree browser the group and properties are added to
static void populateBrowser_MsgInterface(vc::device::McMsgItfConfig *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Interface"));
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

/// Adds a "Protocol format" group to `browser` and fills it with one property per Qt property
/// exposed on the MC context gadget's meta-object (frame type, data code, addressing, etc.), via
/// addPropertyToBrowser.
/// @param gadget the MC protocol context whose properties are mirrored
/// @param manager variant property manager backing the new properties
/// @param browser tree browser the group and properties are added to
static void populateBrowser_McContext(vc::device::McContext *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Protocol format"));
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

/// Constructs the widget for device `dv`, sets up the generated .ui, and calls initWidget() to
/// build the property browser, the M/D monitor widgets, and all signal/slot wiring.
MitsubishiMcDeviceWidget::MitsubishiMcDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                          vc::runtime::PlcRunner *runner,
                                          ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent),
    ui(new Ui::MitsubishiMcDeviceWidget),
    m_device(dv),
    m_dock(dock),
    m_runner(runner)  {

    ui->setupUi(this);
    initWidget();
}

/// Destroys the generated UI object. The device (shared_ptr) and runner (owned elsewhere) are
/// not deleted here.
MitsubishiMcDeviceWidget::~MitsubishiMcDeviceWidget() {
    delete ui;
}

/// Returns the id of the underlying device.
QString MitsubishiMcDeviceWidget::deviceId() {
    return m_device->id();
}

/// No-op override: property-browser edits are already pushed to the device immediately (via
/// onPropertyValueChanged/saveConfig), so there is nothing extra to push here.
void MitsubishiMcDeviceWidget::loadConfigToDevice() {
    // do nothing
}

/// No-op override: the widget's fields are (re)populated as part of initWidget()/populateBrowser(),
/// so there is nothing extra to load here.
void MitsubishiMcDeviceWidget::loadConfigToWidget() {
    // do nothing
}

/// Slot for QtVariantPropertyManager::valueChanged: looks up which meta-object owns the edited
/// property by name (the device itself, the MC context, or its message-interface config) and
/// writes the new value back onto the matching gadget/object via QMetaProperty::writeOnGadget
/// (or, for the device's "name" property, via DeviceManager::changeDeviceName, reverting the
/// property display if the rename is rejected). Ignored while populateBrowser() is rebuilding
/// the browser (m_populating_browser) to avoid feedback loops. Context/interface edits trigger
/// saveConfig() plus the relevant UI refresh (monitor ranges + meta summary, or connection
/// fields).
void MitsubishiMcDeviceWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant) {
    if (m_populating_browser) {
        return;
    }

    vc::device::McContext *context = this->m_config.context();
    vc::device::McMsgItfConfig *msg_cfg = context->msgConfig();

    QString propName = property->propertyName();

    const QMetaObject *idevice_meta = m_device->metaObject();
    const QMetaObject &meta_context = context->getMetaObject();
    const QMetaObject &meta_msg = msg_cfg->getMetaObject();

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

    // mc context
    index = meta_context.indexOfProperty(propName.toUtf8());
    if (index != -1) {
        QMetaProperty mProp = meta_context.property(index);
        mProp.writeOnGadget(context, variant);
        // qDebug() << "Updated context: " << propName << "to" << variant;
        this->saveConfig();
        rebuildMonitorRanges();
        refreshMetaSummary();
        return;
    }

    // interface config
    index = meta_msg.indexOfProperty(propName.toUtf8());
    if (index != -1) {
        QMetaProperty mProp = meta_msg.property(index);
        mProp.writeOnGadget(msg_cfg, variant);
        // qDebug() << "Updated context: " << propName << "to" << variant;
        this->saveConfig();
        populateConnectionFields();
        return;
    }
}

/// Slot for the connect/disconnect button: requests a connect through the runner if the device
/// is currently not connected, otherwise requests a disconnect. No-op if the device or runner
/// is missing.
void MitsubishiMcDeviceWidget::onBtnConnect() {
    if (!m_device || !m_runner) {
        return;
    }

    if (!m_device->isDeviceConnected()) {
        // saveConfig();
        m_runner->requestConnect();
    } else {
        m_runner->requestDisconnect();
    }
}

/// Reapplies the current in-memory config's monitor ranges and pushes m_config to the device
/// via McProtocolDevice::setMcProtocolConfig.
void MitsubishiMcDeviceWidget::saveConfig() {
    rebuildMonitorRanges();
    m_mc_device->setMcProtocolConfig(m_config);
}

/// Slot for PlcRunner::connectStatusChanged: refreshes the connection visual, and additionally
/// clears both monitor widgets' cached bit/word values when the link is disconnected, lost, or
/// failed to connect. Every ConnectStatus value is enumerated explicitly (no default:) so that
/// adding a new value surfaces a compiler warning here.
void MitsubishiMcDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state) {
    updateConnectionVisual(state);
    switch (state) {
    // Lost/closed link → clear the cached monitor values.
    case vc::device::ConnectStatus::Disconnected:
    case vc::device::ConnectStatus::LostConnected:
    case vc::device::ConnectStatus::ConnectFailed:
        if (m_monitor_m) m_monitor_m->clearAllStatuses();
        if (m_monitor_d) m_monitor_d->clearAllStatuses();
        break;
    // Enumerate the rest explicitly (no default:) so adding a ConnectStatus
    // value surfaces a -Wswitch / C4062 warning here.
    case vc::device::ConnectStatus::NoConnection:
    case vc::device::ConnectStatus::Connected:
    case vc::device::ConnectStatus::Connecting:
        break;
    }
}

/// Slot for PlcRunner::pollingUpdate: downcasts `device_map` to McDeviceMap and forwards the
/// polled M (bit) and D (word) register values into the corresponding monitor widgets.
void MitsubishiMcDeviceWidget::onPollingUpdateValue(std::shared_ptr<vc::device::PlcValueMap> device_map) {
    m_device_map = *(static_cast<vc::device::McDeviceMap*>(device_map.get()));
    if (m_monitor_m) {
        const auto &m_map = m_device_map.device_map_m;
        for (auto it = m_map.begin(); it != m_map.end(); ++it) {
            m_monitor_m->setBitState(it->first, it->second);
        }
    }
    if (m_monitor_d) {
        const auto &d_map = m_device_map.device_map_d;
        for (auto it = d_map.begin(); it != d_map.end(); ++it) {
            m_monitor_d->setWordValue(it->first, it->second);
        }
    }
}

/// One-time widget setup: initialises the property browser, applies the per-form theme QSS,
/// wires the runner's connectStatusChanged/pollingUpdate signals (logging an error if no runner
/// was supplied, leaving controls disabled), caches the device's current McProtocolConfig,
/// creates and embeds the M (bit) and D (word) DevicesMonitorWidget instances into the .ui
/// splitter containers, refreshes the monitor ranges/meta summary/property browser, and
/// connects the connection-card controls (connect button, IP/port/timeout edits).
void MitsubishiMcDeviceWidget::initWidget() {
    initPropertyBrowser();
    setupThemeReload(QStringLiteral(":/styles/mitsubishi_mc_device_widget_dark.qss"),
                    QStringLiteral(":/styles/mitsubishi_mc_device_widget_light.qss"));

    if (m_device) {
        m_mc_device = static_cast<vc::device::McProtocolDevice*>(m_device.get());

        // ── Wire to runner (NOT to device directly) ──────────────────────────
        // The runner forwards device signals onto the GUI thread via
        // QueuedConnection, so it is safe to update widgets from these slots.
        // Widget never owns a QThread.
        if (m_runner) {
            connect(m_runner, &vc::runtime::PlcRunner::connectStatusChanged,
                    this,     &MitsubishiMcDeviceWidget::onConnectionStateChanged);
            connect(m_runner, &vc::runtime::PlcRunner::pollingUpdate,
                    this,     &MitsubishiMcDeviceWidget::onPollingUpdateValue);
        } else {
            LOG_DEV_ERR << "MitsubishiMcDeviceWidget: no runner provided — control disabled";
        }

        m_config = m_mc_device->mcProtocolConfig();

        // Build the two stacked monitor widgets and slot them into the .ui
        // splitter containers.
        m_monitor_m = new vc::widgets::DevicesMonitorWidget(
            vc::widgets::DevicesMonitorWidget::Mode::Bit, this);
        m_monitor_d = new vc::widgets::DevicesMonitorWidget(
            vc::widgets::DevicesMonitorWidget::Mode::Word, this);


        if (auto *layM = ui->container_m->layout()) layM->addWidget(m_monitor_m);
        if (auto *layD = ui->container_d->layout()) layD->addWidget(m_monitor_d);
        connect(m_monitor_m, &vc::widgets::DevicesMonitorWidget::bitWriteRequested,
                this,        &MitsubishiMcDeviceWidget::onBitWriteRequested);
        connect(m_monitor_d, &vc::widgets::DevicesMonitorWidget::wordWriteRequested,
                this,        &MitsubishiMcDeviceWidget::onWordWriteRequested);

        rebuildMonitorRanges();
        populateConnectionFields();
        refreshMetaSummary();
        populateBrowser();

        connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                this, &MitsubishiMcDeviceWidget::onPropertyValueChanged);
    }

    connect(ui->btn_connect, &QPushButton::clicked,
            this, &MitsubishiMcDeviceWidget::onBtnConnect);
    connect(ui->ledit_ip, &QLineEdit::editingFinished,
            this, &MitsubishiMcDeviceWidget::onIpEditFinished);
    connect(ui->spb_port, &QSpinBox::editingFinished,
            this, &MitsubishiMcDeviceWidget::onPortEditFinished);
    connect(ui->spb_conn_timeout, &QSpinBox::editingFinished,
            this, &MitsubishiMcDeviceWidget::onConnectTimeoutEditFinished);
    connect(ui->spb_resp_timeout, &QSpinBox::editingFinished,
            this, &MitsubishiMcDeviceWidget::onResponseTimeoutEditFinished);

    updateConnectionVisual(m_device && m_device->isDeviceConnected()
                               ? vc::device::ConnectStatus::Connected
                               : vc::device::ConnectStatus::Disconnected);
}


/// Reapplies the current M/D start-address and amount from m_config's context onto the M and D
/// monitor widgets' ranges. No-op if the context is not set.
void MitsubishiMcDeviceWidget::rebuildMonitorRanges() {
    if (!m_config.context()) return;
    const int m_start = m_config.context()->startMAddress();
    const int m_amount = m_config.context()->amountMAddress();
    const int d_start = m_config.context()->startDAddress();
    const int d_amount = m_config.context()->amountDAddress();
    if (m_monitor_m) m_monitor_m->setRange(m_start, m_amount);
    if (m_monitor_d) m_monitor_d->setRange(d_start, d_amount);
}

/// Updates the meta-summary label with the current frame type, data code, and refresh interval
/// read from m_config's context (formatted as "FRAME type - code - INTERVAL n ms", with an em
/// dash placeholder when the frame/code strings are empty). No-op if the context is missing.
void MitsubishiMcDeviceWidget::refreshMetaSummary() {
    auto *ctx = m_config.context();
    if (!ctx) return;
    const QString frame = vc::device::mc::McFrameTypeToString(ctx->frameType());
    const QString code  = vc::device::mc::McDataCodeToString(ctx->dataCode());
    ui->lbl_meta_frame->setText(QString("FRAME %1 · %2 · INTERVAL %3 ms")
                                    .arg(frame.isEmpty() ? QStringLiteral("—") : frame.toUpper())
                                    .arg(code.isEmpty()  ? QStringLiteral("—") : code.toUpper())
                                    .arg(ctx->refreshInterval()));
}

/// Loads the connection card's fields from the current message-interface config: IP/port only
/// when the interface is EthernetTCPIP (otherwise the IP field is cleared/disabled and the port
/// field is disabled), plus the connect and response timeouts. Signal blockers on all four
/// fields (and the m_loading_connection_fields flag) prevent the edits from re-triggering the
/// on*EditFinished handlers. No-op if the context or message config is not set.
void MitsubishiMcDeviceWidget::populateConnectionFields() {
    if (!m_config.context()) return;
    auto *msg = m_config.context()->msgConfig();
    if (!msg) return;

    m_loading_connection_fields = true;
    QSignalBlocker bIp(ui->ledit_ip);
    QSignalBlocker bPort(ui->spb_port);
    QSignalBlocker bConn(ui->spb_conn_timeout);
    QSignalBlocker bResp(ui->spb_resp_timeout);

    if (msg->type() == vc::device::mc::McMsgItfType::EthernetTCPIP) {
        auto *eth = static_cast<vc::device::McMsgEthernetTcpCfg *>(msg);
        ui->ledit_ip->setText(eth->m_ipAddress);
        ui->spb_port->setValue(eth->m_portNumber);
    } else {
        ui->ledit_ip->setText(QString());
        ui->ledit_ip->setEnabled(false);
        ui->spb_port->setEnabled(false);
    }
    ui->spb_conn_timeout->setValue(msg->m_connectTimeout);
    ui->spb_resp_timeout->setValue(msg->m_responseTimeout);

    m_loading_connection_fields = false;
}

/// Slot for the IP field's editingFinished: commits the trimmed IP text into the Ethernet-TCP
/// message config and calls saveConfig(). Skipped while fields are being programmatically
/// loaded, if the interface is not EthernetTCPIP, or if the value did not change.
void MitsubishiMcDeviceWidget::onIpEditFinished() {
    if (m_loading_connection_fields) return;
    auto *msg = m_config.context() ? m_config.context()->msgConfig() : nullptr;
    if (!msg || msg->type() != vc::device::mc::McMsgItfType::EthernetTCPIP) return;
    auto *eth = static_cast<vc::device::McMsgEthernetTcpCfg *>(msg);
    const QString newIp = ui->ledit_ip->text().trimmed();
    if (newIp == eth->m_ipAddress) return;
    eth->m_ipAddress = newIp;
    saveConfig();
}

/// Slot for the port field's editingFinished: commits the new port number into the
/// Ethernet-TCP message config and calls saveConfig(). Skipped while fields are being
/// programmatically loaded, if the interface is not EthernetTCPIP, or if the value did not
/// change.
void MitsubishiMcDeviceWidget::onPortEditFinished() {
    if (m_loading_connection_fields) return;
    auto *msg = m_config.context() ? m_config.context()->msgConfig() : nullptr;
    if (!msg || msg->type() != vc::device::mc::McMsgItfType::EthernetTCPIP) return;
    auto *eth = static_cast<vc::device::McMsgEthernetTcpCfg *>(msg);
    const int newPort = ui->spb_port->value();
    if (newPort == eth->m_portNumber) return;
    eth->m_portNumber = newPort;
    saveConfig();
}

/// Slot for the connect-timeout field's editingFinished: applies the new value via
/// McMsgItfConfig::SetConnectTimeout and calls saveConfig(). Skipped while fields are being
/// programmatically loaded, if there is no message config, or if the value did not change.
void MitsubishiMcDeviceWidget::onConnectTimeoutEditFinished() {
    if (m_loading_connection_fields) return;
    auto *msg = m_config.context() ? m_config.context()->msgConfig() : nullptr;
    if (!msg) return;
    const int v = ui->spb_conn_timeout->value();
    if (v == msg->m_connectTimeout) return;
    msg->SetConnectTimeout(v);
    saveConfig();
}

/// Slot for the response-timeout field's editingFinished: applies the new value via
/// McMsgItfConfig::SetWaitResponseTimeout and calls saveConfig(). Skipped while fields are
/// being programmatically loaded, if there is no message config, or if the value did not
/// change.
void MitsubishiMcDeviceWidget::onResponseTimeoutEditFinished() {
    if (m_loading_connection_fields) return;
    auto *msg = m_config.context() ? m_config.context()->msgConfig() : nullptr;
    if (!msg) return;
    const int v = ui->spb_resp_timeout->value();
    if (v == msg->m_responseTimeout) return;
    msg->SetWaitResponseTimeout(v);
    saveConfig();
}

/// Updates the connection dot/label/button to reflect `status`: sets the "connectionState"
/// dynamic property ("connected"/"disconnected") on all three widgets, updates their text
/// ("CONNECTED"/"DISCONNECTED", "Disconnect"/"Connect"), and re-polishes each so the QSS
/// styling keyed on that property takes effect immediately.
void MitsubishiMcDeviceWidget::updateConnectionVisual(vc::device::ConnectStatus status) {
    const bool connected = (status == vc::device::ConnectStatus::Connected);
    const QString stateVal = connected ? QStringLiteral("connected")
                                       : QStringLiteral("disconnected");

    ui->lbl_conn_dot->setProperty("connectionState", stateVal);
    ui->lbl_conn_state->setProperty("connectionState", stateVal);
    ui->btn_connect->setProperty("connectionState", stateVal);

    ui->lbl_conn_state->setText(connected ? tr("CONNECTED") : tr("DISCONNECTED"));
    ui->btn_connect->setText(connected ? tr("Disconnect") : tr("Connect"));

    for (QWidget *w : std::initializer_list<QWidget*>{
             ui->lbl_conn_dot, ui->lbl_conn_state, ui->btn_connect}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}

/// Currently a stub: only guards on a missing device and otherwise does nothing.
void MitsubishiMcDeviceWidget::refreshConfig() {
    if (!m_device) {
        return;
    }
}

/// Slot for DevicesMonitorWidget::bitWriteRequested (M/bit monitor): builds an MCRequest::WriteBit
/// request for register "M" + address (4-digit zero-padded) with `value` as the single bit
/// payload and pushes it to the device. No-op if the device is missing or not connected.
/// @param address the M-register address to write
void MitsubishiMcDeviceWidget::onBitWriteRequested(int address, quint8 value) {
    if (!m_device || !m_device->isDeviceConnected()) {
        return;
    }
    const QString name = QString("M%1").arg(address, 4, 10, QChar('0'));
    vc::device::MCRequest request(vc::device::MCRequest::WriteBit, name, 1);
    request.buildWriteData_Bit_Device(value ? 0x01 : 0x00);
    m_device->pushRequest(static_cast<vc::device::IRequest*>(&request));
}

/// Slot for DevicesMonitorWidget::wordWriteRequested (D/word monitor): builds an
/// MCRequest::WriteWord request for register "D" + address (4-digit zero-padded) with `value`
/// as the single word payload and pushes it to the device. No-op if the device is missing or
/// not connected.
/// @param address the D-register address to write
/// @param value the word value to write
void MitsubishiMcDeviceWidget::onWordWriteRequested(int address, qint16 value) {
    if (!m_device || !m_device->isDeviceConnected()) {
        return;
    }
    const QString name = QString("D%1").arg(address, 4, 10, QChar('0'));
    vc::device::MCRequest request(vc::device::MCRequest::WriteWord, name, 1);
    request.buildWriteData_Word_Device_Word(static_cast<quint16>(value));
    m_device->pushRequest(static_cast<vc::device::IRequest*>(&request));
}

/// Rebuilds the property browser from scratch: clears the variant manager (destroying the
/// previous properties) then re-adds the Device Information, Protocol format, and Interface
/// groups via populateBrowser_Device/populateBrowser_McContext/populateBrowser_MsgInterface.
/// Guards re-entrant edits with m_populating_browser and suppresses editor repaints via
/// blockSignals for the duration of the rebuild.
void MitsubishiMcDeviceWidget::populateBrowser() {
    m_variantEditor->blockSignals(true);
    m_populating_browser = true;

    m_variantManager->clear();

    populateBrowser_Device(m_device.get(), m_variantManager, m_variantEditor);
    populateBrowser_McContext(m_config.context(), m_variantManager, m_variantEditor);
    populateBrowser_MsgInterface(m_config.context()->msgConfig(), m_variantManager, m_variantEditor);

    m_populating_browser = false;
    m_variantEditor->blockSignals(false);
}
