#ifndef HUAYAN_COMMAND_PORT_H
#define HUAYAN_COMMAND_PORT_H

#include "robot/robot_tcp_client.h"
// #include "rbot/huayan/huayantypes.h"
#include"robot/huayan/huayan_msg_return.h"

namespace rb {

class HuayanCommandPort : public TCPClient {
public:
    HuayanCommandPort();

    bool setWriteTimeout(int timeout);
    int getWriteTimeout();
    void SendMsg(QString cmd);
    QList<HuayanMsgReturn> ReceiveResponse();

private:
    // polling receive response buffer
    int m_write_timeout = 500;
    QByteArray m_receive_buffer;

    const QString end_comma = ",;";
};

}

#endif // HUAYAN_COMMAND_PORT_H
