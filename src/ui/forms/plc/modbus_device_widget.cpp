#include "modbus_device_widget.h"
#include "ui_modbus_device_widget.h"

#include "core/logger/app_logger.h"
#include "core/qgadget_macro.h"
#include "core/utils/theme_manager.h"
#include "device/plc/modbus/modbus_result_layout.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>

namespace {

/// Creates one QtVariantProperty mirroring meta-property `prop` (read from `value`), with the
/// display name and min/max attributes taken from the gadget's Q_CLASSINFO entries.
///
/// @note This is the third copy of this function in the UI layer (Basler and Mitsubishi have
///       their own). Extracting it is real debt and is recorded in
///       docs/backlog/later_todo_list.md rather than done here — it would mean editing two
///       commissioned widgets in a task that is about adding a third.
QtVariantProperty *addPropertyToBrowser(const QMetaObject &meta, const QMetaProperty &prop,
                                        const QVariant &value,
                                        QtVariantPropertyManager *manager)
{
    const QString propName = QString::fromLatin1(prop.name());
    if (propName == QLatin1String("objectName")) {
        return nullptr;
    }

    QtVariantProperty *variantProp = nullptr;
    if (prop.isEnumType()) {
        variantProp = manager->addProperty(QtVariantPropertyManager::enumTypeId(), propName);
        // Enum labels translated in the enum's own scope — see vc::gadget_meta::enumKeyNames.
        variantProp->setAttribute(QLatin1String("enumNames"),
                                  vc::gadget_meta::enumKeyNames(prop));
        variantProp->setValue(value.toInt());
    } else {
        variantProp = manager->addProperty(value.userType(), propName);
        if (variantProp) {
            variantProp->setValue(value);
        }
    }

    if (!variantProp) {
        return nullptr;
    }

    variantProp->setDisplayName(vc::gadget_meta::displayName(meta, prop.name()));

    const int minIdx = meta.indexOfClassInfo(QStringLiteral("%1_min").arg(propName).toUtf8());
    if (minIdx != -1) {
        variantProp->setAttribute(QLatin1String("minimum"),
                                  QString::fromUtf8(meta.classInfo(minIdx).value()).toInt());
    }
    const int maxIdx = meta.indexOfClassInfo(QStringLiteral("%1_max").arg(propName).toUtf8());
    if (maxIdx != -1) {
        variantProp->setAttribute(QLatin1String("maximum"),
                                  QString::fromUtf8(meta.classInfo(maxIdx).value()).toInt());
    }

    if (!prop.isWritable()) {
        variantProp->setEnabled(false);
    }
    return variantProp;
}

/// Adds a group to `browser` holding one property per Qt property on `gadget`'s meta-object.
void populateGadgetGroup(const QMetaObject &meta, void *gadget, const QString &groupTitle,
                         QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser)
{
    QtProperty *topItem =
        manager->addProperty(QtVariantPropertyManager::groupTypeId(), groupTitle);
    browser->addProperty(topItem);

    for (int i = 0; i < meta.propertyCount(); ++i) {
        const QMetaProperty prop = meta.property(i);
        const QVariant value = prop.readOnGadget(gadget);
        if (QtVariantProperty *variantProp =
                addPropertyToBrowser(meta, prop, value, manager)) {
            topItem->addSubProperty(variantProp);
        }
    }
}

/// Adds the device-identity group (id, name, …) read off the QObject meta-object.
void populateDeviceGroup(vc::device::IDevice *device, QtVariantPropertyManager *manager,
                         QtTreePropertyBrowser *browser)
{
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Device Information"));
    browser->addProperty(topItem);

    const QMetaObject *meta = device->metaObject();
    for (int i = 0; i < meta->propertyCount(); ++i) {
        const QMetaProperty prop = meta->property(i);
        const QVariant value = device->property(prop.name());
        if (QtVariantProperty *variantProp =
                addPropertyToBrowser(*meta, prop, value, manager)) {
            topItem->addSubProperty(variantProp);
        }
    }
}

/// Fills an area combo with `areas`, using the tag prefix as the label so what the operator
/// picks reads the same as the tag names in the signal map.
void fillAreaCombo(QComboBox *combo, std::initializer_list<vc::device::ModbusArea> areas)
{
    combo->clear();
    for (const vc::device::ModbusArea area : areas) {
        combo->addItem(vc::device::modbus_tags::prefix(area), static_cast<int>(area));
    }
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────────────────────

ModbusDeviceWidget::ModbusDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                       vc::runtime::PlcRunner *runner,
                                       ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent)
    , ui(new Ui::ModbusDeviceWidget)
    , m_device(std::move(dv))
    , m_dock(dock)
    , m_runner(runner)
{
    ui->setupUi(this);
    initWidget();
}

ModbusDeviceWidget::~ModbusDeviceWidget()
{
    delete ui;
}

QString ModbusDeviceWidget::deviceId()
{
    return m_device ? m_device->id() : QString();
}

void ModbusDeviceWidget::loadConfigToDevice() {}
void ModbusDeviceWidget::loadConfigToWidget() {}

vc::device::ModbusConfig *ModbusDeviceWidget::config() const
{
    if (m_client) {
        return const_cast<vc::device::ModbusTcpClientCfg *>(&m_clientConfig);
    }
    if (m_server) {
        return const_cast<vc::device::ModbusTcpServerCfg *>(&m_serverConfig);
    }
    return nullptr;
}

// ── Setup ────────────────────────────────────────────────────────────────────────────────────

void ModbusDeviceWidget::initWidget()
{
    initPropertyBrowser();
    setupThemeReload(QStringLiteral(":/styles/modbus_device_widget_dark.qss"),
                     QStringLiteral(":/styles/modbus_device_widget_light.qss"));

    if (m_device) {
        m_client = qobject_cast<vc::device::ModbusTcpClientDevice *>(m_device.get());
        m_server = qobject_cast<vc::device::ModbusTcpServerDevice *>(m_device.get());
        if (!m_client && !m_server) {
            LOG_DEV_ERR << "ModbusDeviceWidget: device is neither Modbus sub-type."
                        << "deviceId=" << m_device->id();
            return;
        }

        // Wire to the runner, never to the device: the runner forwards device signals onto the
        // GUI thread via QueuedConnection. A widget never owns or touches a device thread.
        if (m_runner) {
            connect(m_runner, &vc::runtime::PlcRunner::connectStatusChanged,
                    this,     &ModbusDeviceWidget::onConnectionStateChanged);
            connect(m_runner, &vc::runtime::PlcRunner::pollingUpdate,
                    this,     &ModbusDeviceWidget::onPollingUpdateValue);
        } else {
            LOG_DEV_ERR << "ModbusDeviceWidget: no runner provided — control disabled";
        }

        if (m_client) {
            m_clientConfig = m_client->modbusConfig();
        } else {
            m_serverConfig = m_server->modbusConfig();
        }

        m_monitor_bits = new vc::widgets::DevicesMonitorWidget(
            vc::widgets::DevicesMonitorWidget::Mode::Bit, this);
        m_monitor_words = new vc::widgets::DevicesMonitorWidget(
            vc::widgets::DevicesMonitorWidget::Mode::Word, this);

        if (auto *layout = ui->container_bits->layout()) {
            layout->addWidget(m_monitor_bits);
        }
        if (auto *layout = ui->container_words->layout()) {
            layout->addWidget(m_monitor_words);
        }
        connect(m_monitor_bits, &vc::widgets::DevicesMonitorWidget::bitWriteRequested,
                this,           &ModbusDeviceWidget::onBitWriteRequested);
        connect(m_monitor_words, &vc::widgets::DevicesMonitorWidget::wordWriteRequested,
                this,            &ModbusDeviceWidget::onWordWriteRequested);

        // A Modbus device has four areas where the Mitsubishi one has two, and two of the four
        // are read-only by protocol. Rather than four stacked monitors, each monitor shows one
        // area at a time and the combo picks which — the addresses of two areas overlap, so a
        // single monitor could not show both without the rows becoming ambiguous.
        fillAreaCombo(ui->cbx_bit_area,
                      { vc::device::ModbusArea::Coils, vc::device::ModbusArea::DiscreteInputs });
        fillAreaCombo(ui->cbx_word_area,
                      { vc::device::ModbusArea::HoldingRegisters,
                        vc::device::ModbusArea::InputRegisters });

        rebuildMonitorRanges();
        populateConnectionFields();
        refreshMetaSummary();
        populateBrowser();

        connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                this, &ModbusDeviceWidget::onPropertyValueChanged);
    }

