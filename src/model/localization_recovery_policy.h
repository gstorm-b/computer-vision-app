#ifndef LOCALIZATION_RECOVERY_POLICY_H
#define LOCALIZATION_RECOVERY_POLICY_H

#include <QString>

#include "device/idevice.h"

/**
 * @file localization_recovery_policy.h
 * @brief Connection-recovery policy model for localization-owned devices (camera, PLC, vision
 *        output): retry decision rules and default per-role policies.
 *
 * @note Reconnect is UNLIMITED. A recoverable connection loss is retried at the policy's
 *       interval until the runtime is torn down; it never escalates to a task fault. The
 *       goal is that a dropped link recovers and returns to Ready without any operator
 *       action.
 *
 *       Operator-visible consequence: a device that never comes back leaves the task in
 *       Recovering indefinitely, publishing bTaskReady=false with bTaskFault=false. A PLC
 *       program that detects a dead link by waiting for bTaskFault will wait forever — it
 *       must watch bTaskReady (or a timeout of its own) instead. See
 *       docs/domains/task_localization/plc_signal_contract.md → "Recovery Behavior".
 */

namespace vc::model {

/**
 * @enum LocalizationRecoveryAction
 * @brief Recovery decision the localization runtime should take in response to a device
 *        connection-status change (see decideRecoveryAction()).
 *
 * @note There is deliberately no "escalate to fault" action. Reconnect is unlimited: a
 *       recoverable status is retried until the runtime ends, so a lost device never
 *       turns into a task fault on its own. See the file-level note.
 */
enum class LocalizationRecoveryAction {
    Ignore,         ///< Status is not recoverable per policy, or a retry is already pending.
    RetryScheduled  ///< Perform another reconnect attempt.
};

/**
 * @struct LocalizationRecoveryPolicy
 * @brief Per-role (camera/PLC/vision-output) reconnect policy: which connection statuses are
 *        recoverable at all, and at what interval/timeout to retry them.
 *
 * The policy answers "how often do we retry", never "how many times". Retrying is
 * unbounded by design — the runtime keeps trying until it is torn down.
 */
struct LocalizationRecoveryPolicy {
    QString roleName;                 ///< Human-readable role identifier (e.g. "camera").
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

/**
 * @brief Decides how the localization runtime should react to a device's connection-status
 *        change, given its recovery policy and current retry state.
 * @param[in] policy               recovery policy for the device's role
 * @param[in] status               the device's new connection status
 * @param[in] retryAlreadyScheduled true if a retry is already pending, to avoid double-scheduling
 * @return Ignore if `status` isn't recoverable per `policy` or a retry is already scheduled;
 *         otherwise RetryScheduled
 */
inline LocalizationRecoveryAction decideRecoveryAction(
    const LocalizationRecoveryPolicy &policy,
    vc::device::ConnectStatus status,
    bool retryAlreadyScheduled)
{
    if (!policy.isRecoverableStatus(status)) {
        return LocalizationRecoveryAction::Ignore;
    }

    if (retryAlreadyScheduled) {
        return LocalizationRecoveryAction::Ignore;
    }

    return LocalizationRecoveryAction::RetryScheduled;
}

} // namespace vc::model

#endif // LOCALIZATION_RECOVERY_POLICY_H
