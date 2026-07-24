#ifndef KW_SET_IF_FLAG_H
#define KW_SET_IF_FLAG_H

#include "robot/kawasaki/kawasaki_context.h"
#include "robot/kawasaki/command/kawasaki_command.h"

namespace rb {

class KwSetIFCommand : public KawasakiCommand {
public:
    KwSetIFCommand(KwInternalFlag flag)
        : KawasakiCommand(), m_flag(flag) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawsaki command Set internal flag error, m_interface pointer null.";
            return;
        }

        m_interface->flag.setInternalFlag(m_flag);
        m_execute_state = ExecuteState::Executed;
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwSetIFCommand>(*this);
    }

private:
    KwInternalFlag m_flag;
};

}

#endif // KW_SET_IF_FLAG_H
