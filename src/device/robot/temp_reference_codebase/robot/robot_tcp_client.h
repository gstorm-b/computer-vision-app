#ifndef ROBOT_TCP_CLIENT_H
#define ROBOT_TCP_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>

#define CLIENT_NORMAL_CONNECT_TIMEOUT  1000

class TCPClient: public QObject {
public:
    enum TcState {
        NotInit = 0,
        NotConnect,
        Connected,
        ConnectFail,
        Error
    };

    TCPClient();
    ~TCPClient();

    /**
     * @brief SetClientConenctTimeout
     * @param timeout: time out in range 500 - 10000ms
     * @return true time-out valid
     */
    const bool SetClientConenctTimeout(int timeout);

    /**
     * @brief GetClientConnectTimeout
     * @return time out value
     */
    const int GetClientConnectTimeout();

    /**
     * @brief ConnectToPort: connect to TCP server port
     * @param address: server address
     * @param port: port number
     * @return type TcState, connection state
     */
    TcState ConnectToPort(QString address, quint16 port);
    TcState DisconnectFromPort();

    inline TcState state() { return m_port_state; };

public slots:
    void DestroyClientPort();

protected:
    TcState m_port_state;
    QTcpSocket *m_socket = nullptr;

private:
    int m_connect_timeout = 1000;
};

#endif // ROBOT_TCP_CLIENT_H
