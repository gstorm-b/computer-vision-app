#include "vision_tcpip_device_widget.h"
#include "ui_vision_tcpip_device_widget.h"

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

VisionTcpipDeviceWidget::~VisionTcpipDeviceWidget()
{
    delete ui;
}

QString VisionTcpipDeviceWidget::deviceId() {
    return m_device->id();
}

void VisionTcpipDeviceWidget::loadConfigToDevice() {
    if (!m_output_device) return;
    m_output_device->setVisionTcpipConfig(m_config);
}

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

void VisionTcpipDeviceWidget::saveConfig() {
    if (!m_output_device) return;
    if (!m_output_device->setVisionTcpipConfig(m_config)) {
        LOG_USER_WARN << tr("Cannot save Vision Output settings while the device "
                            "is connected. Disconnect first, then retry.");
    }
}

void VisionTcpipDeviceWidget::refreshConfig() {
    if (!m_device) {
        return;
    }
}

void VisionTcpipDeviceWidget::onFieldConfigChanged() {
    if (m_populating_browser) return;
    m_config.m_listenAddress  = ui->ledit_ip->text().trimmed();
    m_config.m_mainPort       = ui->spb_port_data->value();
    m_config.m_heartbeatPort  = ui->spb_port_heartbeat->value();
    saveConfig();
    populateBrowser();
}

void VisionTcpipDeviceWidget::onConnectClicked() {
    if (!m_runner) return;
    if (m_device && m_device->isDeviceConnected()) {
        m_runner->requestDisconnect();
    } else {
        m_runner->requestConnect();
    }
}

void VisionTcpipDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state) {
    updateConnectionVisual(state);
    populateBrowser();
    const bool mainConnected = m_output_device
                               && m_output_device->runtimeState().mainClientConnected;
    updateSendSection(mainConnected);
}

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

void VisionTcpipDeviceWidget::onMainClientStateChanged(bool connected) {
    updateSendSection(connected);
}

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

void VisionTcpipDeviceWidget::onAddRow() {
    if (ui->tbl_positions->rowCount() < 20) {
        addPositionRow();
    }
}

void VisionTcpipDeviceWidget::onRemoveRow() {
    const int row = ui->tbl_positions->currentRow();
    if (row >= 0) {
        ui->tbl_positions->removeRow(row);
    } else if (ui->tbl_positions->rowCount() > 0) {
        ui->tbl_positions->removeRow(ui->tbl_positions->rowCount() - 1);
    }
}

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


