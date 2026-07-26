#ifndef ITASK_CONFIG_H
#define ITASK_CONFIG_H

#include <QObject>
#include <QJsonObject>
#include "model/task_define.h"

/// Model classes for projects, tasks, and task configuration.
namespace vc::model {

/// Abstract per-task-type configuration: defines the JSON (de)serialization and
/// cloning contract that every concrete ITaskConfig subclass (one per TaskType) must
/// implement. Not a QObject; concrete subclasses expose their own QMetaObject via
/// getMetaObject() for property introspection.
class ITaskConfig  {
public:
    /// Default-destructs the configuration.
    virtual ~ITaskConfig() = default;

    /// Returns the QMetaObject of the concrete subclass, for property introspection.
    virtual const QMetaObject &getMetaObject() const = 0;
    /// Returns the TaskType this configuration is for.
    virtual TaskType taskType() const = 0;
    /// Serializes the configuration's fields to a JSON object.
    virtual QJsonObject toJson() const = 0;
    /// Restores the configuration's fields from a JSON object previously produced
    /// by toJson().
    /// @param obj serialized configuration
    /// @return true on success, false if the data was invalid/incompatible
    virtual bool fromJson(const QJsonObject& obj) = 0;
    /// Creates and returns a new heap-allocated copy of this configuration.
    /// @return a newly allocated clone; caller takes ownership
    virtual ITaskConfig* copy() = 0;

};

}

#endif // ITASK_CONFIG_H
