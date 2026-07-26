#include "vision_tcpip_client_device.h"

#include <QMutexLocker>
#include <utility>

namespace vc::device {

/// Constructs the device with its default-initialized VisionTcpipClientDeviceCfg, registers
/// that config with the base IDevice, and syncs the initial runtime-state snapshot.
VisionTcpipClientDevice::VisionTcpipClientDevice(QString id, QString name, QObject* parent)
    : VisionTcpipDeviceBase(std::move(id), std::move(name), parent) {

    IDevice::setDeviceConfig(&m_config);
    syncRuntimeState();
}

/// Tears down the transport (aborts/deletes any connector or attached sockets, stops the
/// reconnect timer) before destruction.
VisionTcpipClientDevice::~VisionTcpipClientDevice() {
    stopTransport();
}

/// Replaces the active config after validating `cfg` is a VisionOutput config whose
/// sub-type is VisionTcpipClient. Rejects (logs and returns) a null/wrong-type config or
/// any change attempted while the device is connected; otherwise copies `cfg` into
/// m_config under m_mutex and re-registers it with the base IDevice.
/// @param cfg candidate config; must be a VisionTcpipClientDeviceCfg
void VisionTcpipClientDevice::setDeviceConfig(IDeviceCfg *cfg) {
    if (!cfg) {
        return;
    }
    if (cfg->deviceType() != DeviceType::VisionOutput) {
        LOG_DEV_ERR << "VisionTcpipClientDevice: wrong config type";
        return;
    }
    auto *vcfg = dynamic_cast<VisionOutputDeviceCfg*>(cfg);
    if (!vcfg || vcfg->visionOutputType() != VisionOutputType::VisionTcpipClient) {
        LOG_DEV_ERR << "VisionTcpipClientDevice: wrong sub-type config";
        return;
    }
    if (isDeviceConnected()) {
        LOG_DEV_INFO << "VisionTcpipClientDevice: cannot change config while connected";
        return;
    }

    QMutexLocker locker(&m_mutex);
    VisionTcpipClientDeviceCfg *casted = static_cast<VisionTcpipClientDeviceCfg*>(cfg);
    m_config = *casted;
    IDevice::setDeviceConfig(&m_config);
}

/// Typed config setter: copies `cfg` into m_config under m_mutex and re-registers it with
/// the base IDevice. No-op while the device is connected.
/// @param cfg the new client config, copied by value into m_config
void VisionTcpipClientDevice::setVisionTcpipClientConfig(VisionTcpipClientDeviceCfg& cfg) {
    if (this->isDeviceConnected()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_config = cfg;
    IDevice::setDeviceConfig(&m_config);
}

/// Deserializes device state (including the config) from JSON, holding m_mutex for the
/// duration of the base IDevice::fromJson() call.
/// @param obj JSON object describing this device
/// @return whatever IDevice::fromJson() reports
bool VisionTcpipClientDevice::fromJson(const QJsonObject &obj) {
    QMutexLocker locker(&m_mutex);
    return IDevice::fromJson(obj);
}

// =====================================================================
// Transport: dial out. Connected is reported ONLY once both the main and
// heartbeat links are actually up (see evaluateConnected()); while dialing
// or reconnecting the status is Connecting.
// =====================================================================
/// Starts the outbound transport: marks the device Connecting and kicks off
/// attemptConnect() to dial both the main and heartbeat ports.
/// @return always true (dialing is asynchronous; failures surface via onMainConnectError /
/// onHeartbeatConnectError instead)
bool VisionTcpipClientDevice::startTransport() {
    setConnectionStatus(ConnectStatus::Connecting);
    attemptConnect();
    LOG_DEV_INFO << "VisionTcpipClientDevice dialing" << m_config.m_serverAddress
                 << "main port:" << m_config.m_mainPort
                 << "heartbeat port:" << m_config.m_heartbeatPort;
    return true;
}

/// Stops the reconnect timer and aborts/deletes any in-flight connector sockets, then
/// detaches the attached heartbeat and main sockets via the base class helpers.
void VisionTcpipClientDevice::stopTransport() {
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    if (m_mainConnector) {
        m_mainConnector->disconnect(this);
        m_mainConnector->abort();
        m_mainConnector->deleteLater();
        m_mainConnector = nullptr;
    }
    if (m_hbConnector) {
        m_hbConnector->disconnect(this);
        m_hbConnector->abort();
        m_hbConnector->deleteLater();
        m_hbConnector = nullptr;
    }

    detachHeartbeatSocket();
    detachMainSocket();
}

/// Reacts to a dropped link: falls back from Connected to Connecting (unless already in a
/// LostConnected state from a heartbeat-loss path) and schedules a reconnect attempt.
void VisionTcpipClientDevice::onLinkLost() {
    // A link dropped. If we were fully Connected, fall back to Connecting; a
    // heartbeat-loss path already set LostConnected and must stay visible
    // until the redial actually starts (handled in attemptConnect()).
    if (connectStatus() == ConnectStatus::Connected) {
        setConnectionStatus(ConnectStatus::Connecting);
    }
    scheduleReconnect();
}

// =====================================================================
// Outbound connection management
// =====================================================================
/// Dials the main and heartbeat ports if not already connected/connecting: for each link
/// still missing both its live socket and its in-flight connector, creates a QTcpSocket,
/// wires its connected/errorOccurred signals, and calls connectToHost(). No-op if the
/// device is not active.
void VisionTcpipClientDevice::attemptConnect() {
    if (!isActive()) {
        return;
    }

    // Redialing (e.g. after LostConnected): reflect the in-progress state.
    if (connectStatus() != ConnectStatus::Connected
        && connectStatus() != ConnectStatus::Connecting) {
        setConnectionStatus(ConnectStatus::Connecting);
    }

    if (!m_mainSocket && !m_mainConnector) {
        m_mainConnector = new QTcpSocket(this);
        connect(m_mainConnector, &QTcpSocket::connected,
                this, &VisionTcpipClientDevice::onMainConnected);
        connect(m_mainConnector, &QAbstractSocket::errorOccurred,
                this, &VisionTcpipClientDevice::onMainConnectError);
        m_mainConnector->connectToHost(m_config.m_serverAddress,
                                       static_cast<quint16>(m_config.m_mainPort));
    }

    if (!m_hbSocket && !m_hbConnector) {
        m_hbConnector = new QTcpSocket(this);
        connect(m_hbConnector, &QTcpSocket::connected,
                this, &VisionTcpipClientDevice::onHeartbeatConnected);
        connect(m_hbConnector, &QAbstractSocket::errorOccurred,
                this, &VisionTcpipClientDevice::onHeartbeatConnectError);
        m_hbConnector->connectToHost(m_config.m_serverAddress,
                                     static_cast<quint16>(m_config.m_heartbeatPort));
    }
}

/// Slot for the main connector's connected() signal: hands the now-live socket off to
/// attachMainSocket() (which the base rewires for readyRead/disconnected) and re-checks
/// whether both links are up.
void VisionTcpipClientDevice::onMainConnected() {
    QTcpSocket *sock = m_mainConnector;
    m_mainConnector = nullptr;
    if (!sock) return;

    // Drop the dialing-stage handlers; the base re-wires readyRead/disconnected.
    sock->disconnect(this);
    attachMainSocket(sock);
    evaluateConnected();
}

/// Slot for the heartbeat connector's connected() signal: hands the now-live socket off to
/// attachHeartbeatSocket() and re-checks whether both links are up.
void VisionTcpipClientDevice::onHeartbeatConnected() {
    QTcpSocket *sock = m_hbConnector;
    m_hbConnector = nullptr;
    if (!sock) return;

    sock->disconnect(this);
    attachHeartbeatSocket(sock);
    evaluateConnected();
}

/// Slot for the main connector's errorOccurred() signal: records the error in
/// m_diagnostics.lastError, discards the failed connector, and schedules a reconnect if
/// the device is still active.
/// @param err unused; the failure text is instead read from the connector's errorString()
void VisionTcpipClientDevice::onMainConnectError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    if (m_mainConnector) {
        m_diagnostics.lastError = QString("Main connect failed: %1")
                                      .arg(m_mainConnector->errorString());
        m_mainConnector->disconnect(this);
        m_mainConnector->deleteLater();
        m_mainConnector = nullptr;
    }
    if (isActive()) {
        scheduleReconnect();
    }
}

/// Slot for the heartbeat connector's errorOccurred() signal: records the error in
/// m_diagnostics.lastError, discards the failed connector, and schedules a reconnect if
/// the device is still active.
/// @param err unused; the failure text is instead read from the connector's errorString()
void VisionTcpipClientDevice::onHeartbeatConnectError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    if (m_hbConnector) {
        m_diagnostics.lastError = QString("Heartbeat connect failed: %1")
                                      .arg(m_hbConnector->errorString());
        m_hbConnector->disconnect(this);
        m_hbConnector->deleteLater();
        m_hbConnector = nullptr;
    }
    if (isActive()) {
        scheduleReconnect();
    }
}

/// Arms a one-shot reconnect: lazily creates m_reconnectTimer wired to attemptConnect(),
/// then (re)starts it for m_config.m_reconnectIntervalMs if it isn't already running.
/// No-op if the device is not active.
void VisionTcpipClientDevice::scheduleReconnect() {
    if (!isActive()) {
        return;
    }
    if (!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        connect(m_reconnectTimer, &QTimer::timeout,
                this, &VisionTcpipClientDevice::attemptConnect);
    }
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_config.m_reconnectIntervalMs);
    }
}

/// Promotes the device to ConnectStatus::Connected once the device is active and both the
/// main and heartbeat sockets exist and report QAbstractSocket::ConnectedState; otherwise
/// leaves the current status untouched.
void VisionTcpipClientDevice::evaluateConnected() {
    // Connected is reported ONLY when both the main and heartbeat links are up.
    if (isActive() && m_mainSocket && m_hbSocket
        && m_mainSocket->state() == QAbstractSocket::ConnectedState
        && m_hbSocket->state() == QAbstractSocket::ConnectedState) {
        if (connectStatus() != ConnectStatus::Connected) {
            setConnectionStatus(ConnectStatus::Connected);
        }
    }
}

} // namespace vc::device
