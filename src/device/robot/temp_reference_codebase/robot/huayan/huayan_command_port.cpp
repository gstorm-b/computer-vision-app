#include "huayan_command_port.h"

namespace rb {

HuayanCommandPort::HuayanCommandPort()
    : TCPClient() {

    m_receive_buffer.clear();
}

inline bool HuayanCommandPort::setWriteTimeout(int timeout) {
    if ((timeout >= 50) && (timeout <= 1000)) {
        m_write_timeout = timeout;
        return true;
    }
    return false;
}

inline int HuayanCommandPort::getWriteTimeout() {
    return m_write_timeout;
}

void HuayanCommandPort::SendMsg(QString cmd) {
    qInfo() << "Huayan robot Command port sent to robot:" << cmd;
    m_socket->write(cmd.toUtf8());
    m_socket->waitForBytesWritten(m_write_timeout);
}

QList<HuayanMsgReturn> HuayanCommandPort::ReceiveResponse() {
    /// wait to read data from tcp buffer
    m_socket->waitForReadyRead(1);
    // QByteArray buffer = m_socket->readAll();
    m_receive_buffer.append(m_socket->readAll());

    QList<HuayanMsgReturn> valid_responses;
    if (m_receive_buffer.isEmpty()) {
        return valid_responses;
    }

    /// get response command
    QString temp_buffer(m_receive_buffer);
    QStringList resultStrings = temp_buffer.split(end_comma);

    /// DbResponse response(resultStrings.first() + ';');
    for (int counter=0;counter<resultStrings.count();counter++) {
        if (resultStrings.at(counter).isEmpty()) {
            continue;
        }

        /// add separator and parse response command
        HuayanMsgReturn response(resultStrings.at(counter) + end_comma);
        if (response.isValid()) {
            // valid handle when receive a response
            valid_responses.push_back(response);
            qInfo() << "Huayan robot Command port recieved from robot:" << response.GetRawResponse();
        }
    }

    m_receive_buffer.clear();
    m_receive_buffer.append(resultStrings.last().toUtf8());

    return valid_responses;
}

}
