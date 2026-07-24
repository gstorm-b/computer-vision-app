#ifndef HY_SETTER_COMMAND_H
#define HY_SETTER_COMMAND_H

#include <QVariant>
#include "robot/huayan/huayan_command.h"

namespace rb {

class HySetterCommand : public HuayanCommand {
public:
    HySetterCommand(QString header);
    HySetterCommand(QString header, QList<QVariant> &params);

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_is_waiting_response = false;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HySetterCommand>(*this);
    }

private:
    bool m_is_waiting_response;

    QString m_command_header;
    QString m_command_str;
    QList<QVariant> m_params;
};

}

#endif // HY_SETTER_COMMAND_H
