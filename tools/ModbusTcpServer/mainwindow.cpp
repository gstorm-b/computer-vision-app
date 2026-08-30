#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "datacodec.h"
#include "modbusrunner.h"
#include "rowhoverdelegate.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHostAddress>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <utility>

using namespace ModbusDemo;

namespace {

//! Column layout shared by all four area tables.
//! Bit areas call column 2 "State" and column 3 "Value"; register areas call
//! them "Value" and "Raw". The indices are the same either way.
enum AreaColumn {
    AddressColumn = 0,
    ReferenceColumn = 1,
    ValueColumn = 2,
    ExtraColumn = 3
};

//! Column layout of the Data Monitor table.
enum MonitorColumn {
    MonitorNameColumn = 0,
    MonitorAreaColumn,
    MonitorAddressColumn,
    MonitorTypeColumn,
    MonitorOrderColumn,
    MonitorRegistersColumn,
    MonitorValueColumn,
    MonitorWriteColumn
};

QString levelTag(int level)
{
    switch (level) {
    case LogDebug:   return QStringLiteral("DEBUG");
    case LogWarning: return QStringLiteral("WARN");
    case LogError:   return QStringLiteral("ERROR");
    case LogInfo:
    default:         return QStringLiteral("INFO");
    }
}

//! Only the timestamp and the tag are coloured; the message itself keeps the
//! palette text colour so it stays readable in light and dark themes.
QString levelColor(int level)
{
    switch (level) {
    case LogDebug:   return QStringLiteral("#9e9e9e");
    case LogWarning: return QStringLiteral("#d9820b");
    case LogError:   return QStringLiteral("#e05a4f");
    case LogInfo:
    default:         return QStringLiteral("#4caf50");
    }
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

//! "40" or "40 - 41" depending on how many registers the row covers.
QString spanText(int first, int count)
{
    if (count <= 1)
        return QString::number(first);
    return QStringLiteral("%1 - %2").arg(first).arg(first + count - 1);
}

//! The registers behind a row, as raw hex words.
QString rawWords(const QList<quint16> &values)
{
    QStringList parts;
    parts.reserve(int(values.size()));
    for (quint16 value : values)
        parts << QString::number(value, 16).toUpper().rightJustified(4, QLatin1Char('0'));
    return parts.join(QLatin1Char(' '));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupTables();
    setupMonitor();
    setupConnections();
    startWorkerThread();

    logLocal(LogInfo, tr("Modbus TCP server demo started."));
    showLocalAddresses();

    ui->statusbar->showMessage(tr("Stopped"));
}

MainWindow::~MainWindow()
{
    shutdownWorker();   // must come first: nothing may emit into a half
    delete ui;          // destroyed window
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    shutdownWorker();
    QMainWindow::closeEvent(event);
}

void MainWindow::shutdownWorker()
{
    if (m_shutDown)
        return;
    m_shutDown = true;

    if (!m_thread)
        return;

    if (m_thread->isRunning()) {
        // Close the listening socket and every client socket from inside the
        // worker thread, synchronously, so nothing is in flight when the event
        // loop dies. This cannot deadlock: the runner never blocks on the GUI.
        if (m_runner) {
            QMetaObject::invokeMethod(m_runner.data(), &ModbusRunner::stopServer,
                                      Qt::BlockingQueuedConnection);
        }
        m_thread->quit();
        if (!m_thread->wait(5000)) {
            // Destroying a running QThread aborts the process, so leak the
            // object instead of taking the whole application down with it.
            qWarning("Modbus worker thread did not stop within 5 s, leaking it.");
            m_thread = nullptr;
            return;
        }
    }

    // QThread::finished -> deleteLater already destroyed the runner in its own
    // thread, so m_runner is null from here on.
    delete m_thread;
    m_thread = nullptr;
}

// ------------------------------------------------------------------ setup ---

void MainWindow::setupTables()
{
    m_tables = { ui->coilsTable,
                 ui->discreteInputsTable,
                 ui->inputRegistersTable,
                 ui->holdingRegistersTable };

    m_plcRefChecks[Coils]            = ui->coilsPlcRefCheck;
    m_plcRefChecks[DiscreteInputs]   = ui->discreteInputsPlcRefCheck;
    m_plcRefChecks[InputRegisters]   = ui->inputRegistersPlcRefCheck;
    m_plcRefChecks[HoldingRegisters] = ui->holdingRegistersPlcRefCheck;

    m_plcBaseSpins[Coils]            = ui->coilsPlcBaseSpin;
    m_plcBaseSpins[DiscreteInputs]   = ui->discreteInputsPlcBaseSpin;
    m_plcBaseSpins[InputRegisters]   = ui->inputRegistersPlcBaseSpin;
    m_plcBaseSpins[HoldingRegisters] = ui->holdingRegistersPlcBaseSpin;

    // Only the two register areas can be shown in another data format; bits
    // stay ON/OFF check boxes.
    m_formatCombos[InputRegisters]   = ui->inputRegistersFormatCombo;
    m_formatCombos[HoldingRegisters] = ui->holdingRegistersFormatCombo;
    m_orderCombos[InputRegisters]    = ui->inputRegistersOrderCombo;
    m_orderCombos[HoldingRegisters]  = ui->holdingRegistersOrderCombo;

    for (int area = 0; area < AreaCount; ++area) {
        QTableWidget *table = m_tables.at(area);

        m_values[area] = QList<quint16>(kAddressCount, 0);
        m_format[area] = DataCodec::Int16;
        m_order[area] = DataCodec::OrderABCD;
        m_plcBaseSpins[area]->setValue(defaultPlcBase(area));

        if (m_formatCombos[area]) {
            m_formatCombos[area]->addItems(DataCodec::typeNames());
            m_formatCombos[area]->setCurrentIndex(m_format[area]);
        }
        if (m_orderCombos[area]) {
            m_orderCombos[area]->addItems(DataCodec::byteOrderNames());
            m_orderCombos[area]->setCurrentIndex(m_order[area]);
            m_orderCombos[area]->setEnabled(false);   // INT16 spans one register
        }

        // Whole-row selection and whole-row hover, like a PLC batch monitor.
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        new RowHoverDelegate(table);

        rebuildAreaTable(area);
    }

    ui->monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->monitorTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    new RowHoverDelegate(ui->monitorTable);
}

void MainWindow::setupMonitor()
{
    ui->monitorAreaCombo->addItem(tr("Input Registers (3x)"), InputRegisters);
    ui->monitorAreaCombo->addItem(tr("Holding Registers (4x)"), HoldingRegisters);
    ui->monitorAreaCombo->setCurrentIndex(1);

    ui->monitorTypeCombo->addItems(DataCodec::typeNames());
    ui->monitorTypeCombo->setCurrentIndex(DataCodec::Int16);

    ui->monitorOrderCombo->addItems(DataCodec::byteOrderNames());
    ui->monitorOrderCombo->setCurrentIndex(DataCodec::OrderABCD);

    ui->monitorAddressSpin->setMaximum(kAddressCount - 1);
    ui->monitorLengthSpin->setMaximum(kAddressCount);

    ui->monitorTable->setColumnCount(MonitorWriteColumn + 1);
    onMonitorTypeChanged();
}

void MainWindow::setupConnections()
{
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(ui->saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLog);

    connect(ui->actionSaveLog, &QAction::triggered, this, &MainWindow::onSaveLog);
    connect(ui->actionClearLog, &QAction::triggered, this, &MainWindow::onClearLog);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    connect(ui->acceptConnectionsCheck, &QCheckBox::toggled,
            this, &MainWindow::acceptConnectionsRequested);

    // The worker drops filtered messages at the source, so a fast polling
    // client cannot flood the GUI event queue with read notifications.
    ui->logLevelCombo->addItems({ tr("Debug"), tr("Info"), tr("Warning"), tr("Error") });
    ui->logLevelCombo->setCurrentIndex(LogInfo);
    connect(ui->logLevelCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::logLevelChanged);

    // Per-area tool bars. The widget order matches ModbusDemo::Area.
    struct AreaControls {
        QSpinBox *fillSpin;
        QPushButton *fillButton;
        QPushButton *clearButton;
        QPushButton *randomButton;
    };
    const AreaControls controls[AreaCount] = {
        { ui->coilsFillSpin, ui->coilsFillButton,
          ui->coilsClearButton, ui->coilsRandomButton },
        { ui->discreteInputsFillSpin, ui->discreteInputsFillButton,
          ui->discreteInputsClearButton, ui->discreteInputsRandomButton },
        { ui->inputRegistersFillSpin, ui->inputRegistersFillButton,
          ui->inputRegistersClearButton, ui->inputRegistersRandomButton },
        { ui->holdingRegistersFillSpin, ui->holdingRegistersFillButton,
          ui->holdingRegistersClearButton, ui->holdingRegistersRandomButton },
    };

    for (int area = 0; area < AreaCount; ++area) {
        const AreaControls &c = controls[area];
        QSpinBox *spin = c.fillSpin;

        connect(c.fillButton, &QPushButton::clicked, this,
                [this, area, spin]() { emit fillAreaRequested(area, spin->value()); });
        connect(c.clearButton, &QPushButton::clicked, this,
                [this, area]() { emit fillAreaRequested(area, 0); });
        connect(c.randomButton, &QPushButton::clicked, this,
                [this, area]() { emit randomizeAreaRequested(area); });

        connect(m_plcRefChecks[area], &QCheckBox::toggled, this,
                [this, area]() { updatePlcReferences(area); });
        connect(m_plcBaseSpins[area], &QSpinBox::valueChanged, this,
                [this, area]() { updatePlcReferences(area); });

        if (QComboBox *formatCombo = m_formatCombos[area]) {
            connect(formatCombo, &QComboBox::currentIndexChanged, this, [this, area](int index) {
                m_format[area] = index;
                if (m_orderCombos[area])
                    m_orderCombos[area]->setEnabled(DataCodec::registerCount(index, 1) > 1);
                rebuildAreaTable(area);
            });
        }
        if (QComboBox *orderCombo = m_orderCombos[area]) {
            connect(orderCombo, &QComboBox::currentIndexChanged, this, [this, area](int index) {
                m_order[area] = index;
                refreshAreaRange(area, kStartAddress, kStartAddress + kAddressCount - 1);
            });
        }

        connect(m_tables.at(area), &QTableWidget::itemChanged,
                this, &MainWindow::onTableItemChanged);
    }

    connect(ui->monitorTypeCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onMonitorTypeChanged);
    connect(ui->monitorAddButton, &QPushButton::clicked, this, &MainWindow::onMonitorAdd);
    connect(ui->monitorRemoveButton, &QPushButton::clicked, this, &MainWindow::onMonitorRemove);
    connect(ui->monitorClearButton, &QPushButton::clicked, this, &MainWindow::onMonitorClear);
    connect(ui->monitorWriteButton, &QPushButton::clicked, this, &MainWindow::onMonitorWrite);
}

void MainWindow::startWorkerThread()
{
    m_thread = new QThread;           // not parented: shutdownWorker() deletes it
    m_thread->setObjectName(QStringLiteral("ModbusRunnerThread"));

    m_runner = new ModbusRunner;      // no parent: required before moveToThread()
    m_runner->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_runner, &QObject::deleteLater);
    connect(m_thread, &QThread::started, m_runner, &ModbusRunner::initialise);

    // GUI -> worker (queued automatically, the objects live in different threads)
    connect(this, &MainWindow::startServerRequested, m_runner, &ModbusRunner::startServer);
    connect(this, &MainWindow::stopServerRequested, m_runner, &ModbusRunner::stopServer);
    connect(this, &MainWindow::writeValueRequested, m_runner, &ModbusRunner::writeValue);
    connect(this, &MainWindow::writeBlockRequested, m_runner, &ModbusRunner::writeBlock);
    connect(this, &MainWindow::fillAreaRequested, m_runner, &ModbusRunner::fillArea);
    connect(this, &MainWindow::randomizeAreaRequested, m_runner, &ModbusRunner::randomizeArea);
    connect(this, &MainWindow::acceptConnectionsRequested,
            m_runner, &ModbusRunner::setAcceptingConnections);
    connect(this, &MainWindow::logLevelChanged, m_runner, &ModbusRunner::setLogLevel);

    // worker -> GUI
    connect(m_runner, &ModbusRunner::logMessage, this, &MainWindow::onLogMessage);
    connect(m_runner, &ModbusRunner::serverStateChanged, this, &MainWindow::onServerStateChanged);
    connect(m_runner, &ModbusRunner::blockChanged, this, &MainWindow::onBlockChanged);
    connect(m_runner, &ModbusRunner::statsChanged, this, &MainWindow::onStatsChanged);

    m_thread->start();

    // Push the current filter so the runner starts out in sync with the combo.
    emit logLevelChanged(ui->logLevelCombo->currentIndex());
}

// ------------------------------------------------------------ area tables ---

int MainWindow::registersPerRow(int area) const
{
    if (area < 0 || area >= AreaCount)
        return 1;
    if (isBitArea(area))
        return 1;
    // ASCII is shown like a PLC batch monitor does it: one register per row,
    // that is two characters.
    return qMax(1, DataCodec::registerCount(m_format[area], 1));
}

int MainWindow::rowsForArea(int area) const
{
    return kAddressCount / registersPerRow(area);
}

void MainWindow::rebuildAreaTable(int area)
{
    if (area < 0 || area >= m_tables.size())
        return;

    QTableWidget *table = m_tables.at(area);
    const bool bits = isBitArea(area);
    const int perRow = registersPerRow(area);
    const int rows = rowsForArea(area);

    m_updatingTables = true;
    table->clearContents();
    table->setRowCount(rows);

    for (int row = 0; row < rows; ++row) {
        const int address = kStartAddress + row * perRow;

        table->setItem(row, AddressColumn, readOnlyItem(spanText(address, perRow)));
        table->setItem(row, ReferenceColumn, readOnlyItem(QString()));

        QTableWidgetItem *valueItem = new QTableWidgetItem;
        if (bits) {
            valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                | Qt::ItemIsUserCheckable);
            valueItem->setCheckState(Qt::Unchecked);
        } else {
            valueItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                                | Qt::ItemIsEditable);
            valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        table->setItem(row, ValueColumn, valueItem);

        table->setItem(row, ExtraColumn, readOnlyItem(QString()));
    }
    m_updatingTables = false;

