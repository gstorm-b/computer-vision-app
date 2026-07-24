#ifndef HY_JUMP_COMMAND_H
#define HY_JUMP_COMMAND_H

#include "robot/huayan/huayan_context.h"
#include "robot/huayan/huayan_command.h"

namespace rb {

class HyJumpLineCommand : public HuayanCommand {
public:

    /**
     * @brief HyJumpLineCommand
     * @param line: line numer, 0 is start cycle line number, -1 is end cycle line number
     */
    HyJumpLineCommand(int line)
        : HuayanCommand(),
        m_line_number(line) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HySetIFCommand m_interface pointer null.";
            return;
        }

        m_interface->flag.setJumpToLine(m_line_number);
        m_interface->flag.setInternalFlag(HyInternalFlag::HyfJumpToLine);
        m_execute_state = ExecuteState::Executed;
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyJumpLineCommand>(*this);
    }

private:
    int m_line_number;
};

}

#endif // HY_JUMP_COMMAND_H
