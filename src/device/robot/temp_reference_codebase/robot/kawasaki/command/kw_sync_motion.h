#ifndef KW_SYNC_MOTION_H
#define KW_SYNC_MOTION_H

#include "robot/kawasaki/command/kawasaki_command.h"


namespace rb {

class KwSyncMotion : public KawasakiCommand {
public:
    KwSyncMotion() : KawasakiCommand() {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_stage = SyncExecuteStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawasaki SetClamp command error, m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

    _start_point:
        switch (m_stage) {
        case SyncExecuteStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_stage = SyncExecuteStage::SendCommand;
            goto _start_point;
            break;

        case SyncExecuteStage::SendCommand:
            send_set_clamp_command();
            break;

        case SyncExecuteStage::WaitResponse:
            wait_output_command_response();
            break;

        case SyncExecuteStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case SyncExecuteStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwSyncMotion>(*this);
    }


private:
    enum SyncExecuteStage {
        UnExecute = 0,
        SendCommand,
        WaitResponse,
        Finish,
        Error
    };

    inline void send_set_clamp_command() {
        m_command_str = QString(KW_MOTION_SYNC) + ",";
        m_interface->msg.send_msg(m_command_str);
        m_stage = SyncExecuteStage::WaitResponse;
    }

    inline void wait_output_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv motion finished") {
                    qDebug() << "Kawsaki command SyncMotion, found response, return success";
                    m_stage = SyncExecuteStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command SyncMotion error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command SyncMotion error, ressponse instance return null pointer.";
            }
            m_stage = SyncExecuteStage::Error;
        }
    }

private:
    SyncExecuteStage m_stage = SyncExecuteStage::UnExecute;
    QString m_command_str;
};

}

#endif // KW_SYNC_MOTION_H
