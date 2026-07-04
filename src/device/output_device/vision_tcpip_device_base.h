#ifndef VISION_TCPIP_DEVICE_BASE_H
#define VISION_TCPIP_DEVICE_BASE_H

#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"
#include "device/output_device/vision_tcpip_protocol.h"

#include <QByteArray>
#include <QVector>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

namespace vc::device {

/**
 * @brief VisionTcpipDeviceBase — shared protocol core for the TCP/IP
 *        vision-output transports (server + client).
 *
 *  Owns the pair of active sockets (main + heartbeat), the RX framing
 *  buffers, the heartbeat timer/state, runtime state and diagnostics.
 *  It implements everything that is identical between server and client:
 *
 *    - main-channel framing (';' delimiter) + result write (pushRequest)
 *    - heartbeat probe / ack parsing / timeout (the software is always
 *      the heartbeat master, regardless of connection direction)
 *    - lost-connection handling, runtime-state / diagnostics bookkeeping
 *
 *  Concrete transports differ only in HOW a socket is obtained:
 *    - VisionTcpipDevice (server): QTcpServer::listen + accept
 *    - VisionTcpipClientDevice    : QTcpSocket::connectToHost + reconnect
 *
 *  A subclass opens/closes its transport in startTransport()/stopTransport()
 *  and, whenever a link comes up, hands the connected socket to
 *  attachMainSocket() / attachHeartbeatSocket().  The base wires the rest.
 */
class VisionTcpipDeviceBase : public VisionOutputDevice {
    Q_OBJECT

public:
    explicit VisionTcpipDeviceBase(QString id, QString name, QObject* parent = nullptr)
        : VisionOutputDevice(std::move(id), std::move(name), parent) {}
    ~VisionTcpipDeviceBase() override = default;

    // ── IDevice ───────────────────────────────────────────────────────────
    bool deviceConnect() override;
    bool deviceDisconnect() override;
    bool isDeviceConnected() const override {
        return connectStatus() == ConnectStatus::Connected;
    }

    /**
     * @brief Push one matching-result request out on the main channel.
     *        Returns false if no main link is up or the request is foreign.
     */
    bool pushRequest(IRequest *request) override;

    // ── Read-only runtime state (UI / tests) ───────────────────────────────
    VisionTcpipRuntimeState runtimeState() const { return m_runtimeState; }
    VisionTcpipDiagnostics diagnostics() const { return m_diagnostics; }

    bool isMainClientConnected() const      { return m_mainSocket != nullptr; }
    bool isHeartbeatClientConnected() const { return m_hbSocket   != nullptr; }
    quint16 lastAckCount() const            { return m_lastAckCount; }
    quint16 expectedAckCount() const        { return m_expectedAckCount; }

public slots:
    void deviceTerminate() override;

signals:
    /// Emitted when a full message (terminated by ';') arrives on the main link.
    void mainRequestReceived(const QByteArray &payload);
    /// Emitted after a result payload is written to the main link.
    void resultSent(const QByteArray &payload);
    /// Emitted when the heartbeat declares a lost connection (timeout/bad format).
    void heartbeatLost(const QString &reason);
    /// Emitted when the main link comes up (true) or drops (false).
    void mainClientStateChanged(bool connected);
    /// Emitted after a result is sent while the robot kinematic check is enabled:
    /// how many of the `total` poses were reachable (within limits, non-singular).
    void kinematicCheckResult(int total, int reachable);

protected:
    // ── Transport contract (subclass implements) ───────────────────────────
    /// Open the transport (listen / dial). Return false and set m_last_msg on
    /// failure. On success the subclass sets ConnectStatus::Connected.
    virtual bool startTransport() = 0;
    /// Tear down all transport-owned objects (servers / connectors).
    virtual void stopTransport() = 0;
    /// Hook fired after a link drops or a lost-connection is declared while the
    /// transport is active. Server: no-op (keeps listening). Client: reconnect.
    virtual void onLinkLost() {}

    /// Transport-neutral config accessors, sourced from the concrete config.
    virtual int cfgMainPort() const = 0;
    virtual int cfgHeartbeatPort() const = 0;
    virtual int cfgHeartbeatIntervalMs() const = 0;
    virtual int cfgHeartbeatTimeoutMs() const = 0;
    /// Robot kinematic check settings, sourced from the concrete config's base.
    virtual RobotKinematicCheckConfig kinematicCheckConfig() const = 0;

    // Run the robot kinematic reachability check over outgoing pick poses (when
    // enabled). Logs unreachable poses and emits kinematicCheckResult. Advisory
    // only — never blocks or alters the sent payload.
    void runKinematicCheck(const QVector<VisionOutputPosition> &positions);

    // ── Called by the subclass when a link is established ───────────────────
    void attachMainSocket(QTcpSocket *sock);
    void attachHeartbeatSocket(QTcpSocket *sock);

    // ── Shared teardown / state helpers (usable by subclass) ────────────────
    void detachMainSocket();
    void detachHeartbeatSocket();
    void declareLostConnection(const QString &reason);
    void resetHeartbeatState();
    void syncRuntimeState();

    bool isActive() const { return m_active; }

protected slots:
    void onMainSocketReadyRead();
    void onMainSocketDisconnected();
    void onHeartbeatSocketReadyRead();
    void onHeartbeatSocketDisconnected();
    void onHeartbeatTick();

protected:
    void handleMainPayload(const QByteArray &payload);
    void handleHeartbeatPayload(const QByteArray &payload);
    // Best-effort graceful goodbye on the heartbeat channel before teardown.
    // No-op when the heartbeat link is down. Blocks briefly (bounded) so the
    // notice leaves the socket before detachHeartbeatSocket() aborts it.
    void sendDisconnectNotice();
    void sendHeartbeatProbe();
    void startHeartbeatTimer();
    void stopHeartbeatTimer();

    VisionTcpipRuntimeState m_runtimeState;
    VisionTcpipDiagnostics m_diagnostics;

    QTcpSocket *m_mainSocket{nullptr};
    QTcpSocket *m_hbSocket{nullptr};

    QByteArray m_mainRxBuffer;
    QByteArray m_hbRxBuffer;

    QTimer *m_hbTimer{nullptr};
    QElapsedTimer m_hbLastReplyTimer;

    quint16 m_expectedAckCount{0};
    quint16 m_lastAckCount{0};
    bool m_hbAwaitingReply{false};

    // True between deviceConnect() and deviceDisconnect(). Lets the client
    // transport know whether a dropped link should trigger a reconnect.
    bool m_active{false};
};

} // namespace vc::device

#endif // VISION_TCPIP_DEVICE_BASE_H
