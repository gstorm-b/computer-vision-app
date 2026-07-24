#ifndef HY_IF_COMMAND_H
#define HY_IF_COMMAND_H

#include <memory>
#include "robot/huayan/huayan_command.h"

namespace rb {

class HyIfStatement : public HuayanCommand {
public:
    HyIfStatement(QString device, QString address, bool state, bool inverse = false) :
        m_cmd_state(DhIfState::UnExecute),
        m_input_device(device),
        m_input_address(address),
        m_input_state(state),
        m_reverse_state(inverse),
        m_check_state(false) {

    }

    void setChildCommand(std::vector<std::shared_ptr<RbCommand>> &children) override {
        m_child_cmds.clear();
        for(int idx=0;idx<children.size();idx++) {
            m_child_cmds.push_back(children[idx]->clone());
        }

        for (int index=0;index<m_child_cmds.size();index++) {
            m_child_cmds[index]->resetCommand();
        }
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = DhIfState::UnExecute;
        m_check_state = false;
        if (m_child_cmds.size() > 0) {
            for (int index=0;index<m_child_cmds.size();index++) {
                m_child_cmds[index]->resetCommand();
            }
        }
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyIfStatement m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

        _start_point:
        switch (m_cmd_state) {
        case DhIfState::UnExecute:
            m_address_number = m_input_address.toInt(&is_ok);
            if (!is_ok) {
                m_cmd_state = DhIfState::Error;
                goto _start_point;
            }

            if (m_input_device == "EndDI") {
                m_cmd_state = DhIfState::CheckingEndDI;
            } else if (m_input_device == "EndDO") {
                m_cmd_state = DhIfState::CheckingEndDO;
            } else if (m_input_device == "BoxDI") {
                m_cmd_state = DhIfState::CheckingBoxDI;
            } else if (m_input_device == "BoxDO") {
                m_cmd_state = DhIfState::CheckingBoxDO;
            } else if (m_input_device == "PLC") {
                m_cmd_state = DhIfState::CheckingPLC;
            } else {
                m_cmd_state = DhIfState::Error;
            }
            goto _start_point;
            break;

        case DhIfState::CheckingEndDI:
            m_check_state = (m_input_state == m_interface->flag.get_enddi_state(m_address_number));
            m_check_state = m_reverse_state ? (!m_check_state) : m_check_state;
            qInfo() << "Checking ENDI, execute state:" << m_check_state;
            m_cmd_state = m_check_state ? DhIfState::StartFirstCommand : DhIfState::Finish;
            break;

        case DhIfState::CheckingEndDO:
            m_check_state = (m_input_state == m_interface->flag.get_enddo_state(m_address_number));
            m_check_state = m_reverse_state ? (!m_check_state) : m_check_state;
            qInfo() << "Checking ENDO, execute state:" << m_check_state;
            m_cmd_state = m_check_state ? DhIfState::StartFirstCommand : DhIfState::Finish;
            break;

        case DhIfState::CheckingBoxDI:
            m_check_state = (m_input_state == m_interface->flag.get_boxdi_state(m_address_number));
            m_check_state = m_reverse_state ? (!m_check_state) : m_check_state;
            qInfo() << "Checking BoxDI, execute state:" << m_check_state;
            m_cmd_state = m_check_state ? DhIfState::StartFirstCommand : DhIfState::Finish;
            break;

        case DhIfState::CheckingBoxDO:
            m_check_state = (m_input_state == m_interface->flag.get_boxdo_state(m_address_number));
            m_check_state = m_reverse_state ? (!m_check_state) : m_check_state;
            qInfo() << "Checking BoxDO, execute state:" << m_check_state;
            m_cmd_state = m_check_state ? DhIfState::StartFirstCommand : DhIfState::Finish;
            break;

        case DhIfState::CheckingPLC:
        {
            bool plc_value = false;
            m_interface->flag.plc_comunicator()->Fx3GetMDeviceValue(m_address_number, plc_value);
            m_check_state = (m_input_state == plc_value);
            m_check_state = m_reverse_state ? (!m_check_state) : m_check_state;
            qInfo() << "Checking PLC, execute state:" << m_check_state;
            m_cmd_state = m_check_state ? DhIfState::StartFirstCommand : DhIfState::Finish;
        }
        break;


        case DhIfState::StartFirstCommand:
            if (m_child_cmds.size() <= 0) {
                m_cmd_state = DhIfState::Finish;
                goto _start_point;
            }

            if (m_child_cmd != nullptr) {
                m_child_cmd.reset();
            }
            m_child_execute_index = 0;
            m_child_cmd = m_child_cmds.at(m_child_execute_index);
            m_child_cmd->setContext(m_interface);
            m_cmd_state = DhIfState::Executing;
            qInfo() << "IF statement start executing";
            // goto _start_point;
            break;

        case DhIfState::Executing:
            if (m_child_cmd == nullptr) {
                m_cmd_state = DhIfState::Error;
                goto _start_point;
            }

            if (m_child_cmd->isComplete()) {
                qInfo() << "IF statement child command complete";
                m_child_execute_index++;
                if (m_child_execute_index >= m_child_cmds.size()) {
                    m_cmd_state = DhIfState::Finish;
                    goto _start_point;
                } else {
                    m_child_cmd = m_child_cmds.at(m_child_execute_index);
                    m_child_cmd->setContext(m_interface);
                }
            } else {
                m_child_cmd->execute();
            }
            break;

        case DhIfState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case DhIfState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyIfStatement>(*this);
    }

private:
    enum DhIfState {
        UnExecute,
        CheckingEndDI,
        CheckingEndDO,
        CheckingBoxDI,
        CheckingBoxDO,
        CheckingPLC,
        StartFirstCommand,
        Executing,
        Finish,
        Error,
    };

private:
    DhIfState m_cmd_state = DhIfState::UnExecute;

    bool is_ok{false};

    QString m_input_device;
    QString m_input_address;
    bool m_input_state;
    int m_address_number;
    bool m_reverse_state;

    bool m_check_state;

    std::vector<std::shared_ptr<RbCommand>> m_child_cmds;
    std::shared_ptr<RbCommand> m_child_cmd;
    int m_child_execute_index;
};

}

#endif // HY_IF_COMMAND_H
