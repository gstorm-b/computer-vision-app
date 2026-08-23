#ifndef TASK_FACTORY_H
#define TASK_FACTORY_H

#include "model/itask.h"

/**
 * @file task_factory.h
 * @brief TaskFactory — static, non-instantiable factory that builds concrete ITask instances
 *        from JSON.
 */
namespace vc::model {

/**
 * @class TaskFactory
 * @brief Static, non-instantiable factory that builds concrete ITask instances (currently
 *        LocalizationTask) from JSON, dispatching by TaskType.
 */
class TaskFactory {

public:
    TaskFactory() = delete;
    ~TaskFactory() = delete;
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory& operator=(const TaskFactory&) = delete;

    /**
     * @brief Builds an ITask from a JSON object, validating that the required top-level keys
     *        (id, name, taskType, taskConfig) are present before dispatching to create().
     * @param[in] o      JSON object describing the task
     * @param[in] parent QObject parent passed through to the created task
     * @return the created ITask, or nullptr if the JSON is missing required keys or
     *         no matching task could be created
     */
    static ITask* fromJson(const QJsonObject& o,
                           QObject* parent = nullptr);

    /**
     * @brief Dispatches task creation to the type-specific create* helper matching `type`.
     * @param[in] type   task family to create
     * @param[in] obj    JSON object describing the task
     * @param[in] parent QObject parent passed through to the created task
     * @return the created ITask, or nullptr for any type other than LocalizationTask
     */
    static ITask* create(const TaskType& type,
                         const QJsonObject& obj,
                         QObject* parent = nullptr);

    /**
     * @brief Creates a TaskLocalization from `obj`: reads its id/name, then restores the rest
     *        (config, device ids, pattern library) via TaskLocalization::fromJson(). The task is
     *        still returned (for the user to see/repair in the UI) even if that restore fails.
     * @param[in] obj    JSON object describing the task
     * @param[in] parent QObject parent passed through to the created task
     * @return the created TaskLocalization, or nullptr if `obj["id"]` is empty
     */
    static ITask* createTaskLocalization(const QJsonObject& obj,
                                         QObject* parent = nullptr);

};

}

#endif // TASK_FACTORY_H
