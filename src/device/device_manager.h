#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QSet>
#include <QMutex>
#include <set>

#include "device/idevice.h"

namespace vc::model {
    class Project;
}

/// Device abstraction layer: device interfaces/configs/requests and the manager that owns
/// and creates device instances.
namespace vc::device {

/// Device manager owned by a Project via shared pointer; tracks reserved/committed device
/// instances by id, enforces per-DeviceType creation limits, and is always released by the
/// Project's destructor (never outlives it).
class DeviceManager : public QObject {
    Q_OBJECT
public:
    /// Constructs the manager for `proj`, seeding the per-DeviceType creation limits
    /// (Camera/PLC/VisionOutput/Robot) and populating the device-type and sub-type name
    /// lists from DeviceRegistry.
    /// @param proj owning project (may be nullptr); stored and returned by project()
    explicit DeviceManager(vc::model::Project* proj = nullptr, QObject *parent = nullptr);
    /// Terminates every tracked device (blocking cross-thread call if needed) and clears
    /// the device map.
    ~DeviceManager();

    /// Returns the owning project passed to the constructor.
    vc::model::Project* project();

    /// Checks whether the number of active devices of `type` has reached its configured maximum.
    bool isLimitReached(DeviceType type) const;
    /// Reserves `id`/`name` for `device` while loading a project (bypasses the per-type
    /// limit check used by commitDevice), validating id format/uniqueness and tracking id
    /// sequencing for later allocation.
    /// @return true if reserved; false if id is malformed or id/name is already occupied
    bool reserveDevice(const QString &id, const QString &name, std::shared_ptr<IDevice> device);
    /// Removes an active device by `id`, frees its name, decrements its type counter, and
    /// recycles the numeric id for reuse; emits devicesChanged()/deviceDeleted(id).
    void releaseDevice(const QString &id);
    /// Discards a pending (reserved-but-not-committed) `id` and recycles its numeric value,
    /// without touching name/type bookkeeping since none was set.
    void releasePendingId(const QString &id);
    /// Generates a new device id and inserts it into the device map with a null device
    /// pointer to mark it pending until commitDevice() is called.
    /// @return the newly allocated id, or an empty string if the id space is exhausted
    QString allocatePendingId();
    /// Finalizes a pending `id` (allocated via allocatePendingId) with `device`, enforcing
    /// the per-type creation limit and name uniqueness; emits devicesChanged()/deviceCreated(id).
    /// @return true on success; false if the type limit is reached, id is not pending,
    /// or name is already occupied
    bool commitDevice(const QString &id, const QString &name, std::shared_ptr<IDevice> device);
    /// Looks up the display name of the device registered under `id`.
    /// @return the device name, or an empty string if `id` is not registered
    QString getDeviceName(const QString &id);
    /// Checks whether `name` is already in use by a tracked device.
    bool isNameExists(const QString &name);
    /// Checks whether `id` is currently tracked (reserved, pending, or committed).
    bool isOccupied(const QString &id);
    /// Returns the device instance registered under `id`, or nullptr if none exists.
    std::shared_ptr<IDevice> deviceById(const QString &id);

    /// Renames the device `id` to `new_name` after checking for a name collision; updates
    /// the occupied-names set and emits deviceModified(id).
    /// @return true if renamed (or already equal to new_name); false if new_name is taken
    bool changeDeviceName(const QString &id, const QString new_name);

    /// Returns id->name pairs for every tracked device of type PLC.
    QMap<QString, QString> commDevices();
    /// Returns id->name pairs for every tracked device of type VisionOutput.
    QMap<QString, QString> outputDevices();
    /// Returns id->name pairs for every tracked device of type Camera.
    QMap<QString, QString> cameraDevices();

    /// Returns the names of tracked PLC devices as a flat list (built via QString::arg with
    /// only a %1 placeholder present, so only the name ends up in each entry).
    QStringList commDevicesNameList();
    /// Returns the names of tracked VisionOutput devices as a flat list (built via
    /// QString::arg with only a %1 placeholder present, so only the name ends up in each entry).
    QStringList outputDevicesNameList();
    /// Returns the names of tracked Camera devices as a flat list (built via QString::arg
    /// with only a %1 placeholder present, so only the name ends up in each entry).
    QStringList cameraDevicesNameList();

    /// Returns the list of top-level device type strings (Camera/PLC/VisionOutput/Robot)
    /// built at construction time.
    QStringList& getDeviceTypeList();
    /// Returns the sub-device display-name list for `type`, sourced from DeviceRegistry.
    QStringList& getSubDeviceTypeList(DeviceType type);

    /// Returns a const reference to the live id->device instance map.
    const QMap<QString, std::shared_ptr<IDevice>>& getCurrentDevices() {
        return deviceInstances;
    }

signals:
    /// Emitted whenever the tracked device collection changes (reserve/commit/release).
    void devicesChanged();
    /// Emitted when a device's config or name changes (forwarded from
    /// IDevice::configChanged, or raised directly by changeDeviceName()).
    void deviceModified(QString id);
    /// Emitted after a device is successfully reserved or committed.
    void deviceCreated(QString id);
    /// Emitted after a device is released via releaseDevice().
    void deviceDeleted(QString id);

private:
    /// Allocates the next available device id: reuses the lowest recycled id from
    /// freedIds if any exist, otherwise takes currentMaxId and advances it.
    /// @return a zero-padded numeric id string, or empty if ids are exhausted (>9999)
    QString generateId();
    /// Placeholder for validating a device's name; unimplemented (no definition exists
    /// elsewhere in the codebase).
    void deviceNameCheck(IDevice* device);
    /// Forwards a device's configChanged signal to deviceModified(id). Called by
    /// both creation paths (reserveDevice on project load, commitDevice via the
    /// Add-Device wizard) so config edits mark the project modified consistently.
    void connectConfigChanged(const std::shared_ptr<IDevice> &device);

private:
    QStringList dvTypeStrings;  ///< Display strings for the top-level DeviceType values.
    QMap<DeviceType, QStringList> subDeviceTypeLists;  ///< Sub-type display names per DeviceType, from DeviceRegistry.

    QString lastMsg;  ///< Last user-facing status/error message (e.g. commitDevice()'s limit-reached message).

    QMutex mutex;  ///< Guards deviceInstances/occupiedNames/id bookkeeping across threads.
    int currentMaxId;  ///< Next numeric id to allocate when freedIds is empty.
    std::set<int> freedIds;  ///< Pool of released/recyclable numeric ids below currentMaxId.
    QSet<QString> occupiedNames;  ///< Names currently in use by tracked devices.

    QMap<QString, std::shared_ptr<IDevice>> deviceInstances;  ///< All tracked devices by id (pending entries map to nullptr).
    // QMap<QString, QThread> deviceWorkers;

    QMap<DeviceType, int> maxLimits;  ///< Maximum allowed device count per DeviceType.
    QMap<DeviceType, int> currentCounts;  ///< Current active device count per DeviceType.

    vc::model::Project* parent_proj{nullptr};  ///< Owning project (non-owning pointer).
};

} // namespace vc::device

#endif // DEVICE_MANAGER_H