    connect(ui->btn_connect, &QPushButton::clicked,
            this, &ModbusDeviceWidget::onBtnConnect);
    connect(ui->ledit_host, &QLineEdit::editingFinished,
            this, &ModbusDeviceWidget::onHostEditFinished);
    connect(ui->ledit_listen, &QLineEdit::editingFinished,
            this, &ModbusDeviceWidget::onListenEditFinished);
    connect(ui->spb_client_port, &QSpinBox::editingFinished,
            this, &ModbusDeviceWidget::onPortChanged);
    connect(ui->spb_server_port, &QSpinBox::editingFinished,
            this, &ModbusDeviceWidget::onPortChanged);
    connect(ui->spb_unit_id, &QSpinBox::editingFinished,
            this, &ModbusDeviceWidget::onUnitIdChanged);
    connect(ui->spb_server_unit_id, &QSpinBox::editingFinished,
            this, &ModbusDeviceWidget::onUnitIdChanged);
    connect(ui->spb_refresh, &QSpinBox::editingFinished,
            this, &ModbusDeviceWidget::onRefreshIntervalChanged);
    connect(ui->cbx_bit_area, &QComboBox::currentIndexChanged,
            this, &ModbusDeviceWidget::onBitAreaChanged);
    connect(ui->cbx_word_area, &QComboBox::currentIndexChanged,
            this, &ModbusDeviceWidget::onWordAreaChanged);

