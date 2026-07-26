#ifndef TASK_DEVICE_BINDING_H
#define TASK_DEVICE_BINDING_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QString>
#include <utility>

/// Application data-model types: tasks, device bindings, and localization signal mapping.
namespace vc::model {

/// Purpose a device serves within a task's device bindings.
enum class TaskDeviceRole {
    Unknown,      ///< No role assigned / unrecognized role string.
    PrimaryPlc,   ///< The task's main PLC device.
    VisionOutput, ///< The device the vision result/output is written to.
    CameraNumber, ///< A camera bound to a specific numeric camera slot (see TaskDeviceBinding::cameraNumber()).
};

/// Maps a TaskDeviceRole to its JSON wire string; the inverse of taskDeviceRoleFromString().
/// @return the role's string token, or an empty string for TaskDeviceRole::Unknown
inline QString taskDeviceRoleToString(TaskDeviceRole role)
{
    switch (role) {
    case TaskDeviceRole::PrimaryPlc:
        return QStringLiteral("primary_plc");
    case TaskDeviceRole::VisionOutput:
        return QStringLiteral("vision_output");
    case TaskDeviceRole::CameraNumber:
        return QStringLiteral("camera_number");
    case TaskDeviceRole::Unknown:
        return QString();
    }
    return QString();
}

/// Parses a JSON wire string produced by taskDeviceRoleToString() back into a TaskDeviceRole.
/// @return the matching role, or TaskDeviceRole::Unknown if `value` does not match a known token
inline TaskDeviceRole taskDeviceRoleFromString(const QString &value)
{
    if (value == QStringLiteral("primary_plc")) return TaskDeviceRole::PrimaryPlc;
    if (value == QStringLiteral("vision_output")) return TaskDeviceRole::VisionOutput;
    if (value == QStringLiteral("camera_number")) return TaskDeviceRole::CameraNumber;
    return TaskDeviceRole::Unknown;
}

/// A single role->device assignment (optionally with a camera slot number) for a task; the
/// building block that TaskDeviceBindings stores a list of.
class TaskDeviceBinding {
public:
    /// Camera bindings use 1..16 numbers — the task enforces the same bound at
    /// runtime via TaskLocalization::kLimitNumCamera. Device ids are short
    /// (numeric or short tokens); cap the length to reject malformed/hostile
    /// input. Kept local to avoid a dependency on TaskLocalization here.
    static constexpr int kMinCameraNumber = 1;    ///< Lowest allowed TaskDeviceRole::CameraNumber slot.
    static constexpr int kMaxCameraNumber = 16;   ///< Highest allowed TaskDeviceRole::CameraNumber slot.
    static constexpr int kMaxDeviceIdLength = 64; ///< Max accepted length of a deserialized device id.

    /// Constructs an unassigned binding (role TaskDeviceRole::Unknown, empty device id).
    TaskDeviceBinding() = default;

    /// Constructs a binding for `role` pointing at `deviceId`, optionally with a camera slot number
    /// (meaningful only when `role` is TaskDeviceRole::CameraNumber).
    TaskDeviceBinding(TaskDeviceRole role, QString deviceId, int cameraNumber = 0)
        : m_role(role), m_deviceId(std::move(deviceId)), m_cameraNumber(cameraNumber) {}

    /// Returns the role this binding fulfills.
    TaskDeviceRole role() const { return m_role; }
    /// Returns the bound device's id.
    QString deviceId() const { return m_deviceId; }
    /// Returns the camera slot number (only meaningful when role() is TaskDeviceRole::CameraNumber).
    int cameraNumber() const { return m_cameraNumber; }

    /// Serializes this binding to JSON; `cameraNumber` is included only for the CameraNumber role.
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["role"] = taskDeviceRoleToString(m_role);
        obj["deviceId"] = m_deviceId;
        if (m_role == TaskDeviceRole::CameraNumber)
            obj["cameraNumber"] = m_cameraNumber;
        return obj;
    }

    /// Parses this binding from `obj`, validating role, device id length, and (for CameraNumber)
    /// the camera slot range.
    /// @return false if the role is unrecognized, the device id exceeds kMaxDeviceIdLength, or a
    ///         CameraNumber role's number is outside [kMinCameraNumber, kMaxCameraNumber]
    bool fromJson(const QJsonObject &obj)
    {
        m_role = taskDeviceRoleFromString(obj["role"].toString());
        m_deviceId = obj["deviceId"].toString();
        m_cameraNumber = obj["cameraNumber"].toInt(0);

        if (m_role == TaskDeviceRole::Unknown)
            return false;

        // Reject malformed/hostile imports: over-long device ids and out-of-range
        // camera numbers. Invalid bindings are dropped by the bool-return
        // convention in TaskDeviceBindings::fromJson().
        if (m_deviceId.size() > kMaxDeviceIdLength)
            return false;

        if (m_role == TaskDeviceRole::CameraNumber &&
            (m_cameraNumber < kMinCameraNumber || m_cameraNumber > kMaxCameraNumber))
            return false;

        return true;
    }

private:
    TaskDeviceRole m_role{TaskDeviceRole::Unknown}; ///< Role this binding fulfills.
    QString m_deviceId;                             ///< Bound device's id.
    int m_cameraNumber{0};                          ///< Camera slot number (CameraNumber role only).
};

