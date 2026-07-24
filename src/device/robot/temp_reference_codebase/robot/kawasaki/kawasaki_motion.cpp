#include "kawasaki_motion.h"
#include "log_helper/log_wrapper.h"

#define LOG_NAME "[Kawasaki robot] motion port: "


namespace rb {

KawasakiMotionPort::KawasakiMotionPort()
    : TCPClient() {

    m_receive_buffer.clear();
}

inline bool KawasakiMotionPort::setWriteTimeout(int timeout) {
    if ((timeout >= 50) && (timeout <= 1000)) {
        m_write_timeout = timeout;
        return true;
    }
    return false;
}

inline int KawasakiMotionPort::getWriteTimeout() {
    return m_write_timeout;
}

void KawasakiMotionPort::SendMsg(QString cmd) {
    try {
        m_socket->write(cmd.toUtf8());
        m_socket->waitForBytesWritten(m_write_timeout);
        OLOG_INFO << "sent string:" << cmd;
    } catch (const std::exception& e){
        OLOG_CRITICAL << "send string" << cmd << ", fail:" << e.what();
        // m_socket->disconnectFromHost();
        m_port_state = TcState::Error;
    }
}

void KawasakiMotionPort::clearBuffer() {
    try {
        m_socket->waitForReadyRead(1);
        m_socket->readAll();
    } catch (const std::exception& e){
        OLOG_CRITICAL << "clear buffer, fail:" << e.what();
        // m_socket->disconnectFromHost();
        m_port_state = TcState::Error;
    }
}

QList<KawasakiMsgReturn> KawasakiMotionPort::ReceiveResponse() {
    /// wait to read data from tcp buffer
    m_socket->waitForReadyRead(1);
    try {
        // QByteArray buffer = m_socket->readAll();
        m_receive_buffer.append(m_socket->readAll());
    } catch (const std::exception& e){
        OLOG_CRITICAL << "try to read from buffer fail:" << e.what();
        m_port_state = TcState::Error;
        // m_socket->disconnectFromHost();
        return QList<KawasakiMsgReturn>();
    }

    QList<KawasakiMsgReturn> valid_responses;
    if (m_receive_buffer.isEmpty()) {
        return valid_responses;
    }

    /// get response command
    QString temp_buffer(m_receive_buffer);
    QStringList resultStrings = temp_buffer.split(",;;");

    /// DbResponse response(resultStrings.first() + ';');
    for (int counter=0;counter<resultStrings.count();counter++) {
        if (resultStrings.at(counter).isEmpty()) {
            continue;
        }

        /// add separator and parse response command
        KawasakiMsgReturn response(resultStrings.at(counter) + ",;;");
        if (response.isValid()) {
            // valid handle when receive a response
            valid_responses.push_back(response);
            OLOG_INFO << "found valid response:" << response.GetRawResponse();
        }
    }

    m_receive_buffer.clear();
    m_receive_buffer.append(resultStrings.last().toUtf8());

    return valid_responses;
}

}
