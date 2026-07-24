#include "hy_waypoint.h"

namespace rb {

HyWayPoint::HyWayPoint(RbMotionType motion_type,
           double velocity, double accel, double blendRadius,
           CartesianPoint target_point, JointPoint target_joint_point,
           QString unique_id,
           QString tcp, QString ucs, bool isUseJoint)
    : HuayanCommand(),
    m_end_point(target_point),
    m_end_joint_point(target_joint_point),
    m_tcp_name(tcp),
    m_ucs_name(ucs),
    m_velocity(velocity),
    m_acceleration(accel),
    m_blend_radius(blendRadius),
    m_move_type(motion_type),
    m_is_use_joint((isUseJoint) ? 1 : 0),
    m_uuid_string(unique_id) {

    // if (!tcp.isEmpty()) {
    //     m_tcp_name = tcp;
    // }

    // if (!ucs.isEmpty()) {
    //     m_ucs_name = ucs;
    // }
}

void HyWayPoint::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HyWayPoint m_interface pointer null.";
        return;
    }

    _start_point:
    switch (m_cmd_state) {
    case WayPointExecuteState::UnExecute:
        m_execute_state = ExecuteState::Executing;
        m_cmd_state = WayPointExecuteState::SendMoveCommand;
        goto _start_point;
        break;

    case WayPointExecuteState::SendMoveCommand:
        send_waypoint_command();
        break;

    case WayPointExecuteState::WaitMoveCommandReponse:
        wait_waypoint_command_response();
        break;

    case WayPointExecuteState::Finish:
        m_execute_state = ExecuteState::Executed;
        break;

    case WayPointExecuteState::Error:
        m_execute_state = ExecuteState::ExecuteError;
        break;
    }
}

inline void HyWayPoint::send_waypoint_command() {
    if (m_tcp_name.isEmpty()) {
        m_tcp_name = m_interface->flag.get_gobal_tcp_name();
    }

    if (m_ucs_name.isEmpty()) {
        m_ucs_name = m_interface->flag.get_gobal_ucs_name();
    }

    QString str_move_type = (m_move_type == RbMotionType::mtLinear)
                                ? "1" : "0";

    m_waypoint_cmd = "WayPoint," +
                     m_interface->flag.get_robot_id_str() + "," +
                     m_end_point.toQString() + "," +
                     m_end_joint_point.toQString() + "," +
                     m_tcp_name + "," +
                     m_ucs_name + "," +
                     QString::number(m_velocity, 'f', 3) + "," +
                     QString::number(m_acceleration, 'f', 3) + "," +
                     QString::number(m_blend_radius, 'f', 3) + "," +
                     str_move_type + "," +
                     QString::number(m_is_use_joint, 10) + "," +
                     QString::number(m_is_seek, 10) + "," +
                     QString::number(m_io_bit, 10) + "," +
                     QString::number(m_io_state, 10) + "," +
                     m_uuid_string + ",;";

    m_interface->msg.send_msg(m_waypoint_cmd);
    // set command state to wait for response
    m_cmd_state = WayPointExecuteState::WaitMoveCommandReponse;
}

inline void HyWayPoint::wait_waypoint_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_waypoint_cmd_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyWayPoint]: found response, return success";
                    // set command state wait finished movement
                    m_cmd_state = WayPointExecuteState::Finish;
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyWayPoint]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyWayPoint]: wrong command response header";
            }
        } else {
            qCritical() << "HyWayPoint fail, ressponse instance return null pointer.";
        }
        m_cmd_state = WayPointExecuteState::Error;
    }
}

// inline QString HyWayPoint::get_move_command() {
//     return "WayPoint," +
//            m_interface->flag.get_robot_id_str() + "," +
//            m_end_point.toQString() + "," +
//            m_end_joint_point.toQString() + "," +
//            m_tcp_name + "," +
//            m_ucs_name + "," +
//            QString::number(m_velocity, 'f', 3) + "," +
//            QString::number(m_acceleration, 'f', 3) + "," +
//            QString::number(m_blend_radius, 'f', 3) + "," +
//            QString::number(m_move_type, 10) + "," +
//            QString::number(m_is_use_joint, 10) + "," +
//            QString::number(m_is_seek, 10) + "," +
//            QString::number(m_io_bit, 10) + "," +
//            QString::number(m_io_state, 10) + "," +
//            m_uuid_string + ",;";
//     }
}

