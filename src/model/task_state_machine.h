#ifndef TASK_STATE_MACHINE_H
#define TASK_STATE_MACHINE_H

#include <QString>

#include "model/task_define.h"

/// Task model layer: per-task configuration types, state machines, and device/camera
/// bindings for the picking application.
namespace vc::model {

/// Checks whether a task is allowed to move from state `from` to state `to` according to
/// the task lifecycle graph (Idle -> Commission* -> Runtime* -> Ready/RunningCycle/
/// Recovering -> Stopping -> Idle, with Faulted reachable from most states and only able
/// to proceed to Stopping). Staying in the same state is always allowed.
/// @param from current task state
/// @param to candidate next task state
/// @return true if the transition is permitted by the state machine
inline bool canTransitionTaskState(TaskState from, TaskState to)
{
    if (from == to) {
        return true;
    }

    switch (from) {
    case TaskState::Idle:
        return to == TaskState::CommissionStarting ||
               to == TaskState::RuntimeStarting ||
               to == TaskState::Faulted;

    case TaskState::CommissionStarting:
        return to == TaskState::Commission ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::Commission:
        return to == TaskState::RuntimeStarting ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::RuntimeStarting:
        return to == TaskState::Ready ||
               to == TaskState::Recovering ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::Ready:
        return to == TaskState::RunningCycle ||
               to == TaskState::Recovering ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::RunningCycle:
        return to == TaskState::Ready ||
               to == TaskState::Recovering ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::Recovering:
        return to == TaskState::Ready ||
               to == TaskState::RunningCycle ||
               to == TaskState::Stopping ||
               to == TaskState::Faulted;

    case TaskState::Faulted:
        return to == TaskState::Stopping;

    case TaskState::Stopping:
        return to == TaskState::Idle ||
               to == TaskState::Faulted;
    }

    return false;
}

/// Builds a human-readable log message describing an accepted task state transition, in
/// the form "Task state transition: <from> -> <to>", optionally suffixed with "(reason)".
/// @param from previous task state
/// @param to new task state
/// @param reason optional free-text reason appended in parentheses; ignored if blank
/// @return the formatted message
inline QString buildTaskStateTransitionMessage(TaskState from,
                                               TaskState to,
                                               const QString &reason = QString())
{
    QString message = QStringLiteral("Task state transition: %1 -> %2")
                          .arg(taskStateToString(from), taskStateToString(to));
    if (!reason.trimmed().isEmpty()) {
        message += QStringLiteral(" (%1)").arg(reason.trimmed());
    }
    return message;
}

/// Builds a human-readable log message describing a rejected task state transition, in the
/// form "Rejected task state transition: <from> -> <to>", optionally suffixed with "(reason)".
/// @param from current task state (transition was not applied)
/// @param to the disallowed candidate state
/// @param reason optional free-text reason appended in parentheses; ignored if blank
/// @return the formatted message
inline QString buildInvalidTaskStateTransitionMessage(TaskState from,
                                                      TaskState to,
                                                      const QString &reason = QString())
{
    QString message = QStringLiteral("Rejected task state transition: %1 -> %2")
                          .arg(taskStateToString(from), taskStateToString(to));
    if (!reason.trimmed().isEmpty()) {
        message += QStringLiteral(" (%1)").arg(reason.trimmed());
    }
    return message;
}

} // namespace vc::model

#endif // TASK_STATE_MACHINE_H
