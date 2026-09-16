#ifndef MODBUS_TCP_CLIENT_CONFIG_H
#define MODBUS_TCP_CLIENT_CONFIG_H

/**
 * @file modbus_tcp_client_config.h
 * @brief Config for the Modbus TCP client device (we are the master): which slave to dial, how
 *        often to poll it, and how long to wait for a reply.
 *
 * @note One config class per header, following `vision_tcpip_config.h` /
 *       `vision_tcpip_client_config.h`. The architecture contract test reads each header and
 *       requires every QT_TRANSLATE_NOOP context in it to name that header's class, so two
 *       marker tables in one file cannot both be right.
 */

#include "core/qgadget_macro.h"
#include "device/plc/modbus/modbus_config.h"

#include <QString>

namespace vc::device {

/**
 * @class ModbusTcpClientCfg
 * @brief Modbus TCP master config: the shared register map plus connection and timing.
 */
class ModbusTcpClientCfg : public ModbusConfig {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, hostAddress, "Slave Address")
    G_PROPERTY_NUMBER_READWRITE(int, port, 1, 65535, "Port")
    G_PROPERTY_NUMBER_READWRITE(int, refreshInterval, 10, 10000, "Refresh Interval")
    G_PROPERTY_NUMBER_READWRITE(int, responseTimeout, 50, 10000, "Response Timeout")
    G_PROPERTY_NUMBER_READWRITE(int, retryCount, 0, 10, "Retry Count")

    /// Translation markers for the Q_CLASSINFO display names above; see ModbusConfig for why
    /// this table has to exist and why its context must be this class's className().
public:
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpClientCfg", "Slave Address"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpClientCfg", "Port"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpClientCfg", "Refresh Interval"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpClientCfg", "Response Timeout"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpClientCfg", "Retry Count"),
    };

public:
    const QMetaObject &getMetaObject() const override {
        return vc::device::ModbusTcpClientCfg::staticMetaObject;
    }

    PlcType plcType() const override { return PlcType::ModbusTcpClient; }

    QJsonObject toJson() const override {
        QJsonObject obj = ModbusConfig::toJson();
        obj[DEVICE_JSK_MB_HOST_ADDRESS]     = m_hostAddress;
        obj[DEVICE_JSK_MB_PORT]             = m_port;
        obj[DEVICE_JSK_MB_REFRESH_INTERVAL] = m_refreshInterval;
        obj[DEVICE_JSK_MB_RESPONSE_TIMEOUT] = m_responseTimeout;
        obj[DEVICE_JSK_MB_RETRY_COUNT]      = m_retryCount;
        return obj;
    }

    bool fromJson(const QJsonObject &obj) override {
        if (!ModbusConfig::fromJson(obj)) {
            return false;
        }
        m_hostAddress     = obj[DEVICE_JSK_MB_HOST_ADDRESS].toString(m_hostAddress);
        m_port            = obj[DEVICE_JSK_MB_PORT].toInt(m_port);
        m_refreshInterval = obj[DEVICE_JSK_MB_REFRESH_INTERVAL].toInt(m_refreshInterval);
        m_responseTimeout = obj[DEVICE_JSK_MB_RESPONSE_TIMEOUT].toInt(m_responseTimeout);
        m_retryCount      = obj[DEVICE_JSK_MB_RETRY_COUNT].toInt(m_retryCount);
        return true;
    }

    IDeviceCfg *clone() override { return new ModbusTcpClientCfg(*this); }

public:
    QString m_hostAddress{QStringLiteral("192.168.0.10")};  ///< Slave address to dial.
    int m_port{502};                ///< Modbus TCP port; 502 is the registered default.
    int m_refreshInterval{100};     ///< Poll period, in milliseconds.
    int m_responseTimeout{1000};    ///< Per-request reply timeout, in milliseconds.
    int m_retryCount{3};            ///< Request retries before the link is declared lost.
};

} // namespace vc::device

#endif // MODBUS_TCP_CLIENT_CONFIG_H