    updatePlcReferences(area);
    refreshAreaRange(area, kStartAddress, kStartAddress + kAddressCount - 1);
    table->resizeColumnsToContents();
}

void MainWindow::updateAreaRow(int area, int row)
{
    if (area < 0 || area >= m_tables.size())
        return;

    QTableWidget *table = m_tables.at(area);
    if (row < 0 || row >= table->rowCount())
        return;

    const int perRow = registersPerRow(area);
    const QList<quint16> group = valuesFor(area, kStartAddress + row * perRow, perRow);
    if (group.isEmpty())
        return;

    QTableWidgetItem *valueItem = table->item(row, ValueColumn);
    QTableWidgetItem *extraItem = table->item(row, ExtraColumn);
    if (!valueItem || !extraItem)
        return;

    m_updatingTables = true;
    if (isBitArea(area)) {
        const bool on = group.at(0) != 0;
        valueItem->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        valueItem->setText(on ? tr("ON") : tr("OFF"));
        extraItem->setText(on ? QStringLiteral("1") : QStringLiteral("0"));
    } else {
        valueItem->setText(DataCodec::decode(m_format[area], m_order[area], group));
        extraItem->setText(rawWords(group));
    }
    m_updatingTables = false;
}

void MainWindow::refreshAreaRange(int area, int firstAddress, int lastAddress)
{
    if (area < 0 || area >= m_tables.size())
        return;

    const int perRow = registersPerRow(area);
    const int rows = m_tables.at(area)->rowCount();

    int firstRow = (firstAddress - kStartAddress) / perRow;
    int lastRow = (lastAddress - kStartAddress) / perRow;
    firstRow = qBound(0, firstRow, rows - 1);
    lastRow = qBound(0, lastRow, rows - 1);

    for (int row = firstRow; row <= lastRow; ++row)
        updateAreaRow(area, row);
}

