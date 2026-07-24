#ifndef KAWASAKI_MOTION_H
#define KAWASAKI_MOTION_H

#include "robot/robot_tcp_client.h"

#define KW_MOTION_MOVEJ                 "1"
#define KW_MOTION_MOVEL                 "2"
#define KW_MOTION_STOP                  "10"
#define KW_MOTION_SYNC                  "11"
#define KW_MOTION_SET_DO                "20"
#define KW_MOTION_SET_TCP               "21"
#define KW_MOTION_SET_CLAMP             "22"
#define KW_MOTION_SET_DO_WAIT           "23"
#define KW_MOTION_GET_TOOL              "30"
#define KW_MOTION_GET_FLANGE_POSE       "31"
#define KW_MOTION_CONL_RESET            "50"
#define KW_MOTION_DISCONNECT            "100"
#define KW_MOTION_VERSION               "101"

namespace rb {

class KawasakiMsgReturn {
public:
    KawasakiMsgReturn() :
        m_raw(""),
        m_msg(""),
        m_valid(false),
        m_has_params(false) {

    }

    KawasakiMsgReturn(QString response)
        : KawasakiMsgReturn() {
        if (response.isEmpty()) {
            return;
        }
        m_raw = response;
        this->ParseResponse();
    }

    bool isValid() {
        return m_valid;
    }

    QString GetRawResponse() {
        return m_raw;
    }

    QString GetMsg() {
        return m_msg;
    }

    bool hasParams() {
        return m_has_params;
    }

    QStringList GetParams() {
        return m_params;
    }

    QString GetCommandCode() {
        return m_cmd_code;
    }

private:
    void ParseResponse() {
        if (!m_raw.endsWith(",;;")) {
            m_valid = false;
            return;
        }

        m_msg = m_raw.mid(0, m_raw.size() - 3);
        m_params = m_msg.split(',');
        m_cmd_code = m_params.first();
        m_params.removeFirst();
        m_valid = true;
        m_has_params = !m_params.isEmpty();
    }

private:
    QString m_raw;
    QString m_msg;
    QString m_cmd_code;

    bool m_valid;

    bool m_has_params;
    QStringList m_params;

    // const QString end_comma = ",;;";
};

class KawasakiMotionPort : public TCPClient {
public:
    KawasakiMotionPort();

    bool setWriteTimeout(int timeout);
    int getWriteTimeout();
    void SendMsg(QString cmd);
    void clearBuffer();
    QList<KawasakiMsgReturn> ReceiveResponse();

private:
    // polling receive response buffer
    int m_write_timeout = 500;
    QByteArray m_receive_buffer;
};

}


#endif // KAWASAKI_MOTION_H
