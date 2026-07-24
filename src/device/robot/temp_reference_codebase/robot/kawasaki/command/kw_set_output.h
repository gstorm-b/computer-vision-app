#ifndef KW_SET_OUTPUT_H
#define KW_SET_OUTPUT_H

#include "kawasaki_command.h"
#include "robot/kawasaki/kawasaki_motion.h"


namespace rb {

class KwSetOutput : public KawasakiCommand {
public:
    KwSetOutput(QString device, QString address, bool state, bool wait_stop = true) :
        KawasakiCommand(),
        m_stage(OutputExecuteStage::UnExecute),
        m_wait_stop_move(wait_stop),
        m_output_device(device),
        m_output_address(address),
        m_output_state(state) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_stage = OutputExecuteStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawasaki SetOutput command error, m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

    _start_point:
        switch (m_stage) {
        case OutputExecuteStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            if (m_wait_stop_move) {
                m_stage = OutputExecuteStage::WaitStopMoving;
            } else {
                m_stage = OutputExecuteStage::GetDeviceName;
            }
            goto _start_point;
            break;

        case OutputExecuteStage::WaitStopMoving:
            // if (m_interface->flag.is_robot_moving()) {
            //     return;
            // }
            m_stage = OutputExecuteStage::GetDeviceName;
            goto _start_point;
            break;

        case OutputExecuteStage::GetDeviceName:
            if (m_output_device == "ExternalOutput") {
                m_stage = OutputExecuteStage::SendOutputCommand;
            } else if (m_output_device == "InternalSignal") {
                m_stage = OutputExecuteStage::SendOutputCommand;
            } else if (m_output_device == "PLC") {
                m_stage = OutputExecuteStage::PLCOutput;
            }  else {
                m_stage = OutputExecuteStage::Finish;
            }

            goto _start_point;
            break;

        case OutputExecuteStage::SendOutputCommand:
            send_output_command();
            break;

        case OutputExecuteStage::WaitOutputReponse:
            wait_output_command_response();
            break;

        case OutputExecuteStage::PLCOutput:
            set_plc_output();
            break;

        case OutputExecuteStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case OutputExecuteStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwSetOutput>(*this);
    }

private:
    enum OutputExecuteStage {
        UnExecute = 0,
        WaitStopMoving,
        GetDeviceName,
        SendOutputCommand,
        WaitOutputReponse,
        PLCOutput,
        Finish,
        Error
    };

    inline void send_output_command() {
        if (m_wait_stop_move) {
            m_command_str = QString(KW_MOTION_SET_DO_WAIT) + "," +
                            m_output_address + "," +
                            ((m_output_state) ? "1" : "0") + ",";
        } else {
            m_command_str = QString(KW_MOTION_SET_DO) + "," +
                            m_output_address + "," +
                            ((m_output_state) ? "1" : "0") + ",";
        }

        m_interface->msg.send_msg(m_command_str);
        // set command state to wait for response
        m_stage = OutputExecuteStage::WaitOutputReponse;
    }

    inline void wait_output_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv set do") {
                    qDebug() << "Kawsaki command SetOutput, found response, return success";
                    m_stage = OutputExecuteStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command SetOutput error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command SetOutput error, ressponse instance return null pointer.";
            }
            m_stage = OutputExecuteStage::Error;
        }
    }

    inline void set_plc_output() {
        Fx3Communicator *plc = m_interface->flag.plc_comunicator();
        bool is_number = false;
        int dv_address = m_output_address.toInt(&is_number, 10);
        if (!is_number) {
            m_stage = OutputExecuteStage::Error;
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }
        plc->Fx3ModifyDevice_M(dv_address, ((m_output_state) ? 1 : 0));
        m_stage = OutputExecuteStage::Finish;
        m_execute_state = ExecuteState::Executed;
    }


private:
    OutputExecuteStage m_stage = OutputExecuteStage::UnExecute;

    bool m_wait_stop_move;

    QString m_output_device;
    QString m_output_address;
    bool m_output_state;

    QString m_command_header;
    QString m_command_str;
};

}

#endif // KW_SET_OUTPUT_H
