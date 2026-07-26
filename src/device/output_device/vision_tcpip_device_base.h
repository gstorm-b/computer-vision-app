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

/// Device-layer classes implementing the two-channel (main + heartbeat) vision-output
/// TCP/IP protocol shared by the server and client transports.
namespace vc::device {

/// VisionTcpipDeviceBase — shared protocol core for the TCP/IP
/// vision-output transports (server + client).
///
/// Owns the pair of active sockets (main + heartbeat), the RX framing
/// buffers, the heartbeat timer/state, runtime state and diagnostics.
/// It implements everything that is identical between server and client:
///
///   - main-channel framing (';' delimiter) + result write (pushRequest)
///   - heartbeat probe / ack parsing / timeout (the software is always
///     the heartbeat master, regardless of connection direction)
///   - lost-connection handling, runtime-state / diagnostics bookkeeping
///
/// Concrete transports differ only in HOW a socket is obtained:
///   - VisionTcpipDevice (server): QTcpServer::listen + accept
///   - VisionTcpipClientDevice    : QTcpSocket::connectToHost + reconnect
///
/// A subclass opens/closes its transport in startTransport()/stopTransport()
/// and, whenever a link comes up, hands the connected socket to
/// attachMainSocket() / attachHeartbeatSocket(). The base wires the rest.
class VisionTcpipDeviceBase : public VisionOutputDevice {
    Q_OBJECT

public:
    /// Constructs the base with the given device id/name; sockets and timers
    /// are created lazily as the transport comes up.
    explicit VisionTcpipDeviceBase(QString id, QString name, QObject* parent = nullptr)
        : VisionOutputDevice(std::move(id), std::move(name), parent) {}
    /// Default destructor.
    ~VisionTcpipDeviceBase() override = default;

    // ── IDevice ───────────────────────────────────────────────────────────
    /// Activates the device: opens the transport via startTransport() (idempotent —
    /// a no-op returning true if already active). Sets ConnectFailed and returns
    /// false if the transport fails to open.
    bool deviceConnect() override;
    /// Deactivates the device: sends a best-effort heartbeat disconnect notice,
    /// tears down the transport via stopTransport(), and sets Disconnected.
    bool deviceDisconnect() override;
    /// True when the current connection status is ConnectStatus::Connected.
    bool isDeviceConnected() const override {
        return connectStatus() == ConnectStatus::Connected;
    }

    /// Push one matching-result request out on the main channel: drops requests
    /// that aren't Request_VisionOutput, runs the optional robot-kinematics
    /// reachability check on Result-kind requests, then writes the built payload
    /// and emits resultSent().
    /// @return false if no main link is up, the request is foreign, or the write is short.
    bool pushRequest(IRequest *request) override;

    // ── Read-only runtime state (UI / tests) ───────────────────────────────
    /// Returns the live link/heartbeat runtime-state snapshot (read-only, for UI/tests).
    VisionTcpipRuntimeState runtimeState() const { return m_runtimeState; }
    /// Returns the cumulative diagnostics counters and last error (read-only, for UI/tests).
    VisionTcpipDiagnostics diagnostics() const { return m_diagnostics; }

    /// True when the main-channel socket is currently attached.
    bool isMainClientConnected() const      { return m_mainSocket != nullptr; }
    /// True when the heartbeat-channel socket is currently attached.
    bool isHeartbeatClientConnected() const { return m_hbSocket   != nullptr; }
    /// Returns the msg_count of the last accepted heartbeat ack.
    quint16 lastAckCount() const            { return m_lastAckCount; }
    /// Returns the msg_count expected in the next heartbeat ack.
    quint16 expectedAckCount() const        { return m_expectedAckCount; }

public slots:
    /// Terminates the device: forces a deviceDisconnect() if currently connected
    /// or active.
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
    /// Main-channel TCP port.
    virtual int cfgMainPort() const = 0;
    /// Heartbeat-channel TCP port.
    virtual int cfgHeartbeatPort() const = 0;
    /// Interval between heartbeat probes, in milliseconds.
    virtual int cfgHeartbeatIntervalMs() const = 0;
    /// Maximum time since the last valid heartbeat reply before the link is
    /// declared lost, in milliseconds.
    virtual int cfgHeartbeatTimeoutMs() const = 0;
    /// Robot kinematic check settings, sourced from the concrete config's base.
    virtual RobotKinematicCheckConfig kinematicCheckConfig() const = 0;

    /// Runs the robot kinematic reachability check over outgoing pick poses (when
    /// enabled). Logs unreachable poses and emits kinematicCheckResult. Advisory
    /// only — never blocks or alters the sent payload.
    void runKinematicCheck(const QVector<VisionOutputPosition> &positions);

