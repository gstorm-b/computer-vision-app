#ifndef HY_JOG_COMMAND_H
#define HY_JOG_COMMAND_H

#include "utils/chronocounter.h"
#include "robot/rbutils.h"
#include "robot/huayan/huayan_command.h"

/**
 * To Jog Robot:
 * 1. send LongJogL or LongJogJ with nState is 1
 * 2. send LongMoveEvent constantly, period within 200ms, otherwise motion will stop
 * 2. send LongJogL or LongJogJ again but nState is 0
 * The maximum speed no more than 30 mm/s
 * Later, should add time out while wait for response,
 * especially wait for response LongMoveEvent
 * add timer 120ms to send cyclce
 */

namespace rb {

class HyJogCommand : public HuayanCommand {
public:
    HyJogCommand(RbMotionType type, RbAxis axis, RbDirection direction);

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = JgCmdState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyJogCommand>(*this);
    }

private:
    enum JgCmdState {
        UnExecute = 0,
        SendJogCommand,
        WaitJogCommandReponse,
        WaitCycle,
        SendLongMoveEventCommand,
        WaitLongMoveEventCommandResponse,
        SendStopJogCommand,
        WaitStopJogCommandResponse,
        Finish,
        Error
    };

    inline void send_jog_command();
    inline void wait_jog_command_response();
    inline void send_long_move_command();
    inline void wait_long_move_command_response();
    inline void send_stop_jog_command();
    inline void wait_stop_jog_command();


private:
    RbMotionType m_type;
    RbAxis m_axis;
    RbDirection m_direction;

    ChronoCounter m_time_count;

    JgCmdState m_cmd_state = JgCmdState::UnExecute;
    QString m_jog_cmd_header;
    QString m_jog_cmd;
    QString m_jog_stop_cmd;
    int m_axis_id = -1;
    int m_i_direction;
};

}

#endif // HY_JOG_COMMAND_H