void MainWindow::updatePlcReferences(int area)
{
    if (area < 0 || area >= m_tables.size())
        return;

    QTableWidget *table = m_tables.at(area);
    const bool visible = m_plcRefChecks[area]->isChecked();
    const int base = m_plcBaseSpins[area]->value();
    const int perRow = registersPerRow(area);

    m_plcBaseSpins[area]->setEnabled(visible);
    table->setColumnHidden(ReferenceColumn, !visible);
    if (!visible)
        return;

    m_updatingTables = true;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (QTableWidgetItem *item = table->item(row, ReferenceColumn))
            item->setText(spanText(base + row * perRow, perRow));
    }
    m_updatingTables = false;

    table->resizeColumnToContents(ReferenceColumn);
}

QList<quint16> MainWindow::valuesFor(int area, int address, int count) const
{
    if (area < 0 || area >= AreaCount || count <= 0)
        return {};

    const int offset = address - kStartAddress;
    if (offset < 0 || offset + count > m_values[area].size())
        return {};

    return m_values[area].mid(offset, count);
}

// --------------------------------------------------------- worker signals ---

void MainWindow::onLogMessage(int level, const QString &message)
{
    if (level < ui->logLevelCombo->currentIndex())
        return;
    appendLogLine(level, message);
}

void MainWindow::appendLogLine(int level, const QString &message)
{
    QScrollBar *bar = ui->logView->verticalScrollBar();
    const int previousPosition = bar->value();
    const int blocksBefore = ui->logView->blockCount();

    const QString line =
        QStringLiteral("<span style=\"color:%1;\">%2 [%3]</span> %4")
            .arg(levelColor(level),
                 QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                 levelTag(level),
                 message.toHtmlEscaped());
    ui->logView->appendHtml(line);

    if (ui->autoScrollCheck->isChecked()) {
        bar->setValue(bar->maximum());
    } else {
        // Once maximumBlockCount is reached, appending drops the oldest line
        // and the visible text shifts up by one block. Compensate so the view
        // keeps showing the same messages.
        const bool trimmed = ui->logView->blockCount() == blocksBefore;
        bar->setValue(qMax(0, previousPosition - (trimmed ? 1 : 0)));
    }
}

