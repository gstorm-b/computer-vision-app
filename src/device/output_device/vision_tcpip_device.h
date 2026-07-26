#ifndef VISION_TCPIP_DEVICE_H
#define VISION_TCPIP_DEVICE_H

#include "device/output_device/vision_tcpip_device_base.h"
#include "device/output_device/vision_tcpip_config.h"

#include <QTcpServer>

namespace vc::device {

/// TCP/IP **server** transport of the vision-output family: listens on two ports
/// (main + heartbeat), accepts at most one client per port (pending duplicates are
/// rejected), and delegates all protocol handling to VisionTcpipDeviceBase. The
/// software remains the heartbeat master: it sends "connection_check." and expects
/// "ack,{count}.". On lost connection the device drops the current client sockets
/// but keeps the listeners open for the next reconnect.
class VisionTcpipDevice : public VisionTcpipDeviceBase {
    Q_OBJECT

public:
    /// Constructs the device and installs its own VisionTcpipDeviceCfg as the
    /// active IDeviceCfg.
    explicit VisionTcpipDevice(QString id, QString name, QObject* parent = nullptr);
    /// Tears down the transport (stopTransport()) before destruction.
    ~VisionTcpipDevice() override;

    /// Identifies this device as the TCP/IP vision-output sub-type.
    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VisionTCPIP;
    }

    /// Validates `cfg` (must be a VisionOutputDeviceCfg of type VisionTCPIP) and,
    /// unless the device is currently connected, copies it into m_config and installs
    /// it via IDevice::setDeviceConfig(). Logs and returns without effect on mismatch.
    void setDeviceConfig(IDeviceCfg *cfg) override;
    /// Replaces m_config with `cfg` when the device is not connected.
    /// @param cfg the new TCP/IP configuration to install
    /// @return true if the config was applied; false (unchanged) if the device is connected
    bool setVisionTcpipConfig(VisionTcpipDeviceCfg& cfg);
    /// Returns a copy of the current TCP/IP configuration.
    VisionTcpipDeviceCfg visionTcpipConfig() const { return m_config; }

    /// Delegates to IDevice::fromJson() under the device mutex.
    bool fromJson(const QJsonObject &obj) override;

protected:
    // ── Transport: listen + accept ─────────────────────────────────────────
    /// Creates and starts the main and heartbeat QTcpServer listeners on
    /// m_config's address/ports, wiring their newConnection signals. Sets
    /// ConnectStatus::Connected on success; on failure records m_last_msg and
    /// returns false (the caller tears down via stopTransport()).
    bool startTransport() override;
    /// Detaches any live sockets and closes/deletes both listening servers.
    void stopTransport() override;

    /// Main-channel listen port, sourced from m_config.
    int cfgMainPort() const override            { return m_config.m_mainPort; }
    /// Heartbeat-channel listen port, sourced from m_config.
    int cfgHeartbeatPort() const override        { return m_config.m_heartbeatPort; }
    /// Heartbeat probe interval (ms), sourced from m_config.
    int cfgHeartbeatIntervalMs() const override  { return m_config.m_heartbeatIntervalMs; }
    /// Heartbeat reply timeout (ms), sourced from m_config.
    int cfgHeartbeatTimeoutMs() const override   { return m_config.m_heartbeatTimeoutMs; }
    /// Robot kinematic reachability-check settings, sourced from m_config.
    RobotKinematicCheckConfig kinematicCheckConfig() const override { return m_config.m_kinematicCheck; }

private slots:
    /// Accepts pending connections on the main listener; rejects (disconnects)
    /// any additional client while one is already attached, otherwise attaches
    /// the socket via attachMainSocket().
    void onMainNewConnection();
    /// Accepts pending connections on the heartbeat listener; rejects (disconnects)
    /// any additional client while one is already attached, otherwise attaches
    /// the socket via attachHeartbeatSocket().
    void onHeartbeatNewConnection();

private:
    VisionTcpipDeviceCfg m_config;           ///< Active TCP/IP configuration (ports, address, heartbeat/kinematic settings).
    QTcpServer *m_mainServer{nullptr};       ///< Listener for the main (result/request) channel; owned, created in startTransport().
    QTcpServer *m_hbServer{nullptr};         ///< Listener for the heartbeat channel; owned, created in startTransport().
};

} // namespace vc::device

#endif // VISION_TCPIP_DEVICE_H
