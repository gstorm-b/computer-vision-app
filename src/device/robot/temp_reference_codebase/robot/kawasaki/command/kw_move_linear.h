#ifndef KW_MOVE_LINEAR_H
#define KW_MOVE_LINEAR_H

// #include "utils/chronocounter.h"
// #include "rbot/rbutils.h"
#include "kawasaki_command.h"
#include "robot/kawasaki/kawasaki_motion.h"

namespace rb {

class KwMoveL : public KawasakiCommand {
public:
    KwMoveL(CartesianPoint point, double velocity, double accel, double accuracy, bool use_rpy, bool is_vs_coordinate) :
        KawasakiCommand(),
        m_stage(KwMoveL::MoveStage::UnExecute),
        m_send_cmd(""),
        m_end_point(point),
        m_velocity(velocity),
        m_acceleration(accel),
        m_accuracy(accuracy),
        m_is_use_rpy(use_rpy),
        m_is_use_vs_point(is_vs_coordinate) {

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
            qCritical() << "Kawsaki command MoveL error, m_interface pointer null.";
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
        if (m_is_use_vs_point) {
            if (!m_interface->flag.is_vs_coordinate_available()) {
              return;
            }

            m_end_point = m_interface->flag.get_vs_coordinate();
        }

        if (m_velocity > 100.0) {
            m_velocity = 100.0;
        }

        if (m_acceleration > 100.0) {
            m_velocity = 100.0;
        }

        if (m_is_use_rpy || m_is_use_vs_point) {
            CartesianPoint zyz_point = eulerXYZ_2_ZYZ(m_end_point);
            // qInfo() << zyz_point.toQString();
            m_send_cmd = QString(KW_MOTION_MOVEL) + "," +
                         zyz_point.toQString() + "," +
                         QString::number(m_velocity, 'f', 3)  + "," +
                         QString::number(m_acceleration, 'f', 3)  + "," +
                         QString::number(m_accuracy, 'f', 3)  + ",";
        } else {
            m_send_cmd = QString(KW_MOTION_MOVEL) + "," +
                         m_end_point.toQString() + "," +
                         QString::number(m_velocity, 'f', 3)  + "," +
                         QString::number(m_acceleration, 'f', 3)  + "," +
                         QString::number(m_accuracy, 'f', 3)  + ",";
        }

        m_interface->msg.send_msg(m_send_cmd);
        // set command state to wait for response
        m_stage = MoveStage::WaitMoveCommandReponse;
    }

    inline void wait_command_response() {
        if (m_interface->msg.is_response_received()) {
            KawasakiMsgReturn *response = m_interface->msg.retrieve_response();
            if (response != nullptr) {
                if (response->GetCommandCode() == "recv move") {
                    qDebug() << "Kawsaki command MoveL, found response, return success";
                    m_stage = MoveStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command MoveL error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command MoveL error, ressponse instance return null pointer.";
            }
            m_stage = MoveStage::Error;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwMoveL>(*this);
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

    CartesianPoint m_end_point;
    double m_velocity{50};
    double m_acceleration{50};
    double m_accuracy{0};
    bool m_is_use_rpy{false};
    bool m_is_use_vs_point{false};
};

}

#endif // KW_MOVE_LINEAR_H
