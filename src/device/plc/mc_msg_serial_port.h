#ifndef MC_MSG_SERIAL_PORT_H
#define MC_MSG_SERIAL_PORT_H

/**
 * @file mc_msg_serial_port.h
 * @brief Serial line config (McMsgSerialCfg) and synchronous serial transport
 *        (McMsgSerialPort) for the Mitsubishi MC-protocol PLC integration.
 *
 * This is the transport the computer-link frames (1C/3C) run on. `McMsgItfType::SerialPort`
 * has been declared since the transport interface was written; this file is the
 * implementation behind it. Nothing here is frame-specific — a future 1E/3E-over-serial
 * setup selects this same transport.
 */

#include "device/plc/mc_msg_interface.h"

#include <QByteArray>
#include <QJsonObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>

namespace vc::device {

/**
 * @class McMsgSerialCfg
 * @brief McMsgItfConfig for the serial (computer-link) transport: port name and line
 *        settings, on top of the base connect/write/response timeouts.
 *
 * @note Defaults are the common Mitsubishi computer-link line setting (9600 baud, 7 data
 *       bits, even parity, 1 stop bit, no flow control). They are a starting point, not a
 *       negotiated value: a serial link has no handshake that can discover the other end's
 *       settings, so these must be set to match the C24/FX port's actual configuration or
 *       every frame comes back as garbage rather than as a connection error.
 */
class McMsgSerialCfg : public McMsgItfConfig {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, portName, "Serial port")   ///< Q_GADGET property backed by m_portName; system port name, e.g. "COM3".
    G_PROPERTY_NUMBER_READWRITE(int, baudRate, 300, 921600, "Baud rate")   ///< Q_GADGET property backed by m_baudRate; line speed in bit/s.
    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McSerialDataBits, dataBits, "Data bits")   ///< Q_GADGET property backed by m_dataBits; character length.
    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McSerialParity, parity, "Parity")   ///< Q_GADGET property backed by m_parity; parity scheme.
    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McSerialStopBits, stopBits, "Stop bits")   ///< Q_GADGET property backed by m_stopBits; number of stop bits.
    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McSerialFlowControl, flowControl, "Flow control")   ///< Q_GADGET property backed by m_flowControl; flow-control scheme.

    /// Translation markers for the display names above. Not read by any code: lupdate cannot
    /// see Q_CLASSINFO, so without this table these labels never enter the .ts. The context
    /// must be this class's className(). See TaskLocalizeConfig::kDisplayNameSources for the
    /// full reasoning; the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Serial port"),
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Baud rate"),
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Data bits"),
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Parity"),
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Stop bits"),
        QT_TRANSLATE_NOOP("vc::device::McMsgSerialCfg", "Flow control"),
    };

public:
    /// Default-constructs the config with the member-initializer defaults (9600 7E1, no
    /// flow control, no port selected).
    explicit McMsgSerialCfg() {

    }

    /// Returns this gadget's static meta-object, used by property-browser/serialization
    /// code to enumerate its Q_PROPERTY / Q_CLASSINFO entries.
    const QMetaObject &getMetaObject() const override {
        return vc::device::McMsgSerialCfg::staticMetaObject;
    }

    /// Identifies this config as the serial transport type.
    /// @return McMsgItfType::SerialPort
    McMsgItfType type() const override {
        return McMsgItfType::SerialPort;
    }

    /// Lists the system port names currently present on this machine (e.g. "COM3"), for the
    /// UI's port picker.
    /// @return the available port names, in the order QSerialPortInfo reports them
    static QStringList availablePortNames() {
        QStringList names;
        const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        names.reserve(ports.size());
        for (const QSerialPortInfo &info : ports) {
            names.append(info.portName());
        }
        return names;
    }

    /// Serializes this config to JSON, extending McMsgItfConfig::toJson() with the port name
    /// and line settings. Enums are written as their key names, not their numeric values, so
    /// reordering an enumerator cannot silently change a saved project's meaning.
    /// @return the populated JSON object
    QJsonObject toJson() const override {
        QJsonObject obj = McMsgItfConfig::toJson();
        obj["portName"] = m_portName;
        obj["baudRate"] = m_baudRate;
        obj["dataBits"] = qenumToString(m_dataBits);
        obj["parity"] = qenumToString(m_parity);
        obj["stopBits"] = qenumToString(m_stopBits);
        obj["flowControl"] = qenumToString(m_flowControl);
        return obj;
    }

    /// Populates this config from JSON written by toJson(), delegating the base timeouts to
    /// McMsgItfConfig::fromJson() first. A missing or unrecognized key falls back to the
    /// current field value rather than to a zero enum, so a partially written document does
    /// not produce an unopenable port setting.
    /// @return always true
    bool fromJson(const QJsonObject &obj) override {
        McMsgItfConfig::fromJson(obj);
        m_portName = obj["portName"].toString(m_portName);
        m_baudRate = obj["baudRate"].toInt(m_baudRate);
        m_dataBits = stringToQEnum(obj["dataBits"].toString(), m_dataBits);
        m_parity = stringToQEnum(obj["parity"].toString(), m_parity);
        m_stopBits = stringToQEnum(obj["stopBits"].toString(), m_stopBits);
        m_flowControl = stringToQEnum(obj["flowControl"].toString(), m_flowControl);
        return true;
    }

