#ifndef HY_ALIGNTOZ_H
#define HY_ALIGNTOZ_H

#include "utils/chronocounter.h"
#include "robot/huayan/huayan_command.h"

namespace rb {

class HyAlignToZ : public HuayanCommand {
public:
    HyAlignToZ(QString &tcp_name, QString &ucs_name);

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = AlignToZState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyAlignToZ>(*this);
    }

private:
    enum AlignToZState {
        UnExecute = 0,
        SendAlignCommand,
        WaitAlignCommandResponse,
        SendLongMoveEventCommand,
        WaitLongMoveEventCommandResponse,
        WaitCycle,
        Finish,
        Error
    };

    inline void send_align_to_z_command();
    inline void wait_align_to_z_command_response();
    inline void send_long_move_command();
    inline void wait_long_move_comamnd_response();

private:
    AlignToZState m_cmd_state = AlignToZState::UnExecute;
    ChronoCounter m_time_count;

    QString m_command_header;
    QString m_command_str;

    QString m_tcp_name;
    QString m_ucs_name;
};

}

#endif // HY_ALIGNTOZ_H
