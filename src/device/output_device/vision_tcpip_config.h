#ifndef VISION_TCPIP_CONFIG_H
#define VISION_TCPIP_CONFIG_H

/**
 * @file vision_tcpip_config.h
 * @brief Concrete config for the TCP/IP server transport of VisionOutput (VisionTcpipDeviceCfg).
 *        The device listens on two ports: main (matching request/result) and heartbeat
 *        ("connection_check." / "ack,{count}." protocol). Clients that fail to ack within
 *        heartbeatTimeoutMs cause the device to enter LostConnected.
 */

#include "device/output_device/vision_output_device.h"
#include "core/qgadget_macro.h"

#include <QJsonObject>
#include <QString>

namespace vc::device {

/**
 * @class VisionTcpipDeviceCfg
 * @brief Concrete config for the TCP/IP server transport of VisionOutput: listen address,
 *        main and heartbeat port numbers, heartbeat probe interval and reply timeout.
 */
class VisionTcpipDeviceCfg : public VisionOutputDeviceCfg {
    Q_GADGET

    /// Address to bind both TCP listeners to (e.g. "0.0.0.0" for all interfaces).
    G_PROPERTY_STRING_READWRITE(QString, listenAddress, "Listen Address")
    /// Main-channel listen port (matching requests / results).
    G_PROPERTY_NUMBER_READWRITE(int, mainPort, 1, 65535, "Main Port (Port 1)")
    /// Heartbeat-channel listen port.
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatPort, 1, 65535, "Heartbeat Port (Port 2)")
    /// Interval between heartbeat probes sent to the client, in milliseconds.
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatIntervalMs, 100, 60000, "Heartbeat Interval (ms)")
    /// Maximum time to wait for a valid heartbeat ack before declaring
    /// LostConnected, in milliseconds.
    G_PROPERTY_NUMBER_READWRITE(int, heartbeatTimeoutMs, 100, 60000, "Heartbeat Timeout (ms)")

    /// Translation markers for the display names above. Not read by any code: lupdate cannot
    /// see Q_CLASSINFO, so without this table these labels never enter the .ts. The context
    /// must be this class's className(). See TaskLocalizeConfig::kDisplayNameSources for the
    /// full reasoning; the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::VisionTcpipDeviceCfg", "Listen Address"),
        QT_TRANSLATE_NOOP("vc::device::VisionTcpipDeviceCfg", "Main Port (Port 1)"),
        QT_TRANSLATE_NOOP("vc::device::VisionTcpipDeviceCfg", "Heartbeat Port (Port 2)"),
        QT_TRANSLATE_NOOP("vc::device::VisionTcpipDeviceCfg", "Heartbeat Interval (ms)"),
        QT_TRANSLATE_NOOP("vc::device::VisionTcpipDeviceCfg", "Heartbeat Timeout (ms)"),
    };

public:
    /// Default-constructs the config with the base VisionOutputDeviceCfg defaults.
    explicit VisionTcpipDeviceCfg() : VisionOutputDeviceCfg() {}

    /// Returns this gadget's static QMetaObject, for property-browser introspection.
    const QMetaObject &getMetaObject() const override {
        return vc::device::VisionTcpipDeviceCfg::staticMetaObject;
    }

    /// Identifies this config as the TCP/IP server sub-type.
    VisionOutputType visionOutputType() const override {
        return VisionOutputType::VisionTCPIP;
    }

    /// Serializes this config to the VisionTcpipDeviceCfg JSON schema:
    /// {
    ///   "VisionOutputType":     "VisionTCPIP",
    ///   "ListenAddress":        string,   // bind address ("0.0.0.0" cho mọi NIC)
    ///   "MainPort":             int,      // Port 1 — matching channel
    ///   "HeartbeatPort":        int,      // Port 2 — heartbeat channel
    ///   "HeartbeatIntervalMs":  int,      // chu kỳ gửi probe (ms)
    ///   "HeartbeatTimeoutMs":   int       // timeout chờ reply (ms)
    /// }
    /// Key constants: DEVICE_JSK_VOUT_* trong idevice_config.h.
    /// fromJson đọc cùng schema, default-fill khi thiếu key.
    QJsonObject toJson() const override {
        QJsonObject obj = VisionOutputDeviceCfg::toJson();
        obj[DEVICE_JSK_VOUT_LISTEN_ADDR] = m_listenAddress;
        obj[DEVICE_JSK_VOUT_MAIN_PORT]   = m_mainPort;
        obj[DEVICE_JSK_VOUT_HB_PORT]     = m_heartbeatPort;
        obj[DEVICE_JSK_VOUT_HB_INTERVAL] = m_heartbeatIntervalMs;
        obj[DEVICE_JSK_VOUT_HB_TIMEOUT]  = m_heartbeatTimeoutMs;
        return obj;
    }

    /// Reads the fields written by toJson(), default-filling any key missing
    /// from `obj` (see the schema documented on toJson()).
    /// @return false if the base VisionOutputDeviceCfg::fromJson fails; true otherwise
    bool fromJson(const QJsonObject &obj) override {
        if (!VisionOutputDeviceCfg::fromJson(obj)) return false;
        m_listenAddress       = obj[DEVICE_JSK_VOUT_LISTEN_ADDR].toString("0.0.0.0");
        m_mainPort            = obj[DEVICE_JSK_VOUT_MAIN_PORT].toInt(5000);
        m_heartbeatPort       = obj[DEVICE_JSK_VOUT_HB_PORT].toInt(5001);
        m_heartbeatIntervalMs = obj[DEVICE_JSK_VOUT_HB_INTERVAL].toInt(1000);
        m_heartbeatTimeoutMs  = obj[DEVICE_JSK_VOUT_HB_TIMEOUT].toInt(3000);
        return true;
    }

    /// Returns a heap-allocated copy of this config; caller takes ownership.
    IDeviceCfg* clone() override {
        return new VisionTcpipDeviceCfg(*this);
    }

public:
    QString m_listenAddress{"0.0.0.0"};             ///< Bind address for both TCP listeners.
    int m_mainPort{5000};                           ///< Main-channel listen port.
    int m_heartbeatPort{5001};                       ///< Heartbeat-channel listen port.
    int m_heartbeatIntervalMs{1000};                 ///< Heartbeat probe period, in milliseconds.
    int m_heartbeatTimeoutMs{3000};                  ///< Heartbeat reply timeout, in milliseconds.
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::VisionTcpipDeviceCfg)

#endif // VISION_TCPIP_CONFIG_H