    updateConnectionVisual(m_device && m_device->isDeviceConnected()
                               ? vc::device::ConnectStatus::Connected
                               : vc::device::ConnectStatus::Disconnected);
}

vc::device::ModbusArea ModbusDeviceWidget::selectedArea(const QComboBox *combo,
                                                        vc::device::ModbusArea fallback) const
{
    if (!combo || combo->currentIndex() < 0) {
        return fallback;
    }
    return static_cast<vc::device::ModbusArea>(combo->currentData().toInt());
}

bool ModbusDeviceWidget::canWriteArea(vc::device::ModbusArea area) const
{
    return isClient() ? vc::device::modbus_tags::isMasterWritable(area)
                      : vc::device::modbus_tags::isServerWritable(area);
}

void ModbusDeviceWidget::rebuildMonitorRanges()
{
    const vc::device::ModbusConfig *cfg = config();
    if (!cfg) {
        return;
    }

    // The subtitle says what this device may do with the area, not what "the protocol allows"
    // in the abstract — those are different questions and answering the wrong one is what made
    // the server refuse writes it is entitled to make.
    const auto subtitleFor = [this](vc::device::ModbusArea area) {
        if (!canWriteArea(area)) {
            return tr("read-only — a master cannot write this area");
        }
        return vc::device::modbus_tags::isMasterWritable(area)
            ? tr("read / write — the master can write it too")
            : tr("write-only outward — we write it, the master can only read");
    };

    if (m_monitor_bits) {
        const vc::device::ModbusArea area =
            selectedArea(ui->cbx_bit_area, vc::device::ModbusArea::Coils);
        const vc::device::ModbusRange range = cfg->range(area);
        m_monitor_bits->setTitle(vc::device::modbus_tags::prefix(area));
        m_monitor_bits->setSubtitle(subtitleFor(area));
        // Before setRange(), and re-applied on every area change: the address column and the
        // filter must spell the tag the signal map accepts. This widget reuses a monitor written
        // for the Mitsubishi device map, which would otherwise print M0000/D0000 here — prefixes
        // the Modbus parser rejects, at a width one digit short of a real tag.
        m_monitor_bits->setAddressFormat(vc::device::modbus_tags::prefix(area),
                                         vc::device::modbus_tags::kAddressDigits);
        m_monitor_bits->setRange(range.start, range.count);
    }

    if (m_monitor_words) {
        const vc::device::ModbusArea area =
            selectedArea(ui->cbx_word_area, vc::device::ModbusArea::HoldingRegisters);
        const vc::device::ModbusRange range = cfg->range(area);
        m_monitor_words->setTitle(vc::device::modbus_tags::prefix(area));
        m_monitor_words->setSubtitle(subtitleFor(area));
        m_monitor_words->setAddressFormat(vc::device::modbus_tags::prefix(area),
                                          vc::device::modbus_tags::kAddressDigits);
        m_monitor_words->setRange(range.start, range.count);
    }
}

