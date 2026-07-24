#ifndef ROBOT_COMMAND_H
#define ROBOT_COMMAND_H

#include <vector>
#include <memory>
#include "robot_abstract.h"

namespace rb {

class RbCmdInterface {
public:
    virtual ~RbCmdInterface() = default;
};

class RbCommand {
public:
    enum ExecuteState {
        UnExecuted,
        Executing,
        Executed,
        ExecuteError
    };

    virtual ~RbCommand() = default;

    /**
     * @brief setContext: set context and services for command
     * @param _interface
     */
    virtual void setContext(std::shared_ptr<RbCmdInterface> _interface) = 0;

    /**
     * @brief execute: main execute method for all command
     */
    virtual void execute() = 0;

    /**
     * @brief validateCmd: validate command method for command;
     * @return
     */
    virtual const bool validateCmd() = 0;

    /**
     * @brief resetComamnd: reset command to run again
     */
    virtual void resetCommand() = 0;

    /**
     * @brief clone: return a new command clone from this one with share pointer
     * @return
     */
    virtual std::shared_ptr<RbCommand> clone() const = 0;

    /**
     * @brief setChildCommand: set list of child command
     * @param children
     */
    virtual void setChildCommand(std::vector<std::shared_ptr<RbCommand>> &children) {
        // do nothing
    }

    /**
     * @brief getChildCommand
     * @return return list of it children command
     */
    virtual std::vector<std::shared_ptr<RbCommand>> getChildCommand() {
        return std::vector<std::shared_ptr<RbCommand>>();
    }

    /**
     * @brief isMotionCommand
     * @return
     */
    virtual const bool isMotionCommand() = 0;

    /**
     * @brief isComplete
     * @return
     */
    virtual const bool isComplete() = 0;

    /**
     * @brief isExecuting
     * @return
     */
    virtual const bool isExecuting() = 0;

    /**
     * @brief getExecuteState
     * @return
     */
    virtual const ExecuteState getExecuteState() = 0;

    /**
     * @brief getRobotType
     * @return
     */
    virtual const RbType getRobotType() = 0;

    /**
     * @brief setCommandLineNumber
     * @param line_number
     */
    virtual void setLineNumber(int line_number) = 0;

    /**
     * @brief getCommandLineNumber
     * @return
     */
    virtual const int getLineNumber() = 0;
};

}

#endif // ROBOT_COMMAND_H
