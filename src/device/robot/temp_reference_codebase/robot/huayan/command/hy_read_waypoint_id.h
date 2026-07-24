#ifndef HY_READ_WAYPOINT_ID_H
#define HY_READ_WAYPOINT_ID_H


#include "robot/huayan/huayan_command.h"

namespace rb {

class HyReadWayPointID : public HuayanCommand {
public:
    HyReadWayPointID() :
        HuayanCommand(),
        m_cmd_state(ReadWayPointState::UnExecute) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = ReadWayPointState::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyReadWayPointID m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

        _start_point:
        switch (m_cmd_state) {
        case ReadWayPointState::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_cmd_state = ReadWayPointState::SendComamnd;
            goto _start_point;
            break;

        case ReadWayPointState::SendComamnd:
            m_command_str = m_command_header + ","
                            + m_interface->flag.get_robot_id_str() + ",;";
            m_interface->msg.send_msg(m_command_str);
            m_cmd_state = ReadWayPointState::WaitFeedBack;
            break;

        case ReadWayPointState::WaitFeedBack:
            wait_feedback();
            break;

        case ReadWayPointState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case ReadWayPointState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyReadWayPointID>(*this);
    }

    inline void wait_feedback() {
        if (m_interface->msg.is_response_received()) {
            HuayanMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandHeader() == m_command_header) {
                    // qInfo() << "Huayan robot controller [HyReadWayPointID]: Receivedddd";

                    if (response->isCommandOK()) {
                        // qInfo() << "Huayan robot controller [HyReadWayPointID]: found response, return success";
                        // set command state wait finished movement
                        if (response->hasParams()) {
                            QString waypoint_id = response->GetParams().first();
                            m_interface->flag.set_new_current_waypoint(waypoint_id);
                            qInfo() << "Huayan robot controller [HyReadWayPointID]: current waypoint ID" << waypoint_id;
                        } else {
                            qInfo() << "Huayan robot controller [HyReadWayPointID]: no current waypoint ID";
                        }
                        m_cmd_state = ReadWayPointState::Finish;
                        return;
                    } else {
                        qWarning() << "Huayan robot controller [HyReadWayPointID]: found response, return error"
                                 << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;
                    }
                } else {
                    qWarning() << "Huayan robot controller [HyReadWayPointID]: wrong command response header";
                }
            } else {
                qCritical() << "Huayan robot controller [HyReadWayPointID]: fail, ressponse instance return null pointer.";
            }
            m_cmd_state = ReadWayPointState::Error;
        }
    }

private:
    enum ReadWayPointState {
        UnExecute = 0,
        SendComamnd,
        WaitFeedBack,
        Finish,
        Error
    };

    ReadWayPointState m_cmd_state = ReadWayPointState::UnExecute;
    QString m_current_waypoint_id;

    const QString m_command_header = "ReadCurWayPointID";
    QString m_command_str;
};

}

#endif // HY_READ_WAYPOINT_ID_H
