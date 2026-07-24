#ifndef KW_WAIT_TIME_H
#define KW_WAIT_TIME_H

#include "robot/kawasaki/command/kawasaki_command.h"
#include "utils/chronocounter.h"

namespace rb {

class KwWaitTime : public KawasakiCommand {
public:
    KwWaitTime(int time) :
        KawasakiCommand(),
        m_waittime(time),
        m_startwaiting(false) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_startwaiting = false;
        m_counter.StopTimeCounter();
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "Kawasaki command WaitTime error, m_interface pointer null.";
            return;
        }

        if (!m_startwaiting) {
            m_execute_state = ExecuteState::Executing;
            m_startwaiting = true;
            m_counter.StartTimeCounter(m_waittime);
        } else {
            if (m_counter.StartTimeCounter(m_waittime)) {
                m_execute_state = ExecuteState::Executed;
                return;
            }
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwWaitTime>(*this);
    }

private:
    int m_waittime{0};
    bool m_startwaiting{false};
    ChronoCounter m_counter;
};

}

#endif // KW_WAIT_TIME_H
