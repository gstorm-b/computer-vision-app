#ifndef HY_MOVEL_H
#define HY_MOVEL_H

#include "utils/chronocounter.h"
// #include "rbot/rbutils.h"
#include "robot/huayan/huayan_command.h"

namespace rb {

class HyMoveL : public HuayanCommand {
public:
    HyMoveL(CartesianPoint point, QString tcp = "TCP", QString ucs = "Base") :
        HuayanCommand(), m_end_point(point),
        m_tcp_name(tcp),
        m_ucs_name(ucs) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = MoveLState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyMoveL m_interface pointer null.";
            return;
        }

        _start_point:
        switch (m_cmd_state) {
        case MoveLState::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_cmd_state = MoveLState::SendMoveCommand;
            this->m_interface->flag.set_flag_start_jog();
            goto _start_point;
            break;

        case MoveLState::SendMoveCommand:
            send_move_command();
            break;

        case MoveLState::WaitMoveCommandReponse:
            wait_move_command_response();
            break;

        case MoveLState::SendLongMoveEventCommand:
            send_long_move_command();
            break;

        case MoveLState::WaitLongMoveEventCommandResponse:
            wait_long_move_comamnd_response();
            break;

        case MoveLState::WaitCycle:
            if (m_interface->flag.has_jog_cmd_stop_raised() ||
                (!m_interface->flag.is_robot_moving())) {
            // if (m_interface->flag.has_jog_cmd_stop_raised()) {
                m_cmd_state = MoveLState::WaitStop;
                // goto _start_point;
                break;
            }

            if (m_time_count.StartTimeCounter(50)) {
                m_cmd_state = MoveLState::SendLongMoveEventCommand;
            }
            break;

        case MoveLState::WaitStop:
            if (!m_interface->flag.is_robot_moving()) {
                qInfo() << "MoveLTo Robot stop moving, switch to finish";
                m_cmd_state = MoveLState::Finish;
                goto _start_point;
            }
            break;

        case MoveLState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case MoveLState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }


    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyMoveL>(*this);
    }

private:
    enum MoveLState {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        // WaitMoving,
        SendLongMoveEventCommand,
        WaitLongMoveEventCommandResponse,
        WaitCycle,
        WaitStop,
        Finish,
        Error
    };

    inline void send_move_command() {
        // update tcp and ucs name
        if (m_tcp_name.isEmpty()) {
            m_tcp_name = m_interface->flag.get_gobal_tcp_name();
        }

        if (m_ucs_name.isEmpty()) {
            m_ucs_name = m_interface->flag.get_gobal_ucs_name();
        }

        m_movel_cmd = m_movel_cmd_header + "," +
                      m_interface->flag.get_robot_id_str() + "," +
                      m_end_point.toQString() + "," +
                      m_tcp_name+ "," +
                      m_ucs_name + ",;";
        m_interface->msg.send_msg(m_movel_cmd);
        // set command state to wait for response
        m_cmd_state = MoveLState::WaitMoveCommandReponse;
    }

    inline void wait_move_command_response() {
        if (m_interface->msg.is_response_received()) {
            HuayanMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandHeader() == m_movel_cmd_header) {
                    if (response->isCommandOK()) {
                        qDebug() << "Huayan robot controller [HyMoveL]: found response, return success";
                        // set command state wait finished movement
                        m_cmd_state = MoveLState::SendLongMoveEventCommand;
                        return;
                    } else {
                        qDebug() << "Huayan robot controller [HyMoveL]: found response, return error"
                                 << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain();
                        /// set command state to error
                    }
                } else {
                    qDebug() << "Huayan robot controller [HyMoveL]: wrong command response header";
                }
            } else {
                qCritical() << "HyMoveL fail, ressponse instance return null pointer.";
            }
            m_cmd_state = MoveLState::Error;
        }
    }

    inline void send_long_move_command() {
        // if (!m_interface->flag.is_robot_moving()) {
        //     qInfo() << "ROBOT Stop Moving, move to finish";
        //     m_cmd_state = MoveLState::Finish;
        //     return;
        // }

        QString long_move_cmd = "LongMoveEvent," + m_interface->flag.get_robot_id_str() + ",;";
        m_interface->msg.send_msg(long_move_cmd);
        // set command state to wait for response
        m_cmd_state = MoveLState::WaitLongMoveEventCommandResponse;
    }

    inline void wait_long_move_comamnd_response() {
        if (m_interface->msg.is_response_received()) {
            HuayanMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandHeader() == "LongMoveEvent") {
                    if (response->isCommandOK()) {
                        // qDebug() << "Huayan robot controller [HyMoveL]: found response, return success.";
                        // set command state to send long move event command
                        // if (m_interface->flag.has_jog_cmd_stop_raised()) {
                        //     m_cmd_state = MoveLState::Finish;
                        // } else {
                        //     m_time_count.StopTimeCounter();
                        //     m_cmd_state = MoveLState::WaitCycle;
                        // }
                        m_time_count.StopTimeCounter();
                        m_cmd_state = MoveLState::WaitCycle;
                        return;
                    } else {
                        // qDebug() << "Huayan robot controller [HyMoveL]: found response, return error"
                                 // << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain();

                        /// set command state to error
                    }
                } else {
                    // qDebug() << "Huayan robot controller [HyMoveL]: wrong command response header";
                }
            } else {
                qCritical() << "HyAlignToZ fail, ressponse instance return null pointer.";
            }
            m_cmd_state = MoveLState::Error;
        }
    }

private:
    MoveLState m_cmd_state = MoveLState::UnExecute;
    QString m_movel_cmd;
    const QString m_movel_cmd_header{"MoveLTo"};

    ChronoCounter m_time_count;

    CartesianPoint m_end_point;
    QString m_tcp_name{"TCP"};
    QString m_ucs_name{"Base"};
};

}

#endif // HY_MOVEL_H
