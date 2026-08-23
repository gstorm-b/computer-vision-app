#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <QObject>
#include <QString>

#include <memory>

#include "core/auth/admin_credential_provider.h"

/**
 * @file access_control.h
 * @brief AccessRole and AccessControl — who the current user is allowed to be, and the
 *        single place that decides it.
 */

namespace vc::auth {

/**
 * @enum AccessRole
 * @brief What the current user is allowed to do.
 */
enum class AccessRole {
    Operator,  ///< Run tasks and watch dashboards. The default, and needs no password.
    Admin      ///< Commissioning: devices, calibration, patterns, signals. Needs the admin password.
};

/**
 * @class AccessControl
 * @brief Process-wide current access role, and the only place a role is granted.
 *
 * Both shells use it, for different questions: the commissioning app decides what its
 * Access Level menu reports and what it lets through, and the operator runtime uses it to
 * refuse a switch into commissioning by someone who only has the operator's authority.
 *
 * `Operator` is the role every process starts in. Nothing prompts for a password until
 * something asks to be `Admin`, so an operator who never touches commissioning never sees
 * a login dialog.
 *
 * **What this is and is not.** It answers "is this person allowed to do the thing" and it
 * is deliberately separate from "did this person mean to do the thing" — a switch between
 * shells confirms as well as authorises, because a mis-tap by someone who legitimately
 * holds the admin password still stops a production line.
 *
 * @note The role is per process and is dropped on exit. There is no "stay logged in", by
 *       design: an unattended station left in Admin is the failure this is meant to avoid.
 * @note The credential source is swappable via setCredentialProvider() — see
 *       IAdminCredentialProvider for why that seam exists.
 */
class AccessControl : public QObject {
    Q_OBJECT

public:
    /// Returns the process-wide instance, lazily constructed and parented to qApp.
    static AccessControl *instance();

    /**
     * @brief Replaces the credential source.
     *
     * Call before the first elevate(). Passing null restores the default settings-backed
     * provider rather than leaving the application with no way to authenticate.
     *
     * @param[in] provider the new source; ownership is taken
     */
    void setCredentialProvider(std::unique_ptr<IAdminCredentialProvider> provider);

    /// Returns the current role.
    AccessRole role() const { return m_role; }
    /// Convenience for the common check.
    bool isAdmin() const { return m_role == AccessRole::Admin; }

    /// Whether a credential source is available at all. False means elevate() cannot
    /// succeed, and a caller should say so rather than showing a dialog that can only fail.
    bool canElevate() const;

    /**
     * @brief Attempts to become Admin.
     * @param[in] password password entered by the user
     * @return true if the password verified and the role is now Admin
     * @note A failed attempt leaves the current role untouched and is logged, so a run of
     *       failures is visible after the fact rather than only to whoever was watching.
     */
    bool elevate(const QString &password);

    /// Returns to Operator. Always succeeds — giving up authority never needs permission.
    void dropToOperator();

    /// Human-readable name for `role`, for menus and log lines.
    static QString roleName(AccessRole role);

signals:
    /// Emitted whenever the role actually changes; not emitted for a failed elevate().
    void roleChanged(AccessRole role);

private:
    /// Starts in Operator with no provider; the default provider is created on first use.
    explicit AccessControl(QObject *parent = nullptr);

    /// Returns the active provider, creating the default settings-backed one on first call.
    IAdminCredentialProvider *provider() const;

    AccessRole m_role{AccessRole::Operator};                             ///< Current role; Operator until something elevates.
    mutable std::unique_ptr<IAdminCredentialProvider> m_provider;        ///< Credential source; created lazily so construction order does not matter.
    static AccessControl *s_instance;                                    ///< Backing storage for instance().
};

}  // namespace vc::auth

#endif // ACCESS_CONTROL_H
