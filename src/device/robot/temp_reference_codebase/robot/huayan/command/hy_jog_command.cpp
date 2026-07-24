#include "hy_jog_command.h"

namespace rb {

HyJogCommand::HyJogCommand(RbMotionType type, RbAxis axis, RbDirection direction) :
    HuayanCommand(),
    m_type(type),
    m_axis(axis),
    m_direction(direction), m_cmd_state(JgCmdState::UnExecute) {

}

void HyJogCommand::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HyJogCommand m_interface pointer null.";
        return;
    }

    _start_point:
    switch (m_cmd_state) {
    case JgCmdState::UnExecute:
        // set command state to executing
        m_execute_state = ExecuteState::Executing;
        m_cmd_state = JgCmdState::SendJogCommand;
        this->m_interface->flag.set_flag_start_jog();
        goto _start_point;
        break;

    case JgCmdState::SendJogCommand:
        // qDebug() << "Step into Send Jog Command";
        send_jog_command();
        break;

    case JgCmdState::WaitJogCommandReponse:
        // qDebug() << "Step into Wait jog command response";
        wait_jog_command_response();
        break;

    case JgCmdState::SendLongMoveEventCommand:
        // qDebug() << "Step into Send move event command";
        send_long_move_command();
        break;

    case JgCmdState::WaitLongMoveEventCommandResponse:
        // qDebug() << "Step into wait long move evnet command response";
        wait_long_move_command_response();
        break;

    case JgCmdState::WaitCycle:
        if (m_interface->flag.has_jog_cmd_stop_raised()) {
            m_cmd_state = JgCmdState::SendStopJogCommand;
            break;
        }

        if (m_time_count.StartTimeCounter(100)) {
            m_cmd_state = JgCmdState::SendLongMoveEventCommand;
        }
        break;

    case JgCmdState::SendStopJogCommand:
        // qDebug() << "Step into send stop jog command";
        send_stop_jog_command();
        break;

    case JgCmdState::WaitStopJogCommandResponse:
        // qDebug() << "Step into wait response jog command";
        wait_stop_jog_command();
        break;

    case JgCmdState::Finish:
        // qDebug() << "Step into finish";
        m_execute_state = ExecuteState::Executed;
        break;

    case JgCmdState::Error:
        // qDebug() << "Step into error";
        m_execute_state = ExecuteState::ExecuteError;
        break;
    }
}

inline void HyJogCommand::send_jog_command() {
    if (m_type == RbMotionType::mtJoint) {
        m_jog_cmd_header = "LongJogJ";
    } else {
        m_jog_cmd_header = "LongJogL";
    }

    m_axis_id = RbAxisToID(m_axis);
    m_i_direction = RbDirectionToInt(m_direction);

    m_jog_cmd = m_jog_cmd_header + "," +
                m_interface->flag.get_robot_id_str() + "," +
                QString::number(m_axis_id, 10) + "," +
                QString::number(m_i_direction, 10) + ",1,;";

    m_interface->msg.send_msg(m_jog_cmd);
    // set command state to wait for response
    m_cmd_state = JgCmdState::WaitJogCommandReponse;
}

inline void HyJogCommand::wait_jog_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_jog_cmd_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return success, start jogging, direction:"
                             << m_axis_id;
                    // set command state to send long move event command
                    m_cmd_state = JgCmdState::SendLongMoveEventCommand;
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyJogCommand]: wrong command response header";
            }
        } else {
            qCritical() << "HyJogCommand fail, ressponse instance return null pointer.";
        }
        m_cmd_state = JgCmdState::Error;
    }
}

inline void HyJogCommand::send_long_move_command() {
    QString long_move_cmd = "LongMoveEvent," + m_interface->flag.get_robot_id_str() + ",;";
    m_interface->msg.send_msg(long_move_cmd);
    // set command state to wait for response
    m_cmd_state = JgCmdState::WaitLongMoveEventCommandResponse;
}

inline void HyJogCommand::wait_long_move_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == "LongMoveEvent") {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return success.";
                    // set command state to send long move event command
                    if (m_interface->flag.has_jog_cmd_stop_raised()) {
                        m_cmd_state = JgCmdState::SendStopJogCommand;
                    } else {
                        m_time_count.StopTimeCounter();
                        m_cmd_state = JgCmdState::WaitCycle;
                    }
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain();

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyJogCommand]: wrong command response header";
            }
        } else {
            qCritical() << "HyJogCommand fail, ressponse instance return null pointer.";
        }
        m_cmd_state = JgCmdState::Error;
    }
}

inline void HyJogCommand::send_stop_jog_command() {
    m_jog_stop_cmd = m_jog_cmd_header + "," +
                     m_interface->flag.get_robot_id_str() + "," +
                     QString::number(m_axis_id, 10) + "," +
                     QString::number(m_i_direction, 10) + ",0,;";

    m_interface->msg.send_msg(m_jog_stop_cmd);
    // set command state to wait for response
    m_cmd_state = JgCmdState::WaitStopJogCommandResponse;
}

inline void HyJogCommand::wait_stop_jog_command() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_jog_cmd_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return success, stop jog, direction:"
                             << m_i_direction;
                    m_execute_state = ExecuteState::Executed;
                    m_cmd_state = JgCmdState::Finish;
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyJogCommand]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain();

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyJogCommand]: wrong command response header";
            }
        } else {
            qCritical() << "HyJogCommand fail, ressponse instance return null pointer.";
        }
        m_cmd_state = JgCmdState::Error;
    }
}

}

// WayPoint,0,450,-114,415,180,0,0,0,0,0,0,0,0,TCP,Base,50,100,50,1,0,0,0,0,ACDD123,;

