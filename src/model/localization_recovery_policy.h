#ifndef LOCALIZATION_RECOVERY_POLICY_H
#define LOCALIZATION_RECOVERY_POLICY_H

#include <QString>

#include "device/idevice.h"

/// Connection-recovery policy model for localization-owned devices (camera, PLC, vision
/// output): retry/escalate decision rules and default per-role policies.
namespace vc::model {

/// Recovery decision the localization runtime should take in response to a device
/// connection-status change (see decideRecoveryAction()).
enum class LocalizationRecoveryAction {
    Ignore,          ///< Status is not recoverable per policy, or a retry is already pending.
    RetryScheduled,  ///< Perform another reconnect attempt (retry budget not yet exhausted).
    EscalateFault    ///< Retry budget exhausted; the runtime should raise a localization fault.
};

/// Per-role (camera/PLC/vision-output) reconnect policy: which connection statuses are
/// recoverable at all, how many times to retry, and at what interval/timeout.
struct LocalizationRecoveryPolicy {
    QString roleName;                 ///< Human-readable role identifier (e.g. "camera").
    int maxRetries{10};                ///< Max reconnect attempts before escalating to a fault.
    int retryIntervalMs{5000};         ///< Delay between reconnect attempts, in milliseconds.
    int connectTimeoutMs{3000};        ///< Timeout for a single reconnect attempt, in milliseconds.
    bool retryOnConnectFailed{true};   ///< Whether ConnectStatus::ConnectFailed is recoverable.
    bool retryOnLostConnected{true};   ///< Whether ConnectStatus::LostConnected is recoverable.

    /// Returns whether `status` is one this policy will attempt to recover from (per
    /// retryOnConnectFailed / retryOnLostConnected); any other status is never recoverable.
    bool isRecoverableStatus(vc::device::ConnectStatus status) const
    {
        if (status == vc::device::ConnectStatus::ConnectFailed) {
            return retryOnConnectFailed;
        }
        if (status == vc::device::ConnectStatus::LostConnected) {
            return retryOnLostConnected;
        }
        return false;
    }

    /// Returns true if `currentRetryCount` is still below maxRetries.
    bool canRetry(int currentRetryCount) const
    {
        return currentRetryCount < maxRetries;
    }
};

/// Builds the default recovery policy for the camera role (roleName "camera"; all other fields
/// use LocalizationRecoveryPolicy's defaults).
inline LocalizationRecoveryPolicy defaultCameraRecoveryPolicy()
{
    LocalizationRecoveryPolicy policy;
    policy.roleName = QStringLiteral("camera");
    return policy;
}

/// Builds the default recovery policy for the primary PLC role (roleName "primary_plc"; all
/// other fields use LocalizationRecoveryPolicy's defaults).
inline LocalizationRecoveryPolicy defaultPlcRecoveryPolicy()
{
    LocalizationRecoveryPolicy policy;
    policy.roleName = QStringLiteral("primary_plc");
    return policy;
}

/// Builds the default recovery policy for the vision-output role (roleName "vision_output"; all
/// other fields use LocalizationRecoveryPolicy's defaults).
inline LocalizationRecoveryPolicy defaultVisionOutputRecoveryPolicy()
{
    LocalizationRecoveryPolicy policy;
    policy.roleName = QStringLiteral("vision_output");
    return policy;
}

/// Decides how the localization runtime should react to a device's connection-status change,
/// given its recovery policy and current retry state.
/// @param policy recovery policy for the device's role
/// @param status the device's new connection status
/// @param currentRetryCount number of reconnect attempts already performed
/// @param retryAlreadyScheduled true if a retry is already pending, to avoid double-scheduling
/// @return Ignore if `status` isn't recoverable per `policy` or a retry is already scheduled;
///         EscalateFault if the retry budget is exhausted; otherwise RetryScheduled
inline LocalizationRecoveryAction decideRecoveryAction(
    const LocalizationRecoveryPolicy &policy,
    vc::device::ConnectStatus status,
    int currentRetryCount,
    bool retryAlreadyScheduled)
{
    if (!policy.isRecoverableStatus(status)) {
        return LocalizationRecoveryAction::Ignore;
    }

    if (retryAlreadyScheduled) {
        return LocalizationRecoveryAction::Ignore;
    }

    if (!policy.canRetry(currentRetryCount)) {
        return LocalizationRecoveryAction::EscalateFault;
    }

    return LocalizationRecoveryAction::RetryScheduled;
}

/// Builds a human-readable fault message summarizing a failed recovery attempt (role, device id,
/// status, retry count/budget, and timing parameters), suitable for logging or as a fault
/// message payload.
/// @param policy recovery policy in effect for the device's role
/// @param deviceId identifier of the device that failed to recover
/// @param status the device's connection status that triggered the fault
/// @param performedRetries number of reconnect attempts already performed
/// @return the formatted "Recovery failed for role=... deviceId=... status=..." message
inline QString buildRecoveryFaultMessage(
    const LocalizationRecoveryPolicy &policy,
    const QString &deviceId,
    vc::device::ConnectStatus status,
    int performedRetries)
{
    // Fallback covers any future ConnectStatus value; every current value is
    // enumerated (no default:) so -Wswitch / C4062 flags additions.
    QString statusText = QStringLiteral("UnknownStatus");
    switch (status) {
    case vc::device::ConnectStatus::LostConnected:
        statusText = QStringLiteral("LostConnected");
        break;
    case vc::device::ConnectStatus::ConnectFailed:
        statusText = QStringLiteral("ConnectFailed");
        break;
    case vc::device::ConnectStatus::NoConnection:
        statusText = QStringLiteral("NoConnection");
        break;
    case vc::device::ConnectStatus::Disconnected:
        statusText = QStringLiteral("Disconnected");
        break;
    case vc::device::ConnectStatus::Connected:
        statusText = QStringLiteral("Connected");
        break;
    case vc::device::ConnectStatus::Connecting:
        statusText = QStringLiteral("Connecting");
        break;
    }

    return QStringLiteral("Recovery failed for role=%1 deviceId=%2 status=%3 retries=%4/%5 intervalMs=%6 timeoutMs=%7")
        .arg(policy.roleName,
             deviceId,
             statusText)
        .arg(performedRetries)
        .arg(policy.maxRetries)
        .arg(policy.retryIntervalMs)
        .arg(policy.connectTimeoutMs);
}

} // namespace vc::model

#endif // LOCALIZATION_RECOVERY_POLICY_H
