#ifndef HY_WAIT_TIME_H
#define HY_WAIT_TIME_H

#include "robot/huayan/huayan_command.h"
#include "utils/chronocounter.h"

namespace rb {

class HyWaitTime : public HuayanCommand {
public:
    HyWaitTime(int time) :
        HuayanCommand(),
        m_waittime(time),
        m_startwaiting(false) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_startwaiting = false;
        m_counter.StopTimeCounter();
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyWaitTime m_interface pointer null.";
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
        return std::make_shared<HyWaitTime>(*this);
    }

private:
    int m_waittime{0};
    bool m_startwaiting{false};
    ChronoCounter m_counter;
};


}

#endif // HY_WAIT_TIME_H