public:
    QString m_portName;   ///< System port name (e.g. "COM3"); backing store for the portName property. Empty until configured.
    int m_baudRate{9600};   ///< Line speed in bit/s; backing store for the baudRate property.
    McSerialDataBits m_dataBits{McSerialDataBits::DataBits_7};   ///< Character length; backing store for the dataBits property.
    McSerialParity m_parity{McSerialParity::Parity_Even};   ///< Parity scheme; backing store for the parity property.
    McSerialStopBits m_stopBits{McSerialStopBits::StopBits_One};   ///< Number of stop bits; backing store for the stopBits property.
    McSerialFlowControl m_flowControl{McSerialFlowControl::FlowControl_None};   ///< Flow-control scheme; backing store for the flowControl property.
};

/**
 * @class McMsgSerialPort
 * @brief McMsgInterface transport that talks to the PLC over a serial line (QSerialPort),
 *        driven synchronously via waitFor*() calls — the same shape as McEthernetTcpPort, so
 *        McProtocolDevice's send/receive/retry logic is unchanged by the transport swap.
 */
class McMsgSerialPort : public McMsgInterface {
public:
    /// Constructs the port in the NotInit state and allocates the underlying QSerialPort
    /// (not yet opened).
    McMsgSerialPort() :
        m_serial(nullptr) {

        m_port_state = MsgIfState::NotInit;
        m_error_state = MsgErrorState::NoError;
        m_error_description = "";
        m_serial = new QSerialPort;
    }

    /// Destructor. Does not close/delete the port itself; callers must invoke
    /// DestroyMsgPort() first if that cleanup is needed (matching McEthernetTcpPort).
    ~McMsgSerialPort() {

    }

    /// Adopts `cfg` as this port's line/timeout configuration.
    /// @return false if `cfg` is null or not a SerialPort config, true after copying it
    bool SetConfig(McMsgItfConfig *cfg) override {
        if (!cfg) {
            return false;
        }

        if (cfg->type() != McMsgItfType::SerialPort) {
            return false;
        }

        m_config = *(static_cast<McMsgSerialCfg*>(cfg));
        return true;
    }

    /// Reports whether the serial port is currently open.
    /// @return true if the port exists and is open
    const bool ConnectionCheck() override {
        return (m_serial != nullptr) && m_serial->isOpen();
    }

