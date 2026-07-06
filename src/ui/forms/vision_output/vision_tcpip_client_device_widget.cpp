#include "vision_tcpip_client_device_widget.h"
#include "ui_vision_tcpip_client_device_widget.h"

#include <QDoubleSpinBox>
#include <QHeaderView>

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

VisionTcpipClientDeviceWidget::~VisionTcpipClientDeviceWidget()
{
    delete ui;
}

QString VisionTcpipClientDeviceWidget::deviceId() {
    return m_device->id();
}

void VisionTcpipClientDeviceWidget::loadConfigToDevice() {
    if (!m_output_device) return;
    m_output_device->setVisionTcpipClientConfig(m_config);
}

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

void VisionTcpipClientDeviceWidget::saveConfig() {
    m_output_device->setVisionTcpipClientConfig(m_config);
}

void VisionTcpipClientDeviceWidget::refreshConfig() {
    if (!m_device) {
        return;
    }
}

void VisionTcpipClientDeviceWidget::onFieldConfigChanged() {
    if (m_populating_browser) return;
    m_config.m_serverAddress  = ui->ledit_ip->text().trimmed();
    m_config.m_mainPort       = ui->spb_port_data->value();
    m_config.m_heartbeatPort  = ui->spb_port_heartbeat->value();
    saveConfig();
    populateBrowser();
}

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

void VisionTcpipClientDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state) {
    updateConnectionVisual(state);
    populateBrowser();
    const bool mainConnected = m_output_device
                               && m_output_device->runtimeState().mainClientConnected;
    updateSendSection(mainConnected);
}

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

void VisionTcpipClientDeviceWidget::onMainClientStateChanged(bool connected) {
    updateSendSection(connected);
}

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

void VisionTcpipClientDeviceWidget::onAddRow() {
    if (ui->tbl_positions->rowCount() < 20) {
        addPositionRow();
    }
}

void VisionTcpipClientDeviceWidget::onRemoveRow() {
    const int row = ui->tbl_positions->currentRow();
    if (row >= 0) {
        ui->tbl_positions->removeRow(row);
    } else if (ui->tbl_positions->rowCount() > 0) {
        ui->tbl_positions->removeRow(ui->tbl_positions->rowCount() - 1);
    }
}

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