void ModbusDeviceWidget::refreshMetaSummary()
{
    const vc::device::ModbusConfig *cfg = config();
    if (!cfg) {
        return;
    }

    if (m_client) {
        ui->lbl_meta_summary->setText(
            tr("MODBUS TCP CLIENT · UNIT %1 · INTERVAL %2 ms")
                .arg(cfg->m_unitId)
                .arg(m_clientConfig.m_refreshInterval));
    } else {
        ui->lbl_meta_summary->setText(
            tr("MODBUS TCP SERVER · UNIT %1 · PUBLISHES ON WRITE").arg(cfg->m_unitId));
    }

    // The result block, spelled out. An operator commissioning the cell should be able to read
    // the register range the vision result occupies off this panel, without a PLC programming
    // tool and without opening the contract document.
    const vc::device::ModbusRange result = cfg->resultRange();
    const vc::device::ModbusArea resultArea = cfg->resultArea();
    const QString prefix = vc::device::modbus_tags::prefix(resultArea);
    QString text = tr("Vision result registers: %1%2 … %1%3  (%4 position slots, "
                      "count word at %1%2)")
                       .arg(prefix)
                       .arg(result.start, 5, 10, QChar('0'))
                       .arg(result.last(), 5, 10, QChar('0'))
                       .arg(cfg->m_resultMaxPositions);

    if (resultArea == vc::device::ModbusArea::InputRegisters) {
        // Worth stating on the panel: this is the reason to choose it, and it is also the reason
        // a PLC program written for the default layout will not find the block.
        text += QStringLiteral("  ") +
                tr("Published as input registers (3x): a master can read them but has no function "
                   "code to overwrite them.");
    }

    if (!cfg->resultFitsSingleRequest()) {
        // Worth saying on the panel, not only in the doc: above this capacity the publish spans
        // more than one Modbus request, and a master that reads without honouring the count-last
        // handshake can observe a half-written block.
        text += QStringLiteral("  ") +
                tr("Above %1 positions the result is published in more than one Modbus write; "
                   "the master must read the count word first.")
                    .arg(vc::device::ModbusResultLayout::kMaxPositionsInSingleRequest);
    }
    if (cfg->overlapsPolledResultRange()) {
        text += QStringLiteral("  ") +
                tr("The result block overlaps the mapped %1 range.").arg(prefix);
    }
    ui->lbl_result_block->setText(text);
}

