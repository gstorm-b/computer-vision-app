#include "vision_virtual_client.h"

#include <QHostAddress>

VisionTcpipVirtualClient::VisionTcpipVirtualClient(QObject *parent)
    : QObject(parent) {}

// ───────────────────────────────��─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void VisionTcpipVirtualClient::connectToServer(const VirtualClientConfig &cfg) {
    cleanup();
    m_config = cfg;
    m_stats  = {};

    m_mainSocket = new QTcpSocket(this);
    connect(m_mainSocket, &QTcpSocket::connected,    this, &VisionTcpipVirtualClient::onMainConnected);
    connect(m_mainSocket, &QTcpSocket::disconnected, this, &VisionTcpipVirtualClient::onMainDisconnected);
    connect(m_mainSocket, &QTcpSocket::readyRead,    this, &VisionTcpipVirtualClient::onMainReadyRead);
    connect(m_mainSocket, &QAbstractSocket::errorOccurred,
            this, &VisionTcpipVirtualClient::onMainError);

    m_hbSocket = new QTcpSocket(this);
    connect(m_hbSocket, &QTcpSocket::connected,    this, &VisionTcpipVirtualClient::onHbConnected);
    connect(m_hbSocket, &QTcpSocket::disconnected, this, &VisionTcpipVirtualClient::onHbDisconnected);
    connect(m_hbSocket, &QTcpSocket::readyRead,    this, &VisionTcpipVirtualClient::onHbReadyRead);
    connect(m_hbSocket, &QAbstractSocket::errorOccurred,
            this, &VisionTcpipVirtualClient::onHbError);

    m_mainSocket->connectToHost(cfg.host, static_cast<quint16>(cfg.mainPort));
    m_hbSocket->connectToHost(cfg.host, static_cast<quint16>(cfg.hbPort));
}

void VisionTcpipVirtualClient::disconnectFromServer() {
    cleanup();
}

void VisionTcpipVirtualClient::sendMainPayload(const QByteArray &data) {
    if (!m_mainSocket || m_mainSocket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("Main socket not connected"));
        return;
    }
    QByteArray payload = data;
    if (!payload.endsWith(VCLIENT_MAIN_TERM)) {
        payload.append(VCLIENT_MAIN_TERM);
    }
    m_mainSocket->write(payload);
    m_mainSocket->flush();
    ++m_stats.mainPayloadsSent;
}

bool VisionTcpipVirtualClient::isMainConnected() const {
    return m_mainSocket
           && m_mainSocket->state() == QAbstractSocket::ConnectedState;
}

bool VisionTcpipVirtualClient::isHbConnected() const {
    return m_hbSocket
           && m_hbSocket->state() == QAbstractSocket::ConnectedState;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main port
// ─────────────────────────────────────────────────────────────────────────────

void VisionTcpipVirtualClient::onMainConnected() {
    m_mainRxBuf.clear();
    emit mainConnectedChanged(true);
}

void VisionTcpipVirtualClient::onMainDisconnected() {
    m_mainRxBuf.clear();
    emit mainConnectedChanged(false);
}

void VisionTcpipVirtualClient::onMainError(QAbstractSocket::SocketError) {
    if (m_mainSocket) {
        emit errorOccurred(QStringLiteral("Main: ") + m_mainSocket->errorString());
    }
}

void VisionTcpipVirtualClient::onMainReadyRead() {
    if (!m_mainSocket) return;
    m_mainRxBuf.append(m_mainSocket->readAll());

    int idx = m_mainRxBuf.indexOf(VCLIENT_MAIN_TERM);
    while (idx >= 0) {
        QByteArray msg = m_mainRxBuf.left(idx + 1);
        m_mainRxBuf.remove(0, idx + 1);
        ++m_stats.mainPayloadsReceived;
        emit mainPayloadReceived(msg);
        idx = m_mainRxBuf.indexOf(VCLIENT_MAIN_TERM);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Heartbeat port
// ─────────────────────────────────────────────────────────────────────────────

void VisionTcpipVirtualClient::onHbConnected() {
    m_hbRxBuf.clear();
    m_stats.hbAckCount = 0;
    emit hbConnectedChanged(true);
}

void VisionTcpipVirtualClient::onHbDisconnected() {
    m_hbRxBuf.clear();
    emit hbConnectedChanged(false);
}

void VisionTcpipVirtualClient::onHbError(QAbstractSocket::SocketError) {
    if (m_hbSocket) {
        emit errorOccurred(QStringLiteral("Heartbeat: ") + m_hbSocket->errorString());
    }
}

void VisionTcpipVirtualClient::onHbReadyRead() {
    if (!m_hbSocket) return;
    m_hbRxBuf.append(m_hbSocket->readAll());

    int idx = m_hbRxBuf.indexOf(VCLIENT_HB_TERM);
    while (idx >= 0) {
        QByteArray msg = m_hbRxBuf.left(idx + 1);
        m_hbRxBuf.remove(0, idx + 1);
        handleHbMessage(msg);
        idx = m_hbRxBuf.indexOf(VCLIENT_HB_TERM);
    }
}

void VisionTcpipVirtualClient::handleHbMessage(const QByteArray &msg) {
    if (msg == QByteArray(VCLIENT_HB_DISCONNECT)) {
        // Server announced a planned close; the link will go down next. This is
        // expected, not a fault — surface it distinctly from socket errors.
        emit serverDisconnectNotice();
        return;
    }
    if (msg != QByteArray(VCLIENT_HB_PROBE)) {
        // Unexpected message — ignore silently (server could send non-probe in future)
        return;
    }
    ++m_stats.hbProbesReceived;

    // Reply: "ack,{count}."
    const QByteArray ack =
        QByteArray(VCLIENT_HB_ACK_PFX)
        + QByteArray::number(m_stats.hbAckCount)
        + VCLIENT_HB_TERM;

    m_hbSocket->write(ack);
    m_hbSocket->flush();

    const quint16 sentCount = m_stats.hbAckCount;
    // Wrap: increment as quint16, rollover is well-defined for unsigned types.
    ++m_stats.hbAckCount;
    ++m_stats.hbAcksSent;

    emit hbAckSent(sentCount);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal
// ─────────────────────────────────────────────────────────────────────────────

void VisionTcpipVirtualClient::cleanup() {
    if (m_mainSocket) {
        m_mainSocket->disconnect(this);
        m_mainSocket->abort();
        m_mainSocket->deleteLater();
        m_mainSocket = nullptr;
    }
    if (m_hbSocket) {
        m_hbSocket->disconnect(this);
        m_hbSocket->abort();
        m_hbSocket->deleteLater();
        m_hbSocket = nullptr;
    }
    m_mainRxBuf.clear();
    m_hbRxBuf.clear();
}
