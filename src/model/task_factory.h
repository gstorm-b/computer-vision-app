#ifndef TASK_FACTORY_H
#define TASK_FACTORY_H

#include "model/itask.h"

namespace vc::model {

/// Static, non-instantiable factory that builds concrete ITask instances (currently
/// LocalizationTask) from JSON, dispatching by TaskType.
class TaskFactory {

public:
    TaskFactory() = delete;
    ~TaskFactory() = delete;
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory& operator=(const TaskFactory&) = delete;

    /// Builds an ITask from a JSON object, validating that the required top-level keys
    /// (id, name, taskType, taskConfig) are present before dispatching to create().
    /// @param o JSON object describing the task
    /// @param parent QObject parent passed through to the created task
    /// @return the created ITask, or nullptr if the JSON is missing required keys or
    ///         no matching task could be created
    static ITask* fromJson(const QJsonObject& o,
                           QObject* parent = nullptr);

    /// Dispatches task creation to the type-specific create* helper matching `type`.
    /// @param type task family to create
    /// @param obj JSON object describing the task
    /// @param parent QObject parent passed through to the created task
    /// @return the created ITask, or nullptr for any type other than LocalizationTask
    static ITask* create(const TaskType& type,
                         const QJsonObject& obj,
                         QObject* parent = nullptr);

    /// Creates a TaskLocalization from `obj`: reads its id/name, then restores the rest
    /// (config, device ids, pattern library) via TaskLocalization::fromJson(). The task is
    /// still returned (for the user to see/repair in the UI) even if that restore fails.
    /// @param obj JSON object describing the task
    /// @param parent QObject parent passed through to the created task
    /// @return the created TaskLocalization, or nullptr if `obj["id"]` is empty
    static ITask* createTaskLocalization(const QJsonObject& obj,
                                         QObject* parent = nullptr);

};

}

#endif // TASK_FACTORY_H