    /**
     * @brief Applies the configured line settings and opens the port for read/write.
     *
     * Any previously open handle is closed first, so a reconnect after a failure never
     * stacks handles on the same COM port — a serial port is exclusive, and a leaked handle
     * makes every later attempt fail with "access denied" for reasons that look like the
     * cable.
     *
     * @note The base class's connect timeout does not apply here: opening a serial port is a
     *       local, immediate operation with no peer to wait for. It either succeeds or
     *       reports why.
     * @return Connected once the port is open, ConnectFail otherwise (m_error_description
     *         carries QSerialPort's reason)
     */
    MsgIfState ConnectToPort() override {
        m_total_wait_time = 0;

        if (!m_serial) {
            m_error_description = "serial port object destroyed";
            m_port_state = MsgIfState::ConnectFail;
            return m_port_state;
        }

        if (m_serial->isOpen()) {
            m_serial->close();
        }

        if (m_config.m_portName.isEmpty()) {
            m_error_description = "no serial port configured";
            m_port_state = MsgIfState::ConnectFail;
            return m_port_state;
        }

        m_serial->setPortName(m_config.m_portName);

        if (!m_serial->setBaudRate(m_config.m_baudRate) ||
            !m_serial->setDataBits(toQtDataBits(m_config.m_dataBits)) ||
            !m_serial->setParity(toQtParity(m_config.m_parity)) ||
            !m_serial->setStopBits(toQtStopBits(m_config.m_stopBits)) ||
            !m_serial->setFlowControl(toQtFlowControl(m_config.m_flowControl))) {
            // Settings are applied to the (still closed) port object; a rejection here means
            // the value itself is unsupported, which is a configuration fault, not a wiring
            // one. Reported separately so it is not mistaken for a missing cable.
            m_error_description = QStringLiteral("unsupported serial line setting: %1")
                                      .arg(m_serial->errorString());
            m_port_state = MsgIfState::ConnectFail;
            return m_port_state;
        }

        if (!m_serial->open(QIODevice::ReadWrite)) {
            m_error_description = QStringLiteral("cannot open %1: %2")
                                      .arg(m_config.m_portName, m_serial->errorString());
            m_port_state = MsgIfState::ConnectFail;
            return m_port_state;
        }

        m_serial->clear();
        m_port_state = MsgIfState::Connected;
        return m_port_state;
    }

    /// Closes the serial port if it is open.
    /// @return NoConnection (always)
    MsgIfState DisconnectFromPort() override {
        if (m_serial && m_serial->isOpen()) {
            m_serial->close();
        }
        m_port_state = MsgIfState::NoConnection;
        return m_port_state;
    }

    /// Writes `buffer` to the port, blocking up to m_config.m_writeTimeout ms for the bytes
    /// to be flushed.
    /// @return BufferEmpty if `buffer` is empty, WriteTimeout on write timeout, ErrorOcurred
    /// if the port is unusable or an exception was thrown (m_error_description is set),
    /// NoError on success
    const MsgErrorState SendMsg(QByteArray &buffer) override {
        if (buffer.isEmpty()) {
            m_error_state = MsgErrorState::BufferEmpty;
            return m_error_state;
        }

        if (!m_serial || !m_serial->isOpen()) {
            m_error_state = MsgErrorState::ErrorOcurred;
            m_error_description = "serial port is not open";
            return m_error_state;
        }

        try {
            m_serial->write(buffer);
            if (!m_serial->waitForBytesWritten(m_config.m_writeTimeout)) {
                m_error_state = MsgErrorState::WriteTimeout;
                return m_error_state;
            }
        } catch (const std::exception& e){
            m_error_state = MsgErrorState::ErrorOcurred;
            m_error_description = QString::fromStdString(e.what());
            return m_error_state;
        }

        m_error_state = MsgErrorState::NoError;
        return m_error_state;
    }

    /**
     * @brief Waits up to `wait_buffer` ms for readyRead, then appends any available bytes to
     *        `buffer`. Accumulated wait time triggers ResponseTimeout once it exceeds
     *        m_config.m_responseTimeout.
     *
     * A computer-link response arrives in pieces far more often than a TCP one does — the
     * frame is assembled by the caller across repeated calls, which is why partial reads are
     * reported as BufferEmpty rather than as an error.
     *
     * @param[out] buffer buffer to append received bytes to
     * @param[in]  wait_buffer per-call read wait time in milliseconds
     * @return NoError if bytes were appended (resets accumulated time), BufferEmpty while
     *         still under the response timeout, ResponseTimeout once it is exceeded
     */
    const MsgErrorState ReceiveMsg(QByteArray &buffer, int wait_buffer = 1) override {
        if (!m_serial || !m_serial->isOpen()) {
            m_error_state = MsgErrorState::ErrorOcurred;
            m_error_description = "serial port is not open";
            return m_error_state;
        }

        m_serial->waitForReadyRead(wait_buffer);

        if (m_serial->bytesAvailable() < 1) {
            m_total_wait_time += wait_buffer;
            if (m_total_wait_time > m_config.m_responseTimeout) {
                m_error_state = MsgErrorState::ResponseTimeout;
                return m_error_state;
            }

            m_error_state = MsgErrorState::BufferEmpty;
            return m_error_state;
        }

        m_total_wait_time = 0;
        QByteArray read_bytes = m_serial->readAll();
        buffer.append(read_bytes);
        m_error_state = MsgErrorState::NoError;
        return m_error_state;
    }

