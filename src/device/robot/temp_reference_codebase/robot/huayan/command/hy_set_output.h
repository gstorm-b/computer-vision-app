#ifndef HY_SET_OUTPUT_H
#define HY_SET_OUTPUT_H

#include "robot/huayan/huayan_command.h"

namespace rb {

class HySetOutput : public HuayanCommand {
public:
    HySetOutput(QString device, QString address, bool state, bool wait_stop = true) :
        HuayanCommand(),
        m_cmd_state(OutputExecuteState::UnExecute),
        m_wait_stop_move(wait_stop),
        m_output_device(device),
        m_output_address(address),
        m_output_state(state) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = OutputExecuteState::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HySetOutput m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

        _start_point:
        switch (m_cmd_state) {
        case OutputExecuteState::UnExecute:
            m_execute_state = ExecuteState::Executing;
            if (m_wait_stop_move) {
                m_cmd_state = OutputExecuteState::WaitStopMoving;
            } else {
                m_cmd_state = OutputExecuteState::GetDeviceName;
            }
            goto _start_point;
            break;

        case OutputExecuteState::WaitStopMoving:
            // qInfo() << "Wait robot stop moving";
            if (m_interface->flag.is_robot_moving()) {
                return;
            }
            m_cmd_state = OutputExecuteState::GetDeviceName;
            goto _start_point;
            break;

        case OutputExecuteState::GetDeviceName:
            get_command_name();
            goto _start_point;
            break;

        case OutputExecuteState::SendOutputCommand:
            send_output_command();
            break;

        case OutputExecuteState::WaitOutputReponse:
            wait_output_command_response();
            break;

        case OutputExecuteState::PLCOutput:
            set_plc_output();
            break;

        case OutputExecuteState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case OutputExecuteState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HySetOutput>(*this);
    }

private:
    enum OutputExecuteState {
        UnExecute = 0,
        WaitStopMoving,
        GetDeviceName,
        SendOutputCommand,
        WaitOutputReponse,
        PLCOutput,
        Finish,
        Error
    };

    inline void get_command_name() {
        if (m_output_device == "EndDO") {
            m_command_header = "SetEndDO";
        } else if (m_output_device == "BoxDO") {
            m_command_header = "SetBoxDO";
        } else if (m_output_device == "PLC") {
            m_cmd_state = OutputExecuteState::PLCOutput;
            return;
        }  else {
            m_cmd_state = OutputExecuteState::Finish;
            return;
        }

        m_cmd_state = OutputExecuteState::SendOutputCommand;
    }

    inline void send_output_command() {
        m_command_str = m_command_header + "," +
                        m_interface->flag.get_robot_id_str() + "," +
                        m_output_address + "," +
                        ((m_output_state) ? "1" : "0") + ",;";

        m_interface->msg.send_msg(m_command_str);
        // set command state to wait for response
        m_cmd_state = OutputExecuteState::WaitOutputReponse;
    }

    inline void wait_output_command_response() {
        if (m_interface->msg.is_response_received()) {
            HuayanMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandHeader() == m_command_header) {
                    if (response->isCommandOK()) {
                        qDebug() << "Huayan robot controller [HySetOutput]: found response, return success";
                        // set command state wait finished movement
                        m_cmd_state = OutputExecuteState::Finish;
                        return;
                    } else {
                        qDebug() << "Huayan robot controller [HySetOutput]: found response, return error"
                                 << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;

                        /// set command state to error
                    }
                } else {
                    qDebug() << "Huayan robot controller [HySetOutput]: wrong command response header";
                }
            } else {
                qCritical() << "HySetOutput fail, ressponse instance return null pointer.";
            }
            m_cmd_state = OutputExecuteState::Error;
        }
    }

    inline void set_plc_output() {
        Fx3Communicator *plc = m_interface->flag.plc_comunicator();
        bool is_number = false;
        int dv_address = m_output_address.toInt(&is_number, 10);
        if (!is_number) {
            m_cmd_state = OutputExecuteState::Error;
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }
        plc->Fx3ModifyDevice_M(dv_address, ((m_output_state) ? 1 : 0));
        m_cmd_state = OutputExecuteState::Finish;
        m_execute_state = ExecuteState::Executed;
    }

private:
    OutputExecuteState m_cmd_state = OutputExecuteState::UnExecute;

    bool m_wait_stop_move;

    QString m_output_device;
    QString m_output_address;
    bool m_output_state;

    QString m_command_header;
    QString m_command_str;
};

}

#endif // HY_SET_OUTPUT_H
