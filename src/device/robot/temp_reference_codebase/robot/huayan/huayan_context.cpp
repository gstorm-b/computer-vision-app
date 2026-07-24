#include "huayan_context.h"

namespace rb {

/// HYTMSGRANSPORT

HyMsgTransport::HyMsgTransport() :
    m_flag_send(false),
    m_flag_recv(false),
    m_send_msg(""),
    m_response_msg() {

}

const bool HyMsgTransport::send_msg(QString &msg) {
    if (m_flag_send == true) {
        return false;
    }

    m_send_msg = msg;
    m_flag_send = true;
    return true;
}

void HyMsgTransport::mark_response(HuayanMsgReturn& response) {
    // TODO: considering overwrite reponse even have not retrieve
    if (response.isValid()) {
        m_flag_recv = true;
        m_response_msg = response;
    }
}

const bool HyMsgTransport::is_sending_marked() {
    return m_flag_send;
}

const bool HyMsgTransport::is_response_received() {
    return m_flag_recv;
}

QString HyMsgTransport::retrieve_sending_msg() {
    m_flag_send = false;
    return m_send_msg;
}

HuayanMsgReturn* HyMsgTransport::retrieve_response() {
    if (m_flag_recv) {
        m_flag_recv = false;
        return &m_response_msg;
    }
    return nullptr;
}

void HyMsgTransport::resetContext() {
    m_flag_recv = false;
    m_flag_send = false;
}

/// END HYMSGTRANSPORT

/// HYFLAGRASIER

HyFlagTransport::HyFlagTransport() {

}

HyFlagTransport::HyFlagTransport(int rb_id) {
    set_robot_id(rb_id);
}

const void HyFlagTransport::setInternalFlag(HyInternalFlag flag) {
    m_is_internal_flag_raised = true;
    m_interal_flag = flag;
}

const bool HyFlagTransport::is_internal_flag_raised() {
    return m_is_internal_flag_raised;
}

HyInternalFlag HyFlagTransport::retrieve_internal_flag() {
    m_is_internal_flag_raised = false;
    return m_interal_flag;
}

void HyFlagTransport::set_robot_id(int id) {
    if ((id >= 0) && (id <= 5)) {
        m_robot_id = id;
        m_robot_id_str = QString::number(m_robot_id);
    }
}

const bool HyFlagTransport::has_jog_cmd_stop_raised() {
    if (m_jog_stop_flag) {
        m_jog_stop_flag = false;
        return true;
    }
    return false;
}

const bool HyFlagTransport::set_jog_cmd_stop_flag() {
    if (m_jog_stop_flag) {
        return false;
    }
    m_jog_stop_flag = true;
    return true;
}

void HyFlagTransport::set_enddo_ptr(int *enddo) {
    if (enddo!=nullptr) {
        m_enddo_ptr = enddo;
    }
}

void HyFlagTransport::set_enddi_ptr(int *enddi) {
    if (enddi!=nullptr) {
        m_enddi_ptr = enddi;
    }
}

void HyFlagTransport::set_boxdo_ptr(int *boxdo) {
    if (boxdo!=nullptr) {
        m_boxdo_ptr = boxdo;
    }
}

void HyFlagTransport::set_boxdi_ptr(int *boxdi) {
    if (boxdi!=nullptr) {
        m_boxdi_ptr = boxdi;
    }
}

const bool HyFlagTransport::get_enddo_state(int number) {
    if ((number>=0) && (number<4)) {
        return (m_enddo_ptr[number]==0) ? false : true;
    }
    return false;
}

const bool HyFlagTransport::get_enddi_state(int number) {
    if ((number>=0) && (number<4)) {
        return (m_enddi_ptr[number]==0) ? false : true;
    }
    return false;
}

const bool HyFlagTransport::get_boxdo_state(int number) {
    if ((number>=0) && (number<8)) {
        return (m_boxdo_ptr[number]==0) ? false : true;
    }
    return false;
}

const bool HyFlagTransport::get_boxdi_state(int number) {
    if ((number>=0) && (number<8)) {
        return (m_boxdi_ptr[number]==0) ? false : true;
    }
    return false;
}

Fx3Communicator* HyFlagTransport::plc_comunicator() {
    return m_plc;
}

void HyFlagTransport::set_plc_communicator_ptr(Fx3Communicator* ptr) {
    if (ptr!=nullptr) {
        m_plc = ptr;
    }
}

const bool HyFlagTransport::is_vs_coordinate_available() {
    return m_is_contain_coordinate;
}

void HyFlagTransport::request_vs_coordinate() {
    m_is_request_coordinate = true;
    m_is_contain_coordinate = false;
    m_vs_request_count += 1;
}

const bool HyFlagTransport::is_request_coordinate() {
    if (m_is_request_coordinate) {
        m_is_request_coordinate = false;
        return true;
    }
    return false;
}

void HyFlagTransport::set_auto_request_coordinate(bool enb, int after_index) {
    m_is_auto_request = enb;
    m_auto_request_index = after_index;
}

const CartesianPoint HyFlagTransport::get_vs_coordinate() {
    return m_matched_point;
}

const bool  HyFlagTransport::is_auto_request_setted(int &index) {
    index = m_auto_request_index;
    return m_is_auto_request;
}

void HyFlagTransport::set_vs_coordinate(CartesianPoint point) {
    m_is_contain_coordinate = true;
    m_matched_point = point;
}

void HyFlagTransport::clear_vs_coordinate() {
    m_is_contain_coordinate = false;
}

const bool HyFlagTransport::is_vision_fail() {
    if (m_is_vision_fail) {
        m_is_vision_fail = false;
        return true;
    }
    return false;
}

const int HyFlagTransport::get_vs_request_count()  {
    return m_vs_request_count;
}

void HyFlagTransport::raise_vision_fail() {
    m_is_vision_fail = true;
}

const void HyFlagTransport::set_new_current_waypoint(QString id) {
    m_current_waypoint_id = id;
    m_is_new_waypoint = true;
}

const bool HyFlagTransport::is_waypointID_changed() {
    return m_is_new_waypoint;
}

const QString HyFlagTransport::retrive_waypointId() {
    m_is_new_waypoint = false;
    return m_current_waypoint_id;
}

void HyFlagTransport::setJumpToLine(int number) {
    m_jump_line_number = number;
}

const int HyFlagTransport::getJumpToLine() {
    return m_jump_line_number;
}


void HyFlagTransport::resetContext() {
    m_is_internal_flag_raised = false;
    m_is_first_cycle = false;
    m_interal_flag = HyInternalFlag::HyfNone;

    m_jog_stop_flag = false;
    m_is_robot_moving = false;

    m_is_contain_coordinate = false;
    m_is_request_coordinate = false;
    m_is_auto_request = false;
    m_auto_request_index = 0;
    m_matched_point = CartesianPoint();
    m_is_vision_fail = false;
    m_vs_request_count = 0;
    m_jump_line_number = 0;

    m_is_new_waypoint = false;
}

/// END HYFLAGRAISER


}