    /// Discards any bytes currently buffered on the port, in both directions.
    /// @note QSerialPort::clear() drops the driver's buffers too, which readAll() alone does
    ///       not — after a timeout the unread tail of a stale frame is still in the OS buffer
    ///       and would be parsed as the head of the next response.
    void clearBuffer() override {
        if (m_serial && m_serial->isOpen()) {
            m_serial->clear();
        }
    }

    /// Returns the description of the most recent open/send/receive error, if any.
    QString GetErrorDescription() override {
        return m_error_description;
    }

    /// Identifies this port as the serial transport type.
    /// @return McMsgItfType::SerialPort
    const McMsgItfType type() const override {
        return McMsgItfType::SerialPort;
    }

    /// Returns the underlying QSerialPort as a QIODevice; ownership stays with this port.
    QIODevice* ioDevice() const override {
        return static_cast<QIODevice*>(m_serial);
    }

    /// Closes the port, then deletes it synchronously on the calling (worker) thread.
    /// @note deliberately not deleteLater(): during teardown the worker event loop may
    /// already be stopped, which would leak the port object and leave the COM port claimed
    /// until the process exits. Same rationale as McEthernetTcpPort::DestroyMsgPort().
    void DestroyMsgPort() override {
        DisconnectFromPort();
        if (m_serial != nullptr) {
            delete m_serial;
            m_serial = nullptr;
        }
    }

private:
    /// Maps the transport-independent data-bits enum to QSerialPort's.
    static QSerialPort::DataBits toQtDataBits(McSerialDataBits bits) {
        switch (bits) {
        case McSerialDataBits::DataBits_5: return QSerialPort::Data5;
        case McSerialDataBits::DataBits_6: return QSerialPort::Data6;
        case McSerialDataBits::DataBits_7: return QSerialPort::Data7;
        case McSerialDataBits::DataBits_8: return QSerialPort::Data8;
        }
        return QSerialPort::Data7;
    }

    /// Maps the transport-independent parity enum to QSerialPort's.
    static QSerialPort::Parity toQtParity(McSerialParity parity) {
        switch (parity) {
        case McSerialParity::Parity_None:  return QSerialPort::NoParity;
        case McSerialParity::Parity_Even:  return QSerialPort::EvenParity;
        case McSerialParity::Parity_Odd:   return QSerialPort::OddParity;
        case McSerialParity::Parity_Space: return QSerialPort::SpaceParity;
        case McSerialParity::Parity_Mark:  return QSerialPort::MarkParity;
        }
        return QSerialPort::EvenParity;
    }

    /// Maps the transport-independent stop-bits enum to QSerialPort's.
    static QSerialPort::StopBits toQtStopBits(McSerialStopBits bits) {
        switch (bits) {
        case McSerialStopBits::StopBits_One:        return QSerialPort::OneStop;
        case McSerialStopBits::StopBits_OneAndHalf: return QSerialPort::OneAndHalfStop;
        case McSerialStopBits::StopBits_Two:        return QSerialPort::TwoStop;
        }
        return QSerialPort::OneStop;
    }

    /// Maps the transport-independent flow-control enum to QSerialPort's.
    static QSerialPort::FlowControl toQtFlowControl(McSerialFlowControl flow) {
        switch (flow) {
        case McSerialFlowControl::FlowControl_None:     return QSerialPort::NoFlowControl;
        case McSerialFlowControl::FlowControl_Hardware: return QSerialPort::HardwareControl;
        case McSerialFlowControl::FlowControl_Software: return QSerialPort::SoftwareControl;
        }
        return QSerialPort::NoFlowControl;
    }

private:
    QSerialPort *m_serial;   ///< Owned serial port; null after DestroyMsgPort().
    McMsgSerialCfg m_config;   ///< Line/timeout configuration applied via SetConfig().
    int m_total_wait_time{0};   ///< Elapsed wait time (ms) accumulated across ReceiveMsg() calls toward m_config.m_responseTimeout.
};

} // namespace vc::device


Q_DECLARE_METATYPE(vc::device::McMsgSerialCfg)

#endif // MC_MSG_SERIAL_PORT_H
