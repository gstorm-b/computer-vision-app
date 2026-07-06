#ifndef VISION_TCPIP_CLIENT_CONFIG_H
#define VISION_TCPIP_CLIENT_CONFIG_H

#include "device/output_device/vision_output_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>
#include <QString>

namespace vc::device {

/**
 * @brief Concrete config for the TCP/IP **client** transport of VisionOutput.
 *
 *  Same protocol as the server transport, but the software dials OUT to a
 *  remote endpoint that acts as the TCP server (e.g. a robot controller).
 *
 *      - serverAddress    : remote host to connect to.
 *      - mainPort         : Port 1 — matching request / result channel.
 *      - heartbeatPort    : Port 2 — heartbeat channel. Software stays the
 *                           heartbeat master ("connection_check." -> "ack,N.").
 *      - reconnectIntervalMs : delay before retrying a dropped/failed link.
 *
 *  On heartbeat timeout / lost connection the device drops both links and
 *  retries the outbound connection every `reconnectIntervalMs`.
 */
class VisionTcpipClientDeviceCfg : public VisionOutputDeviceCfg {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, serverAddress, "Server Address")
    G_PROPERTY_NUMBER_READWRITE(int, mainPort, 1, 65535, "Main Port (Port 1)")
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatPort, 1, 65535, "Heartbeat Port (Port 2)")
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatIntervalMs, 100, 60000, "Heartbeat Interval (ms)")
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatTimeoutMs, 100, 60000, "Heartbeat Timeout (ms)")
    G_PROPERTY_NUMBER_READWRITE(int, reconnectIntervalMs, 100, 60000, "Reconnect Interval (ms)")

public:
    explicit VisionTcpipClientDeviceCfg() : VisionOutputDeviceCfg() {}

    const QMetaObject &getMetaObject() const override {
        return vc::device::VisionTcpipClientDeviceCfg::staticMetaObject;
    }

    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VisionTcpipClient;
    }

    /**
     * @brief toJson — VisionTcpipClientDeviceCfg schema
     * {
     *   "VisionOutputType":     "VisionTcpipClient",
     *   "ServerAddress":        string,   // remote host to dial
     *   "MainPort":             int,      // Port 1 — matching channel
     *   "HeartbeatPort":        int,      // Port 2 — heartbeat channel
     *   "HeartbeatIntervalMs":  int,      // probe interval (ms)
     *   "HeartbeatTimeoutMs":   int,      // reply timeout (ms)
     *   "ReconnectIntervalMs":  int       // retry delay after a dropped link (ms)
     * }
     * Key constants: DEVICE_JSK_VOUT_* in idevice_config.h.
     * fromJson reads the same schema, default-fills when a key is missing.
     */
    QJsonObject toJson() const override {
        QJsonObject obj = VisionOutputDeviceCfg::toJson();
        obj[DEVICE_JSK_VOUT_SERVER_ADDR] = m_serverAddress;
        obj[DEVICE_JSK_VOUT_MAIN_PORT]   = m_mainPort;
        obj[DEVICE_JSK_VOUT_HB_PORT]     = m_heartbeatPort;
        obj[DEVICE_JSK_VOUT_HB_INTERVAL] = m_heartbeatIntervalMs;
        obj[DEVICE_JSK_VOUT_HB_TIMEOUT]  = m_heartbeatTimeoutMs;
        obj[DEVICE_JSK_VOUT_RECONNECT]   = m_reconnectIntervalMs;
        return obj;
    }

    bool fromJson(const QJsonObject &obj) override {
        if (!VisionOutputDeviceCfg::fromJson(obj)) return false;
        m_serverAddress       = obj[DEVICE_JSK_VOUT_SERVER_ADDR].toString("127.0.0.1");
        m_mainPort            = obj[DEVICE_JSK_VOUT_MAIN_PORT].toInt(5000);
        m_heartbeatPort       = obj[DEVICE_JSK_VOUT_HB_PORT].toInt(5001);
        m_heartbeatIntervalMs = obj[DEVICE_JSK_VOUT_HB_INTERVAL].toInt(1000);
        m_heartbeatTimeoutMs  = obj[DEVICE_JSK_VOUT_HB_TIMEOUT].toInt(3000);
        m_reconnectIntervalMs = obj[DEVICE_JSK_VOUT_RECONNECT].toInt(2000);
        return true;
    }

    IDeviceCfg* clone() override {
        return new VisionTcpipClientDeviceCfg(*this);
    }

public:
    QString m_serverAddress{"127.0.0.1"};
    int m_mainPort{5000};
    int m_heartbeatPort{5001};
    int m_heartbeatIntervalMs{1000};
    int m_heartbeatTimeoutMs{3000};
    int m_reconnectIntervalMs{2000};
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::VisionTcpipClientDeviceCfg)

#endif // VISION_TCPIP_CLIENT_CONFIG_H
