#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gadget_table.h"

#include "device/plc/mc_context_1c.h"
#include "device/plc/mc_context_3c.h"
#include "device/plc/mc_context_3e.h"
#include "device/plc/mc_context_factory.h"
#include "device/plc/mc_msg_serial_port.h"
#include "device/plc/mc_msg_tcp_client.h"

#include <QCloseEvent>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>

using namespace vc::device;

namespace {

/// Frames offered, in the order they appear in the combo. Only the ones a codec exists for —
/// Frame_1E has no context factory arm, so offering it would produce a bench that cannot run.
constexpr mc::McFrameType kFrames[] = {
    mc::McFrameType::Frame_3E,
    mc::McFrameType::Frame_1C,
    mc::McFrameType::Frame_3C
};

/// Settings key under which each frame's parameters are stored, so the 1C serial settings and
/// the 3E IP address survive switching back and forth.
QString contextKey(mc::McFrameType frame) {
    return QStringLiteral("context/") + mc::McFrameTypeToString(frame);
}

/// Formats microseconds as milliseconds with two decimals, or "—" when there is no sample.
QString ms(qint64 microseconds) {
    if (microseconds <= 0) {
        return QStringLiteral("—");
    }
    return QString::number(microseconds / 1000.0, 'f', 2);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow) {

    ui->setupUi(this);
    qRegisterMetaType<BenchReport>("BenchReport");

    m_transportTable = new GadgetTable(this);
    m_contextTable = new GadgetTable(this);
    ui->grp_transport->layout()->addWidget(m_transportTable);
    ui->grp_protocol->layout()->addWidget(m_contextTable);

    ui->tbl_results->setColumnCount(9);
    ui->tbl_results->setHorizontalHeaderLabels(
        {tr("Command"), tr("Sent"), tr("OK"), tr("Failed"),
         tr("min ms"), tr("avg ms"), tr("p95 ms"), tr("max ms"), tr("Failures by stage")});
    ui->tbl_results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tbl_results->horizontalHeader()->setStretchLastSection(true);
    ui->tbl_results->verticalHeader()->setVisible(false);

    for (const auto frame : kFrames) {
        ui->cbx_frame->addItem(mc::McFrameTypeToString(frame), static_cast<int>(frame));
    }

    connect(ui->cbx_frame, &QComboBox::currentIndexChanged, this, &MainWindow::onFrameChanged);
    connect(ui->btn_run, &QPushButton::clicked, this, &MainWindow::onRun);
    connect(ui->btn_stop, &QPushButton::clicked, this, &MainWindow::onStop);

    loadSettings();
    onFrameChanged();
}

MainWindow::~MainWindow() {
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_thread && m_thread->isRunning()) {
        // Closing mid-run would tear the transport down from the wrong thread while a request
        // is in flight.
        QMessageBox::information(this, tr("Bench running"),
                                 tr("Stop the run before closing."));
        event->ignore();
        return;
    }
    commitTables();
    saveContext();
    saveSettings();
    QMainWindow::closeEvent(event);
}

/// Creates the context for `frame`, seeded with this bench's defaults for that frame.
///
/// The 1C defaults are the line settings the project owner measured on the real port
/// (COM3, 38400 8N1, no flow control) — a bench that starts on settings known to have worked
/// once saves re-entering them on every launch. Everything is editable, and whatever is edited
/// is remembered per frame.
void MainWindow::buildContext(mc::McFrameType frame) {
    m_context = Factory::contextFactory(frame);
    if (!m_context) {
        return;
    }

    // A small poll area by default. A bench that starts by reading 64 devices makes the first
    // run's numbers hard to compare against a single-device round trip, and on an unfamiliar
    // PLC a wide range is more likely to hit an unconfigured address and fail for a reason that
    // has nothing to do with timing.
    m_context->setStartMAddress(100);
    m_context->setAmountMAddress(16);
    m_context->setStartDAddress(100);
    m_context->setAmountDAddress(8);

    switch (frame) {
    case mc::McFrameType::Frame_1C: {
        auto *ctx = static_cast<Context_Mc1C *>(m_context.get());
        auto *serial = static_cast<McMsgSerialCfg *>(ctx->msgConfig());
        serial->m_portName = QStringLiteral("COM3");
        serial->m_baudRate = 38400;
        serial->m_dataBits = mc::McSerialDataBits::DataBits_8;
        serial->m_parity = mc::McSerialParity::Parity_None;
        serial->m_stopBits = mc::McSerialStopBits::StopBits_One;
        serial->m_flowControl = mc::McSerialFlowControl::FlowControl_None;
        break;
    }
    case mc::McFrameType::Frame_3C: {
        // Same physical port as 1C — the two C-frames are alternative protocols over one
        // cable, so starting them on the same line settings is the useful default. The
        // access-route numbers keep the reference implementation's values.
        auto *ctx = static_cast<Context_Mc3C *>(m_context.get());
        auto *serial = static_cast<McMsgSerialCfg *>(ctx->msgConfig());
        serial->m_portName = QStringLiteral("COM3");
        serial->m_baudRate = 38400;
        serial->m_dataBits = mc::McSerialDataBits::DataBits_8;
        serial->m_parity = mc::McSerialParity::Parity_None;
        serial->m_stopBits = mc::McSerialStopBits::StopBits_One;
        serial->m_flowControl = mc::McSerialFlowControl::FlowControl_None;
        break;
    }
    case mc::McFrameType::Frame_3E:
        // The Ethernet defaults are McMsgEthernetTcpCfg's own (192.168.0.1:5000); leaving them
        // alone keeps the one frame with field history on the values it has always used.
        break;
    default:
        break;
    }
}