    // ── Called by the subclass when a link is established ───────────────────
    /// Adopts `sock` as the main-channel socket: clears the RX buffer, wires up
    /// readyRead/disconnected, and emits mainClientStateChanged(true). No-op if
    /// `sock` is null.
    void attachMainSocket(QTcpSocket *sock);
    /// Adopts `sock` as the heartbeat-channel socket: clears the RX buffer, resets
    /// heartbeat state, wires up readyRead/disconnected, then (since the software
    /// is always the heartbeat master) sends the first probe and starts the timer.
    /// No-op if `sock` is null.
    void attachHeartbeatSocket(QTcpSocket *sock);

    // ── Shared teardown / state helpers (usable by subclass) ────────────────
    /// Disconnects signals, aborts and schedules deletion of the main socket,
    /// clears its RX buffer, and emits mainClientStateChanged(false). No-op if
    /// no main socket is attached.
    void detachMainSocket();
    /// Stops the heartbeat timer, disconnects signals, aborts and schedules
    /// deletion of the heartbeat socket, then clears its RX buffer and resets
    /// heartbeat state.
    void detachHeartbeatSocket();
    /// Records `reason` in diagnostics, emits heartbeatLost(reason), detaches both
    /// sockets, sets ConnectStatus::LostConnected, and invokes onLinkLost() if active.
    void declareLostConnection(const QString &reason);
    /// Resets the heartbeat ack counters and awaiting-reply/reply-timer state.
    void resetHeartbeatState();
    /// Recomputes m_runtimeState from the current socket connection states and
    /// heartbeat counters.
    void syncRuntimeState();

    /// True between deviceConnect() and deviceDisconnect(); lets the client
    /// transport know whether a dropped link should trigger a reconnect.
    bool isActive() const { return m_active; }

protected slots:
    /// Reads all available bytes into the main RX buffer and dispatches each
    /// ';'-terminated message to handleMainPayload().
    void onMainSocketReadyRead();
    /// Detaches the main socket and, if the device is active, invokes onLinkLost().
    void onMainSocketDisconnected();
    /// Reads all available bytes into the heartbeat RX buffer and dispatches each
    /// '.'-terminated message to handleHeartbeatPayload().
    void onHeartbeatSocketReadyRead();
    /// Detaches the heartbeat socket, resyncs runtime state, and, if the device is
    /// active, invokes onLinkLost().
    void onHeartbeatSocketDisconnected();
    /// Periodic heartbeat-timer callback: declares the connection lost if no valid
    /// reply arrived within the configured timeout, otherwise sends the next probe.
    void onHeartbeatTick();

protected:
    /// Counts the payload in diagnostics and emits mainRequestReceived(payload).
    void handleMainPayload(const QByteArray &payload);
    /// Parses a heartbeat reply of the form "ack,{msg_count}."; declares the
    /// connection lost on malformed input or a msg_count mismatch, otherwise
    /// records the ack, clears the awaiting-reply flag, restarts the reply timer,
    /// and advances the expected counter (wrapping at 2^16).
    void handleHeartbeatPayload(const QByteArray &payload);
    /// Best-effort graceful goodbye on the heartbeat channel before teardown.
    /// No-op when the heartbeat link is down. Blocks briefly (bounded) so the
    /// notice leaves the socket before detachHeartbeatSocket() aborts it.
    void sendDisconnectNotice();
    /// Writes the heartbeat probe message, marks a reply as awaited, and (if not
    /// already running) starts the reply timer.
    void sendHeartbeatProbe();
    /// Lazily creates the heartbeat QTimer (precise, wired to onHeartbeatTick())
    /// and (re)starts it at the configured interval.
    void startHeartbeatTimer();
    /// Stops the heartbeat timer if it is running.
    void stopHeartbeatTimer();

    VisionTcpipRuntimeState m_runtimeState;  ///< Live link/heartbeat state snapshot exposed to UI/tests.
    VisionTcpipDiagnostics m_diagnostics;    ///< Cumulative counters and last error exposed to UI/tests.

    QTcpSocket *m_mainSocket{nullptr};  ///< Attached main-channel socket, or null when no link is up.
    QTcpSocket *m_hbSocket{nullptr};    ///< Attached heartbeat-channel socket, or null when no link is up.

    QByteArray m_mainRxBuffer;  ///< Accumulates main-channel bytes until a ';' terminator is seen.
    QByteArray m_hbRxBuffer;    ///< Accumulates heartbeat-channel bytes until a '.' terminator is seen.

    QTimer *m_hbTimer{nullptr};             ///< Drives periodic heartbeat probes at cfgHeartbeatIntervalMs().
    QElapsedTimer m_hbLastReplyTimer;       ///< Time since the last valid heartbeat reply (or first probe).

    quint16 m_expectedAckCount{0};   ///< msg_count expected in the next heartbeat ack.
    quint16 m_lastAckCount{0};       ///< msg_count of the last accepted heartbeat ack.
    bool m_hbAwaitingReply{false};   ///< True while a heartbeat probe has been sent but not yet acked.

    /// True between deviceConnect() and deviceDisconnect(). Lets the client
    /// transport know whether a dropped link should trigger a reconnect.
    bool m_active{false};
};

} // namespace vc::device

#endif // VISION_TCPIP_DEVICE_BASE_H
