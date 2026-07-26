#ifndef MC_MSG_INTERFACE_H
#define MC_MSG_INTERFACE_H

#include <QByteArray>
#include <QIODevice>
#include <QJsonObject>
#include <QObject>
#include <QMetaType>
#include <qtmetamacros.h>
#include "device/plc/mc_define.h"


#include "core/qgadget_macro.h"

using namespace vc::device::mc;

/// Device-family classes for the MC (Mitsubishi) protocol PLC integration.
namespace vc::device {

/// Abstract base for a message-interface (transport) configuration: the common
/// connect/write/response timeout Q_GADGET properties shared by every McMsgInterface
/// transport, plus the JSON (de)serialization and meta-object hooks each transport-specific
/// subclass (e.g. McMsgEthernetTcpCfg) must implement.
class McMsgItfConfig {
    Q_GADGET

    G_PROPERTY_NUMBER_READWRITE(int, connectTimeout, 1000, 10000, "Connect timeout")   ///< Q_GADGET property backed by m_connectTimeout; max wait to establish the connection.
    G_PROPERTY_NUMBER_READWRITE(int, responseTimeout, 1000, 10000, "Response timeout")   ///< Q_GADGET property backed by m_responseTimeout; max wait for a PLC response.
    // GADGET_PROPERTY_NUMBER_READWRITE(int, writeTimeout, "1000", "10000", "Write timeout")

public:
    /// Virtual destructor so concrete transport configs can be destroyed through a McMsgItfConfig pointer.
    virtual ~McMsgItfConfig() = default;

    /// Returns this gadget's static meta-object, used by property-browser/serialization
    /// code to enumerate its Q_PROPERTY / Q_CLASSINFO entries.
    virtual const QMetaObject &getMetaObject() const = 0;

    /// Identifies the concrete transport (e.g. EthernetTCPIP) this config is for.
    virtual McMsgItfType type() const = 0;

    /// Sets the connect timeout (ms) if `timeout` is in (1, 10000].
    /// @return true if the value was accepted and applied, false otherwise (unchanged)
    virtual const bool SetConnectTimeout(int timeout) {
        if ((timeout > 1) && (timeout <= 10000)) {
            m_connectTimeout = timeout;
            return true;
        }
        return false;
    }

    /// Sets the write timeout (ms) if `timeout` is in (1, 5000).
    /// @return true if the value was accepted and applied, false otherwise (unchanged)
    virtual const bool SetWriteTimeout(int timeout) {
        if ((timeout > 1) && (timeout < 5000)) {
            m_writeTimeout = timeout;
            return true;
        }
        return false;
    }

    /// Sets the response-wait timeout (ms) if `timeout` is in (1, 5000).
    /// @return true if the value was accepted and applied, false otherwise (unchanged)
    virtual const bool SetWaitResponseTimeout(int timeout) {
        if ((timeout > 1) && (timeout < 5000)) {
            m_responseTimeout = timeout;
            return true;
        }
        return false;
    }

    /// Serializes the connect/write/response timeouts to JSON.
    /// @return a JSON object with "connectTimeout", "writeTimeout", "responseTimeout" keys
    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["connectTimeout"] = m_connectTimeout;
        obj["writeTimeout"] = m_writeTimeout;
        obj["responseTimeout"] = m_responseTimeout;
        return obj;
    }

    /// Populates the connect/write/response timeouts from JSON written by toJson(),
    /// defaulting each to 2000/1000/1000 ms respectively if its key is missing.
    /// @return always true
    virtual bool fromJson(const QJsonObject &obj) {
        m_connectTimeout = obj["connectTimeout"].toInt(2000);
        m_writeTimeout = obj["writeTimeout"].toInt(1000);
        m_responseTimeout = obj["responseTimeout"].toInt(1000);
        return true;
    }

public:
    int m_connectTimeout{2000};   ///< Connect timeout in ms; backing store for the connectTimeout property.
    int m_writeTimeout{1000};   ///< Write timeout in ms; not currently exposed as a Q_GADGET property.
    int m_responseTimeout{1000};   ///< Response-wait timeout in ms; backing store for the responseTimeout property.
};


