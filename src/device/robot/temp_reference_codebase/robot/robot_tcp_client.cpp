#include "robot_tcp_client.h"

TCPClient::TCPClient() :
    m_port_state(TCPClient::TcState::NotConnect),
    m_connect_timeout(CLIENT_NORMAL_CONNECT_TIMEOUT) {

    /// init TCP Socket
    m_socket = new QTcpSocket;
}

TCPClient::~TCPClient() {
    DisconnectFromPort();
    m_socket->deleteLater();
}

const bool TCPClient::SetClientConenctTimeout(int timeout) {
    if ((timeout >= 500) && ((timeout <= 10000))) {
        m_connect_timeout = timeout;
        return true;
    }
    return false;
}

const int TCPClient::GetClientConnectTimeout() {
    return m_connect_timeout;
}

TCPClient::TcState TCPClient::ConnectToPort(QString address, quint16 port) {
    if (m_socket == nullptr) {
        /// qDebug() << "TCP Socket not initialzed";
        m_port_state = TCPClient::TcState::NotInit;
        return m_port_state;
    }

    // qInfo() << "Start Connect to host";
    m_socket->connectToHost(QHostAddress(address), port);
    if (m_socket->waitForConnected(m_connect_timeout)) {
        m_port_state = TCPClient::TcState::Connected;
    } else {
        m_port_state = TCPClient::TcState::ConnectFail;
    }

    return m_port_state;
}

TCPClient::TcState TCPClient::DisconnectFromPort() {
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
    m_port_state = TCPClient::TcState::NotConnect;
    return m_port_state;
}

void TCPClient::DestroyClientPort() {
    DisconnectFromPort();
    m_socket->deleteLater();
    this->deleteLater();
}

