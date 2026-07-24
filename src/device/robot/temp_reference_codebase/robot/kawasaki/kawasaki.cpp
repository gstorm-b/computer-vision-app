#include "kawasaki.h"
#include "command/kw_command_factory.h"
#include "command/kawasaki_command.h"
#include "log_helper/log_wrapper.h"

#define LOG_NAME "[Kawasaki robot]"

#define AS_MOTION_PORT      31400
#define AS_STATUS_PORT      31401

namespace rb {

RbKawasaki::RbKawasaki(QObject *parent)
    : RobotAbstract(parent),
    m_fb_output_timeout(300),
    m_sq_stage(SqRunningStage::SqIdle),
    m_is_sq_runnning(false),
    m_is_sq_continuous(false),
    m_controller_running(false),
    m_robot_connected(false),
    m_is_ready_connect(true),
    m_feedback_lost_connect(false),
    m_is_first_feedback(false),
    m_emergency_stop_triggered(false),
    m_error_stop(false),
    m_status_enable(false),
    m_is_ready_for_repeat_run(false) {

}

RbKawasaki::~RbKawasaki() {
    robotTerminate();
}

bool RbKawasaki::RobotConnect(RbConnectParams *params) {
    bool state = false;

    KawasakiConnectParams *connect_params = static_cast<KawasakiConnectParams*>(params);

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

void RbKawasaki::RobotDisconnect() {
    RB_LOCK_DATA;
    if (m_controller_running) {
        m_controller_running = false;
    }
    RB_UNLOCK_DATA;
}

const bool RbKawasaki::RobotReadyForConnect() {
    bool state = false;
    RB_LOCK_DATA;
    state = m_is_ready_connect;
    RB_UNLOCK_DATA;
    return state;
}

const bool RbKawasaki::isRobotConnected() {
    bool ret = false;
    RB_LOCK_DATA;
    ret = m_robot_connected;
    RB_UNLOCK_DATA;
    return ret;
}
const bool RbKawasaki::isRobotDisconnected()  {
    return !this->isRobotConnected();
}

const bool RbKawasaki::RobotEnableState() {
    bool ret = false;
    RB_LOCK_DATA;
    ret = m_status_enable;
    RB_UNLOCK_DATA;
    return ret;
}

void RbKawasaki::RobotEmergencyStop() {
    RB_LOCK_DATA;
    m_emergency_stop_triggered = true;
    RB_UNLOCK_DATA;
}

CartesianPoint RbKawasaki::RobotCurrentPosision() {
    CartesianPoint point;
    RB_LOCK_DATA;
    point = m_feedback_data.current_pose_oat;
    RB_UNLOCK_DATA;
    return point;
}

const bool RbKawasaki::RobotHomeReturn() {
    return false;
}

void RbKawasaki::RobotRunContinuous() {
    // call command reset queue in robot before run


    RB_LOCK_DATA;
    if ((!m_is_sq_runnning) && (m_is_ready_for_repeat_run)) {
        m_is_sq_runnning = true;
        m_is_sq_continuous = true;
    }
    RB_UNLOCK_DATA;
}

void RbKawasaki::RobotRunSingleCycle() {
    RB_LOCK_DATA;
    if ((!m_is_sq_runnning) && (m_is_ready_for_repeat_run)) {
        m_is_sq_runnning = true;
        m_is_sq_continuous = false;
    }
    RB_UNLOCK_DATA;
}

void RbKawasaki::RobotStopCycle() {
    RB_LOCK_DATA;
    if (m_is_sq_runnning) {
        m_is_sq_continuous = false;
    }
    RB_UNLOCK_DATA;
}

void RbKawasaki::RobotForceStop() {
    RB_LOCK_DATA;
    if (m_is_sq_runnning) {
        m_is_sq_runnning = false;
    }
    RB_UNLOCK_DATA;
}

bool RbKawasaki::RobotIsSqRunning() {
    bool state;
    RB_LOCK_DATA;
    state = m_is_sq_runnning;
    RB_UNLOCK_DATA;
    return state;
}

bool RbKawasaki::RobotIsInError() {
    bool state;
    RB_LOCK_DATA;
    state = m_error_stop;
    RB_UNLOCK_DATA;
    return state;
}

void RbKawasaki::RobotPushMainSequence(QList<std::shared_ptr<RbCommand>> &cmd_sq) {
    RB_LOCK_DATA;
    if (!m_is_sq_runnning) {
        // clear main sequence and clone
        m_sq_cmd_queue.clear();
        for (int idx=0;idx<cmd_sq.size();idx++) {
            m_sq_cmd_queue.push_back(cmd_sq[idx]->clone());
        }

        m_sq_cmd_queue.push_front(KwCmdFactory::SetIFComamnd(KwInternalFlag::KwfStartSqCycle));
        m_sq_cmd_queue.push_back(KwCmdFactory::SetIFComamnd(KwInternalFlag::KwfEndSqCycle));
        OLOG_INFO << "Command sequence pushed";
    }
    RB_UNLOCK_DATA;
}


void RbKawasaki::SetPlcCommunicator(Fx3Communicator *plc_communicator) {
    RB_LOCK_DATA;
    m_plc_communicator = plc_communicator;
    // connect(m_plc_communicator, &Fx3Communicator::s_polling_update,
    //         this, &RbHuayan::UpdatePLCDeviceMap);
    RB_UNLOCK_DATA;
}

void RbKawasaki::VisionReturn(CartesianPoint coor) {
    RB_UNLOCK_DATA;
    m_vs_coordinate = coor;
    m_ctx_flag_transport->asReceiver().set_vs_coordinate(m_vs_coordinate);
    RB_UNLOCK_DATA;
    OLOG_INFO << "Receive matched point:" << coor.toQString();
}

void RbKawasaki::VisionFail() {
    RB_UNLOCK_DATA;
    m_ctx_flag_transport->asReceiver().raise_vision_fail();
    RB_UNLOCK_DATA;
}

void RbKawasaki::VisionReset() {
    RB_UNLOCK_DATA;
    m_ctx_flag_transport->asReceiver().clear_vs_coordinate();
    RB_UNLOCK_DATA;
}

void RbKawasaki::run() {
    /// Controller auto init TCP Socket when thread startings
    /// and clean it before thread stop

    OLOG_INFO << "robot communicator thread starting...";
    remoteControllerStart();

    while(m_controller_running) {
        // done
        emergencyStopHandle();
        // need to change some statement
        feedbackPortHandle();
        // auto run state handle
        autoRunHandle();
        // command execute
        if (!m_is_sq_runnning) {
            subCommandHandle();
        } else {
            sequenceCommandHandle();
        }

        // polling send out feedback
        if (m_feedback_output_timer->StartTimeCounter(m_fb_output_timeout)) {
            emit s_RobotFeedbackUpdate(this->m_feedback_data);
        }
        // m_is_ready_for_operate = (m_cmd_queue.isEmpty()) ? true : false;
    }

    // send disconnect command to set 2 port on robot controller to listen state again
    if (m_motion_port->state() == TCPClient::TcState::Connected) {
        m_motion_port->SendMsg(QString(KW_MOTION_DISCONNECT) + ",");
    }
    // clean and delete allocated memory
    remoteControllerClean();
    OLOG_INFO << "robot communicator thread terminate...";
}

void RbKawasaki::robotTerminate() {
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

void RbKawasaki::remoteControllerStart() {
    m_is_ready_connect = false;

    m_motion_port = std::make_unique<KawasakiMotionPort>();
    m_status_port = std::make_unique<KawasakiStatusPort>();

    TCPClient::TcState port_motion_state = m_motion_port->ConnectToPort(m_tcp_address, AS_MOTION_PORT);
    TCPClient::TcState port_status_state = m_status_port->ConnectToPort(m_tcp_address, AS_STATUS_PORT);

    if ((port_motion_state == TCPClient::TcState::ConnectFail)
        || (port_status_state == TCPClient::TcState::ConnectFail)) {
        // emit connect fail signal
        emit RobotAbstract::s_RobotConnectFailed();
    } else {
        if ((port_motion_state == TCPClient::TcState::Connected)
            && (port_status_state == TCPClient::TcState::Connected)) {
            // emit connect success signal
            m_robot_connected = true;
            emit RobotAbstract::s_RobotConnected();
        }
    }

    if (m_robot_connected) {
        resetStatusBeforeLoop();
        m_controller_running = true;
        OLOG_INFO << "TCP port connect successful...";
    } else {
        OLOG_INFO << "TCP port connect fail...";
    }
}

void RbKawasaki::remoteControllerClean() {
    m_motion_port->DisconnectFromPort();
    m_status_port->DisconnectFromPort();

    m_motion_port.release();
    m_status_port.release();

    if (m_robot_connected && !m_feedback_lost_connect) {
        emit RobotAbstract::s_RobotDisconnected();
    }

    m_feedback_lost_connect = false;
    m_robot_connected = false;
    m_is_ready_connect = true;
    m_status_enable = false;
    m_is_sq_runnning = false;
    m_is_sq_continuous = false;
    // m_sq_execute_counter = 0;
    m_emergency_stop_triggered = false;
    m_error_stop = false;
    m_sq_stage = SqRunningStage::SqIdle;
}

void RbKawasaki::resetStatusBeforeLoop() {
    m_is_first_feedback = true;
    m_stand_at_home = false;

    m_emergency_stop_triggered = false;
    m_error_stop = false;

    m_feedback_output_timer.reset(new ChronoCounter());

    // Reset all command contexts
    if (m_ctx_msg_transport != nullptr) {
        m_ctx_msg_transport.reset();
    }
    m_ctx_msg_transport = std::make_shared<KwMsgTransport>();

    if (m_ctx_flag_transport != nullptr) {
        m_ctx_flag_transport.reset();
    }
    // allocate flag transport service with robot id = 0
    m_ctx_flag_transport = std::make_shared<KwFlagTransport>();

    if (m_cmd_interface != nullptr) {
        m_cmd_interface.reset();
    }
    m_cmd_interface = std::make_shared<KawasakiCmdInterface>(
        m_ctx_msg_transport->asSender(), m_ctx_flag_transport->asSender());

    m_ctx_flag_transport->set_external_input_ptr(m_feedback_data.external_input);
    m_ctx_flag_transport->set_external_output_ptr(m_feedback_data.external_output);
    m_ctx_flag_transport->set_internal_signal_ptr(m_feedback_data.internal_signal);
    m_ctx_flag_transport->set_plc_communicator_ptr(m_plc_communicator);

    m_sq_cmd_queue.clear();
    m_current_cmd.reset();
    m_sq_executing_idx = -1;
    m_sq_next_execute_idx = -1;
    m_sq_previous_execute_idx = -1;

    m_sub_cmd_queue.clear();
    m_sub_current_cmd.reset();

    // // Reset all command contexts of sequence queue
    // if (m_ctx_sq_msg_transport != nullptr) {
    //     m_ctx_sq_msg_transport.reset();
    // }
    // m_ctx_sq_msg_transport = std::make_shared<HyMsgTransport>();

    // if (m_ctx_sq_flag_transport != nullptr) {
    //     m_ctx_sq_flag_transport.reset();
    // }
    // // allocate flag transport service with robot id = 0
    // m_ctx_sq_flag_transport = std::make_shared<HyFlagTransport>(0);

    // if (m_cmd_sq_interface != nullptr) {
    //     m_cmd_sq_interface.reset();
    // }
    // m_cmd_sq_interface = std::make_shared<HuayanCmdInterface>(
    //     m_ctx_sq_msg_transport->asSender(), m_ctx_sq_flag_transport->asSender());

    // m_ctx_sq_msg_transport->resetContext();
    // m_ctx_sq_flag_transport->resetContext();
    // m_ctx_sq_flag_transport->set_global_tcp_name(m_normal_tcp_name);
    // m_ctx_sq_flag_transport->set_global_ucs_name(m_normal_ucs_name);
    // m_ctx_sq_flag_transport->set_boxdo_ptr(m_feedback_data.BoxDO);
    // m_ctx_sq_flag_transport->set_boxdi_ptr(m_feedback_data.BoxDI);
    // m_ctx_sq_flag_transport->set_enddo_ptr(m_feedback_data.EndDO);
    // m_ctx_sq_flag_transport->set_enddi_ptr(m_feedback_data.EndDI);
    // m_ctx_sq_flag_transport->set_plc_communicator_ptr(m_plc_communicator);

    // m_cmd_queue.clear();
    // m_current_cmd.reset();

    // if (!m_cmd_sq_queue.empty()) {
    //     m_cmd_sq_queue.clear();
    // }
    // m_current_sq_cmd.reset();
    // m_sq_stage = SqRunningStage::SqIdle;

    m_is_sq_runnning = false;
    m_is_sq_continuous = false;

    m_emergency_stop_triggered = false;
    m_error_stop = false;
}

inline void RbKawasaki::emergencyStopHandle() {
    // if (m_emergency_stop_triggered) {
    //     resetCommandQueue();
    //     m_cmd_queue.push_back(HyCmdFactory::GrpStop());
    //     m_cmd_queue.push_back(HyCmdFactory::GrpDisable());
    //     m_is_sq_runnning = false;
    //     m_in_moving = false;
    //     m_sq_stage = SqForceStop;
    //     m_emergency_stop_triggered = false;
    //     m_error_stop = true;
    //     emit s_RobotErrorStop();
    // }
}

inline void RbKawasaki::resetCommandQueue() {
    // remove all command in queue and current executing command
    m_current_cmd.reset();
    // m_sq_cmd_queue.clear();
    // reset all command context
    m_ctx_msg_transport->resetContext();
    m_ctx_flag_transport->resetContext();
}

inline  void RbKawasaki::autoRunHandle() {
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

            // temporary debug
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
        for (int index=0;index<m_sq_cmd_queue.count();index++) {
            m_sq_cmd_queue[index]->resetCommand();
        }
        m_motion_port->clearBuffer();
        setSqStartCommandPointer();
        m_ctx_msg_transport->resetContext();
        m_ctx_flag_transport->resetContext();
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
            m_sq_stage = SqPrepareForRun;
            OLOG_INFO << "Sequence control state switch to [Prepare for run]";
        } else {
            m_is_sq_runnning = false;
            setSqStopCommandPointer();
            // temporary set
            emit s_RobotSqRunFinished();
            m_sq_stage = SqRunningStage::SqIdle;
            OLOG_INFO << "Sequence control state switch to [Idle]";
        }
        break;

    case SqRunningStage::SqWaitFinish:
        if (!m_is_sq_runnning) {
            m_sq_stage = SqForceStop;
            OLOG_INFO << "Sequence control state switch to [Force stop] while waiting for finish remain commands";
            goto _auto_run_start_point;
        }
        return;

    case SqRunningStage::SqForceStop:
        m_motion_port->SendMsg(QString(KW_MOTION_STOP) + ",");

        setSqStopCommandPointer();
        m_ctx_msg_transport->resetContext();
        m_ctx_flag_transport->resetContext();
        OLOG_INFO << "Sequence control state switch to [SqIdle]";

        // m_current_sq_cmd.reset();
        // m_cmd_queue.clear();
        // m_cmd_sq_queue.clear();
        m_sq_stage = SqIdle;
        emit s_RobotForceStopped();
        break;
    }
}

inline void RbKawasaki::feedbackPortHandle() {
    // polling receive feedback
    if (!m_status_port->polling()) {
        if (m_status_port->state() == TCPClient::TcState::Error) {
            OLOG_WARNING << "status port exception catched, close thread!";
        } else {
            OLOG_WARNING << "TCP port lost connected!";
        }
        emit s_RobotConnectFailed();
        m_feedback_lost_connect = true;
        m_controller_running = false;
        return;
    }

    // // only handle when new package came
    if (m_status_port->retrieveReadStatus()) {
        KawasakiSheet last_feedback = m_feedback_data;
        m_feedback_data = m_status_port->GetFeedbackData();

        m_feedback_data.current_pose_rpy = eulerZYZ_2_XYZ(m_feedback_data.current_pose_oat);

        m_status_enable = (m_feedback_data.state_power == 1) ? true : false;
        m_is_ready_for_repeat_run = (m_feedback_data.state_power == 1) &
                                    (m_feedback_data.state_cycle_start == 1) &
                                    (m_feedback_data.state_run == 1) &
                                    (m_feedback_data.state_repeat == 1);

        bool temp_error = false;
        if (m_feedback_data.state_emergency != last_feedback.state_emergency) {
            temp_error = m_feedback_data.state_emergency;
        }

        if (m_feedback_data.state_error != last_feedback.state_error) {
            temp_error = m_feedback_data.state_error | temp_error;
        }

        if (m_error_stop != temp_error) {
            if (!m_error_stop) {
                m_error_stop = true;
                resetCommandQueue();
                if (m_is_sq_runnning) {
                    m_is_sq_runnning = false;
                    m_is_sq_continuous = false;
                }

                OLOG_INFO << ((m_feedback_data.state_emergency) ? "robot error stop" : "robot emergency stop");
                emit s_RobotErrorStop();
            } else {
                OLOG_INFO << "robot error reset";
                emit s_RobotErrorHadReset();
                m_error_stop = false;
            }
        }

        // if ((m_feedback_data.state_error == 1) && (!m_error_stop)) {
        //     m_error_stop = true;
        //     resetCommandQueue();
        //     if (m_is_sq_runnning) {
        //         m_is_sq_runnning = false;
        //         m_is_sq_continuous = false;
        //     }
        //     OLOG_INFO << "robot error stop";
        //     emit s_RobotErrorStop();
        // } else if ((m_feedback_data.state_error == 0) && (m_error_stop)) {
        //     m_error_stop = false;
        // }

        bool is_home = this->isAtHome();
        if (m_stand_at_home != is_home) {
            m_stand_at_home = is_home;
            emit s_RobotAtHomePosition(is_home);
        }
    }
}

inline void RbKawasaki::subCommandHandle() {
    // get move command
    if ((!m_sub_cmd_queue.empty()) && (m_sub_current_cmd == nullptr)) {
        m_sub_current_cmd.reset();
        m_sub_current_cmd = m_sub_cmd_queue.first();
        m_sub_current_cmd->setContext(m_cmd_interface);
    }

    // if (m_current_cmd != nullptr) {
    //     if (m_current_cmd->isComplete()) {
    //         m_current_cmd.reset();
    //         m_cmd_queue.first().reset();
    //         m_cmd_queue.removeFirst();
    //     } else {
    //         // safe message transport service
    //         HyMsgReceiver &msg_receiver = m_ctx_msg_transport->asReceiver();

    //         if (msg_receiver.is_sending_marked()) {
    //             m_command_port->SendMsg(msg_receiver.retrieve_sending_msg());
    //         } else {
    //             QList<HuayanMsgReturn> responses = m_command_port->ReceiveResponse();
    //             /// response handle
    //             if (!responses.isEmpty()) {
    //                 m_cmd_responses.append(responses);
    //             }

    //             // check valid response comming
    //             if (!m_cmd_responses.empty()) {
    //                 HuayanMsgReturn valid_response = m_cmd_responses.takeFirst();
    //                 msg_receiver.mark_response(valid_response);
    //             }
    //         }

    //         m_current_cmd->execute();
    //     }
    // }
}

inline void RbKawasaki::sequenceCommandHandle() {
    // execute move command
    if (m_current_cmd != nullptr) {
        // message transport service
        KwMsgReceiver &msg_receiver = m_ctx_msg_transport->asReceiver();
        KwFlagReceiver &flag_receiver = m_ctx_flag_transport->asReceiver();

        // check if command complete
        if (m_current_cmd->isComplete()) {
            // reset command for next time execute
            m_current_cmd->resetCommand();
            // next index id error check
            if (m_sq_next_execute_idx > m_sq_cmd_queue.size()) {
                m_sq_stage = SqRunningStage::SqLastCommandExecuted;
                return;
            }
            // set nex command pointer index
            setSqNextCommandPointer();
        } else {
            // message send request
            if (msg_receiver.is_sending_marked()) {
                m_motion_port->SendMsg(msg_receiver.retrieve_sending_msg());
            } else {
                QList<KawasakiMsgReturn> responses = m_motion_port->ReceiveResponse();
                /// response handle
                if (!responses.isEmpty()) {
                    m_cmd_sq_responses.append(responses);
                }

                // check valid response comming
                if (!m_cmd_sq_responses.empty()) {
                    KawasakiMsgReturn valid_response = m_cmd_sq_responses.takeFirst();
                    msg_receiver.mark_response(valid_response);
                }
            }

            // command execute
            m_current_cmd->execute();

            // if (flag_receiver.is_request_coordinate()) {
            //     emit s_RobotVisionTrigger();
            // }

            // internal flag handle
            if (flag_receiver.is_internal_flag_raised()) {
                internalFlagHandle(flag_receiver.retrieve_internal_flag());
            }
        }
    }
}

// index rule: start from 0, if next index is equal or greater then command size
// -> end of sequence
inline void RbKawasaki::setSqStartCommandPointer() {
    m_sq_next_execute_idx = -1;
    m_sq_previous_execute_idx = -1;
    m_sq_executing_idx = 0;
    if (m_sq_cmd_queue.isEmpty()) {
        OLOG_INFO << "Error, command Sequence is empty.";
        return;
    }

    m_current_cmd = m_sq_cmd_queue[m_sq_executing_idx];
    m_current_cmd->setContext(m_cmd_interface);
    // emit start executing command
    emit s_StartExecutingCommand(m_sq_executing_idx);
    m_sq_next_execute_idx = m_sq_executing_idx + 1;

    if (m_current_cmd == nullptr) {
        OLOG_INFO << "Error, starting, current command pointer is null";
    }
}

inline void RbKawasaki::setSqStopCommandPointer() {
    m_sq_next_execute_idx = -1;
    m_sq_previous_execute_idx = -1;
    m_sq_executing_idx = -1;
    if (m_current_cmd != nullptr) {
        m_current_cmd.reset();
    }
}

inline void RbKawasaki::setSqNextCommandPointer() {
    if (m_sq_next_execute_idx >= m_sq_cmd_queue.size() ||
        (m_sq_next_execute_idx < 0)) {
        OLOG_INFO << "Error, command Sequence is empty.";
        return;
    }
    m_current_cmd = m_sq_cmd_queue[m_sq_next_execute_idx];
    m_current_cmd->setContext(m_cmd_interface);
    m_sq_previous_execute_idx = m_sq_executing_idx;
    m_sq_executing_idx = m_sq_next_execute_idx;
    m_sq_next_execute_idx += 1;
    // emit start executing command
    emit s_StartExecutingCommand(m_sq_executing_idx);

    if (m_current_cmd == nullptr) {
        OLOG_INFO << "Error, command index" << m_sq_executing_idx << ", command pointer is null";
    }
}

inline void RbKawasaki::internalFlagHandle(KwInternalFlag flag) {
    switch (flag) {
    case rb::KwInternalFlag::KwfNone:
        qInfo() << "Internal Flag Trigger: Flag NONE";
        break;
    case rb::KwInternalFlag::KwfHomingStart:
        qInfo() << "Internal Flag Trigger: Robot Homing started";
        emit s_RobotStartHoming();
        break;
    case rb::KwInternalFlag::KwfHomingDone:
        qInfo() << "Internal Flag Trigger: Robot Homing done";
        emit s_RobotHomingFinished();
        break;
    case rb::KwInternalFlag::KwfStartSqCycle:
        qInfo() << "Internal Flag Trigger: Robot sequence start new cycle";
        emit RobotAbstract::s_RobotSingleCycleStarted(m_is_sq_continuous);
        break;
    case rb::KwInternalFlag::KwfEndSqCycle:
        qInfo() << "Internal Flag Trigger: Robot sequence end cycle, Sequence control state switch to [Last Command Executed]";
        m_sq_stage = SqRunningStage::SqLastCommandExecuted;
        break;
    case rb::KwInternalFlag::KwfGetVisionCoordinate:

        break;

    case rb::KwInternalFlag::KwfTriggerVision:
        emit RobotAbstract::s_RobotVisionTrigger();
        qInfo() << "Internal Flag Trigger: Object dection triggered";
        break;
    }
}

inline bool RbKawasaki::isAtHome() {
    return ((isInRange(m_feedback_data.current_pose_rpy.x(), m_home_position.x(), 0.15)) &&
            (isInRange(m_feedback_data.current_pose_rpy.y(), m_home_position.y(), 0.15)) &&
            (isInRange(m_feedback_data.current_pose_rpy.z(), m_home_position.z(), 0.15)) &&
            (isInRange(m_feedback_data.current_pose_rpy.rx(), m_home_position.rx(), 0.15)) &&
            (isInRange(m_feedback_data.current_pose_rpy.ry(), m_home_position.ry(), 0.15)) &&
            (isInRange(m_feedback_data.current_pose_rpy.rz(), m_home_position.rz(), 0.15)));
}

inline bool RbKawasaki::isInRange(double value_1, double value_2, double range) {
    return ((value_1 >= (value_2 - range)) && (value_1 <= (value_2 + range)));
}

}
