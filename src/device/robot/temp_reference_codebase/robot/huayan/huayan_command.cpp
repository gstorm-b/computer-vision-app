#include "huayan_command.h"

namespace rb {

HuayanCommand::HuayanCommand()
    : m_execute_state(ExecuteState::UnExecuted),
    m_interface(nullptr),
    m_line_number(0),
    m_robot_type(RbType::Huayan_E) {

}

void HuayanCommand::setContext(std::shared_ptr<RbCmdInterface> cmd_interface) {
    if (cmd_interface != nullptr) {
        this->m_interface = std::dynamic_pointer_cast<HuayanCmdInterface>(cmd_interface);
    }
}

const bool HuayanCommand::isComplete() {
    return ((m_execute_state==ExecuteState::Executed)
            || (m_execute_state==ExecuteState::ExecuteError)) ? true : false;
}

const bool HuayanCommand::isExecuting() {
    return (m_execute_state==ExecuteState::Executing) ? true : false;
}

const RbCommand::ExecuteState HuayanCommand::getExecuteState() {
    return m_execute_state;
}

const RbType HuayanCommand::getRobotType() {
    return m_robot_type;
}

void HuayanCommand::setLineNumber(int line_number) {
    if ((line_number < -1) || (line_number > 1001)) {
        m_line_number = -1;
        return;
    }
    m_line_number = line_number;
}

const int HuayanCommand::getLineNumber() {
    return m_line_number;
}

}

