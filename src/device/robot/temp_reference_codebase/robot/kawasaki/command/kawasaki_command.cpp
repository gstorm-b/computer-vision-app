#include "kawasaki_command.h"

namespace rb {

KawasakiCommand::KawasakiCommand()
    : m_execute_state(ExecuteState::UnExecuted),
    m_interface(nullptr),
    m_line_number(0),
    m_robot_type(RbType::Huayan_E) {

}

void KawasakiCommand::setContext(std::shared_ptr<RbCmdInterface> cmd_interface) {
    if (cmd_interface != nullptr) {
        this->m_interface =
            std::dynamic_pointer_cast<KawasakiCmdInterface>(cmd_interface);
    }
}

const bool KawasakiCommand::isComplete() {
    return ((m_execute_state==ExecuteState::Executed)
            || (m_execute_state==ExecuteState::ExecuteError)) ? true : false;
}

const bool KawasakiCommand::isExecuting() {
    return (m_execute_state==ExecuteState::Executing) ? true : false;
}

const RbCommand::ExecuteState KawasakiCommand::getExecuteState() {
    return m_execute_state;
}

const RbType KawasakiCommand::getRobotType() {
    return m_robot_type;
}

void KawasakiCommand::setLineNumber(int line_number) {
    if ((line_number < -1) || (line_number > 1001)) {
        m_line_number = 0;
        return;
    }
    m_line_number = line_number;
}

const int KawasakiCommand::getLineNumber() {
    return m_line_number;
}


}
