#include "vision_tcpip_device.h"

#include <QHostAddress>
#include <QMutexLocker>
#include <utility>

/// Device-layer types: concrete IDevice/IDeviceCfg implementations (PLC,
/// vision-output transports, etc.) and their supporting config/state types.
namespace vc::device {

/// Constructs the server device: installs m_config as the active device
/// config and syncs the runtime-state snapshot.
VisionTcpipDevice::VisionTcpipDevice(QString id, QString name, QObject* parent)
    : VisionTcpipDeviceBase(std::move(id), std::move(name), parent) {

    IDevice::setDeviceConfig(&m_config);
    syncRuntimeState();
}

/// Tears down the transport (closes/deletes the listeners, detaches any
/// live sockets) before destruction.
VisionTcpipDevice::~VisionTcpipDevice() {
    stopTransport();
}

/// Installs `cfg` as the active config after checking it is a VisionOutput /
/// VisionTCPIP config and the device is not currently connected (config is
/// locked while the link is live).
void VisionTcpipDevice::setDeviceConfig(IDeviceCfg *cfg) {
    if (!cfg) {
        return;
    }
    if (cfg->deviceType() != DeviceType::VisionOutput) {
        LOG_DEV_ERR << "VisionTcpipDevice: wrong config type";
        return;
    }
    // Reject configs whose VisionOutputType is anything other than VisionTCPIP.
    auto *vcfg = dynamic_cast<VisionOutputDeviceCfg*>(cfg);
    if (!vcfg || vcfg->visionOutputType() != VisionOutputType::VisionTCPIP) {
        LOG_DEV_ERR << "VisionTcpipDevice: wrong sub-type config";
        return;
    }
    if (isDeviceConnected()) {
        LOG_DEV_INFO << "VisionTcpipDevice: cannot change config while connected";
        return;
    }

    QMutexLocker locker(&m_mutex);
    VisionTcpipDeviceCfg *casted = static_cast<VisionTcpipDeviceCfg*>(cfg);
    m_config = *casted;
    IDevice::setDeviceConfig(&m_config);
}

/// Replaces the current server config with `cfg`.
/// @return false (config unchanged) if the device is connected — the config
///         is locked while the link is live and the caller must disconnect first
bool VisionTcpipDevice::setVisionTcpipConfig(VisionTcpipDeviceCfg& cfg) {
    if (this->isDeviceConnected()) {
        // Config is locked while the link is live; caller must disconnect first.
        return false;
    }

    QMutexLocker locker(&m_mutex);
    m_config = cfg;
    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
    return true;
}

/// Loads the device config from `obj` (delegates to IDevice::fromJson)
/// while holding the device mutex.
bool VisionTcpipDevice::fromJson(const QJsonObject &obj) {
    QMutexLocker locker(&m_mutex);
    return IDevice::fromJson(obj);
}

// =====================================================================
// Transport: listen + accept (one client per port)
// =====================================================================
/// Opens the main and heartbeat QTcpServer listeners on the configured
/// address/ports and wires their newConnection signals.
/// @return true and sets ConnectStatus::Connected on success; false (with
///         m_last_msg set to the socket error) if either listen() call fails
bool VisionTcpipDevice::startTransport() {
    m_mainServer = new QTcpServer(this);
    m_hbServer   = new QTcpServer(this);

    connect(m_mainServer, &QTcpServer::newConnection,
            this, &VisionTcpipDevice::onMainNewConnection);
    connect(m_hbServer, &QTcpServer::newConnection,
            this, &VisionTcpipDevice::onHeartbeatNewConnection);

    QHostAddress addr(m_config.m_listenAddress);
    if (addr.isNull()) {
        addr = QHostAddress::Any;
    }

    if (!m_mainServer->listen(addr, static_cast<quint16>(m_config.m_mainPort))) {
        m_last_msg = QString("Cannot listen on main port %1: %2")
                         .arg(m_config.m_mainPort)
                         .arg(m_mainServer->errorString());
        LOG_DEV_ERR << m_last_msg;
        return false;
    }
    if (!m_hbServer->listen(addr, static_cast<quint16>(m_config.m_heartbeatPort))) {
        m_last_msg = QString("Cannot listen on heartbeat port %1: %2")
                         .arg(m_config.m_heartbeatPort)
                         .arg(m_hbServer->errorString());
        LOG_DEV_ERR << m_last_msg;
        return false;
    }

    setConnectionStatus(ConnectStatus::Connected);
    LOG_DEV_INFO << "VisionTcpipDevice listening, main port:" << m_config.m_mainPort
                 << " heartbeat port:" << m_config.m_heartbeatPort;
    return true;
}

/// Detaches any live sockets, then closes and schedules deletion of both
/// QTcpServer listeners.
void VisionTcpipDevice::stopTransport() {
    detachHeartbeatSocket();
    detachMainSocket();

    if (m_mainServer) {
        m_mainServer->close();
        m_mainServer->deleteLater();
        m_mainServer = nullptr;
    }
    if (m_hbServer) {
        m_hbServer->close();
        m_hbServer->deleteLater();
        m_hbServer = nullptr;
    }
}

// =====================================================================
// Accept handlers — single active client per port
// =====================================================================
/// Drains pending connections on the main-port listener: the first is
/// attached as the live main socket, any further pending ones (while a
/// main client is already attached) are rejected and disconnected.
void VisionTcpipDevice::onMainNewConnection() {
    while (m_mainServer && m_mainServer->hasPendingConnections()) {
        QTcpSocket *sock = m_mainServer->nextPendingConnection();
        if (m_mainSocket) {
            LOG_DEV_INFO << "VisionTcpipDevice: main port already in use, reject"
                         << sock->peerAddress().toString();
            sock->disconnectFromHost();
            sock->deleteLater();
            continue;
        }
        attachMainSocket(sock);
    }
}

/// Drains pending connections on the heartbeat-port listener: the first is
/// attached as the live heartbeat socket, any further pending ones (while a
/// heartbeat client is already attached) are rejected and disconnected.
void VisionTcpipDevice::onHeartbeatNewConnection() {
    while (m_hbServer && m_hbServer->hasPendingConnections()) {
        QTcpSocket *sock = m_hbServer->nextPendingConnection();
        if (m_hbSocket) {
            LOG_DEV_INFO << "VisionTcpipDevice: heartbeat port already in use, reject"
                         << sock->peerAddress().toString();
            sock->disconnectFromHost();
            sock->deleteLater();
            continue;
        }
        attachHeartbeatSocket(sock);
    }
}

} // namespace vc::device
