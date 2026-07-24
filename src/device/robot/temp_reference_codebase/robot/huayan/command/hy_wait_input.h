#ifndef HY_WAIT_INPUT_H
#define HY_WAIT_INPUT_H

#include "robot/huayan/huayan_command.h"

namespace rb {

class HyWaitInput : public HuayanCommand {
public:
    HyWaitInput(QString device, QString address, bool state) :
        HuayanCommand(),
        m_cmd_state(WaitInputState::UnExecute),
        m_input_device(device),
        m_input_address(address),
        m_input_state(state) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = WaitInputState::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyWaitInput m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

        _start_point:
        switch (m_cmd_state) {
        case WaitInputState::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_cmd_state = WaitInputState::GetWait;
            goto _start_point;
            break;

        case WaitInputState::GetWait:
            m_address_number = m_input_address.toInt(&is_ok);
            if (!is_ok) {
                m_cmd_state = WaitInputState::Error;
                goto _start_point;
            }

            if (m_input_device == "EndDI") {
               m_cmd_state = WaitInputState::WaitingEndDI;
            } else if (m_input_device == "EndDO") {
                m_cmd_state = WaitInputState::WaitingEndDO;
            } else if (m_input_device == "BoxDI") {
                m_cmd_state = WaitInputState::WaitingBoxDI;
            } else if (m_input_device == "BoxDO") {
                m_cmd_state = WaitInputState::WaitingBoxDO;
            } else if (m_input_device == "PLC") {
                m_cmd_state = WaitInputState::WaitingPLC;
            } else {
                m_cmd_state = WaitInputState::Error;
            }
            goto _start_point;
            break;

        case WaitInputState::WaitingEndDI:
            if (m_input_state == m_interface->flag.get_enddi_state(m_address_number)) {
                qDebug() << "[Huayan Robot] Wait input command done, EndDI" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputState::WaitingEndDO:
            if (m_input_state == m_interface->flag.get_enddo_state(m_address_number)) {
                qDebug() << "[Huayan Robot] Wait input command done, EndDO" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputState::WaitingBoxDI:
            if (m_input_state == m_interface->flag.get_boxdi_state(m_address_number)) {
                qDebug() << "[Huayan Robot] Wait input command done, BoxDI" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputState::WaitingBoxDO:
            if (m_input_state == m_interface->flag.get_boxdo_state(m_address_number)) {
                qDebug() << "[Huayan Robot] Wait input command done, BoxDO" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputState::WaitingPLC:
        {
            bool plc_value = false;
            m_interface->flag.plc_comunicator()->Fx3GetMDeviceValue(m_address_number, plc_value);
            if (plc_value == m_input_state) {
                m_execute_state = ExecuteState::Executed;
            }
        }
            break;

        case WaitInputState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case WaitInputState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyWaitInput>(*this);
    }

private:
    enum WaitInputState {
        UnExecute = 0,
        GetWait,
        WaitingEndDI,
        WaitingEndDO,
        WaitingBoxDI,
        WaitingBoxDO,
        WaitingPLC,
        Finish,
        Error
    };


private:
    WaitInputState m_cmd_state = WaitInputState::UnExecute;

    bool is_ok{false};

    QString m_input_device;
    QString m_input_address;
    bool m_input_state;
    int m_address_number;
};


}

#endif // HY_WAIT_INPUT_H