void MainWindow::onServerStateChanged(int state, const QString &text, bool running)
{
    Q_UNUSED(state)

    ui->stateValueLabel->setText(text);
    ui->startButton->setEnabled(!running);
    ui->stopButton->setEnabled(running);
    ui->addressCombo->setEnabled(!running);
    ui->portSpin->setEnabled(!running);
    ui->unitIdSpin->setEnabled(!running);
    ui->statusbar->showMessage(text);
}

void MainWindow::onBlockChanged(const ModbusBlock &block)
{
    if (block.area < 0 || block.area >= AreaCount || block.values.isEmpty())
        return;

    const int offset = block.startAddress - kStartAddress;
    for (qsizetype i = 0; i < block.values.size(); ++i) {
        const qsizetype index = offset + i;
        if (index >= 0 && index < m_values[block.area].size())
            m_values[block.area][index] = block.values.at(i);
    }

    const int last = block.startAddress + int(block.values.size()) - 1;
    refreshAreaRange(block.area, block.startAddress, last);
    refreshMonitorRange(block.area, block.startAddress, last);
}

void MainWindow::onStatsChanged(const ModbusStats &stats)
{
    const QLocale locale;
    ui->clientsValueLabel->setText(QString::number(stats.connectedClients));
    ui->readsValueLabel->setText(locale.toString(qulonglong(stats.readRequests)));
    ui->writesValueLabel->setText(locale.toString(qulonglong(stats.writeRequests)));
    ui->exceptionsValueLabel->setText(locale.toString(qulonglong(stats.exceptions)));
}

