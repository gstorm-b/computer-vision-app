#ifndef DEVICE_FACTORY_H
#define DEVICE_FACTORY_H

#include "device/idevice.h"
#include "device/device_registry.h"
#include <QJsonObject>
#include <QString>
#include <QSet>
#include <QMutex>

namespace vc::device {

/// Static, non-instantiable factory that builds concrete IDevice instances (camera,
/// PLC, vision-output, robot) from JSON, dispatching by DeviceType to the matching
/// registry entry looked up via DeviceRegistry.
class DeviceFactory {
public:
    DeviceFactory() = delete;
    ~DeviceFactory() = delete;
    DeviceFactory(const DeviceFactory&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = delete;

    /// Builds an IDevice from a JSON object, validating that the required top-level keys
    /// (id, name, type, config) are present before dispatching to create().
    /// @param o JSON object describing the device
    /// @param parent QObject parent passed through to the created device
    /// @return the created IDevice, or nullptr if the JSON is missing required keys or
    ///         no matching device could be created
    static IDevice* fromJson(const QJsonObject& o,
                             QObject* parent = nullptr);

    /// Dispatches device creation to the type-specific create* helper matching `type`.
    /// @param type device family to create
    /// @param obj JSON object describing the device
    /// @param parent QObject parent passed through to the created device
    /// @return the created IDevice, or nullptr for DeviceType::UserType or any unhandled type
    static IDevice* create(const DeviceType& type,
                           const QJsonObject& obj,
                           QObject* parent = nullptr);

    /// Creates a camera device from the registry entry matching `obj`'s sub-type.
    /// @return the created IDevice, or nullptr if no matching/available camera entry exists
    static IDevice* createCamera(const QJsonObject& obj,
                                 QObject* parent = nullptr);
    /// Creates a PLC device from the registry entry matching `obj`'s sub-type.
    /// @return the created IDevice, or nullptr if no matching/available PLC entry exists
    static IDevice* createPlcDevice(const QJsonObject& obj,
                                    QObject* parent = nullptr);
    /// Creates a vision-output device from the registry entry matching `obj`'s sub-type.
    /// @return the created IDevice, or nullptr if no matching/available vision-output entry exists
    static IDevice* createVisionOutputDevice(const QJsonObject& obj,
                                             QObject* parent = nullptr);
    /// Creates a robot device from the registry entry matching `obj`'s sub-type.
    /// @return the created IDevice, or nullptr if no matching/available robot entry exists
    static IDevice* createRobotDevice(const QJsonObject& obj,
                                      QObject* parent = nullptr);
};

} // namespace vc::device

#endif // DEVICE_FACTORY_H
