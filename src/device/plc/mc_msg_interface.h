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

namespace vc::device {

class McMsgItfConfig {
    Q_GADGET

    G_PROPERTY_NUMBER_READWRITE(int, connectTimeout, 1000, 10000, "Connect timeout")
    G_PROPERTY_NUMBER_READWRITE(int, responseTimeout, 1000, 10000, "Response timeout")
    // GADGET_PROPERTY_NUMBER_READWRITE(int, writeTimeout, "1000", "10000", "Write timeout")

public:
    virtual ~McMsgItfConfig() = default;

    virtual const QMetaObject &getMetaObject() const = 0;
    virtual McMsgItfType type() const = 0;

    virtual const bool SetConnectTimeout(int timeout) {
        if ((timeout > 1) && (timeout <= 10000)) {
            m_connectTimeout = timeout;
            return true;
        }
        return false;
    }

    virtual const bool SetWriteTimeout(int timeout) {
        if ((timeout > 1) && (timeout < 5000)) {
            m_writeTimeout = timeout;
            return true;
        }
        return false;
    }

    virtual const bool SetWaitResponseTimeout(int timeout) {
        if ((timeout > 1) && (timeout < 5000)) {
            m_responseTimeout = timeout;
            return true;
        }
        return false;
    }

    virtual QJsonObject toJson() const {
        QJsonObject obj;
        obj["connectTimeout"] = m_connectTimeout;
        obj["writeTimeout"] = m_writeTimeout;
        obj["responseTimeout"] = m_responseTimeout;
        return obj;
    }

    virtual bool fromJson(const QJsonObject &obj) {
        m_connectTimeout = obj["connectTimeout"].toInt(2000);
        m_writeTimeout = obj["writeTimeout"].toInt(1000);
        m_responseTimeout = obj["responseTimeout"].toInt(1000);
        return true;
    }

public:
    int m_connectTimeout{2000};
    int m_writeTimeout{1000};
    int m_responseTimeout{1000};
};


class McMsgInterface {
public:
    enum MsgIfState {
        NotInit = 0,
        NoConnection,
        Connected,
        ConnectFail,
        Error
    };

    enum MsgErrorState {
        NoError = 0,
        WriteTimeout,
        ResponseTimeout,
        BufferEmpty,
        ErrorOcurred
    };

    virtual ~McMsgInterface() = default;

    virtual bool SetConfig(McMsgItfConfig *cfg) = 0;

    virtual const bool ConnectionCheck() = 0;
    virtual MsgIfState ConnectToPort() = 0;
    virtual MsgIfState DisconnectFromPort() = 0;

    virtual const MsgErrorState SendMsg(QByteArray &buffer) = 0;
    virtual const MsgErrorState ReceiveMsg(QByteArray &buffer, int wait_buffer = 5) = 0;
    virtual void clearBuffer() = 0;

    virtual const MsgIfState currentState() {
        return m_port_state;
    }

    virtual const MsgErrorState ErrorState() {
        return m_error_state;
    }

    virtual QString GetErrorDescription() {
        return m_error_description;
    }

    virtual const McMsgItfType type() const = 0;

    virtual QIODevice* ioDevice() const = 0;

    virtual void DestroyMsgPort() = 0;

protected:
    MsgIfState m_port_state;
    MsgErrorState m_error_state;
    QString m_error_description;
};

}

Q_DECLARE_METATYPE(vc::device::McMsgItfConfig)

#endif // MC_MSG_INTERFACE_H