// ------------------------------------------------------- widget reactions ---

void MainWindow::onStartClicked()
{
    QString address = ui->addressCombo->currentText().trimmed();
    if (address.isEmpty())
        address = QStringLiteral("0.0.0.0");

    ui->startButton->setEnabled(false);
    emit startServerRequested(address, ui->portSpin->value(), ui->unitIdSpin->value());
}

void MainWindow::onStopClicked()
{
    ui->stopButton->setEnabled(false);
    emit stopServerRequested();
}

void MainWindow::onTableItemChanged(QTableWidgetItem *item)
{
    if (m_updatingTables || !item || item->column() != ValueColumn)
        return;

    const int area = int(m_tables.indexOf(item->tableWidget()));
    if (area < 0)
        return;

    const int perRow = registersPerRow(area);
    const int row = item->row();
    const int address = kStartAddress + row * perRow;

    if (isBitArea(area)) {
        emit writeValueRequested(area, address,
                                 item->checkState() == Qt::Checked ? 1 : 0);
        return;
    }

    QList<quint16> values;
    QString error;
    if (!DataCodec::encode(m_format[area], m_order[area], item->text(), perRow,
                           &values, &error)) {
        logLocal(LogWarning, tr("%1[%2] not written: %3")
                                 .arg(areaName(area), spanText(address, perRow), error));
        updateAreaRow(area, row);   // put the previous value back
        return;
    }

    ModbusBlock block;
    block.area = area;
    block.startAddress = address;
    block.values = values;
    block.fromClient = false;
    emit writeBlockRequested(block);
}

