#ifndef TASK_DEFINE_H
#define TASK_DEFINE_H


#include "core/utils/meta_utils.h"

namespace vc::model {
Q_NAMESPACE

enum class CameraSourceType {
    Source_Owned,
    Source_Shared
};
Q_ENUM_NS(CameraSourceType)

enum class TaskType{
    UndefineTask,
    LocalizationTask,
    PickPlaceTask,
    InspectTask
};
Q_ENUM_NS(TaskType)

enum class TaskState {
    Idle,
    CommissionStarting,
    Commission,
    RuntimeStarting,
    Ready,
    RunningCycle,
    Recovering,
    Faulted,
    Stopping
};
Q_ENUM_NS(TaskState)

// only use for lingust
static inline const char* enum_keys_task_defines[] = {
    // CameraSourceType
    QT_TR_NOOP("Source_Owned"),
    QT_TR_NOOP("Source_Shared"),

    // TaskType
    QT_TR_NOOP("UndefineTask"),
    QT_TR_NOOP("LocalizationTask"),
    QT_TR_NOOP("PickPlaceTask"),
    QT_TR_NOOP("InspectTask"),

    // TaskState
    QT_TR_NOOP("Idle"),
    QT_TR_NOOP("CommissionStarting"),
    QT_TR_NOOP("Commission"),
    QT_TR_NOOP("RuntimeStarting"),
    QT_TR_NOOP("Ready"),
    QT_TR_NOOP("RunningCycle"),
    QT_TR_NOOP("Recovering"),
    QT_TR_NOOP("Faulted"),
    QT_TR_NOOP("Stopping"),
};

[[maybe_unused]] static QString taskTypeToString(TaskType t) {
    return qenumToString(t);
};

[[maybe_unused]] static TaskType taskTypeFromString(QString t) {
    return stringToQEnum(t, TaskType::UndefineTask);
};

[[maybe_unused]] static QString taskStateToString(TaskState s) {
    return qenumToString(s);
};

[[maybe_unused]] static QString taskStateDisplayName(TaskState s) {
    switch (s) {
    case TaskState::Idle:
        return QStringLiteral("Idle");
    case TaskState::CommissionStarting:
        return QStringLiteral("Commission starting");
    case TaskState::Commission:
        return QStringLiteral("Commission");
    case TaskState::RuntimeStarting:
        return QStringLiteral("Runtime starting");
    case TaskState::Ready:
        return QStringLiteral("Ready");
    case TaskState::RunningCycle:
        return QStringLiteral("Running cycle");
    case TaskState::Recovering:
        return QStringLiteral("Recovering");
    case TaskState::Faulted:
        return QStringLiteral("Faulted");
    case TaskState::Stopping:
        return QStringLiteral("Stopping");
    }

    return QStringLiteral("Unknown");
};




}

Q_DECLARE_METATYPE(vc::model::TaskState)

#endif // TASK_DEFINE_H