void ModbusDeviceWidget::populateConnectionFields()
{
    const vc::device::ModbusConfig *cfg = config();
    if (!cfg) {
        return;
    }

    m_loading_connection_fields = true;

    // Show one card and HIDE the other rather than disabling it: a greyed-out "Listen address"
    // on a client reads as a field that could be enabled, which it never can.
    ui->wid_client_fields->setVisible(isClient());
    ui->wid_server_fields->setVisible(!isClient());

    if (isClient()) {
        QSignalBlocker bHost(ui->ledit_host);
        QSignalBlocker bPort(ui->spb_client_port);
        QSignalBlocker bUnit(ui->spb_unit_id);
        QSignalBlocker bRefresh(ui->spb_refresh);
        ui->ledit_host->setText(m_clientConfig.m_hostAddress);
        ui->spb_client_port->setValue(m_clientConfig.m_port);
        ui->spb_unit_id->setValue(m_clientConfig.m_unitId);
        ui->spb_refresh->setValue(m_clientConfig.m_refreshInterval);
        ui->lbl_conn_title->setText(tr("PLC CONNECTION — MODBUS TCP CLIENT"));
    } else {
        QSignalBlocker bListen(ui->ledit_listen);
        QSignalBlocker bPort(ui->spb_server_port);
        QSignalBlocker bUnit(ui->spb_server_unit_id);
        ui->ledit_listen->setText(m_serverConfig.m_listenAddress);
        ui->spb_server_port->setValue(m_serverConfig.m_port);
        ui->spb_server_unit_id->setValue(m_serverConfig.m_unitId);
        ui->lbl_conn_title->setText(tr("PLC CONNECTION — MODBUS TCP SERVER"));
    }

    m_loading_connection_fields = false;
}

void ModbusDeviceWidget::populateBrowser()
{
    if (!m_variantEditor || !m_variantManager || !m_device) {
        return;
    }

    m_populating_browser = true;
    m_variantEditor->clear();
    m_variantManager->clear();

    populateDeviceGroup(m_device.get(), m_variantManager, m_variantEditor);

    if (m_client) {
        populateGadgetGroup(vc::device::ModbusTcpClientCfg::staticMetaObject,
                            &m_clientConfig, QStringLiteral("Modbus"),
                            m_variantManager, m_variantEditor);
    } else if (m_server) {
        populateGadgetGroup(vc::device::ModbusTcpServerCfg::staticMetaObject,
                            &m_serverConfig, QStringLiteral("Modbus"),
                            m_variantManager, m_variantEditor);
    }

    m_populating_browser = false;
}

// ── Edits ────────────────────────────────────────────────────────────────────────────────────

void ModbusDeviceWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant)
{
    if (m_populating_browser || !property) {
        return;
    }

    const QString name = property->propertyName();

    // The device's own QObject properties (name) are written through the device; everything else
    // belongs to the config gadget.
    if (m_device && m_device->metaObject()->indexOfProperty(name.toUtf8().constData()) >= 0) {
        m_device->setProperty(name.toUtf8().constData(), variant);
        return;
    }

    const QMetaObject &meta = m_client
        ? vc::device::ModbusTcpClientCfg::staticMetaObject
        : vc::device::ModbusTcpServerCfg::staticMetaObject;
    void *gadget = m_client ? static_cast<void *>(&m_clientConfig)
                            : static_cast<void *>(&m_serverConfig);

    if (!vc::gadget_meta::writeProperty(meta, gadget, name, variant)) {
        LOG_DEV_ERR << "ModbusDeviceWidget: no config property named" << name;
        return;
    }

    saveConfig();
    populateConnectionFields();
    refreshMetaSummary();
}

void ModbusDeviceWidget::saveConfig()
{
    rebuildMonitorRanges();
    if (m_client) {
        m_client->setDeviceConfig(&m_clientConfig);
    } else if (m_server) {
        m_server->setDeviceConfig(&m_serverConfig);
    }
}

void ModbusDeviceWidget::onBtnConnect()
{
    if (!m_device || !m_runner) {
        return;
    }
    if (!m_device->isDeviceConnected()) {
        m_runner->requestConnect();
    } else {
        m_runner->requestDisconnect();
    }
}

