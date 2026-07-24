#include "hy_move_rel.h"

namespace rb {

HyMoveRel::HyMoveRel(RbMotionType motion_type, RbAxis axis, RbDirection direction,
                     double distance, bool tool_motion) :
    HuayanCommand(),
    m_type(motion_type), m_axis(axis), m_direction(direction),
    m_distance(distance), m_use_tool(tool_motion) {

}


void HyMoveRel::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HyMoveRel m_interface pointer null.";
        return;
    }

    _start_point:
    switch (m_cmd_state) {
    case MoveRelState::UnExecute:
        m_execute_state = ExecuteState::Executing;
        m_cmd_state = MoveRelState::SendMoveCommand;
        goto _start_point;
        break;

    case MoveRelState::SendMoveCommand:
        send_move_command();
        break;

    case MoveRelState::WaitMoveCommandReponse:
        wait_move_command_response();
        break;

    case MoveRelState::WaitFinishMovement:
        // wait_movement_finish();
        if (!m_interface->flag.is_robot_moving()) {
            m_cmd_state = MoveRelState::Finish;
            goto _start_point;
        }
        break;

    case MoveRelState::Finish:
        m_execute_state = ExecuteState::Executed;
        break;

    case MoveRelState::Error:
        m_execute_state = ExecuteState::ExecuteError;
        break;
    }
}

inline void HyMoveRel::send_move_command() {
    if (m_type == RbMotionType::mtJoint) {
        m_move_cmd_header = "MoveRelJ";
    } else {
        m_move_cmd_header = "MoveRelL";
    }

    m_axis_id = RbAxisToID(m_axis);
    m_i_direction = RbDirectionToInt(m_direction);

    m_move_cmd = m_move_cmd_header + "," +
                m_interface->flag.get_robot_id_str() + "," +
                QString::number(m_axis_id, 10) + "," +
                QString::number(m_i_direction, 10) + "," +
                QString::number(m_distance, 'f', 3) + "," +
                ((m_use_tool) ? "1" : "0") + ",;";

    m_interface->msg.send_msg(m_move_cmd);
    // set command state to wait for response
    m_cmd_state = MoveRelState::WaitMoveCommandReponse;
}

inline void HyMoveRel::wait_move_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_move_cmd_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyMoveRel]: found response, return success";
                    // set command state wait finished movement
                    m_cmd_state = MoveRelState::WaitFinishMovement;
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyMoveRel]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyMoveRel]: wrong command response header";
            }
        } else {
            qCritical() << "HyMoveRel fail, ressponse instance return null pointer.";
        }
        m_cmd_state = MoveRelState::Error;
    }
}

// inline void HyMoveRel::wait_movement_finish() {
//     if (!m_interface->flag.is_robot_moving()) {
//         m_cmd_state = MoveRelState::Error;
//     }
// }

}