// ---------------------------------------------------------- data monitor ---

void MainWindow::onMonitorTypeChanged()
{
    const int type = ui->monitorTypeCombo->currentIndex();
    const bool variable = DataCodec::hasVariableLength(type);

    ui->monitorLengthSpin->setEnabled(variable);
    if (!variable)
        ui->monitorLengthSpin->setValue(DataCodec::registerCount(type));
}

void MainWindow::onMonitorAdd()
{
    MonitorEntry entry;
    entry.name = ui->monitorNameEdit->text().trimmed();
    entry.area = ui->monitorAreaCombo->currentData().toInt();
    entry.address = ui->monitorAddressSpin->value();
    entry.type = ui->monitorTypeCombo->currentIndex();
    entry.order = ui->monitorOrderCombo->currentIndex();
    entry.length = ui->monitorLengthSpin->value();

    const int registers = DataCodec::registerCount(entry.type, entry.length);
    if (registers <= 0)
        return;

    if (entry.address - kStartAddress + registers > kAddressCount) {
        QMessageBox::warning(this, tr("Add value"),
                             tr("%1 needs %2 register(s) starting at address %3, "
                                "which runs past the last address (%4).")
                                 .arg(DataCodec::typeName(entry.type))
                                 .arg(registers)
                                 .arg(entry.address)
                                 .arg(kStartAddress + kAddressCount - 1));
        return;
    }

    if (entry.name.isEmpty()) {
        entry.name = tr("%1 @ %2")
                         .arg(DataCodec::typeName(entry.type).section(QLatin1Char(' '), 0, 0))
                         .arg(entry.address);
    }

    m_monitor.append(entry);
    addMonitorRow(entry);
    ui->monitorNameEdit->clear();

    logLocal(LogInfo, tr("Monitoring %1: %2 %3 at %4[%5], %6 register(s).")
                          .arg(entry.name,
                               DataCodec::typeName(entry.type),
                               DataCodec::byteOrderName(entry.order).section(QLatin1Char(' '), 0, 0),
                               areaName(entry.area))
                          .arg(entry.address)
                          .arg(registers));
}

