#ifndef HY_DHGRIPPER_H
#define HY_DHGRIPPER_H

#include <memory>
#include "robot/huayan/huayan_command.h"
#include "robot/huayan/command/hy_set_output.h"
// #include "rbot/huayan/command/hycmdfactory.h"

namespace rb {

class HyDhGripperControl : public HuayanCommand {
public:
    HyDhGripperControl(bool is_close) :
        HuayanCommand(),
        m_cmd_state(DhExecuteState::UnExecute),
        m_is_close(is_close) {

        convert_output_bits();
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
        m_cmd_state = DhExecuteState::UnExecute;
        if (m_child_cmds.size() > 0) {
            for (int index=0;index<m_child_cmds.size();index++) {
                m_child_cmds[index]->resetCommand();
            }
        }
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyDhGripperControl m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

         _start_point:
        switch (m_cmd_state) {
        case DhExecuteState::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_cmd_state = DhExecuteState::CheckBit1;
            goto _start_point;
            break;

        case DhExecuteState::CheckBit1:
            // qInfo() << "DH Gripper check bit 1 " << out_state_1 << (m_interface->flag.get_enddo_state(out_endio_1));
            if (out_state_1 == ((m_interface->flag.get_enddo_state(out_endio_1)) ? 1 : 0)) {
                m_cmd_state = DhExecuteState::CheckBit2;
                goto _start_point;
            }

            m_sub_cmd = std::make_shared<HySetOutput>("EndDO",
                                                      QString::number(out_endio_1, 10),
                                                      ((out_state_1==0) ? false : true),
                                                      true);
            m_sub_cmd->setContext(m_interface);
            m_cmd_state = DhExecuteState::SetBit1;
            goto _start_point;
            break;

        case DhExecuteState::SetBit1:
            m_sub_cmd->execute();
            if (m_sub_cmd->isComplete()) {
                m_sub_cmd.reset();
                // m_wait = true;
                m_cmd_state = DhExecuteState::CheckBit2;
            }
            break;

        case DhExecuteState::CheckBit2:
            // qInfo() << "DH Gripper check bit 2 " << out_state_2 << (m_interface->flag.get_enddo_state(out_endio_2));
            if (out_state_2 == ((m_interface->flag.get_enddo_state(out_endio_2)) ? 1 : 0)) {
                m_wait = ((m_interface->flag.get_enddi_state(in_endio_1) == 0) &&
                          (m_interface->flag.get_enddi_state(in_endio_2) == 0));
                m_cmd_state = m_wait ? DhExecuteState::WaitState : DhExecuteState::Finish;
                // qInfo() << "Add state check bit 2, is into state wait state" << (m_cmd_state == WaitState);
                goto _start_point;
            } else {
                m_sub_cmd = std::make_shared<HySetOutput>("EndDO",
                                                          QString::number(out_endio_2, 10),
                                                          ((out_state_2==0) ? false : true),
                                                          true);
                m_sub_cmd->setContext(m_interface);
                m_cmd_state = DhExecuteState::SetBit2;
                goto _start_point;
            }
            break;

        case DhExecuteState::SetBit2:
            m_sub_cmd->execute();
            if (m_sub_cmd->isComplete()) {
                m_sub_cmd.reset();
                m_cmd_state = DhExecuteState::WaitGripperMove;
            }
            break;

        case DhExecuteState::WaitGripperMove:
            if ( (m_interface->flag.get_enddi_state(in_endio_1) == 0) &&
                (m_interface->flag.get_enddi_state(in_endio_2) == 0) ) {
                // qInfo() << "To wait state";
                m_cmd_state = DhExecuteState::WaitState;
            }
            break;

        case DhExecuteState::WaitState:
            // if ( (m_interface->flag.get_enddi_state(in_endio_1) == 1) ||
            //     (m_interface->flag.get_enddi_state(in_endio_2) == 1) ) {
            //     m_cmd_state = DhExecuteState::Finish;
            //     goto _start_point;
            // }

            // gripper open, reach at position and not hold object
            if ((!m_is_close) && (m_interface->flag.get_enddi_state(in_endio_1) == 1)) {
                m_cmd_state = DhExecuteState::Finish;
                goto _start_point;

            // gripper close, and detect object
            } else if ((m_is_close) && (m_interface->flag.get_enddi_state(in_endio_2) == 1)) {
                m_cmd_state = DhExecuteState::Finish;
                goto _start_point;

            // gripper close, no object detected or object falling
            } else if ((m_is_close) && (m_interface->flag.get_enddi_state(in_endio_1) == 1)) {
                // qInfo() << "Grip failed";

                if (m_child_cmds.size() <= 0) {
                    m_cmd_state = DhExecuteState::Finish;
                    goto _start_point;
                }

                if (m_child_cmd != nullptr) {
                    m_child_cmd.reset();
                }

                // qInfo() << "Grip fail, start action";
                m_child_execute_index = 0;
                m_child_cmd = m_child_cmds.at(m_child_execute_index);
                m_child_cmd->setContext(m_interface);
                m_cmd_state = DhExecuteState::GripFail;
            }

            break;

        case DhExecuteState::GripFail:
            if (m_child_cmd == nullptr) {
                m_cmd_state = DhExecuteState::Finish;
                goto _start_point;
            }

            if (m_child_cmd->isComplete()) {
                m_child_execute_index++;
                if (m_child_execute_index >= m_child_cmds.size()) {
                    m_cmd_state = DhExecuteState::Finish;
                    goto _start_point;
                } else {
                    m_child_cmd = m_child_cmds.at(m_child_execute_index);
                    m_child_cmd->setContext(m_interface);
                }
            } else {
                m_child_cmd->execute();
            }
            break;

        case DhExecuteState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case DhExecuteState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyDhGripperControl>(*this);
    }


private:
    enum DhExecuteState {
        UnExecute,
        CheckBit1,
        SetBit1,
        CheckBit2,
        SetBit2,
        WaitGripperMove,
        WaitState,
        GripFail,
        Finish,
        Error,
    };

    inline void convert_output_bits() {
        if (m_is_close) {
            m_num_state = close_state_number;
        } else {
            m_num_state = open_state_number;
        }

        out_state_1 = m_num_state & 0x01;
        out_state_2 = (m_num_state >> 1) & 0x01;
    }

public:
    static int close_state_number;
    static int open_state_number;
    static int out_endio_1;
    static int out_endio_2;
    static int in_endio_1;
    static int in_endio_2;

private:
    DhExecuteState m_cmd_state = DhExecuteState::UnExecute;
    bool m_is_close;
    int m_num_state;
    int out_state_1;
    int out_state_2;

    bool m_wait;
    std::shared_ptr<HuayanCommand> m_sub_cmd;

    std::vector<std::shared_ptr<RbCommand>> m_child_cmds;
    std::shared_ptr<RbCommand> m_child_cmd;
    int m_child_execute_index;
};

}




#endif // HY_DHGRIPPER_H
