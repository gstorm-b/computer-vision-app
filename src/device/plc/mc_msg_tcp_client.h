#ifndef MC_MSG_TCP_CLIENT_H
#define MC_MSG_TCP_CLIENT_H

#include "device/plc/mc_msg_interface.h"

#include <memory>
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QJsonObject>

/// Device-family classes for the MC (Mitsubishi) protocol PLC integration.
namespace vc::device {

/// McMsgItfConfig for the Ethernet TCP/IP transport: adds the target IP address and port,
/// on top of the base connect/write/response timeouts.
class McMsgEthernetTcpCfg : public McMsgItfConfig {
    Q_GADGET

    G_PROPERTY_STRING_READWRITE(QString, ipAddress, "IP Address")   ///< Q_GADGET property backed by m_ipAddress; PLC IP address to connect to.
    G_PROPERTY_NUMBER_READWRITE(int, portNumber, 0, 100000, "Port number")   ///< Q_GADGET property backed by m_portNumber; PLC TCP port to connect to.

public:
    /// Default-constructs the config with the member-initializer defaults (192.168.0.1:5000).
    explicit McMsgEthernetTcpCfg() {

    }

    /// Returns this gadget's static meta-object, used by property-browser/serialization
    /// code to enumerate its Q_PROPERTY / Q_CLASSINFO entries.
    const QMetaObject &getMetaObject() const override {
        return vc::device::McMsgEthernetTcpCfg::staticMetaObject;
    }

    /// Identifies this config as the Ethernet TCP/IP transport type.
    /// @return McMsgItfType::EthernetTCPIP
    McMsgItfType type() const override {
        return McMsgItfType::EthernetTCPIP;
    }

    /// Serializes this config to JSON, extending McMsgItfConfig::toJson() with
    /// "ipAddress" and "portNumber".
    /// @return the populated JSON object
    QJsonObject toJson() const override {
        QJsonObject obj = McMsgItfConfig::toJson();
        obj["ipAddress"] = m_ipAddress;
        obj["portNumber"] = m_portNumber;
        return obj;
    }

    /// Populates this config from JSON written by toJson(), delegating the base timeouts
    /// to McMsgItfConfig::fromJson() first and defaulting ipAddress/portNumber if missing.
    /// @return always true
    bool fromJson(const QJsonObject &obj) override {
        McMsgItfConfig::fromJson(obj);
        m_ipAddress = obj["ipAddress"].toString("192.168.0.1");
        m_portNumber = obj["portNumber"].toInt(5000);
        return true;
    }

public:
    QString m_ipAddress{"192.168.0.1"};   ///< PLC IP address; backing store for the ipAddress property.
    int m_portNumber{5000};   ///< PLC TCP port; backing store for the portNumber property.
};

/// McMsgInterface transport that talks to the PLC over a plain TCP/IP socket
/// (QTcpSocket), driven synchronously via waitFor*() calls.
class McEthernetTcpPort : public McMsgInterface {
public:
    /// Constructs the port in the NotInit state and allocates the underlying QTcpSocket
    /// (not yet connected).
    McEthernetTcpPort() :
        m_socket(nullptr) {

        m_port_state = MsgIfState::NotInit;
        m_error_state = MsgErrorState::NoError;
        m_error_description = "";
        m_socket = new QTcpSocket;
    }

    /// Destructor. Does not close/delete the socket itself; callers must invoke
    /// DestroyMsgPort() first if that cleanup is needed.
    ~McEthernetTcpPort() {

    }

    /// Adopts `cfg` as this port's IP/port/timeout configuration.
    /// @return false if `cfg` is null or not an EthernetTCPIP config, true after copying it
    bool SetConfig(McMsgItfConfig *cfg) override {
        if (!cfg) {
            return false;
        }

        if (cfg->type() != McMsgItfType::EthernetTCPIP) {
            return false;
        }

        m_config = *(static_cast<McMsgEthernetTcpCfg*>(cfg));
        return true;
    }

    /// @note Not yet implemented; always reports the connection as usable.
    const bool ConnectionCheck() override {
        // implements connection check later

        return true;
    }

    /// Connects the socket to the configured host/port, blocking up to
    /// m_config.m_connectTimeout ms for the connection to complete.
    /// @return Connected on success, ConnectFail on timeout (m_error_description is set)
    MsgIfState ConnectToPort() override {
        m_socket->connectToHost(QHostAddress(m_config.m_ipAddress), m_config.m_portNumber);
        m_total_wait_time = 0;

        if (m_socket->waitForConnected(m_config.m_connectTimeout)) {
            m_port_state = MsgIfState::Connected;
        } else {
            m_error_description = "connect to server timeout";
            m_port_state = MsgIfState::ConnectFail;
        }
        return m_port_state;
    }