/// Abstract transport (message interface) to a PLC: connect/disconnect the port and
/// send/receive raw byte buffers over it. Concrete subclasses (e.g. McEthernetTcpPort)
/// implement the actual I/O for a specific McMsgItfType.
class McMsgInterface {
public:
    /// Connection lifecycle state of the underlying port.
    enum MsgIfState {
        NotInit = 0,   ///< Port not yet initialized/connected.
        NoConnection,  ///< Port initialized but currently disconnected.
        Connected,     ///< Port is connected and ready for I/O.
        ConnectFail,   ///< The most recent ConnectToPort() attempt failed.
        Error          ///< Port is in an unrecoverable error state.
    };

    /// Outcome of the most recent SendMsg()/ReceiveMsg() call.
    enum MsgErrorState {
        NoError = 0,      ///< The last send/receive completed without error.
        WriteTimeout,     ///< SendMsg() timed out waiting for the bytes to be written.
        ResponseTimeout,  ///< ReceiveMsg() timed out waiting for a response.
        BufferEmpty,      ///< SendMsg() was called with an empty buffer, or ReceiveMsg() has no bytes yet.
        ErrorOcurred      ///< An exception/other error occurred during the I/O call.
    };

    /// Virtual destructor so concrete transports can be destroyed through a McMsgInterface pointer.
    virtual ~McMsgInterface() = default;

    /// Applies a transport-specific configuration (e.g. IP/port for TCP).
    /// @return false if `cfg` is null or of the wrong McMsgItfType for this transport
    virtual bool SetConfig(McMsgItfConfig *cfg) = 0;

    /// Checks whether the underlying connection is currently usable.
    virtual const bool ConnectionCheck() = 0;

    /// Opens the port/connection using the last applied config.
    /// @return the resulting MsgIfState (Connected on success, ConnectFail otherwise)
    virtual MsgIfState ConnectToPort() = 0;

    /// Closes the port/connection if open.
    /// @return the resulting MsgIfState (NoConnection)
    virtual MsgIfState DisconnectFromPort() = 0;

    /// Writes `buffer` to the port.
    /// @return NoError on success, BufferEmpty/WriteTimeout/ErrorOcurred on failure
    virtual const MsgErrorState SendMsg(QByteArray &buffer) = 0;

    /// Reads any available bytes into `buffer`, waiting up to `wait_buffer` ms for data to arrive.
    /// @return NoError if bytes were appended, BufferEmpty/ResponseTimeout otherwise
    virtual const MsgErrorState ReceiveMsg(QByteArray &buffer, int wait_buffer = 5) = 0;

    /// Discards any buffered/unread incoming bytes on the port.
    virtual void clearBuffer() = 0;

    /// Returns the last recorded port connection state (see MsgIfState).
    virtual const MsgIfState currentState() {
        return m_port_state;
    }

    /// Returns the last recorded send/receive error state (see MsgErrorState).
    virtual const MsgErrorState ErrorState() {
        return m_error_state;
    }

    /// Returns the human-readable description of the most recent error, if any.
    virtual QString GetErrorDescription() {
        return m_error_description;
    }

    /// Identifies the concrete transport type (e.g. EthernetTCPIP) implemented by this port.
    virtual const McMsgItfType type() const = 0;

    /// Returns the underlying QIODevice (e.g. the QTcpSocket) so callers can hook its signals directly.
    /// @return the device; ownership stays with this McMsgInterface
    virtual QIODevice* ioDevice() const = 0;

    /// Disconnects the port and releases/destroys the underlying transport object.
    virtual void DestroyMsgPort() = 0;

protected:
    MsgIfState m_port_state;   ///< Current port connection state; set by ConnectToPort()/DisconnectFromPort().
    MsgErrorState m_error_state;   ///< Current send/receive error state; set by SendMsg()/ReceiveMsg().
    QString m_error_description;   ///< Human-readable description of the most recent error.
};

}

Q_DECLARE_METATYPE(vc::device::McMsgItfConfig)

#endif // MC_MSG_INTERFACE_H
