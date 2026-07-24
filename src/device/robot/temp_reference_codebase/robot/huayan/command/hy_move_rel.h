#ifndef HY_MOVE_REL_H
#define HY_MOVE_REL_H

#include "robot/rbutils.h"
#include "robot/huayan/huayan_command.h"

namespace rb {

class HyMoveRel : public HuayanCommand {
public:
    HyMoveRel(RbMotionType motion_type, RbAxis axis, RbDirection direction,
              double distance, bool tool_motion = false);

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = MoveRelState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyMoveRel>(*this);
    }

private:
    enum MoveRelState {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        WaitFinishMovement,
        Finish,
        Error
    };

    inline void send_move_command();
    inline void wait_move_command_response();
    // inline void wait_movement_finish();

private:
    RbMotionType m_type;
    RbAxis m_axis;
    RbDirection m_direction;
    double m_distance;
    bool m_use_tool;

    MoveRelState m_cmd_state = MoveRelState::UnExecute;
    QString m_move_cmd_header;
    QString m_move_cmd;
    int m_axis_id = -1;
    int m_i_direction;
};

}


#endif // HY_MOVE_REL_H
