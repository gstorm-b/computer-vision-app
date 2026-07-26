#include "task_factory.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QDebug>

#include "model/task_localization.h"

namespace vc::model {

/// Builds an ITask from a JSON object, validating that the required top-level keys
/// (id, name, taskType, taskConfig) are present before dispatching to create().
ITask* TaskFactory::fromJson(const QJsonObject& obj,
                             QObject* parent) {

    if (!obj.contains("id") ||
        !obj.contains("name") ||
        !obj.contains("taskType") ||
        !obj.contains("taskConfig")) {

        LOG_DEV_INFO << "Cannot convert task from json, wrong json format.";
        LOG_DEV_INFO << QJsonDocument(obj).toJson();
        return nullptr;
    }

    const QString type = obj["taskType"].toString();
    return TaskFactory::create(taskTypeFromString(type), obj, parent);
}

/// Dispatches task creation to the type-specific create* helper matching `type`.
ITask* TaskFactory::create(const TaskType& type, const QJsonObject& obj, QObject* parent) {
    switch (type) {
    case vc::model::TaskType::LocalizationTask:
        return createTaskLocalization(obj, parent);
    default:
        return nullptr;
    }
    return nullptr;
}


/// Creates a TaskLocalization from `obj`: reads its id/name, then restores the rest
/// (config, device ids, pattern library) via TaskLocalization::fromJson(). The task is
/// still returned (for the user to see/repair in the UI) even if that restore fails.
ITask* TaskFactory::createTaskLocalization(const QJsonObject& obj, QObject* parent) {
    const QString taskId = obj["id"].toString();
    QString taskName     = obj["name"].toString();
    if (taskId.isEmpty()) {
        return nullptr;
    }

    TaskLocalization* task = new TaskLocalization(taskName, taskId, parent);

    // Delegate the full restore — TaskLocalization::fromJson restores the
    // embedded TaskLocalizeConfig (via the base class), device IDs, and the
    // entire pattern library from m_patternManager.  Pattern training images
    // are injected later by ProjectRepository::loadImages → loadTaskImageMap.
    if (!task->fromJson(obj)) {
        LOG_DEV_INFO << "TaskFactory::createTaskLocalization – fromJson failed for"
                     << taskId;
        // Keep the task anyway so the user can see/repair it in the UI; the
        // alternative is silently dropping the row.
    }
    return task;
}

} // namespace vc::model
