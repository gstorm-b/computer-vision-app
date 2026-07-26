#ifndef TASK_DEFINE_H
#define TASK_DEFINE_H


#include "core/utils/meta_utils.h"

/// Namespace for the picking application's project domain model: task type/state enums and
/// their Qt meta-object (Q_ENUM_NS) reflection helpers.
namespace vc::model {
Q_NAMESPACE

/// Indicates whether a task's camera is exclusively owned by that task or shared with other tasks.
enum class CameraSourceType {
    Source_Owned,
    Source_Shared
};
Q_ENUM_NS(CameraSourceType)

/// Identifies the kind of work a task performs.
enum class TaskType{
    UndefineTask,
    LocalizationTask,
    PickPlaceTask,
    InspectTask
};
Q_ENUM_NS(TaskType)

/// Lifecycle/runtime state of a task's state machine, from idle through commissioning,
/// running a cycle, recovery, and fault handling.
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

/// String table used only so `lupdate`/Qt Linguist can discover and extract these enum value
/// names as translatable strings; not used at runtime.
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

/// Converts a TaskType value to its Q_ENUM key name (e.g. "LocalizationTask") via qenumToString.
/// @param t task type to convert
/// @return the enum's key name, or its numeric value as a string if it doesn't match a
///         declared enumerator
[[maybe_unused]] static QString taskTypeToString(TaskType t) {
    return qenumToString(t);
};

/// Parses a TaskType Q_ENUM key name back into its enum value via stringToQEnum.
/// @param t key name to look up (e.g. "LocalizationTask")
/// @return the matching TaskType, or TaskType::UndefineTask if `t` doesn't match a declared key
[[maybe_unused]] static TaskType taskTypeFromString(QString t) {
    return stringToQEnum(t, TaskType::UndefineTask);
};

/// Converts a TaskState value to its Q_ENUM key name via qenumToString.
/// @param s task state to convert
/// @return the enum's key name, or its numeric value as a string if it doesn't match a
///         declared enumerator
[[maybe_unused]] static QString taskStateToString(TaskState s) {
    return qenumToString(s);
};

/// Maps a TaskState to a human-readable display label for UI use (e.g. CommissionStarting ->
/// "Commission starting"), independent of the Q_ENUM key name.
/// @param s task state to map
/// @return the display string for `s`, or "Unknown" if `s` doesn't match any case (unreachable
///         for a valid TaskState value)
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

/// Registers TaskState with the Qt meta-type system so it can be used in queued signal/slot
/// connections and stored in a QVariant.
Q_DECLARE_METATYPE(vc::model::TaskState)

#endif // TASK_DEFINE_H
