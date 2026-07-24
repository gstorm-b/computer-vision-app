/* IMPORTANT NOTES
 * EXECUTE EVERY SINGLE COMMAND, STACK TOGETHER
 * SET INTERFACE POINTER
 * THEN POLLING CALL EXECUTE METHOD
 */

#ifndef HUAYAN_COMMAND_H
#define HUAYAN_COMMAND_H

#include <QString>
#include "robot/huayan/huayan_context.h"
#include "robot/robot_command.h"


namespace rb {

class HuayanCmdInterface : public RbCmdInterface {
public:
    HuayanCmdInterface(HyMsgSender& _msg, HyFlagSender& _flag)
        : msg(_msg), flag(_flag) {

    }

    HyMsgSender& msg;
    HyFlagSender& flag;

protected:
    int robot_id;
};

class HuayanCommand : public RbCommand {
public:
    HuayanCommand();

    void setContext(std::shared_ptr<RbCmdInterface> _interface) override;

    virtual void execute() override {
        qInfo() << "Some thing went wrong, this is Huayan Command Abstract class";
    }

    virtual QString comamndId() {
        return "";
    }

    const bool validateCmd() override {
        return true;
    }

    virtual void resetCommand() override {
        // do nothing
    }

    virtual std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HuayanCommand>(*this);
    }

    virtual const bool isMotionCommand() override {
        return false;
    }

    const bool isComplete() override;
    const bool isExecuting() override;
    const ExecuteState getExecuteState() override;
    const RbType getRobotType() override;
    void setLineNumber(int line_number) override;
    const int getLineNumber() override;

protected:
    ExecuteState m_execute_state;
    std::shared_ptr<HuayanCmdInterface> m_interface;
    int m_line_number;

private:
    RbType m_robot_type = RbType::Huayan_E;
};
}

#endif // HUAYAN_COMMAND_H
