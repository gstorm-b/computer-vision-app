#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new VisionTcpipVirtualClient(this))
{
    ui->setupUi(this);

    // ── Wiring ────────────────────────────────────────────────────────────────
    connect(ui->btn_connect,      &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->btn_send_trigger, &QPushButton::clicked, this, &MainWindow::onSendTriggerClicked);
    connect(ui->btn_clear_log,    &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    connect(m_client, &VisionTcpipVirtualClient::mainConnectedChanged,
            this,     &MainWindow::onMainConnectedChanged);
    connect(m_client, &VisionTcpipVirtualClient::hbConnectedChanged,
            this,     &MainWindow::onHbConnectedChanged);
    connect(m_client, &VisionTcpipVirtualClient::mainPayloadReceived,
            this,     &MainWindow::onMainPayloadReceived);
    connect(m_client, &VisionTcpipVirtualClient::hbAckSent,
            this,     &MainWindow::onHbAckSent);
    connect(m_client, &VisionTcpipVirtualClient::errorOccurred,
            this,     &MainWindow::onErrorOccurred);

    updateStatusLabels();
}

MainWindow::~MainWindow() {
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slots — user actions
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onConnectClicked() {
    if (m_client->isMainConnected() || m_client->isHbConnected()) {
        m_client->disconnectFromServer();
        ui->btn_connect->setText(tr("Connect"));
    } else {
        VirtualClientConfig cfg;
        cfg.host     = ui->ledit_host->text().trimmed();
        cfg.mainPort = ui->spb_main_port->value();
        cfg.hbPort   = ui->spb_hb_port->value();
        m_client->connectToServer(cfg);
        ui->btn_connect->setText(tr("Disconnect"));
    }
}

void MainWindow::onSendTriggerClicked() {
    const QByteArray payload = ui->ledit_trigger->text().toUtf8();
    if (payload.isEmpty()) return;
    m_client->sendMainPayload(payload);
    appendLog(QStringLiteral("[TX] ") + QString::fromUtf8(payload));
    ui->lbl_tx_count->setText(
        QString::number(m_client->stats().mainPayloadsSent));
}

void MainWindow::onClearLogClicked() {
    ui->pte_log->clear();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slots — client signals
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onMainConnectedChanged(bool connected) {
    updateStatusLabels();
    appendLog(connected ? QStringLiteral("[INFO] Main port connected")
                        : QStringLiteral("[INFO] Main port disconnected"));
}

void MainWindow::onHbConnectedChanged(bool connected) {
    updateStatusLabels();
    appendLog(connected ? QStringLiteral("[INFO] Heartbeat port connected")
                        : QStringLiteral("[INFO] Heartbeat port disconnected"));
}

void MainWindow::onMainPayloadReceived(const QByteArray &payload) {
    appendLog(QStringLiteral("[RX]  ") + QString::fromUtf8(payload));
    ui->lbl_rx_count->setText(
        QString::number(m_client->stats().mainPayloadsReceived));
}

void MainWindow::onHbAckSent(quint16 ackCount) {
    ui->lbl_hb_probes->setText(
        QString::number(m_client->stats().hbProbesReceived));
    ui->lbl_hb_acks->setText(
        QString::number(m_client->stats().hbAcksSent));
    ui->lbl_hb_count->setText(QString::number(ackCount));
}

void MainWindow::onErrorOccurred(const QString &msg) {
    appendLog(QStringLiteral("[ERR] ") + msg);
    updateStatusLabels();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::updateStatusLabels() {
    const bool mainOk = m_client->isMainConnected();
    const bool hbOk   = m_client->isHbConnected();

    ui->lbl_main_status->setText(mainOk ? tr("Connected") : tr("Disconnected"));
    ui->lbl_hb_status->setText(  hbOk   ? tr("Connected") : tr("Disconnected"));

    ui->lbl_main_status->setProperty("connected", mainOk);
    ui->lbl_hb_status->setProperty("connected",   hbOk);
    for (auto *w : {ui->lbl_main_status, ui->lbl_hb_status}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }

    ui->btn_send_trigger->setEnabled(mainOk);
}

void MainWindow::appendLog(const QString &line) {
    const QString ts = QDateTime::currentDateTime().toString(
        QStringLiteral("hh:mm:ss.zzz"));
    ui->pte_log->appendPlainText(
        QStringLiteral("[") + ts + QStringLiteral("] ") + line);
}
