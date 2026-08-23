#ifndef ADMIN_CREDENTIAL_PROVIDER_H
#define ADMIN_CREDENTIAL_PROVIDER_H

#include <QString>

/**
 * @file admin_credential_provider.h
 * @brief IAdminCredentialProvider — the seam between "is this the admin password?" and
 *        wherever the answer actually comes from.
 */

namespace vc::auth {

/**
 * @class IAdminCredentialProvider
 * @brief Answers whether a supplied password is the admin credential, without revealing
 *        what the credential is.
 *
 * One implementation exists today (`SettingsAdminCredentialProvider`, backed by the
 * application settings file). A hardware-dongle implementation is planned, which is the
 * reason this interface exists at all — `AGENT.md` asks for no abstraction until a second
 * implementation proves the shape, and here the second one is a stated requirement rather
 * than a guess.
 *
 * @note verify() takes the password and returns a verdict; it never returns, exposes or
 *       compares against a retrievable secret. That asymmetry is deliberate and it is what
 *       makes a dongle implementable: a dongle can answer the question without ever handing
 *       the secret to the process.
 * @note Changing the credential is **not** on this interface. Whether a credential can be
 *       changed, and how, is a property of where it lives — a dongle is not rewritten by
 *       the application. The settings-backed implementation exposes its own setter.
 */
class IAdminCredentialProvider {
public:
    virtual ~IAdminCredentialProvider() = default;

    /**
     * @brief Whether a credential is available to check against.
     * @return false when the source is absent — no dongle plugged in, no stored hash. A
     *         caller must treat that as "cannot elevate", never as "everything passes"
     */
    virtual bool isConfigured() const = 0;

    /**
     * @brief Checks `password` against the admin credential.
     * @param[in] password the password entered by the user
     * @return true only when the credential source is available AND the password matches
     */
    virtual bool verify(const QString &password) const = 0;
};

}  // namespace vc::auth

#endif // ADMIN_CREDENTIAL_PROVIDER_H
