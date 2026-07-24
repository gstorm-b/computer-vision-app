#include "hy_waypoint_rel.h"

namespace rb {

HyWayPointRel::HyWayPointRel(RbMotionType motion_type,
                             double velocity, double accel, double blendRadius,
                             CartesianPoint target_distance,
                             CartesianPoint ref_point, JointPoint ref_joint_point,
                             bool use_ref_point, bool ref_is_absolute,
                             QString unique_id,
                             QString tcp, QString ucs,
                             bool use_vs_point, bool isUseJoint)
    : HuayanCommand(),
    m_target_distance(target_distance),
    m_ref_point(ref_point),
    m_ref_joint_point(ref_joint_point),
    m_use_ref_point(use_ref_point),
    m_ref_point_is_absolute(ref_is_absolute),
    m_tcp_name(tcp),
    m_ucs_name(ucs),
    m_velocity(velocity),
    m_acceleration(accel),
    m_blend_radius(blendRadius),
    m_move_type(motion_type),
    m_is_use_joint(isUseJoint),
    m_uuid_string(unique_id),
    m_use_vs_point(use_vs_point),
    m_had_trigger_vision_coordinate(false) {

}

void HyWayPointRel::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HyWayPoint m_interface pointer null.";
        return;
    }

    _start_point:
    switch (m_cmd_state) {
    case WayPointRelExecuteState::UnExecute:
        m_execute_state = ExecuteState::Executing;
        m_cmd_state = WayPointRelExecuteState::SendMoveCommand;
        m_had_trigger_vision_coordinate = false;
        goto _start_point;
        break;

    case WayPointRelExecuteState::SendMoveCommand:
        send_waypointrel_command();
        break;

    case WayPointRelExecuteState::WaitMoveCommandReponse:
        wait_waypointrel_command_response();
        break;

    case WayPointRelExecuteState::Finish:
        m_execute_state = ExecuteState::Executed;
        break;

    case WayPointRelExecuteState::Error:
        m_execute_state = ExecuteState::ExecuteError;
        break;
    }
}

inline QString isSetMask(double number) {
    return (number != 0.0) ? "1" : "0";
}

void HyWayPointRel::send_waypointrel_command() {
    if (m_use_vs_point) {
        // block until get vision coordinate
        if (!m_interface->flag.is_vs_coordinate_available()) {
            if (!m_had_trigger_vision_coordinate) {
                m_interface->flag.request_vs_coordinate();
                m_had_trigger_vision_coordinate = true;
            }
            return;
        }

        m_ref_point = m_interface->flag.get_vs_coordinate();
    }

    // update tcp and ucs name
    if (m_tcp_name.isEmpty()) {
        m_tcp_name = m_interface->flag.get_gobal_tcp_name();
    }

    if (m_ucs_name.isEmpty()) {
        m_ucs_name = m_interface->flag.get_gobal_ucs_name();
    }


    QString str_move_type = (m_move_type == RbMotionType::mtLinear)
                                ? "1" : "0";

    QString str_rel_move_type = (m_ref_point_is_absolute) ? "0" : "1";

    QString str_axis_mask = isSetMask(m_target_distance.x()) + "," +
                            isSetMask(m_target_distance.y()) + "," +
                            isSetMask(m_target_distance.z()) + "," +
                            isSetMask(m_target_distance.rx()) + "," +
                            isSetMask(m_target_distance.ry()) + "," +
                            isSetMask(m_target_distance.rz());

    m_wprel_cmd = m_wprel_cmd_header + "," +
                  // robot id
                  m_interface->flag.get_robot_id_str() + "," +
                  // move type
                  str_move_type + "," +
                  // use point list
                  ((m_use_ref_point) ? "1" : "0") + "," +
                  // target move point
                  m_ref_point.toQString() + "," +
                  // target joint move point
                  m_ref_joint_point.toQString() + "," +
                  // nrelMoveType
                  str_rel_move_type + "," +
                  // axis mask
                  str_axis_mask + "," +
                  // target distance
                  m_target_distance.toQString() + "," +
                  // tcp name
                  m_tcp_name + "," +
                  // ucs name
                  m_ucs_name + "," +
                  QString::number(m_velocity, 'f', 3) + "," +
                  QString::number(m_acceleration, 'f', 3) + "," +
                  QString::number(m_blend_radius, 'f', 3) + "," +
                  QString::number(m_is_use_joint, 10) + "," +
                  QString::number(m_is_seek, 10) + "," +
                  QString::number(m_io_bit, 10) + "," +
                  QString::number(m_io_state, 10) + "," +
                  m_uuid_string + ",;";

    m_interface->msg.send_msg(m_wprel_cmd);
    // set command state to wait for response
    m_cmd_state = WayPointRelExecuteState::WaitMoveCommandReponse;
}

void HyWayPointRel::wait_waypointrel_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_wprel_cmd_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyWayPoint]: found response, return success";
                    // set command state wait finished movement
                    m_cmd_state = WayPointRelExecuteState::Finish;
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
        m_cmd_state = WayPointRelExecuteState::Error;
    }
}



}