void ModbusDeviceWidget::onHostEditFinished()
{
    if (m_loading_connection_fields || !m_client) {
        return;
    }
    const QString host = ui->ledit_host->text().trimmed();
    if (host == m_clientConfig.m_hostAddress) {
        return;
    }
    m_clientConfig.m_hostAddress = host;
    saveConfig();
    refreshMetaSummary();
}

void ModbusDeviceWidget::onListenEditFinished()
{
    if (m_loading_connection_fields || !m_server) {
        return;
    }
    const QString address = ui->ledit_listen->text().trimmed();
    if (address == m_serverConfig.m_listenAddress) {
        return;
    }
    m_serverConfig.m_listenAddress = address;
    saveConfig();
    refreshMetaSummary();
}

void ModbusDeviceWidget::onPortChanged()
{
    if (m_loading_connection_fields) {
        return;
    }
    if (m_client) {
        m_clientConfig.m_port = ui->spb_client_port->value();
    } else if (m_server) {
        m_serverConfig.m_port = ui->spb_server_port->value();
    }
    saveConfig();
    refreshMetaSummary();
}

void ModbusDeviceWidget::onUnitIdChanged()
{
    if (m_loading_connection_fields) {
        return;
    }
    if (m_client) {
        m_clientConfig.m_unitId = ui->spb_unit_id->value();
    } else if (m_server) {
        m_serverConfig.m_unitId = ui->spb_server_unit_id->value();
    }
    saveConfig();
    refreshMetaSummary();
    populateBrowser();
}

void ModbusDeviceWidget::onRefreshIntervalChanged()
{
    if (m_loading_connection_fields || !m_client) {
        return;
    }
    m_clientConfig.m_refreshInterval = ui->spb_refresh->value();
    saveConfig();
    refreshMetaSummary();
}

void ModbusDeviceWidget::onBitAreaChanged()
{
    rebuildMonitorRanges();
    if (m_monitor_bits) {
        // The rows now describe a different area; the cached values belong to the old one.
        m_monitor_bits->clearAllStatuses();
    }
}

void ModbusDeviceWidget::onWordAreaChanged()
{
    rebuildMonitorRanges();
    if (m_monitor_words) {
        m_monitor_words->clearAllStatuses();
    }
}

// ── Writes from the monitors ─────────────────────────────────────────────────────────────────

// Both slots run on the GUI thread, and the device lives on the runner's worker thread. The
// write MUST go through PlcRunner, whose write triggers are queued connections onto that thread.
// Calling IPlcIoWriter directly from here — which this widget did at first — touches a
// thread-affine QModbusTcpClient/QModbusTcpServer from the wrong thread: the request frame never
// reaches the socket, the master waits out its response timeout, and the peer logs nothing at all
// because nothing ever arrived. It also ran transact()'s nested event loop on the GUI thread,
// freezing the UI for the timeout and racing m_inTransaction against the poll.

void ModbusDeviceWidget::onBitWriteRequested(int address, quint8 value)
{
    if (!m_device || !m_device->isDeviceConnected()) {
        return;
    }
    const vc::device::ModbusArea area =
        selectedArea(ui->cbx_bit_area, vc::device::ModbusArea::Coils);
    const QString tag = vc::device::modbus_tags::format(area, address);

    // The device refuses too, with a logged reason; this check exists so the refusal is visible
    // to the operator as a warning line rather than only in the dev log.
    if (!canWriteArea(area)) {
        LOG_USER_WARN << "Discrete inputs cannot be written from a Modbus master: the protocol"
                      << "defines no function code for it." << "tag=" << tag;
        return;
    }

    if (!m_runner) {
        LOG_USER_ERR << "Modbus write dropped: no runner to carry it to the device thread."
                     << "tag=" << tag;
        return;
    }
    m_runner->requestWriteDigitalIo(tag, value != 0);
}

