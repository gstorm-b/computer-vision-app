#ifndef HY_SET_IF_FLAG_H
#define HY_SET_IF_FLAG_H

#include "robot/huayan/huayan_context.h"
#include "robot/huayan/huayan_command.h"

namespace rb {

class HySetIFCommand : public HuayanCommand {
public:
    HySetIFCommand(HyInternalFlag flag)
        : HuayanCommand(), m_flag(flag) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HySetIFCommand m_interface pointer null.";
            return;
        }

        m_interface->flag.setInternalFlag(m_flag);
        m_execute_state = ExecuteState::Executed;
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HySetIFCommand>(*this);
    }

private:
    HyInternalFlag m_flag;
};

}

#endif // HY_SET_IF_FLAG_H