void MainWindow::refreshTables() {
    if (!m_context) {
        m_contextTable->setGadget(QObject::staticMetaObject, nullptr);
        m_transportTable->setGadget(QObject::staticMetaObject, nullptr);
        return;
    }

    m_contextTable->setGadget(m_context->getMetaObject(), m_context.get());

    auto *msg = m_context->msgConfig();
    if (msg) {
        m_transportTable->setGadget(msg->getMetaObject(), msg);
    }
}

void MainWindow::commitTables() {
    m_contextTable->commit();
    m_transportTable->commit();
}

void MainWindow::onFrameChanged() {
    const auto frame = static_cast<mc::McFrameType>(ui->cbx_frame->currentData().toInt());
    buildContext(frame);
    loadContext();
    refreshTables();
}

void MainWindow::onRun() {
    if (m_thread && m_thread->isRunning()) {
        return;
    }

    commitTables();
    saveContext();
    saveSettings();

    if (!m_context) {
        QMessageBox::warning(this, tr("No context"), tr("This frame has no implementation."));
        return;
    }

    BenchConfig config;
    config.context = m_context.get();
    config.readBit = ui->chk_read_bit->isChecked();
    config.readWord = ui->chk_read_word->isChecked();
    config.writeBit = ui->chk_write_bit->isChecked();
    config.writeWord = ui->chk_write_word->isChecked();
    config.writeBitValue = static_cast<quint8>(ui->spb_write_bit_value->value());
    config.writeWordValue = static_cast<qint16>(ui->spb_write_word_value->value());
    config.iterations = ui->spb_iterations->value();

    if (!config.readBit && !config.readWord && !config.writeBit && !config.writeWord) {
        QMessageBox::warning(this, tr("Nothing to run"), tr("Enable at least one command."));
        return;
    }

    if (config.writeBit || config.writeWord) {
        const auto answer = QMessageBox::question(
            this, tr("Write to the PLC?"),
            tr("This run writes to M%1 / D%2 on the connected PLC %3 times.\n\n"
               "On a live machine that can move an axis or start a cycle. Continue?")
                .arg(m_context->startMAddress())
                .arg(m_context->startDAddress())
                .arg(config.iterations));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    ui->txt_log->clear();
    ui->tbl_results->setRowCount(0);
    ui->progress->setValue(0);
    ui->progress->setMaximum(config.iterations);

    m_thread = new QThread(this);
    m_runner = new BenchRunner;
    m_runner->setConfig(config);
    m_runner->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_runner, &BenchRunner::run);
    connect(m_runner, &BenchRunner::logLine, this, &MainWindow::onLogLine);
    connect(m_runner, &BenchRunner::progress, this, &MainWindow::onProgress);
    connect(m_runner, &BenchRunner::finished, this, &MainWindow::onFinished);
    connect(m_runner, &BenchRunner::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_runner, &QObject::deleteLater);

    setRunning(true);
    m_thread->start();
}

void MainWindow::onStop() {
    if (m_runner) {
        m_runner->requestStop();
    }
}

void MainWindow::onLogLine(const QString &line) {
    ui->txt_log->appendPlainText(line);
}

void MainWindow::onProgress(int done, int total) {
    ui->progress->setMaximum(total);
    ui->progress->setValue(done);
}

void MainWindow::onFinished(BenchReport report) {
    renderReport(report);
    setRunning(false);

    if (m_thread) {
        m_thread->wait(3000);
        m_thread->deleteLater();
        m_thread = nullptr;
    }
    m_runner = nullptr;
}

