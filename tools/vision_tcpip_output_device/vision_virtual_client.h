#ifndef VISION_VIRTUAL_CLIENT_H
#define VISION_VIRTUAL_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>

// ──────────────────────────────��─────────────────────────────────��────────────
//  Protocol constants (mirror of VisionTcpipDevice in the main project)
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char *VCLIENT_HB_PROBE      = "connection_check.";
constexpr const char *VCLIENT_HB_ACK_PFX    = "ack,";
constexpr const char *VCLIENT_HB_DISCONNECT = "disconnect.";  // server planned close
constexpr char        VCLIENT_HB_TERM       = '.';
constexpr char        VCLIENT_MAIN_TERM     = ';';
constexpr int         VCLIENT_ACK_WRAP      = 1 << 16;  // quint16 rollover boundary

// ─────────────────────────────────────────────────────────────────────────────
//  VisionTcpipVirtualClient
//
//  Simulates the robot/external-system CLIENT that connects to a running
//  VisionTcpipDevice server (main project).
//
//  Main port  : client sends trigger payloads (';'-terminated).
//               Server responds with result payloads (same terminator).
//  Heartbeat  : server sends "connection_check.", client auto-replies
//               "ack,{count}." where count increments on each valid probe.
//               Count wraps at 2^16 (quint16 rollover).
// ─────────────────────────────────────────────────────────────────────────────

struct VirtualClientConfig {
    QString host{"127.0.0.1"};
    int mainPort{5000};
    int hbPort{5001};
};

struct VirtualClientStats {
    quint64 mainPayloadsSent{0};
    quint64 mainPayloadsReceived{0};
    quint64 hbProbesReceived{0};
    quint64 hbAcksSent{0};
    quint16 hbAckCount{0};
};

class VisionTcpipVirtualClient : public QObject {
    Q_OBJECT

public:
    explicit VisionTcpipVirtualClient(QObject *parent = nullptr);

    void connectToServer(const VirtualClientConfig &cfg);
    void disconnectFromServer();

    // Send a ';'-terminated trigger payload on the main port.
    // If data does not end with ';', it is appended automatically.
    void sendMainPayload(const QByteArray &data);

    bool isMainConnected() const;
    bool isHbConnected()   const;

    const VirtualClientStats &stats() const { return m_stats; }

signals:
    void mainConnectedChanged(bool connected);
    void hbConnectedChanged(bool connected);
    // Emitted for each complete ';'-terminated message received on main port.
    void mainPayloadReceived(const QByteArray &payload);
    // Emitted every time an ack is sent on the heartbeat port.
    void hbAckSent(quint16 ackCount);
    // Emitted when the server announces a planned close ("disconnect.") on the
    // heartbeat port, before the link goes down. Not a fault.
    void serverDisconnectNotice();
    void errorOccurred(const QString &msg);

private slots:
    void onMainConnected();
    void onMainDisconnected();
    void onMainError(QAbstractSocket::SocketError err);
    void onMainReadyRead();

    void onHbConnected();
    void onHbDisconnected();
    void onHbError(QAbstractSocket::SocketError err);
    void onHbReadyRead();

private:
    void handleHbMessage(const QByteArray &msg);
    void cleanup();

    VirtualClientConfig m_config;
    VirtualClientStats  m_stats;

    QTcpSocket *m_mainSocket{nullptr};
    QTcpSocket *m_hbSocket{nullptr};

    QByteArray m_mainRxBuf;
    QByteArray m_hbRxBuf;
};

#endif // VISION_VIRTUAL_CLIENT_H