void MainWindow::addMonitorRow(const MonitorEntry &entry)
{
    const int row = ui->monitorTable->rowCount();
    const int registers = DataCodec::registerCount(entry.type, entry.length);

    ui->monitorTable->insertRow(row);
    ui->monitorTable->setItem(row, MonitorNameColumn, readOnlyItem(entry.name));
    ui->monitorTable->setItem(row, MonitorAreaColumn, readOnlyItem(areaName(entry.area)));
    ui->monitorTable->setItem(row, MonitorAddressColumn,
                              readOnlyItem(spanText(entry.address, registers)));
    ui->monitorTable->setItem(row, MonitorTypeColumn,
                              readOnlyItem(DataCodec::typeName(entry.type)));
    ui->monitorTable->setItem(row, MonitorOrderColumn,
                              readOnlyItem(DataCodec::byteOrderName(entry.order)));
    ui->monitorTable->setItem(row, MonitorRegistersColumn,
                              readOnlyItem(QString::number(registers)));
    ui->monitorTable->setItem(row, MonitorValueColumn, readOnlyItem(QString()));

    QTableWidgetItem *writeItem = new QTableWidgetItem;
    writeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    writeItem->setToolTip(tr("Type a value here and press \"Write selected\"."));
    ui->monitorTable->setItem(row, MonitorWriteColumn, writeItem);

    refreshMonitorRow(row);
    ui->monitorTable->resizeColumnsToContents();
}

void MainWindow::refreshMonitorRow(int row)
{
    if (row < 0 || row >= m_monitor.size())
        return;

    const MonitorEntry &entry = m_monitor.at(row);
    const int registers = DataCodec::registerCount(entry.type, entry.length);
    const QList<quint16> values = valuesFor(entry.area, entry.address, registers);

    QString text = DataCodec::decode(entry.type, entry.order, values);
    if (entry.type == DataCodec::AsciiString)
        text = QLatin1Char('"') + text + QLatin1Char('"');

    if (QTableWidgetItem *item = ui->monitorTable->item(row, MonitorValueColumn))
        item->setText(text);
}

void MainWindow::refreshMonitorRange(int area, int firstAddress, int lastAddress)
{
    for (int row = 0; row < m_monitor.size(); ++row) {
        const MonitorEntry &entry = m_monitor.at(row);
        if (entry.area != area)
            continue;

        const int first = entry.address;
        const int last = entry.address + DataCodec::registerCount(entry.type, entry.length) - 1;
        if (last < firstAddress || first > lastAddress)
            continue;

        refreshMonitorRow(row);
    }
}

void MainWindow::onMonitorRemove()
{
    QList<int> rows;
    const QList<QTableWidgetItem *> selected = ui->monitorTable->selectedItems();
    for (QTableWidgetItem *item : selected) {
        if (!rows.contains(item->row()))
            rows.append(item->row());
    }
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Remove value"), tr("Select a row first."));
        return;
    }

    std::sort(rows.begin(), rows.end());
    for (int i = rows.size() - 1; i >= 0; --i) {
        const int row = rows.at(i);
        if (row >= 0 && row < m_monitor.size()) {
            m_monitor.removeAt(row);
            ui->monitorTable->removeRow(row);
        }
    }
}