void MainWindow::renderReport(const BenchReport &report) {
    if (!report.connected) {
        ui->lbl_summary->setText(tr("Connect failed: %1").arg(report.connectError));
        return;
    }

    ui->tbl_results->setRowCount(report.commands.size());
    for (int row = 0; row < report.commands.size(); ++row) {
        const BenchCommandStat &stat = report.commands.at(row);
        const int failed = stat.sent - stat.ok;

        QStringList stages;
        for (auto it = stat.failuresByStage.cbegin(); it != stat.failuresByStage.cend(); ++it) {
            stages << QStringLiteral("%1×%2").arg(it.key()).arg(it.value());
        }

        const QStringList cells = {
            stat.name,
            QString::number(stat.sent),
            QString::number(stat.ok),
            QString::number(failed),
            ms(stat.minUs()),
            ms(stat.meanUs()),
            ms(stat.percentileUs(95)),
            ms(stat.maxUs()),
            stages.join(QStringLiteral(", "))
        };
        for (int col = 0; col < cells.size(); ++col) {
            ui->tbl_results->setItem(row, col, new QTableWidgetItem(cells.at(col)));
        }

        // The bytes behind a failure, in the log rather than the table: this is the one thing
        // that tells a wrong frame from a wrong wire, and it is worth reading in full.
        for (const BenchFailure &failure : stat.samples) {
            ui->txt_log->appendPlainText(
                tr("[%1] iteration %2 — %3: %4")
                    .arg(stat.name).arg(failure.iteration)
                    .arg(benchStageName(failure.stage), failure.detail));
            if (!failure.request.isEmpty()) {
                ui->txt_log->appendPlainText(
                    tr("    sent : %1").arg(QString::fromLatin1(failure.request.toHex(' '))));
            }
            if (!failure.response.isEmpty()) {
                ui->txt_log->appendPlainText(
                    tr("    recv : %1").arg(QString::fromLatin1(failure.response.toHex(' '))));
            }
        }
    }

    const double perSecond = (report.wallClockMs > 0)
        ? (report.totalOk * 1000.0 / report.wallClockMs)
        : 0.0;

    ui->lbl_summary->setText(
        tr("%1 of %2 requests completed in %3 ms — %4 requests/s.")
            .arg(report.totalOk).arg(report.totalRequests).arg(report.wallClockMs)
            .arg(QString::number(perSecond, 'f', 1)));
}

void MainWindow::setRunning(bool running) {
    ui->btn_run->setEnabled(!running);
    ui->btn_stop->setEnabled(running);
    ui->cbx_frame->setEnabled(!running);
    ui->grp_transport->setEnabled(!running);
    ui->grp_protocol->setEnabled(!running);
    ui->grp_commands->setEnabled(!running);
    ui->spb_iterations->setEnabled(!running);
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("frame"), ui->cbx_frame->currentIndex());
    settings.setValue(QStringLiteral("readBit"), ui->chk_read_bit->isChecked());
    settings.setValue(QStringLiteral("readWord"), ui->chk_read_word->isChecked());
    settings.setValue(QStringLiteral("writeBit"), ui->chk_write_bit->isChecked());
    settings.setValue(QStringLiteral("writeWord"), ui->chk_write_word->isChecked());
    settings.setValue(QStringLiteral("writeBitValue"), ui->spb_write_bit_value->value());
    settings.setValue(QStringLiteral("writeWordValue"), ui->spb_write_word_value->value());
    settings.setValue(QStringLiteral("iterations"), ui->spb_iterations->value());
}

void MainWindow::loadSettings() {
    QSettings settings;
    ui->cbx_frame->setCurrentIndex(settings.value(QStringLiteral("frame"), 0).toInt());
    ui->chk_read_bit->setChecked(settings.value(QStringLiteral("readBit"), true).toBool());
    ui->chk_read_word->setChecked(settings.value(QStringLiteral("readWord"), true).toBool());
    // Writes stay off unless they were explicitly turned on before: a bench that starts armed
    // to write is one misplaced click away from moving a machine.
    ui->chk_write_bit->setChecked(settings.value(QStringLiteral("writeBit"), false).toBool());
    ui->chk_write_word->setChecked(settings.value(QStringLiteral("writeWord"), false).toBool());
    ui->spb_write_bit_value->setValue(settings.value(QStringLiteral("writeBitValue"), 0).toInt());
    ui->spb_write_word_value->setValue(settings.value(QStringLiteral("writeWordValue"), 0).toInt());
    ui->spb_iterations->setValue(settings.value(QStringLiteral("iterations"), 100).toInt());
}

void MainWindow::saveContext() {
    if (!m_context) {
        return;
    }
    const auto frame = static_cast<mc::McFrameType>(ui->cbx_frame->currentData().toInt());
    QSettings settings;
    settings.setValue(contextKey(frame),
                      QString::fromUtf8(QJsonDocument(m_context->toJson()).toJson(
                          QJsonDocument::Compact)));
}

void MainWindow::loadContext() {
    if (!m_context) {
        return;
    }
    const auto frame = static_cast<mc::McFrameType>(ui->cbx_frame->currentData().toInt());
    QSettings settings;
    const QString stored = settings.value(contextKey(frame)).toString();
    if (stored.isEmpty()) {
        return;   // first run for this frame: keep the seeded defaults
    }
    const QJsonDocument doc = QJsonDocument::fromJson(stored.toUtf8());
    if (doc.isObject()) {
        m_context->fromJson(doc.object());
    }
}