/// Ordered collection of a task's TaskDeviceBinding entries, with role-keyed convenience
/// accessors for the single-valued roles (PrimaryPlc, VisionOutput) and the multi-valued
/// CameraNumber role.
class TaskDeviceBindings {
public:
    /// Returns all bindings, regardless of role.
    QList<TaskDeviceBinding> bindings() const { return m_bindings; }

    /// @return the device id bound to TaskDeviceRole::PrimaryPlc, or empty if unset.
    QString primaryPlcDeviceId() const
    {
        return deviceIdForRole(TaskDeviceRole::PrimaryPlc);
    }

    /// Replaces the TaskDeviceRole::PrimaryPlc binding with `deviceId` (removing it if empty).
    void setPrimaryPlcDeviceId(const QString &deviceId)
    {
        setDeviceIdForRole(TaskDeviceRole::PrimaryPlc, deviceId);
    }

    /// @return the device id bound to TaskDeviceRole::VisionOutput, or empty if unset.
    QString visionOutputDeviceId() const
    {
        return deviceIdForRole(TaskDeviceRole::VisionOutput);
    }

    /// Replaces the TaskDeviceRole::VisionOutput binding with `deviceId` (removing it if empty).
    void setVisionOutputDeviceId(const QString &deviceId)
    {
        setDeviceIdForRole(TaskDeviceRole::VisionOutput, deviceId);
    }

    /// Builds a camera-number -> device-id map from all TaskDeviceRole::CameraNumber bindings.
    QMap<int, QString> cameraNumberMap() const
    {
        QMap<int, QString> result;
        for (const auto &binding : m_bindings) {
            if (binding.role() == TaskDeviceRole::CameraNumber)
                result.insert(binding.cameraNumber(), binding.deviceId());
        }
        return result;
    }

    /// Replaces all TaskDeviceRole::CameraNumber bindings with one binding per non-empty
    /// device id in `mapping` (camera number -> device id); other roles are left untouched.
    void setCameraNumberMap(const QMap<int, QString> &mapping)
    {
        for (int i = m_bindings.size() - 1; i >= 0; --i) {
            if (m_bindings.at(i).role() == TaskDeviceRole::CameraNumber)
                m_bindings.removeAt(i);
        }

        for (auto it = mapping.cbegin(); it != mapping.cend(); ++it) {
            if (!it.value().isEmpty())
                m_bindings.append(TaskDeviceBinding(TaskDeviceRole::CameraNumber,
                                                    it.value(),
                                                    it.key()));
        }
    }

    /// @return the device id bound to TaskDeviceRole::CameraNumber slot `cameraNumber`, or empty
    ///         if no binding exists for that slot.
    QString cameraDeviceId(int cameraNumber) const
    {
        for (const auto &binding : m_bindings) {
            if (binding.role() == TaskDeviceRole::CameraNumber &&
                binding.cameraNumber() == cameraNumber) {
                return binding.deviceId();
            }
        }
        return QString();
    }

    /// Serializes all bindings to a JSON array (each via TaskDeviceBinding::toJson()).
    QJsonArray toJson() const
    {
        QJsonArray arr;
        for (const auto &binding : m_bindings)
            arr.append(binding.toJson());
        return arr;
    }

    /// Replaces all bindings by parsing `value` as a JSON array of binding objects. A missing/
    /// null value clears the bindings and succeeds (treated as "no bindings"); a non-array value
    /// fails outright. Individual array entries that fail TaskDeviceBinding::fromJson() or carry
    /// an empty device id are dropped rather than aborting the whole parse.
    /// @return false if `value` is present but not a JSON array, or if any entry failed to parse
    bool fromJson(const QJsonValue &value)
    {
        m_bindings.clear();
        if (value.isUndefined() || value.isNull())
            return true;
        if (!value.isArray())
            return false;

        bool allOk = true;
        const QJsonArray arr = value.toArray();
        for (const QJsonValue &item : arr) {
            TaskDeviceBinding binding;
            if (!item.isObject() || !binding.fromJson(item.toObject())) {
                allOk = false;
                continue;
            }
            if (!binding.deviceId().isEmpty())
                m_bindings.append(binding);
        }
        return allOk;
    }

private:
    /// @return the device id of the first binding matching `role`, or empty if none does.
    QString deviceIdForRole(TaskDeviceRole role) const
    {
        for (const auto &binding : m_bindings) {
            if (binding.role() == role)
                return binding.deviceId();
        }
        return QString();
    }

    /// Removes any existing binding(s) for `role` and, if `deviceId` is non-empty, appends a
    /// single new binding for `role` -> `deviceId`.
    void setDeviceIdForRole(TaskDeviceRole role, const QString &deviceId)
    {
        for (int i = m_bindings.size() - 1; i >= 0; --i) {
            if (m_bindings.at(i).role() == role)
                m_bindings.removeAt(i);
        }
        if (!deviceId.isEmpty())
            m_bindings.append(TaskDeviceBinding(role, deviceId));
    }

    QList<TaskDeviceBinding> m_bindings; ///< All role->device bindings for the task, in insertion order.
};

} // namespace vc::model

#endif // TASK_DEVICE_BINDING_H
