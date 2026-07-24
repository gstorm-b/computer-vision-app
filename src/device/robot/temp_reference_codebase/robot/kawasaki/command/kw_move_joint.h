#ifndef KW_MOVE_JOINT_H
#define KW_MOVE_JOINT_H

// #include "utils/chronocounter.h"
// #include "rbot/rbutils.h"
#include "kawasaki_command.h"
#include "robot/kawasaki/kawasaki_motion.h"

namespace rb {

class KwMoveJ : public KawasakiCommand {
public:
    KwMoveJ(JointPoint target, double velocity, double accel, double accuracy) :
        KawasakiCommand(),
        m_send_cmd(""),
        m_target_joint(target),
        m_velocity(velocity),
        m_acceleration(accel),
        m_accuracy(accuracy) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_stage = MoveStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawsaki command MoveJ error, m_interface pointer null.";
            return;
        }

    _start_point:
        switch (m_stage) {
        case MoveStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_stage = MoveStage::SendMoveCommand;
            goto _start_point;
            break;

        case MoveStage::SendMoveCommand:
            send_move_command();
            break;

        case MoveStage::WaitMoveCommandReponse:
            wait_command_response();
            break;

        case MoveStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case MoveStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    inline void send_move_command() {
        if (m_velocity > 100.0) {
            m_velocity = 100.0;
        }

        if (m_acceleration > 100.0) {
            m_velocity = 100.0;
        }

        m_send_cmd = QString(KW_MOTION_MOVEJ) + "," +
                     m_target_joint.toQString() + "," +
                     QString::number(m_velocity, 'f', 3)  + "," +
                     QString::number(m_acceleration, 'f', 3)  + "," +
                     QString::number(m_accuracy, 'f', 3)  + ",";

        m_interface->msg.send_msg(m_send_cmd);
        // set command state to wait for response
        m_stage = MoveStage::WaitMoveCommandReponse;
    }

    inline void wait_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv move") {
                    qDebug() << "Kawsaki command MoveJ, found response, return success";
                    m_stage = MoveStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command MoveJ error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command MoveJ error, ressponse instance return null pointer.";
            }
            m_stage = MoveStage::Error;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwMoveJ>(*this);
    }

private:
    enum MoveStage {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        Finish,
        Error
    };

private:
    MoveStage m_stage = MoveStage::UnExecute;
    QString m_send_cmd;

    JointPoint m_target_joint;
    double m_velocity{50};
    double m_acceleration{50};
    double m_accuracy{0};
};

}

#endif // KW_MOVE_JOINT_H
