#include "core/auth/access_control.h"

#include <QCoreApplication>

#include "core/auth/settings_admin_credential_provider.h"
#include "core/logger/app_logger.h"

namespace vc::auth {

AccessControl *AccessControl::s_instance = nullptr;

/// Returns the process-wide instance, lazily constructing it parented to qApp so it dies
/// with the application rather than leaking or outliving the settings it reads.
AccessControl *AccessControl::instance()
{
    if (s_instance == nullptr) {
        s_instance = new AccessControl(qApp);
    }
    return s_instance;
}

/// Starts in Operator. No provider is built here: constructing one seeds the settings file,
/// and that must not happen as a side effect of something merely asking for the current
/// role at startup.
AccessControl::AccessControl(QObject *parent)
    : QObject(parent)
{
}

/// Returns the active provider, creating the default settings-backed one on first use.
IAdminCredentialProvider *AccessControl::provider() const
{
    if (!m_provider) {
        m_provider = std::make_unique<SettingsAdminCredentialProvider>();
    }
    return m_provider.get();
}

/// Replaces the credential source. A null argument restores the default rather than
/// leaving the application with no way to authenticate at all.
void AccessControl::setCredentialProvider(std::unique_ptr<IAdminCredentialProvider> provider)
{
    if (provider) {
        m_provider = std::move(provider);
        LOG_DEV_INFO << "Access control: credential provider replaced.";
    } else {
        m_provider.reset();
        LOG_DEV_INFO << "Access control: credential provider reset to the default.";
    }
}

/// Whether a credential source is available to check against.
bool AccessControl::canElevate() const
{
    return provider()->isConfigured();
}

/// Attempts to become Admin.
bool AccessControl::elevate(const QString &password)
{
    if (!provider()->verify(password)) {
        // Logged at USER level on purpose: a run of failed attempts is something the
        // person reading the event log afterwards should be able to see.
        LOG_USER_WARN << "Access level change refused: incorrect admin password.";
        return false;
    }

    if (m_role == AccessRole::Admin) {
        return true;  // Already there; nothing changed, so no signal.
    }

    m_role = AccessRole::Admin;
    LOG_USER_INFO << "Access level changed." << "role=" << roleName(m_role);
    emit roleChanged(m_role);
    return true;
}

/// Returns to Operator.
void AccessControl::dropToOperator()
{
    if (m_role == AccessRole::Operator) {
        return;
    }
    m_role = AccessRole::Operator;
    LOG_USER_INFO << "Access level changed." << "role=" << roleName(m_role);
    emit roleChanged(m_role);
}

/// Human-readable name for `role`.
QString AccessControl::roleName(AccessRole role)
{
    switch (role) {
    case AccessRole::Admin:
        return QStringLiteral("Admin");
    case AccessRole::Operator:
        break;
    }
    return QStringLiteral("Operator");
}

}  // namespace vc::auth