    /// Disconnects the socket from the host if not already unconnected.
    /// @return NoConnection (always)
    MsgIfState DisconnectFromPort() override {
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->disconnectFromHost();
        }
        m_port_state = MsgIfState::NoConnection;
        return m_port_state;
    }

    /// Writes `buffer` to the socket, blocking up to m_config.m_writeTimeout ms for the
    /// bytes to be flushed.
    /// @return BufferEmpty if `buffer` is empty, WriteTimeout on write timeout,
    /// ErrorOcurred if an exception was thrown (m_error_description is set), NoError on success
    const MsgErrorState SendMsg(QByteArray &buffer) override {
        if (buffer.isEmpty()) {
            m_error_state = MsgErrorState::BufferEmpty;
            return m_error_state;
        }

        try {
            m_socket->write(buffer);
            if (!m_socket->waitForBytesWritten(m_config.m_writeTimeout)) {
                m_error_state = MsgErrorState::WriteTimeout;
                return m_error_state;
            }
            // OLOG_INFO << "sent string:" << cmd;
        } catch (const std::exception& e){
            // OLOG_CRITICAL << "send string" << cmd << ", fail:" << e.what();
            m_error_state = MsgErrorState::ErrorOcurred;
            m_error_description = QString::fromStdString(e.what());
            return m_error_state;
        }

        m_error_state = MsgErrorState::NoError;
        return m_error_state;
    }

    /// Waits up to `wait_buffer` ms for readyRead, then appends any available bytes to `buffer`.
    /// Accumulates elapsed wait time across calls (m_total_wait_time) so repeated
    /// BufferEmpty results eventually surface as ResponseTimeout once
    /// m_config.m_responseTimeout is exceeded.
    /// @return NoError if bytes were appended (resets the accumulated wait time), BufferEmpty
    /// while still under the response timeout, ResponseTimeout once it is exceeded
    const MsgErrorState ReceiveMsg(QByteArray &buffer, int wait_buffer = 1) override {        
        m_socket->waitForReadyRead(wait_buffer);

        if (m_socket->bytesAvailable() < 1) {
            m_total_wait_time += wait_buffer;
            if (m_total_wait_time > m_config.m_responseTimeout) {
                m_error_state = MsgErrorState::ResponseTimeout;
                return m_error_state;
            }

            m_error_state = MsgErrorState::BufferEmpty;
            return m_error_state;
        }

        m_total_wait_time = 0;
        QByteArray read_bytes = m_socket->readAll();
        buffer.append(read_bytes);
        m_error_state = MsgErrorState::NoError;
        return m_error_state;
    }

    /// Discards any bytes currently buffered on the socket by reading and dropping them.
    void clearBuffer() override {
        if(m_socket) {
            m_socket->readAll();
        }
    }

    /// Returns the description of the most recent connect/send/receive error, if any.
    QString GetErrorDescription() override {
        return m_error_description;
    }

    /// Identifies this port as the Ethernet TCP/IP transport type.
    /// @return McMsgItfType::EthernetTCPIP
    const McMsgItfType type() const override {
        return McMsgItfType::EthernetTCPIP;
    }

    /// Returns the underlying QTcpSocket as a QIODevice; ownership stays with this port.
    QIODevice* ioDevice() const override {
        return static_cast<QIODevice*>(m_socket);
    }

    /// Disconnects the port, then aborts and deletes the socket synchronously on the
    /// calling (worker) thread.
    /// @note deliberately not deleteLater(): during teardown the worker event loop may
    /// already be stopped, which would leak the socket and leave the PLC connection half-open.
    void DestroyMsgPort() override {
        DisconnectFromPort();
        if (m_socket != nullptr) {
            // Delete synchronously on the socket's own (worker) thread. Using
            // deleteLater() here is unsafe during a phase teardown: the worker
            // event loop may be stopped before the deferred delete runs, which
            // leaks the socket and keeps the PLC connection half-open.
            m_socket->abort();
            delete m_socket;
            m_socket = nullptr;
        }
    }

private:
    QTcpSocket *m_socket;   ///< Owned TCP socket; null after DestroyMsgPort().
    McMsgEthernetTcpCfg m_config;   ///< IP/port/timeout configuration applied via SetConfig().
    int m_total_wait_time{0};   ///< Elapsed wait time (ms) accumulated across ReceiveMsg() calls toward m_config.m_responseTimeout.
};

} // namespace vc::device


Q_DECLARE_METATYPE(vc::device::McMsgEthernetTcpCfg)

#endif // MC_MSG_TCP_CLIENT_H
