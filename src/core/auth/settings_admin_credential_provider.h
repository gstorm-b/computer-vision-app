#ifndef SETTINGS_ADMIN_CREDENTIAL_PROVIDER_H
#define SETTINGS_ADMIN_CREDENTIAL_PROVIDER_H

#include <QByteArray>
#include <QString>

#include "core/auth/admin_credential_provider.h"

/**
 * @file settings_admin_credential_provider.h
 * @brief SettingsAdminCredentialProvider — admin credential stored as a salted hash in the
 *        application settings file.
 */

namespace vc::auth {

/**
 * @class SettingsAdminCredentialProvider
 * @brief Keeps the admin credential in AppSettings as a random salt plus a SHA-256 hash of
 *        salt + password, and seeds itself with the default password on first use.
 *
 * **What this protects against, and what it does not.** The stored value is a hash, so
 * copying `settings.dat` does not hand over the password. It does **not** protect against
 * anyone who knows the shipped default (`admin`), and it cannot: the file lives on a
 * machine the operator uses. This gate stops a mis-tap and casual curiosity. A credential
 * that resists a determined operator has to live somewhere the operator does not control,
 * which is what the planned dongle provider is for.
 *
 * @note Storing a hash rather than the password also means the password cannot be read
 *       back to show in a UI. That is the point, and any "show current password" feature
 *       would have to be refused rather than implemented.
 */
class SettingsAdminCredentialProvider : public IAdminCredentialProvider {
public:
    /// Password the product ships with, used to seed an installation that has none.
    static constexpr char kDefaultPassword[] = "admin";

    /// Seeds the stored credential with kDefaultPassword if the settings file carries none.
    SettingsAdminCredentialProvider();
    ~SettingsAdminCredentialProvider() override = default;

    /// True once a salt and hash are present in the settings file. False only if seeding
    /// failed, in which case elevation is refused rather than waved through.
    bool isConfigured() const override;

    /**
     * @brief Compares `password` against the stored salted hash.
     * @param[in] password password entered by the user
     * @return true when the credential is configured and the hashes match
     * @note Uses a length-constant comparison on the digests. The timing signal on a local
     *       dialog is not a realistic attack, but a variable-time compare on a credential
     *       is the kind of detail that gets copied into somewhere it does matter.
     */
    bool verify(const QString &password) const override;

    /**
     * @brief Replaces the stored credential with a new password.
     *
     * Generates a fresh random salt each time, so the same password never produces the
     * same stored hash on two machines. Not part of IAdminCredentialProvider: whether a
     * credential can be rewritten is a property of where it lives, and a dongle is not
     * rewritten by the application.
     *
     * @param[in] password the new admin password; an empty password is refused
     * @return true if the new credential was stored
     */
    bool setPassword(const QString &password);

private:
    /// Returns SHA-256 over `salt` + the UTF-8 bytes of `password`.
    static QByteArray hashWith(const QByteArray &salt, const QString &password);
    /// Generates a fresh cryptographically-random salt.
    static QByteArray makeSalt();
};

}  // namespace vc::auth

#endif // SETTINGS_ADMIN_CREDENTIAL_PROVIDER_H