void MainWindow::onMonitorClear()
{
    m_monitor.clear();
    ui->monitorTable->setRowCount(0);
}

void MainWindow::onMonitorWrite()
{
    QList<int> rows;
    const QList<QTableWidgetItem *> selected = ui->monitorTable->selectedItems();
    for (QTableWidgetItem *item : selected) {
        if (!rows.contains(item->row()))
            rows.append(item->row());
    }
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Write value"),
                                 tr("Select the rows you want to write first."));
        return;
    }
    std::sort(rows.begin(), rows.end());

    for (int row : std::as_const(rows)) {
        if (row < 0 || row >= m_monitor.size())
            continue;

        const MonitorEntry &entry = m_monitor.at(row);
        const QTableWidgetItem *writeItem = ui->monitorTable->item(row, MonitorWriteColumn);
        const QString text = writeItem ? writeItem->text() : QString();
        if (text.isEmpty())
            continue;

        QList<quint16> values;
        QString error;
        if (!DataCodec::encode(entry.type, entry.order, text, entry.length, &values, &error)) {
            QMessageBox::warning(this, tr("Write value"),
                                 tr("%1: %2").arg(entry.name, error));
            return;
        }

        ModbusBlock block;
        block.area = entry.area;
        block.startAddress = entry.address;
        block.values = values;
        block.fromClient = false;
        emit writeBlockRequested(block);
    }
}

// ----------------------------------------------------------------- files ---

void MainWindow::onClearLog()
{
    ui->logView->clear();
}

void MainWindow::onSaveLog()
{
    const QString suggestion =
        QStringLiteral("modbus-log-%1.txt")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));

    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save event log"), suggestion, tr("Text files (*.txt);;All files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save event log"),
                             tr("Cannot write %1:\n%2").arg(fileName, file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << ui->logView->toPlainText();
    file.close();

    logLocal(LogInfo, tr("Log saved to %1").arg(fileName));
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this, tr("About this demo"),
        tr("<h3>Modbus TCP Server Demo</h3>"
           "<p>A Qt Widgets demo of a Modbus TCP server exposing all four Modbus areas "
           "with %1 addresses each:</p>"
           "<ul>"
           "<li>Coils (0x) - FC01 / FC05 / FC15</li>"
           "<li>Discrete Inputs (1x) - FC02</li>"
           "<li>Input Registers (3x) - FC04</li>"
           "<li>Holding Registers (4x) - FC03 / FC06 / FC16</li>"
           "</ul>"
           "<p>The register tabs display and accept data in the format selected above the "
           "table - INT16/32/64, UINT, HEX, BCD, FLOAT32 (REAL), FLOAT64 or ASCII - in any "
           "of the four byte orders (ABCD, CDAB, BADC, DCBA). The <b>Data Monitor</b> tab "
           "does the same for a hand picked watch list.</p>"
           "<p>The Modbus instance lives in a dedicated worker thread "
           "(<tt>ModbusRunner</tt>), so protocol handling never blocks the HMI.</p>")
            .arg(kAddressCount));
}

// ----------------------------------------------------------------- helpers ---

void MainWindow::logLocal(int level, const QString &message)
{
    // Locally generated notices bypass the filter: they answer something the
    // user just did, so they must never be silently dropped.
    appendLogLine(level, message);
}

void MainWindow::showLocalAddresses()
{
    QStringList addresses;
    const QList<QHostAddress> all = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : all) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol)
            continue;
        if (address.isLoopback())
            continue;
        addresses << address.toString();
    }

    if (addresses.isEmpty())
        logLocal(LogInfo, tr("No external IPv4 address found - use 127.0.0.1."));
    else
        logLocal(LogInfo, tr("Local IPv4 addresses: %1").arg(addresses.join(QLatin1String(", "))));
}
