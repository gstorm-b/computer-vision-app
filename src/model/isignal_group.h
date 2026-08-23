#ifndef ISIGNAL_GROUP_H
#define ISIGNAL_GROUP_H

#include <QObject>

/**
 * @file isignal_group.h
 * @brief ISignalGroup — abstract base for a QObject-derived signal group that can be
 *        (de)serialized to/from JSON.
 */

namespace vc::model {

/**
 * @class ISignalGroup
 * @brief Abstract base for a QObject-derived signal group that can be (de)serialized to/from JSON.
 */
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
    /**
     * @brief Restores this signal group's state from `obj`.
     * @param[in] obj object providing the serialized signal-group state
     * @return true on success; false if `obj` could not be applied
     */
    virtual bool fromJson(const QObject& obj) = 0;
};

}

#endif // ISIGNAL_GROUP_H
