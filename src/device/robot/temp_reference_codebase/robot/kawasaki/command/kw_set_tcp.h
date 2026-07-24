#ifndef KW_SET_TCP_H
#define KW_SET_TCP_H

// #include "utils/chronocounter.h"
// #include "rbot/rbutils.h"
#include "kawasaki_command.h"
#include "robot/kawasaki/kawasaki_motion.h"

namespace rb {

class KwSetTCP : public KawasakiCommand {
public:
    KwSetTCP(CartesianPoint tcp_offset) :
        KawasakiCommand(),
        m_send_cmd(""),
        m_tcp_offset(tcp_offset) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_stage = SetTcpStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawsaki command SetTCP error, m_interface pointer null.";
            return;
        }

    _start_point:
        switch (m_stage) {
        case SetTcpStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_stage = SetTcpStage::SendMoveCommand;
            goto _start_point;
            break;

        case SetTcpStage::SendMoveCommand:
            send_move_command();
            break;

        case SetTcpStage::WaitMoveCommandReponse:
            wait_command_response();
            break;

        case SetTcpStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case SetTcpStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    inline void send_move_command() {
        m_send_cmd = QString(KW_MOTION_SET_TCP) + "," +
                     m_tcp_offset.toQString() + ",";

        m_interface->msg.send_msg(m_send_cmd);
        // set command state to wait for response
        m_stage = SetTcpStage::WaitMoveCommandReponse;
    }

    inline void wait_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv move") {
                    qDebug() << "Kawsaki command SetTCP, found response, return success";
                    m_stage = SetTcpStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command SetTCP error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command SetTCP error, ressponse instance return null pointer.";
            }
            m_stage = SetTcpStage::Error;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwSetTCP>(*this);
    }

private:
    enum SetTcpStage {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        Finish,
        Error
    };

private:
    SetTcpStage m_stage = SetTcpStage::UnExecute;
    QString m_send_cmd;

    CartesianPoint m_tcp_offset;
};

}


#endif // KW_SET_TCP_H