void ModbusDeviceWidget::onWordWriteRequested(int address, qint16 value)
{
    if (!m_device || !m_device->isDeviceConnected()) {
        return;
    }
    const vc::device::ModbusArea area =
        selectedArea(ui->cbx_word_area, vc::device::ModbusArea::HoldingRegisters);
    const QString tag = vc::device::modbus_tags::format(area, address);

    if (!canWriteArea(area)) {
        LOG_USER_WARN << "Input registers cannot be written from a Modbus master: the protocol"
                      << "defines no function code for it." << "tag=" << tag;
        return;
    }

    if (!m_runner) {
        LOG_USER_ERR << "Modbus write dropped: no runner to carry it to the device thread."
                     << "tag=" << tag;
        return;
    }
    m_runner->requestWriteWordIo(tag, value);
}

// ── Runner signals ───────────────────────────────────────────────────────────────────────────

void ModbusDeviceWidget::onConnectionStateChanged(vc::device::ConnectStatus state)
{
    updateConnectionVisual(state);
    switch (state) {
    case vc::device::ConnectStatus::Disconnected:
    case vc::device::ConnectStatus::LostConnected:
    case vc::device::ConnectStatus::ConnectFailed:
        if (m_monitor_bits)  m_monitor_bits->clearAllStatuses();
        if (m_monitor_words) m_monitor_words->clearAllStatuses();
        break;
    // Enumerated explicitly (no default:) so a new ConnectStatus value surfaces a warning here.
    case vc::device::ConnectStatus::NoConnection:
    case vc::device::ConnectStatus::Connected:
    case vc::device::ConnectStatus::Connecting:
        break;
    }
}

void ModbusDeviceWidget::onPollingUpdateValue(std::shared_ptr<vc::device::PlcValueMap> device_map)
{
    auto *map = dynamic_cast<vc::device::ModbusRegisterMap *>(device_map.get());
    if (!map) {
        return;
    }
    m_registerMap = *map;

    if (m_monitor_bits) {
        const vc::device::ModbusArea area =
            selectedArea(ui->cbx_bit_area, vc::device::ModbusArea::Coils);
        const vc::device::ModbusRange range = m_registerMap.range(area);
        for (int offset = 0; offset < range.count; ++offset) {
            const int address = range.start + offset;
            bool ok = false;
            const bool value = m_registerMap.bit(area, address, &ok);
            if (ok) {
                m_monitor_bits->setBitState(address, value ? 1 : 0);
            }
        }
    }

    if (m_monitor_words) {
        const vc::device::ModbusArea area =
            selectedArea(ui->cbx_word_area, vc::device::ModbusArea::HoldingRegisters);
        const vc::device::ModbusRange range = m_registerMap.range(area);
        for (int offset = 0; offset < range.count; ++offset) {
            const int address = range.start + offset;
            bool ok = false;
            const quint16 raw = m_registerMap.word(area, address, &ok);
            if (ok) {
                // Shown signed, matching what the task signal map receives for the same register.
                m_monitor_words->setWordValue(address, static_cast<qint16>(raw));
            }
        }
    }
}

void ModbusDeviceWidget::updateConnectionVisual(vc::device::ConnectStatus status)
{
    const bool connected = (status == vc::device::ConnectStatus::Connected);
    const QString stateVal = connected ? QStringLiteral("connected")
                                       : QStringLiteral("disconnected");

    ui->lbl_conn_dot->setProperty("connectionState", stateVal);
    ui->lbl_conn_state->setProperty("connectionState", stateVal);
    ui->btn_connect->setProperty("connectionState", stateVal);

    ui->lbl_conn_state->setText(connected ? tr("CONNECTED") : tr("DISCONNECTED"));
    // A server does not dial out; "Listen" is what the button actually does.
    const QString connectVerb = isClient() ? tr("Connect") : tr("Listen");
    const QString disconnectVerb = isClient() ? tr("Disconnect") : tr("Stop listening");
    ui->btn_connect->setText(connected ? disconnectVerb : connectVerb);

    for (QWidget *w : std::initializer_list<QWidget *>{
             ui->lbl_conn_dot, ui->lbl_conn_state, ui->btn_connect}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}
