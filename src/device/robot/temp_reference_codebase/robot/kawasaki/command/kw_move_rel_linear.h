#ifndef KW_MOVE_REL_LINEAR_H
#define KW_MOVE_REL_LINEAR_H

#include "kawasaki_command.h"

namespace rb {

class KwMoveRelL : public KawasakiCommand {
public:
    KwMoveRelL(CartesianPoint target_distance,
               CartesianPoint ref_point,
               double velocity, double accel, double accuracy,
               bool use_ref_point, bool use_vs_point = false) :
        KawasakiCommand(),
        m_stage(KwMoveRelL::MoveStage::UnExecute),
        m_send_cmd(""),
        m_end_point(),
        m_distance(target_distance),
        m_ref_point(ref_point),
        m_velocity(velocity),
        m_acceleration(accel),
        m_accuracy(accuracy),
        m_is_use_ref_point(use_ref_point),
        m_is_use_vs_point(use_vs_point){

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
            qCritical() << "Kawsaki command MoveRelL error, m_interface pointer null.";
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

        // term debug
        if (!m_is_use_ref_point) {
            m_stage = MoveStage::Finish;
            return;
        }

        if (m_is_use_vs_point) {
            if (!m_interface->flag.is_vs_coordinate_available()) {
                return;
            }

            m_ref_point = m_interface->flag.get_vs_coordinate();
            m_end_point.setX(m_ref_point.x() + m_distance.x());
            m_end_point.setY(m_ref_point.y() + m_distance.y());
            m_end_point.setZ(m_ref_point.z() + m_distance.z());
            m_end_point.setRx(m_ref_point.rx() + m_distance.rx());
            m_end_point.setRy(m_ref_point.ry() + m_distance.ry());
            m_end_point.setRz(m_ref_point.rz() + m_distance.rz());
        }

        if (m_velocity > 100.0) {
            m_velocity = 100.0;
        }

        if (m_acceleration > 100.0) {
            m_velocity = 100.0;
        }


        m_send_cmd = QString(KW_MOTION_MOVEL) + "," +
                     m_end_point.toQString() + "," +
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
                    qDebug() << "Kawsaki command MoveRelL, found response, return success";
                    m_stage = MoveStage::Finish;
                    return;
                } else {
                    qDebug() << "Kawsaki command MoveRelL error, reponse not as expected" << response->GetCommandCode();
                }
            } else {
                qCritical() << "Kawsaki command MoveRelL error, ressponse instance return null pointer.";
            }
            m_stage = MoveStage::Error;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwMoveRelL>(*this);
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
    CartesianPoint m_distance;
    CartesianPoint m_ref_point;
    double m_velocity{50};
    double m_acceleration{50};
    double m_accuracy{0};
    bool m_is_use_ref_point{false};
    bool m_is_use_vs_point{false};
};


}

#endif // KW_MOVE_REL_LINEAR_H
