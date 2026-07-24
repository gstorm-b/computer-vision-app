#include "huayan.h"
#include "robot/huayan/command/hy_command_factory.h"

#include "log_helper/log_wrapper.h"

#define LOG_NAME "[Huayan robot]"


namespace rb {

RbHuayan::RbHuayan(QObject *parent)
    : RobotAbstract(parent),
    m_fb_output_timeout(300),
    m_controller_running(false),
    m_robot_connected(false),
    m_is_ready_connect(true),
    m_feedback_lost_connect(false),
    m_is_first_feedback(false),
    m_emergency_stop_triggered(false),
    m_error_stop(false),
    m_status_enable(false),
    m_is_sq_runnning(false),
    m_is_sq_continuous(false),
    m_in_moving(false) {

}

RbHuayan::~RbHuayan() {
    robotTerminate();
}

bool RbHuayan::RobotConnect(RbConnectParams *params) {
    bool state = false;

    HuayanConnectParams *connect_params = static_cast<HuayanConnectParams*>(params);

    RB_LOCK_DATA;
    if (m_is_ready_connect) {
        m_tcp_address = connect_params->ip_address;
        m_thread_priority = connect_params->thread_priority;
    }
    state = m_is_ready_connect;
    RB_UNLOCK_DATA;

    if (state) {
        this->start(connect_params->thread_priority);
    }
    return state;
}

void RbHuayan::RobotDisconnect() {
    RB_LOCK_DATA;
    if (m_controller_running) {
        m_controller_running = false;
    }
    RB_UNLOCK_DATA;
}

const bool RbHuayan::RobotReadyForConnect() {
    bool state = false;
    RB_LOCK_DATA;
    state = m_is_ready_connect;
    RB_UNLOCK_DATA;
    return state;
}
const bool RbHuayan::isRobotConnected() {
    bool ret = false;
    RB_LOCK_DATA;
    ret = m_robot_connected;
    RB_UNLOCK_DATA;
    return ret;
}
const bool RbHuayan::isRobotDisconnected()  {
    return !this->isRobotConnected();
}

const bool RbHuayan::RobotEnableState() {
    bool ret = false;
    RB_LOCK_DATA;
    ret = m_status_enable;
    RB_UNLOCK_DATA;
    return ret;
}

void RbHuayan::RobotEmergencyStop() {
    RB_LOCK_DATA;
    m_emergency_stop_triggered = true;
    RB_UNLOCK_DATA;
}

CartesianPoint RbHuayan::RobotCurrentPosision() {
    CartesianPoint point;
    RB_LOCK_DATA;
    point = m_feedback_data.actual_position_cartesian;
    RB_UNLOCK_DATA;
    return point;
}

const bool RbHuayan::RobotHomeReturn() {
    return false;
}

void RbHuayan::RobotPushCommand(std::shared_ptr<RbCommand> cmd) {
    RB_LOCK_DATA;
    std::shared_ptr<RbCommand> clone_cmd = cmd->clone();
    m_cmd_queue.push_back(clone_cmd);
    RB_UNLOCK_DATA;
}

bool RbHuayan::RobotFreeJog(RbMotionType _type, RbAxis _axis, RbDirection _direction) {
    bool jog_start = false;
    RB_LOCK_DATA;
    if (m_is_ready_for_operate) {
        m_cmd_queue.push_back(HyCmdFactory::FreeJog(_type, _axis, _direction));
        jog_start = true;
    }
    RB_UNLOCK_DATA;
    return jog_start;
}

bool RbHuayan::RobotStopFreeJog() {
    bool isValid = false;
    RB_LOCK_DATA;
    if (m_ctx_flag_transport != nullptr) {
        m_ctx_flag_transport->asReceiver().set_jog_cmd_stop_flag();
        isValid = true;
    }
    RB_UNLOCK_DATA;
    return isValid;
}

bool RbHuayan::RobotDistanceJog(RbMotionType motion_type, RbAxis axis, RbDirection direction,
                                double distance, bool tool_motion) {
    bool jog_start = false;
    RB_LOCK_DATA;
    if (m_is_ready_for_operate) {
        m_cmd_queue.push_back(HyCmdFactory::DistanceJog(motion_type, axis, direction, distance, tool_motion));
        jog_start = true;
    }
    RB_UNLOCK_DATA;
    return jog_start;
}

bool RbHuayan::RobotAlignToZ(QString tcp_name, QString ucs_name) {
    bool jog_start = false;
    RB_LOCK_DATA;
    if (m_is_ready_for_operate) {
        m_cmd_queue.push_back(HyCmdFactory::AlignToZ(tcp_name, ucs_name));
        jog_start = true;
    }
    RB_UNLOCK_DATA;
    return jog_start;
}

void RbHuayan::RobotMoveHome() {
    RB_LOCK_DATA;
    if (!m_is_sq_runnning) {
        m_before_movehome_speed = m_feedback_data.actual_overide;
        m_cmd_queue.push_back(HyCmdFactory::SetSpeedOverride(0.50));
        m_cmd_queue.push_back(HyCmdFactory::SetIFComamnd(HyInternalFlag::HyfHomingStart));
        m_cmd_queue.push_back(HyCmdFactory::MoveLTo(m_home_position, "", ""));
        m_cmd_queue.push_back(HyCmdFactory::SetSpeedOverride(m_before_movehome_speed));
        m_cmd_queue.push_back(HyCmdFactory::SetIFComamnd(HyInternalFlag::HyfHomingDone));
        // m_cmd_queue.push_back(HyCmdFactory::ReadWayPointID());
    }
    RB_UNLOCK_DATA;
}

void RbHuayan::RobotSetHomePos(CartesianPoint position) {
    RB_LOCK_DATA;
    m_home_position = position;
    RB_UNLOCK_DATA;
}

CartesianPoint RbHuayan::RobotHomePos() {
    CartesianPoint return_position;
    RB_LOCK_DATA;
    return_position = m_home_position;
    RB_UNLOCK_DATA;
    return return_position;
}

void RbHuayan::RobotRunContinuous() {
    RB_LOCK_DATA;
    if ((!m_is_sq_runnning) && (m_status_enable)) {
        m_is_sq_runnning = true;
        m_is_sq_continuous = true;
    }
    RB_UNLOCK_DATA;
}

void RbHuayan::RobotRunSingleCycle() {
    RB_LOCK_DATA;
    if ((!m_is_sq_runnning) && (m_status_enable)) {
        m_is_sq_runnning = true;
        m_is_sq_continuous = false;
    }
    RB_UNLOCK_DATA;
}

void RbHuayan::RobotStopCycle() {
    RB_LOCK_DATA;
    if (m_is_sq_runnning) {
        m_is_sq_continuous = false;
    }
    RB_UNLOCK_DATA;
}

void RbHuayan::RobotForceStop() {
    RB_LOCK_DATA;
    if (m_is_sq_runnning) {
        m_is_sq_runnning = false;
    }
    RB_UNLOCK_DATA;
}

bool RbHuayan::RobotIsSqRunning() {
    bool state;
    RB_LOCK_DATA;
    state = m_is_sq_runnning;
    RB_UNLOCK_DATA;
    return state;
}

bool RbHuayan::RobotIsInError() {
    bool state;
    RB_LOCK_DATA;
    state = m_error_stop;
    RB_UNLOCK_DATA;
    return state;
}

void RbHuayan::RobotPushMainSequence(QList<std::shared_ptr<RbCommand>> &cmd_sq) {
    RB_LOCK_DATA;
    if (!m_is_sq_runnning) {
        // clear main sequence and clone
        m_sq_cmd_queue.clear();
        for (int idx=0;idx<cmd_sq.size();idx++) {
            m_sq_cmd_queue.push_back(cmd_sq[idx]->clone());
        }

        std::shared_ptr<RbCommand> start_cmd = HyCmdFactory::SetIFComamnd(HyInternalFlag::HyfStartSqCycle);
        start_cmd->setLineNumber(0);
        std::shared_ptr<RbCommand> end_cmd = HyCmdFactory::SetIFComamnd(HyInternalFlag::HyfEndSqCycle);
        end_cmd->setLineNumber(-1);

        m_sq_cmd_queue.push_front(start_cmd);
        m_sq_cmd_queue.push_back(end_cmd);
        OLOG_INFO << "Command sequence pushed";
    }
    RB_UNLOCK_DATA;
}

void RbHuayan::SetPlcCommunicator(Fx3Communicator *plc_communicator) {
    RB_LOCK_DATA;
    m_plc_communicator = plc_communicator;
    RB_UNLOCK_DATA;
}

void RbHuayan::VisionReturn(CartesianPoint coor) {
    RB_LOCK_DATA;
    // m_containing_coordinate = true;
    m_vs_outside_new_coor = true;
    m_vs_coordinate = coor;
    m_vs_outside_coordinate = coor;
    // m_ctx_sq_flag_transport->asReceiver().set_vs_coordinate(m_vs_coordinate);
    // m_wait_for_coordinate = false;
    RB_UNLOCK_DATA;
    OLOG_INFO << "Receive matched point:" << coor.toQString();
}

void RbHuayan::VisionFail() {
    RB_LOCK_DATA;
    // m_ctx_sq_flag_transport->asReceiver().raise_vision_fail();
    m_vs_outside_fail =  true;
    RB_UNLOCK_DATA;
}

void RbHuayan::VisionReset() {
    RB_LOCK_DATA;
    // m_containing_coordinate = false;
    // m_ctx_sq_flag_transport->asReceiver().clear_vs_coordinate();
    m_vs_outside_reset = true;
    RB_UNLOCK_DATA;
}

void RbHuayan::run() {
    /// Controller auto init TCP Socket when thread starting
    /// and clean it before thread stop

    OLOG_INFO << "Robot remote controller starting...";
    remoteControllerStart();

    while(true) {
        // check running or stop avoid race condition
        RB_LOCK_DATA;
        bool should_continue = m_controller_running;
        RB_UNLOCK_DATA;
        if (!should_continue) {
            break;
        }

        emergencyStopHandle();
        feedbackPortHandle();
        autoRunHandle();
        commandPortHandle();
        sqCommandPortHandle();

        msleep(2);

        if (m_feedback_output_counter->StartTimeCounter(m_fb_output_timeout)) {
            // std::shared_ptr<rb::RbAbstractFeedBack> fb = std::make_shared<RbAbstractFeedBack>(this->m_feedback_data);
            emit s_RobotFeedbackUpdate(this->m_feedback_data);
            // emit RobotAbstract::s_RobotFeedbackUpdate(fb);
        }
        m_is_ready_for_operate = (m_cmd_queue.isEmpty()) ? true : false;
    }

    remoteControllerClean();
    OLOG_INFO << "Robot remote controller terminate...";
}

void RbHuayan::robotTerminate() {
    bool is_running = false;

    RB_LOCK_DATA;
    if (m_controller_running) {
        is_running = true;
        m_controller_running = false;
    }
    RB_UNLOCK_DATA;

    // wait until thread destroyed
    if (is_running) {
        this->wait(25000);
    }
}

void RbHuayan::remoteControllerStart() {
    m_is_ready_connect = false;

    m_command_port = std::make_unique<HuayanCommandPort>();
    m_command_sq_port = std::make_unique<HuayanCommandPort>();
    m_feedback_port = std::make_unique<HuayanFeedBackPort>();

    TCPClient::TcState port_cmd_state = m_command_port->ConnectToPort(m_tcp_address, HUAYAN_COMMAND_PORT);
    TCPClient::TcState port_cmd_sq_state = m_command_sq_port->ConnectToPort(m_tcp_address, HUAYAN_COMMAND_PORT);
    TCPClient::TcState port_fb_state = m_feedback_port->ConnectToPort(m_tcp_address, HUAYAN_JDATA_SHEET_PORT_50);

    if ((port_cmd_state == TCPClient::TcState::ConnectFail)
        || (port_cmd_sq_state == TCPClient::TcState::ConnectFail)
        || (port_fb_state == TCPClient::TcState::ConnectFail)) {
        // emit connect fail signal
        emit RobotAbstract::s_RobotConnectFailed();
    } else {
        if ((port_fb_state == TCPClient::TcState::Connected)
            && (port_cmd_state == TCPClient::TcState::Connected)
            && (port_cmd_sq_state == TCPClient::TcState::Connected)) {
            // emit connect success signal
            m_robot_connected = true;
            emit RobotAbstract::s_RobotConnected();
        }
    }

    if (m_robot_connected) {
        resetStatusBeforeLoop();
        m_controller_running = true;
        OLOG_INFO << "Robot TCP server connect successful...";
    } else {
        OLOG_INFO << "Robot TCP server connect fail...";
    }
}

void RbHuayan::remoteControllerClean() {
    m_command_port->DisconnectFromPort();
    m_command_sq_port->DisconnectFromPort();
    m_feedback_port->DisconnectFromPort();

    m_command_port.release();
    m_command_sq_port.release();
    m_feedback_port.release();

    if (m_robot_connected && !m_feedback_lost_connect) {
        emit RobotAbstract::s_RobotDisconnected();
    }

    m_feedback_lost_connect = false;
    m_robot_connected = false;
    m_is_ready_connect = true;
    m_status_enable = false;
    m_is_sq_runnning = false;
    m_in_moving = false;
    m_is_sq_continuous = false;
    // m_sq_counter = 0;
    m_emergency_stop_triggered = false;
    m_error_stop = false;
    m_sq_stage = SqRunningStage::SqIdle;
}

void RbHuayan::resetStatusBeforeLoop() {
    RB_LOCK_DATA;

    m_is_first_feedback = true;
    m_stand_at_home = false;

    m_emergency_stop_triggered = false;
    m_error_stop = false;

    m_feedback_output_counter.reset(new ChronoCounter());

    // Reset all command contexts
    if (m_ctx_msg_transport != nullptr) {
        m_ctx_msg_transport.reset();
    }
    m_ctx_msg_transport = std::make_shared<HyMsgTransport>();

    if (m_ctx_flag_transport != nullptr) {
        m_ctx_flag_transport.reset();
    }
    // allocate flag transport service with robot id = 0
    m_ctx_flag_transport = std::make_shared<HyFlagTransport>(0);

    if (m_cmd_interface != nullptr) {
        m_cmd_interface.reset();
    }
    m_cmd_interface = std::make_shared<HuayanCmdInterface>(
        m_ctx_msg_transport->asSender(), m_ctx_flag_transport->asSender());

    m_ctx_flag_transport->set_global_tcp_name(m_normal_tcp_name);
    m_ctx_flag_transport->set_global_ucs_name(m_normal_ucs_name);
    m_ctx_flag_transport->set_boxdo_ptr(m_feedback_data.BoxDO);
    m_ctx_flag_transport->set_boxdi_ptr(m_feedback_data.BoxDI);
    m_ctx_flag_transport->set_enddo_ptr(m_feedback_data.EndDO);
    m_ctx_flag_transport->set_enddi_ptr(m_feedback_data.EndDI);
    m_ctx_flag_transport->set_plc_communicator_ptr(m_plc_communicator);

    // Reset all command contexts of sequence queue
    if (m_ctx_sq_msg_transport != nullptr) {
        m_ctx_sq_msg_transport.reset();
    }
    m_ctx_sq_msg_transport = std::make_shared<HyMsgTransport>();

    if (m_ctx_sq_flag_transport != nullptr) {
        m_ctx_sq_flag_transport.reset();
    }
    // allocate flag transport service with robot id = 0
    m_ctx_sq_flag_transport = std::make_shared<HyFlagTransport>(0);

    if (m_sq_cmd_interface != nullptr) {
        m_sq_cmd_interface.reset();
    }
    m_sq_cmd_interface = std::make_shared<HuayanCmdInterface>(
        m_ctx_sq_msg_transport->asSender(), m_ctx_sq_flag_transport->asSender());

    m_ctx_sq_msg_transport->resetContext();
    m_ctx_sq_flag_transport->resetContext();
    m_ctx_sq_flag_transport->set_global_tcp_name(m_normal_tcp_name);
    m_ctx_sq_flag_transport->set_global_ucs_name(m_normal_ucs_name);
    m_ctx_sq_flag_transport->set_boxdo_ptr(m_feedback_data.BoxDO);
    m_ctx_sq_flag_transport->set_boxdi_ptr(m_feedback_data.BoxDI);
    m_ctx_sq_flag_transport->set_enddo_ptr(m_feedback_data.EndDO);
    m_ctx_sq_flag_transport->set_enddi_ptr(m_feedback_data.EndDI);
    m_ctx_sq_flag_transport->set_plc_communicator_ptr(m_plc_communicator);

    m_cmd_queue.clear();
    m_current_cmd.reset();

    if (!m_sq_cmd_queue.empty()) {
        m_sq_cmd_queue.clear();
    }
    m_sq_current_cmd.reset();
    m_sq_stage = SqRunningStage::SqIdle;

    m_is_sq_runnning = false;
    m_in_moving = false;
    m_is_sq_continuous = false;
    // m_sq_counter = 0;

    m_emergency_stop_triggered = false;
    m_error_stop = false;

    m_vs_outside_fail  = false;
    m_vs_outside_reset  = false;
    m_vs_outside_new_coor  = false;

    m_wait_for_coordinate = false;
    m_vision_trigger_waypoint_wait = false;
    m_vision_trigger_wait_wp_finish = false;

    RB_UNLOCK_DATA;
}

inline void RbHuayan::emergencyStopHandle() {
    RB_LOCK_DATA;
    bool is_emer = m_emergency_stop_triggered;
    RB_UNLOCK_DATA;

    if (is_emer) {
        resetCommandQueue();
        m_cmd_queue.push_back(HyCmdFactory::GrpStop());
        m_cmd_queue.push_back(HyCmdFactory::GrpDisable());
        m_is_sq_runnning = false;
        m_in_moving = false;
        m_sq_stage = SqForceStop;
        m_emergency_stop_triggered = false;
        m_error_stop = true;
        emit s_RobotErrorStop();
    }
}

inline void RbHuayan::resetCommandQueue() {
    // remove all command in queue and current executing command
    m_current_cmd.reset();
    m_cmd_queue.clear();
    // reset all command context
    m_ctx_msg_transport->resetContext();
    m_ctx_flag_transport->resetContext();

    // remove all command in queue and current executing command
    m_sq_current_cmd.reset();
    m_sq_cmd_queue.clear();
    // reset all command context
    m_ctx_sq_msg_transport->resetContext();
    m_ctx_sq_flag_transport->resetContext();

    m_sq_stage = SqRunningStage::SqIdle;
}

inline void RbHuayan::feedbackPortHandle() {
    // polling receive feedback
    if (!m_feedback_port->polling()) {
        qWarning()<< "Robot TCP server lost connect!";
        emit s_RobotConnectFailed();
        m_feedback_lost_connect = true;
        m_controller_running = false;
        return;
    }

    // // only handle when new package came
    if (m_feedback_port->retrieveReadStatus()) {
        HuayanSheet last_feedback = m_feedback_data;
        m_feedback_data = m_feedback_port->GetFeedbackData();

        if (last_feedback.RobotState != m_feedback_data.RobotState) {
            // OLOG_INFO << "Robot change mode:" << m_feedback_data.RobotState;
            emit s_RobotModeChanged(last_feedback.RobotState, m_feedback_data.RobotState);
        }

        m_status_enable = (last_feedback.RobotEnabled == 1) ? true : false;
        if (m_is_sq_runnning && (!m_status_enable)) {
            OLOG_INFO << "Robot is not enabled, stop sequence";
            m_is_sq_runnning = false;
        }

        if (((last_feedback.RobotState == rb::HuayanMachineState::SaftyGuardError) ||
             (last_feedback.RobotState == rb::HuayanMachineState::SaftyGuardErrorHandling) ||
             (last_feedback.RobotState == rb::HuayanMachineState::Error) )&& (!m_error_stop)) {
            m_error_stop = true;
            resetCommandQueue();
            if (m_is_sq_runnning) {
                m_is_sq_runnning = false;
                m_is_sq_continuous = false;
                m_sq_stage = SqForceStop;
            }
            // qInfo
            emit s_RobotErrorStop();
        }

        bool temp_rb_moving = (m_feedback_data.RobotMoving==0) ? false : true;
        m_ctx_flag_transport->asReceiver().set_robot_moving_flag(temp_rb_moving);
        m_ctx_sq_flag_transport->asReceiver().set_robot_moving_flag(temp_rb_moving);
        if (!m_in_moving && temp_rb_moving) {
            m_in_moving = true;
        }

        if (m_error_stop && (last_feedback.RobotState == rb::HuayanMachineState::Disable)) {
            m_error_stop = false;
            emit s_RobotErrorHadReset();
        }

        bool is_home = this->isAtHome();
        if (m_stand_at_home != is_home) {
            m_stand_at_home = is_home;
            emit s_RobotAtHomePosition(is_home);
        }
    }
}

inline void RbHuayan::commandPortHandle() {
    if (m_in_moving && m_cmd_queue.isEmpty()) {
        m_cmd_queue.push_back(HyCmdFactory::ReadWayPointID());
        if (m_feedback_data.RobotMoving == 0) {
            m_in_moving = false;
        }
    }

    if ((!m_cmd_queue.isEmpty()) && m_current_cmd == nullptr) {
        m_current_cmd.reset();
        m_current_cmd = m_cmd_queue.first();
        m_current_cmd->setContext(m_cmd_interface);
    }

    if (m_current_cmd != nullptr) {
        if (m_current_cmd->isComplete()) {
            m_current_cmd.reset();
            m_cmd_queue.first().reset();
            m_cmd_queue.removeFirst();
        } else {
            // safe message transport service
            HyMsgReceiver &msg_receiver = m_ctx_msg_transport->asReceiver();
            HyFlagReceiver &flag_receiver = m_ctx_flag_transport->asReceiver();

            if (msg_receiver.is_sending_marked()) {
                m_command_port->SendMsg(msg_receiver.retrieve_sending_msg());
            } else {
                QList<HuayanMsgReturn> responses = m_command_port->ReceiveResponse();
                /// response handle
                if (!responses.isEmpty()) {
                    m_cmd_responses.append(responses);
                }

                // check valid response comming
                if (!m_cmd_responses.empty()) {
                    HuayanMsgReturn valid_response = m_cmd_responses.takeFirst();
                    msg_receiver.mark_response(valid_response);
                }
            }

            m_current_cmd->execute();

            if (flag_receiver.is_internal_flag_raised()) {
                internalFlagHandle(flag_receiver.retrieve_internal_flag());
            }
        }
    }
}

inline void RbHuayan::sqCommandPortHandle() {
    // get move command
    // if ((!m_sq_cmd_queue.empty()) && (m_sq_current_cmd == nullptr)) {
    //     m_sq_current_cmd.reset();
    //     m_sq_current_cmd = m_sq_cmd_queue.first();
    //     m_sq_current_cmd->setContext(m_sq_cmd_interface);
    // }

    RB_LOCK_DATA;
    if (m_vs_outside_fail) {
        m_vs_outside_fail = false;
        m_ctx_sq_flag_transport->asReceiver().raise_vision_fail();
    }

    if (m_vs_outside_reset) {
        m_vs_outside_reset = false;
        m_ctx_sq_flag_transport->asReceiver().clear_vs_coordinate();
    }

    if (m_vs_outside_new_coor) {
        m_vs_outside_new_coor = false;
        m_wait_for_coordinate = false;
        // m_vs_coordinate = m_vs_outside_coordinate;
        m_ctx_sq_flag_transport->asReceiver().set_vs_coordinate(m_vs_coordinate);
    }
    RB_UNLOCK_DATA;

    // execute move command
    if (m_sq_current_cmd != nullptr) {
        // safe message transport service
        HyMsgReceiver &msg_receiver = m_ctx_sq_msg_transport->asReceiver();
        HyFlagReceiver &flag_receiver = m_ctx_sq_flag_transport->asReceiver();

        // check if command complete
        if (m_sq_current_cmd->isComplete()) {
            // reset command for next time execute
            m_sq_current_cmd->resetCommand();
            // next index id error check
            if (m_sq_next_execute_idx >= m_sq_cmd_queue.size()) {
                m_sq_stage = SqRunningStage::SqLastCommandExecuted;
                return;
            }
            // set nex command pointer index
            setSqNextCommandPointer();

            int auto_request_index;
            if (flag_receiver.is_auto_request_setted(auto_request_index)) {
                // sequence command queue include 0 which is set flag start sequence command
                if (m_sq_current_line_number == auto_request_index) {
                    m_ctx_sq_flag_transport->clear_vs_coordinate();
                    OLOG_INFO << "vision auto request index found" << m_sq_current_line_number;

                    // request coordinate immediately if current command is not motion command
                    if (!m_sq_current_cmd->isMotionCommand()) {
                        m_ctx_sq_flag_transport->asSender().request_vs_coordinate();
                        OLOG_INFO << "vision request: not motion command trigger immediately";

                    // waiting for motion command finish to request coordinate
                    } else {
                        m_in_moving = true;
                        m_vision_trigger_waypoint_wait = true;
                        m_vision_trigger_wait_wp_finish = false;
                        m_vision_trigger_waypoint_id =
                            std::dynamic_pointer_cast<HuayanCommand>(m_sq_current_cmd)->comamndId();
                        OLOG_INFO << "control auto request vision match coordinate triggred, index:"
                                  << auto_request_index
                                  << m_vision_trigger_waypoint_id;
                    }
                }
            }

        } else {
            // OLOG_INFO << "Is command running";
            if (msg_receiver.is_sending_marked()) {
                m_command_sq_port->SendMsg(msg_receiver.retrieve_sending_msg());
            } else {
                QList<HuayanMsgReturn> responses = m_command_sq_port->ReceiveResponse();
                /// response handle
                if (!responses.isEmpty()) {
                    m_sq_cmd_responses.append(responses);
                }

                // check valid response comming
                if (!m_sq_cmd_responses.empty()) {
                    HuayanMsgReturn valid_response = m_sq_cmd_responses.takeFirst();
                    msg_receiver.mark_response(valid_response);
                }
            }

            m_sq_current_cmd->execute();

            if (m_ctx_sq_flag_transport->is_request_coordinate()) {
                OLOG_INFO << "Sequence control: vision coordinate triggerred" << m_wait_for_coordinate;
                m_ctx_sq_flag_transport->clear_vs_coordinate();
                RB_LOCK_DATA;
                if (!m_wait_for_coordinate) {
                    m_wait_for_coordinate = true;
                    emit s_RobotVisionTrigger();
                }
                RB_UNLOCK_DATA;
            }

            if (flag_receiver.is_internal_flag_raised()) {
                internalFlagHandle(flag_receiver.retrieve_internal_flag());
            }
        }
    }

    if (m_ctx_flag_transport->is_waypointID_changed() && m_vision_trigger_waypoint_wait) {
        QString waypoint_id = m_ctx_flag_transport->retrive_waypointId();
        if (!m_vision_trigger_wait_wp_finish) {
            if (waypoint_id == m_vision_trigger_waypoint_id) {
                m_vision_trigger_wait_wp_finish = true;
            }
        } else {
            if (waypoint_id != m_vision_trigger_waypoint_id) {
                m_vision_trigger_waypoint_wait = false;
                m_vision_trigger_wait_wp_finish = false;
                // emit s_RobotVisionTrigger();
                m_ctx_sq_flag_transport->asSender().request_vs_coordinate();
                OLOG_INFO << "Huayan control auto request vision match coordinate triggred"
                          << m_vision_trigger_waypoint_id;
            }
        }
    }
}

inline  void RbHuayan::autoRunHandle() {
    _auto_run_start_point:
    switch (m_sq_stage) {
    case SqRunningStage::SqIdle:
        if (m_is_sq_runnning) {
            if (m_sq_cmd_queue.empty()) {
                emit RobotAbstract::s_RobotSqQueueEmpty();
                OLOG_INFO << "Sequence command queue is empty.";
                m_is_sq_runnning = false;
                break;
            }

            // temporary
            // if (!m_plc_communicator->Fx3IsRunning()) {
            //     emit RobotAbstract::s_RobotSqRunStartFail();
            //     OLOG_INFO << "Sequence run start fail, plc not connect.";
            //     m_is_sq_runnning = false;
            //     break;
            // }

            OLOG_INFO << "Sequence control state switch to [Prepare for run]";
            emit RobotAbstract::s_RobotSqRunStarted(m_is_sq_continuous);
            m_sq_stage = SqPrepareForRun;
        }
        break;

    case SqRunningStage::SqPrepareForRun:
        RB_LOCK_DATA;
        m_ctx_sq_msg_transport->resetContext();
        m_ctx_sq_flag_transport->resetContext();
        m_wait_for_coordinate = false;
        m_vision_trigger_waypoint_wait = false;
        m_vision_trigger_wait_wp_finish = false;
        m_sq_line_number_map.clear();

        m_vs_outside_fail  = false;
        m_vs_outside_reset  = false;
        m_vs_outside_new_coor  = false;

        for (int index=0;index<m_sq_cmd_queue.count();index++) {
            m_sq_cmd_queue[index]->resetCommand();
            m_sq_line_number_map[index] = m_sq_cmd_queue[index]->getLineNumber();
            qInfo() << "Line number order:" << m_sq_line_number_map.value(index, -1);
        }

        // for (int index=0;index<m_sq_cmd_queue.size();index++) {
        //     m_sq_line_number_map[index] = m_sq_cmd_queue[index]->getLineNumber();
        // }

        setSqStartCommandPointer();
        m_sq_stage = SqRunning;
        OLOG_INFO << "Sequence control state switch to [Running]";
        RB_UNLOCK_DATA;
        goto _auto_run_start_point;
        break;

    case SqRunningStage::SqStartNewRun:
        // reset all command for new cycle
        for (int index=0;index<m_sq_cmd_queue.count();index++) {
            m_sq_cmd_queue[index]->resetCommand();
        }
        setSqStartCommandPointer();
        m_sq_stage = SqRunning;
        OLOG_INFO << "Sequence control state switch to [Running]";
        break;

    case SqRunningStage::SqRunning:
        if (!m_is_sq_runnning) {
            m_sq_stage = SqForceStop;
            OLOG_INFO << "Sequence control state switch to [Force stop]";
            goto _auto_run_start_point;
        }
        return;

    case SqRunningStage::SqLastCommandExecuted:
        if (m_is_sq_continuous) {
            emit s_RobotSingleCycleFinished();
            m_sq_stage = SqStartNewRun;
            OLOG_INFO << "Sequence control state switch to [Start new run]";
            goto _auto_run_start_point;
        } else {
            m_is_sq_runnning = false;
            setSqStopCommandPointer();
            // temporary set
            emit s_RobotSqRunFinished();
            m_sq_stage = SqRunningStage::SqIdle;
            OLOG_INFO << "Sequence control state switch to [Idle]";
        }
        break;

    case SqRunningStage::SqWaitStopMoving:
        if (!m_is_sq_runnning) {
            m_sq_stage = SqForceStop;
            OLOG_INFO << "Force Stop while waiting for robot stop movement, Sequence control state switch to [Prepare for run]";
            goto _auto_run_start_point;
        }

        if (m_feedback_data.RobotMoving == 0) {
            OLOG_INFO << "robot stop moving, switch to [Stop]";
            emit s_RobotSingleCycleFinished();
            m_sq_stage = SqRunningStage::SqStop;
            goto _auto_run_start_point;
        }
        return;

    case SqRunningStage::SqStop:
        m_is_sq_runnning = false;
        m_sq_stage = SqIdle;
        m_vision_trigger_waypoint_wait = false;
        m_vision_trigger_wait_wp_finish = false;
        OLOG_INFO << "switch to [Idle]";
        emit s_RobotSqRunFinished();
        break;

    case SqRunningStage::SqForceStop:        
        m_sq_current_cmd.reset();
        setSqStopCommandPointer();
        m_ctx_sq_msg_transport->resetContext();
        m_ctx_sq_flag_transport->resetContext();

        m_current_cmd.reset();
        m_cmd_queue.clear();
        m_ctx_msg_transport->resetContext();
        m_ctx_flag_transport->resetContext();

        OLOG_INFO << "Sequence forced stop, Switch to [Idle]";
        m_sq_stage = SqIdle;
        m_cmd_queue.push_back(HyCmdFactory::GrpStop());
        // m_cmd_queue.push_back(HyCmdFactory::GrpDisable());
        // m_cmd_queue.push_back(HyCmdFactory::GrpReset());
        m_vision_trigger_waypoint_wait = false;
        m_vision_trigger_wait_wp_finish = false;
        emit s_RobotForceStopped();
        break;
    }
}

// index rule: start from 0, if next index is equal or greater then command size
// -> end of sequence
inline void RbHuayan::setSqStartCommandPointer() {
    m_sq_next_execute_idx = -1;
    m_sq_previous_execute_idx = -1;
    m_sq_executing_idx = 0;
    if (m_sq_cmd_queue.isEmpty()) {
        OLOG_INFO << "Error, command Sequence is empty.";
        return;
    }

    m_sq_next_execute_idx = m_sq_executing_idx + 1;
    m_sq_current_cmd = m_sq_cmd_queue[m_sq_executing_idx];
    m_sq_current_cmd->setContext(m_sq_cmd_interface);
    m_sq_current_line_number = m_sq_current_cmd->getLineNumber();
    // emit start executing command
    emit s_StartExecutingCommand(m_sq_executing_idx);

    if (m_sq_current_cmd == nullptr) {
        OLOG_INFO << "Error, starting, current command pointer is null";
    }
}

inline void RbHuayan::setSqStopCommandPointer() {
    m_sq_next_execute_idx = -1;
    m_sq_previous_execute_idx = -1;
    m_sq_executing_idx = -1;
    if (m_sq_current_cmd != nullptr) {
        m_sq_current_cmd.reset();
    }
}

inline void RbHuayan::setSqNextCommandPointer() {
    if (m_sq_next_execute_idx >= m_sq_cmd_queue.size() ||
        (m_sq_next_execute_idx < 0)) {
        m_is_sq_runnning = false;
        OLOG_INFO << "Error, command pointer index invalid.";
        return;
    }

    m_sq_current_cmd = m_sq_cmd_queue[m_sq_next_execute_idx];
    m_sq_current_cmd->setContext(m_sq_cmd_interface);
    m_sq_current_line_number = m_sq_current_cmd->getLineNumber();
    // emit start executing command
    emit s_StartExecutingCommand(m_sq_executing_idx);

    m_sq_previous_execute_idx = m_sq_executing_idx;
    m_sq_executing_idx = m_sq_next_execute_idx;
    m_sq_next_execute_idx += 1;

    OLOG_INFO << "Next pointer setted" << m_sq_executing_idx;
    if (m_sq_current_cmd == nullptr) {
        OLOG_INFO << "Error, command index" << m_sq_executing_idx << ", command pointer is null";
    }
}

inline void RbHuayan::setSqCommandPointer(int line_number) {
    bool is_valid = m_sq_line_number_map.contains(line_number);
    if (!is_valid) {
        m_is_sq_runnning = false;
    }

    m_sq_previous_execute_idx = m_sq_executing_idx;
    m_sq_executing_idx = m_sq_line_number_map[line_number];
    m_sq_next_execute_idx = m_sq_executing_idx + 1;

    m_sq_current_cmd = m_sq_cmd_queue[m_sq_executing_idx];
    m_sq_current_cmd->setContext(m_sq_cmd_interface);
    m_sq_current_line_number = m_sq_current_cmd->getLineNumber();
    emit s_StartExecutingCommand(m_sq_executing_idx);
}

inline void RbHuayan::internalFlagHandle(HyInternalFlag flag) {
    switch (flag) {
    case rb::HyInternalFlag::HyfNone:
        OLOG_INFO << "Internal Flag Triggerred: Flag NONE";
        break;
    case rb::HyInternalFlag::HyfHomingStart:
        OLOG_INFO << "Internal Flag Triggerred: Robot Homing started";
        emit s_RobotStartHoming();
        break;
    case rb::HyInternalFlag::HyfHomingDone:
        OLOG_INFO << "Internal Flag Triggerred: Robot Homing done";
        emit s_RobotHomingFinished();
        break;
    case rb::HyInternalFlag::HyfStartSqCycle:
        OLOG_INFO << "Internal Flag Triggerred: Robot sequence start new cycle";
        emit RobotAbstract::s_RobotSingleCycleStarted(m_is_sq_continuous);
        break;
    case rb::HyInternalFlag::HyfEndSqCycle:
        OLOG_INFO << "Internal Flag Triggerred: Robot sequence end cycle";
        if (m_sq_stage == SqRunningStage::SqRunning) {
            qInfo() << "Sequence control state switch to [Last Command Executed]";
            m_sq_stage = SqRunningStage::SqLastCommandExecuted;
        }
        break;
    case rb::HyInternalFlag::HyfGetVisionCoordinate:

        break;

    case rb::HyInternalFlag::HyfTriggerVision:
        emit RobotAbstract::s_RobotVisionTrigger();
        OLOG_INFO << "Internal Flag Triggerred: Object dection triggered";
        break;

    case rb::HyInternalFlag::HyfJumpToLine:
        int new_line = m_ctx_sq_flag_transport->getJumpToLine();
        OLOG_INFO << "Internal Flag Triggerred: Jump to line triggered" << new_line;
        if (new_line <= -1) {
            if (m_sq_stage == SqRunningStage::SqRunning) {
                qInfo() << "Jump to end cycle, Sequence control state switch to [Last Command Executed]";
                m_sq_stage = SqRunningStage::SqLastCommandExecuted;
            }
        } else {
            setSqCommandPointer(new_line);
        }
        break;
    }
}

inline bool RbHuayan::isAtHome() {
    return ((isInRange(m_feedback_data.actual_position_cartesian.x(), m_home_position.x(), 0.15)) &&
            (isInRange(m_feedback_data.actual_position_cartesian.y(), m_home_position.y(), 0.15)) &&
            (isInRange(m_feedback_data.actual_position_cartesian.z(), m_home_position.z(), 0.15)) &&
            (isInRange(m_feedback_data.actual_position_cartesian.rx(), m_home_position.rx(), 0.15)) &&
            (isInRange(m_feedback_data.actual_position_cartesian.ry(), m_home_position.ry(), 0.15)) &&
            (isInRange(m_feedback_data.actual_position_cartesian.rz(), m_home_position.rz(), 0.15)));
}

inline bool RbHuayan::isInRange(double value_1, double value_2, double range) {
    return ((value_1 >= (value_2 - range)) && (value_1 <= (value_2 + range)));
}

}
