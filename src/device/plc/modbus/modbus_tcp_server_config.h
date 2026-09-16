#ifndef MODBUS_TCP_SERVER_CONFIG_H
#define MODBUS_TCP_SERVER_CONFIG_H

/**
 * @file modbus_tcp_server_config.h
 * @brief Config for the Modbus TCP server device (we are the slave): where to listen for a
 *        master.
 *
 * @note There is no poll interval here. A server does not poll; values change when a master
 *       writes them and the device publishes on that event. Carrying a poll interval that does
 *       nothing would put a field on the property browser the operator can set and that has no
 *       effect, which is worse than not offering it.
 */

#include "core/qgadget_macro.h"
#include "device/plc/modbus/modbus_config.h"

#include <QString>

namespace vc::device {

/**
 * @class ModbusTcpServerCfg
 * @brief Modbus TCP slave config: the shared register map plus the listen endpoint.
 */
class ModbusTcpServerCfg : public ModbusConfig {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, listenAddress, "Listen Address")
    G_PROPERTY_NUMBER_READWRITE(int, port, 1, 65535, "Port")
    G_PROPERTY_BOOL_READWRITE(bool, resultInInputRegisters, "Result In Input Registers")

    /// Translation markers for the Q_CLASSINFO display names above; see ModbusConfig.
public:
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpServerCfg", "Listen Address"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpServerCfg", "Port"),
        QT_TRANSLATE_NOOP("vc::device::ModbusTcpServerCfg", "Result In Input Registers"),
    };

public:
    const QMetaObject &getMetaObject() const override {
        return vc::device::ModbusTcpServerCfg::staticMetaObject;
    }

    PlcType plcType() const override { return PlcType::ModbusTcpServer; }

    /**
     * @brief Returns the word area the result block lives in: input registers when
     *        `resultInInputRegisters` is set, holding registers otherwise.
     *
     * Only a server can make this choice, and it is the safer of the two: a master has no
     * function code that writes an input register, so a result published there **cannot be
     * overwritten by the master**, accidentally or otherwise. Holding registers stay the default
     * because that is what the client sub-type must use and what the published contract describes;
     * a PLC program pointed at 4x will not find a result block that moved to 3x.
     */
    ModbusArea resultArea() const override {
        return m_resultInInputRegisters ? ModbusArea::InputRegisters
                                        : ModbusArea::HoldingRegisters;
    }

    QJsonObject toJson() const override {
        QJsonObject obj = ModbusConfig::toJson();
        obj[DEVICE_JSK_MB_LISTEN_ADDRESS] = m_listenAddress;
        obj[DEVICE_JSK_MB_PORT]           = m_port;
        obj[DEVICE_JSK_MB_RESULT_IN_INPUT] = m_resultInInputRegisters;
        return obj;
    }

    bool fromJson(const QJsonObject &obj) override {
        if (!ModbusConfig::fromJson(obj)) {
            return false;
        }
        m_listenAddress = obj[DEVICE_JSK_MB_LISTEN_ADDRESS].toString(m_listenAddress);
        m_port          = obj[DEVICE_JSK_MB_PORT].toInt(m_port);
        m_resultInInputRegisters =
            obj[DEVICE_JSK_MB_RESULT_IN_INPUT].toBool(m_resultInInputRegisters);
        return true;
    }

    IDeviceCfg *clone() override { return new ModbusTcpServerCfg(*this); }

public:
    /// Interface to bind. "0.0.0.0" accepts a master on any interface.
    QString m_listenAddress{QStringLiteral("0.0.0.0")};
    int m_port{502};   ///< Modbus TCP port to listen on.
    /// Publish the vision result into input registers (3x) instead of holding registers (4x).
    /// Defaults to false so an unchanged project keeps the published contract's layout.
    bool m_resultInInputRegisters{false};
};

} // namespace vc::device

#endif // MODBUS_TCP_SERVER_CONFIG_H
