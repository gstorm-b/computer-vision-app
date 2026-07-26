#ifndef DEVICE_COMMAND_H
#define DEVICE_COMMAND_H

#include <QMetaType>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <utility>

/// Runtime layer: device command/result value types and the queue/runner machinery that
/// dispatches commands to per-device worker threads.
namespace vc::runtime {

/// Kind of operation a DeviceCommand asks a device runner to perform.
enum class DeviceCommandKind {
    Unknown,           ///< No/unset command kind.
    Connect,           ///< Connect the target device.
    Disconnect,        ///< Disconnect the target device.
    CameraSingleShot,  ///< Trigger a single camera capture.
    CameraApplyParams, ///< Apply camera parameters carried in the command payload.
};

/// Lifecycle status of a DeviceCommandResult.
enum class DeviceCommandResultStatus {
    Accepted,  ///< The command was accepted for execution (may still complete later).
    Rejected,  ///< The command was rejected before execution (see DeviceCommandResultCode).
    Succeeded, ///< The command ran to completion successfully.
    Failed,    ///< The command ran and failed, or was rejected (see DeviceCommandResultCode).
};

/// Reason code accompanying a non-successful DeviceCommandResult.
enum class DeviceCommandResultCode {
    None,               ///< No error (used with Accepted/Succeeded).
    Busy,               ///< Runner was already busy with another command.
    UnsupportedCommand, ///< The target runner does not implement this command kind.
    InvalidTarget,      ///< The target device id did not resolve to a valid device.
    DeviceError,        ///< The device itself reported an error while executing the command.
    TimedOut,           ///< The command did not complete within its timeout.
    QueueFull,          ///< The command queue had no room to accept the command.
};

/// Maps `kind` to a human-readable string for logging/diagnostics.
/// @return the kind's name, or "Unknown" for DeviceCommandKind::Unknown or any unrecognized value
inline QString deviceCommandKindToString(DeviceCommandKind kind)
{
    switch (kind) {
    case DeviceCommandKind::Connect:
        return QStringLiteral("Connect");
    case DeviceCommandKind::Disconnect:
        return QStringLiteral("Disconnect");
    case DeviceCommandKind::CameraSingleShot:
        return QStringLiteral("CameraSingleShot");
    case DeviceCommandKind::CameraApplyParams:
        return QStringLiteral("CameraApplyParams");
    case DeviceCommandKind::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

/// Maps `status` to a human-readable string for logging/diagnostics.
/// @return the status's name, or "Failed" for DeviceCommandResultStatus::Failed or any
///         unrecognized value
inline QString deviceCommandResultStatusToString(DeviceCommandResultStatus status)
{
    switch (status) {
    case DeviceCommandResultStatus::Accepted:
        return QStringLiteral("Accepted");
    case DeviceCommandResultStatus::Rejected:
        return QStringLiteral("Rejected");
    case DeviceCommandResultStatus::Succeeded:
        return QStringLiteral("Succeeded");
    case DeviceCommandResultStatus::Failed:
    default:
        return QStringLiteral("Failed");
    }
}

/// Maps `code` to a human-readable string for logging/diagnostics.
/// @return the code's name, or "UnknownCode" for any value not matching a known enumerator
inline QString deviceCommandResultCodeToString(DeviceCommandResultCode code)
{
    switch (code) {
    case DeviceCommandResultCode::None:
        return QStringLiteral("None");
    case DeviceCommandResultCode::Busy:
        return QStringLiteral("Busy");
    case DeviceCommandResultCode::UnsupportedCommand:
        return QStringLiteral("UnsupportedCommand");
    case DeviceCommandResultCode::InvalidTarget:
        return QStringLiteral("InvalidTarget");
    case DeviceCommandResultCode::DeviceError:
        return QStringLiteral("DeviceError");
    case DeviceCommandResultCode::TimedOut:
        return QStringLiteral("TimedOut");
    case DeviceCommandResultCode::QueueFull:
        return QStringLiteral("QueueFull");
    default:
        return QStringLiteral("UnknownCode");
    }
}

/// A single request submitted to a device runner (e.g. via IDeviceRunner::submitCommand()),
/// identifying the operation, its target device, and an optional payload/timeout.
struct DeviceCommand {
    QString id;                                        ///< Unique command id (UUID, no braces); assigned by create().
    DeviceCommandKind kind{DeviceCommandKind::Unknown}; ///< Operation this command requests.
    QString targetDeviceId;                             ///< Id of the device the command targets.
    int timeoutMs{3000};                                ///< Max time allowed for the command to complete, in ms.
    QVariantMap payload;                                ///< Kind-specific arguments (e.g. camera params).

    /// Builds a DeviceCommand with a freshly generated unique id.
    /// @param kind operation to perform
    /// @param targetDeviceId id of the device to target
    /// @param timeoutMs max time allowed for the command to complete, in ms
    /// @param payload kind-specific arguments
    static DeviceCommand create(DeviceCommandKind kind,
                                const QString &targetDeviceId,
                                int timeoutMs = 3000,
                                QVariantMap payload = {})
    {
        DeviceCommand command;
        command.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        command.kind = kind;
        command.targetDeviceId = targetDeviceId;
        command.timeoutMs = timeoutMs;
        command.payload = std::move(payload);
        return command;
    }
};

/// Outcome of dispatching a DeviceCommand: echoes the originating command's identity plus a
/// status/code/message describing what happened, and any result payload.
struct DeviceCommandResult {
    QString commandId;                                              ///< Id of the DeviceCommand this result answers.
    DeviceCommandKind kind{DeviceCommandKind::Unknown};              ///< Operation that was requested.
    QString targetDeviceId;                                         ///< Id of the device that was targeted.
    DeviceCommandResultStatus status{DeviceCommandResultStatus::Failed}; ///< Outcome status.
    DeviceCommandResultCode code{DeviceCommandResultCode::None};     ///< Reason code (set for non-success statuses).
    QString message;                                                ///< Human-readable detail message.
    QVariantMap payload;                                            ///< Kind-specific result data (e.g. captured image info).

    /// @return true if status is Accepted or Succeeded (i.e. not rejected/failed).
    bool isAccepted() const
    {
        return status == DeviceCommandResultStatus::Accepted ||
               status == DeviceCommandResultStatus::Succeeded;
    }

    /// @return true only if status is Succeeded (the command fully completed).
    bool isTerminalSuccess() const
    {
        return status == DeviceCommandResultStatus::Succeeded;
    }

    /// Builds an Accepted result (code None) echoing `command`'s identity.
    static DeviceCommandResult accepted(const DeviceCommand &command,
                                        QString message = {})
    {
        return make(command,
                    DeviceCommandResultStatus::Accepted,
                    DeviceCommandResultCode::None,
                    std::move(message));
    }

    /// Builds a Rejected result (command was never executed) echoing `command`'s identity.
    static DeviceCommandResult rejected(const DeviceCommand &command,
                                        DeviceCommandResultCode code,
                                        QString message)
    {
        return make(command,
                    DeviceCommandResultStatus::Rejected,
                    code,
                    std::move(message));
    }

    /// Builds a Succeeded result (code None) echoing `command`'s identity, with an optional
    /// result payload.
    static DeviceCommandResult succeeded(const DeviceCommand &command,
                                         QString message = {},
                                         QVariantMap payload = {})
    {
        return make(command,
                    DeviceCommandResultStatus::Succeeded,
                    DeviceCommandResultCode::None,
                    std::move(message),
                    std::move(payload));
    }

    /// Builds a Failed result echoing `command`'s identity, with the given reason code/message
    /// and optional result payload.
    static DeviceCommandResult failed(const DeviceCommand &command,
                                      DeviceCommandResultCode code,
                                      QString message,
                                      QVariantMap payload = {})
    {
        return make(command,
                    DeviceCommandResultStatus::Failed,
                    code,
                    std::move(message),
                    std::move(payload));
    }

private:
    /// Shared factory backing accepted()/rejected()/succeeded()/failed(): copies `command`'s
    /// identity fields (commandId, kind, targetDeviceId) into a new result with the given
    /// status/code/message/payload.
    static DeviceCommandResult make(const DeviceCommand &command,
                                    DeviceCommandResultStatus status,
                                    DeviceCommandResultCode code,
                                    QString message,
                                    QVariantMap payload = {})
    {
        DeviceCommandResult result;
        result.commandId = command.id;
        result.kind = command.kind;
        result.targetDeviceId = command.targetDeviceId;
        result.status = status;
        result.code = code;
        result.message = std::move(message);
        result.payload = std::move(payload);
        return result;
    }
};

} // namespace vc::runtime

Q_DECLARE_METATYPE(vc::runtime::DeviceCommand)
Q_DECLARE_METATYPE(vc::runtime::DeviceCommandResult)

#endif // DEVICE_COMMAND_H
