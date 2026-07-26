#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <QList>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>

#include "device/idevice.h"

/// Device abstraction layer: concrete device family registration and lookup (see DeviceRegistry).
namespace vc::device {

/// One entry in the static device registry: identifies a concrete device sub-type (e.g. the
/// "Basler GigE" camera or "Mitsubishi MC" PLC), how to recognize it in JSON, and how to
/// construct an instance of it.
struct DeviceRegistryEntry {
    DeviceType deviceType{DeviceType::UserType};  ///< Top-level device family this entry belongs to.
    QString subTypeValue;  ///< JSON sub-type token (e.g. "Basler_GigE"); matched by find() and returned by displayNamesFor().
    QString displayName;  ///< Human-readable label for this entry; currently write-only (not read by any lookup below).
    QString configJsonKey;  ///< JSON key (top-level or nested under DeviceConfig) carrying the sub-type token for this entry.
    std::function<IDevice *(const QJsonObject &, QObject *)> creator;  ///< Factory that builds a new heap-allocated device instance from a JSON object.
    bool available{true};  ///< Whether this entry is offered by displayNamesFor() unless includeUnavailable is requested.
};

/// Static registry of known device sub-types (camera/PLC/vision-output/robot implementations)
/// used to create devices from JSON and to list the sub-types offered in the UI. Exposes only
/// static members and is never instantiated.
class DeviceRegistry {
public:
    /// Deleted: DeviceRegistry exposes only static members and is never instantiated.
    DeviceRegistry() = delete;
    /// Deleted: DeviceRegistry exposes only static members and is never instantiated.
    ~DeviceRegistry() = delete;

    /// Returns the full static list of registered device entries.
    static const QList<DeviceRegistryEntry> &entries();
    /// Finds the registry entry for `deviceType` whose subTypeValue equals `subTypeValue`.
    /// @return the matching entry, or nullptr if none is registered
    static const DeviceRegistryEntry *find(DeviceType deviceType,
                                           const QString &subTypeValue);
    /// Finds the registry entry for `deviceType` whose sub-type token — read from `obj` via
    /// each candidate entry's configJsonKey, checked at the top level and then under the
    /// nested DeviceConfig object — matches its subTypeValue.
    /// @return the matching entry, or nullptr if none is registered
    static const DeviceRegistryEntry *find(const QJsonObject &obj,
                                           DeviceType deviceType);
    /// Lists the subTypeValue tokens of entries registered for `deviceType` (despite the
    /// name, these are the JSON sub-type tokens, not the entries' displayName field).
    /// @param includeUnavailable when false (default), entries with available == false are skipped
    static QStringList displayNamesFor(DeviceType deviceType,
                                       bool includeUnavailable = false);
};

} // namespace vc::device

#endif // DEVICE_REGISTRY_H
