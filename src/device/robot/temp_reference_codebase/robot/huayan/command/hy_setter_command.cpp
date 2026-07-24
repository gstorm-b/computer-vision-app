#include "hy_setter_command.h"

namespace rb {

HySetterCommand::HySetterCommand(QString header) :
    HuayanCommand(),
    m_is_waiting_response(false),
    m_command_header(header) {

    m_params.clear();
}

HySetterCommand::HySetterCommand(QString header, QList<QVariant> &params)
    : HuayanCommand(),
    m_is_waiting_response(false),
    m_command_header(header),
    m_params(params) {

}

void HySetterCommand::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HySetterCommand m_interface pointer null.";
        return;
    }

    if (!m_is_waiting_response) {
        m_command_str = m_command_header + ","
                        + m_interface->flag.get_robot_id_str() + ",";
        int params_size = m_params.count();
        for (int idx=0;idx<params_size;idx++) {
            m_command_str += m_params[idx].toString() + ",";
        }
        m_command_str +=  ";";

        m_interface->msg.send_msg(m_command_str);
        m_execute_state = ExecuteState::Executing;
        m_is_waiting_response = true;
        return;
    }

    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_command_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HySetterCommand]: found response, return success.";
                    m_execute_state = ExecuteState::Executed;
                } else {
                    qDebug() << "Huayan robot controller [HySetterCommand]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;
                    m_execute_state = ExecuteState::ExecuteError;
                }
            } else {
                qDebug() << "Huayan robot controller [HySetterCommand]: wrong command response header";
            }
        } else {
            qCritical() << "HySetterCommand fail, ressponse instance return null pointer.";
        }
    }
}

// std::shared_ptr<RCommand> HySetterCommand::clone() {
//     return std::make_shared<HySetterCommand>(new HySetterCommand(*this));
// }

}
