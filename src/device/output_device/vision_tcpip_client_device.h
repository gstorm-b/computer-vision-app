#ifndef VISION_TCPIP_CLIENT_DEVICE_H
#define VISION_TCPIP_CLIENT_DEVICE_H

#include "device/output_device/vision_tcpip_device_base.h"
#include "device/output_device/vision_tcpip_client_config.h"

/// Device-layer types: concrete IDevice/IDeviceCfg implementations (PLC,
/// vision-output transports, etc.) and their supporting config/state types.
namespace vc::device {

/// TCP/IP **client** transport of the vision-output family.
///
/// Same protocol as VisionTcpipDevice, but the software dials OUT to a remote
/// endpoint (which acts as the TCP server, e.g. a robot controller). It keeps
/// the same data/heartbeat semantics: the software is still the heartbeat
/// master ("connection_check." -> "ack,{count}.") and still pushes matching
/// results out on the main channel. Only the connection direction differs:
/// QTcpSocket::connectToHost + auto-reconnect instead of QTcpServer::listen.
///
/// When a link drops or the heartbeat times out, the device retries the
/// outbound connection every `reconnectIntervalMs` while active.
class VisionTcpipClientDevice : public VisionTcpipDeviceBase {
    Q_OBJECT

public:
    /// Constructs the client device: installs m_config as the active device
    /// config and syncs the runtime-state snapshot.
    explicit VisionTcpipClientDevice(QString id, QString name, QObject* parent = nullptr);
    /// Tears down the transport (aborts any in-flight connectors, detaches
    /// live sockets) before destruction.
    ~VisionTcpipClientDevice() override;

    /// Identifies this device as the TCP/IP client sub-type.
    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VisionTcpipClient;
    }

    /// Installs `cfg` as the active config after checking it is a
    /// VisionOutput / VisionTcpipClient config and the device is not
    /// currently connected (config is locked while the link is live).
    void setDeviceConfig(IDeviceCfg *cfg) override;
    /// Replaces the current client config with `cfg`; refuses (no-op) while
    /// the device is connected.
    void setVisionTcpipClientConfig(VisionTcpipClientDeviceCfg& cfg);
    /// Returns a copy of the current client config.
    VisionTcpipClientDeviceCfg visionTcpipClientConfig() const { return m_config; }

    /// Loads the device config from `obj` (delegates to IDevice::fromJson)
    /// while holding the device mutex.
    bool fromJson(const QJsonObject &obj) override;

protected:
    // ── Transport: dial out + reconnect ────────────────────────────────────
    /// Begins dialing out to the configured server (sets ConnectStatus::Connecting
    /// and kicks off attemptConnect()); always returns true (dialing is async).
    bool startTransport() override;
    /// Stops any pending reconnect, aborts and discards the in-flight
    /// connectors, and detaches any already-live sockets.
    void stopTransport() override;
    /// Falls back to Connecting (if we were Connected) and schedules a
    /// reconnect attempt.
    void onLinkLost() override;

    /// Returns the configured remote main-port number to dial.
    int cfgMainPort() const override            { return m_config.m_mainPort; }
    /// Returns the configured remote heartbeat-port number to dial.
    int cfgHeartbeatPort() const override        { return m_config.m_heartbeatPort; }
    /// Returns the configured heartbeat probe interval, in milliseconds.
    int cfgHeartbeatIntervalMs() const override  { return m_config.m_heartbeatIntervalMs; }
    /// Returns the configured heartbeat reply timeout, in milliseconds.
    int cfgHeartbeatTimeoutMs() const override   { return m_config.m_heartbeatTimeoutMs; }
    /// Returns the robot kinematic-check settings from the client config.
    RobotKinematicCheckConfig kinematicCheckConfig() const override { return m_config.m_kinematicCheck; }

private slots:
    /// Opens a main and/or heartbeat QTcpSocket toward the configured server
    /// address if not already dialing/connected; no-op if the device is inactive.
    void attemptConnect();
    /// Handles the main connector's `connected` signal: hands the socket to
    /// the base as the live main link and re-evaluates overall connect state.
    void onMainConnected();
    /// Handles the heartbeat connector's `connected` signal: hands the socket
    /// to the base as the live heartbeat link and re-evaluates connect state.
    void onHeartbeatConnected();
    /// Handles a failed main-socket dial attempt: records the error and
    /// schedules a reconnect if still active.
    /// @param err unused; the error text is read from the connector itself
    void onMainConnectError(QAbstractSocket::SocketError err);
    /// Handles a failed heartbeat-socket dial attempt: records the error and
    /// schedules a reconnect if still active.
    /// @param err unused; the error text is read from the connector itself
    void onHeartbeatConnectError(QAbstractSocket::SocketError err);

private:
    /// Starts (or restarts) the one-shot reconnect timer using
    /// `m_config.m_reconnectIntervalMs`; no-op while inactive or already pending.
    void scheduleReconnect();
    /// Promotes the connection status to Connected once both the main and
    /// heartbeat sockets report QAbstractSocket::ConnectedState.
    void evaluateConnected();

    VisionTcpipClientDeviceCfg m_config;  ///< Active client config (server address, ports, timings).

    /// Sockets used only while dialing out (before the link comes up). Once
    /// connected, ownership passes to the base as m_mainSocket / m_hbSocket.
    QTcpSocket *m_mainConnector{nullptr};  ///< Main-channel connector; valid only mid-dial.
    QTcpSocket *m_hbConnector{nullptr};    ///< Heartbeat-channel connector; valid only mid-dial.

    QTimer *m_reconnectTimer{nullptr};  ///< Single-shot timer that retries attemptConnect() after a drop/failure.
};

} // namespace vc::device

#endif // VISION_TCPIP_CLIENT_DEVICE_H
