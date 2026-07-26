#ifndef ISIGNAL_GROUP_H
#define ISIGNAL_GROUP_H

#include <QObject>

namespace vc::model {

/// Signal-group model interface: abstract base for a QObject-derived signal group that can be
/// (de)serialized to/from JSON.
class ISignalGroup : public QObject   {
    Q_OBJECT

public:
    /// Constructs the signal group with the given QObject `parent`.
    explicit ISignalGroup(QObject* parent = nullptr)
        : QObject(parent) {

    }

    /// Default destructor.
    virtual ~ISignalGroup() = default;

    /// Serializes this signal group's current state to a JSON object.
    /// @return the JSON representation of this signal group
    virtual QJsonObject toJson() const = 0;
    /// Restores this signal group's state from `obj`.
    /// @param obj object providing the serialized signal-group state
    /// @return true on success; false if `obj` could not be applied
    virtual bool fromJson(const QObject& obj) = 0;
};

}

#endif // ISIGNAL_GROUP_H
