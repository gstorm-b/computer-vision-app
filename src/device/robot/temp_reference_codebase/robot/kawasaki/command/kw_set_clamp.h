#ifndef KW_SET_CLAMP_H
#define KW_SET_CLAMP_H


#include "kawasaki_command.h"
#include "robot/kawasaki/kawasaki_motion.h"

namespace rb {

class KwSetClamp : public KawasakiCommand {
public:
    KwSetClamp(int number, bool isClose) :
        KawasakiCommand(),
        m_clamp_number(number),
        m_is_clamp_close(isClose) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_stage = ClampExecuteStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawasaki SetClamp command error, m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

    _start_point:
        switch (m_stage) {
        case ClampExecuteStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_stage = ClampExecuteStage::SendCommand;
            goto _start_point;
            break;

        case ClampExecuteStage::SendCommand:
            send_set_clamp_command();
            break;

        case ClampExecuteStage::WaitResponse:
            wait_output_command_response();
            break;

        case ClampExecuteStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case ClampExecuteStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;

        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwSetClamp>(*this);
    }


private:
    enum ClampExecuteStage {
        UnExecute = 0,
        SendCommand,
        WaitResponse,
        Finish,
        Error
    };

    inline void send_set_clamp_command() {
        m_command_str = QString(KW_MOTION_SET_CLAMP) + "," +
                        QString::number(m_clamp_number, 10) + "," +
                        ((m_is_clamp_close) ? "1" : "0") + ",";

        m_interface->msg.send_msg(m_command_str);
        m_stage = ClampExecuteStage::WaitResponse;
    }

    inline void wait_output_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv move") {
                    qDebug() << "Kawsaki command SetClamp, found response, return success";
                    m_stage = ClampExecuteStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command SetClamp error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command SetClamp error, ressponse instance return null pointer.";
            }
            m_stage = ClampExecuteStage::Error;
        }
    }

private:
    ClampExecuteStage m_stage = ClampExecuteStage::UnExecute;
    int m_clamp_number;
    bool m_is_clamp_close;

    QString m_command_str;
};


}


#endif // KW_SET_CLAMP_H
