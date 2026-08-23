#include "core/auth/settings_admin_credential_provider.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include "core/app_settings/app_settings.h"

namespace vc::auth {

namespace {

/// Salt length in bytes. 16 is the usual floor for a salt whose only job is to make two
/// installations with the same password store different hashes.
constexpr int kSaltBytes = 16;

}  // namespace

/// Seeds the stored credential with the shipped default when the settings file carries
/// none, so a fresh installation has a working admin password rather than an unusable
/// "not configured" state the user cannot get out of.
SettingsAdminCredentialProvider::SettingsAdminCredentialProvider()
{
    if (!isConfigured()) {
        setPassword(QString::fromLatin1(kDefaultPassword));
    }
}

/// True once both halves of the credential are present. Checked as a pair: a salt without
/// a hash (or the reverse) is a half-written state that must not read as configured.
bool SettingsAdminCredentialProvider::isConfigured() const
{
    const AppSettings *settings = AppSettings::instance();
    return !settings->value(QLatin1String(AppKey::adminPasswordSalt)).toString().isEmpty()
        && !settings->value(QLatin1String(AppKey::adminPasswordHash)).toString().isEmpty();
}

/// Compares `password` against the stored salted hash.
bool SettingsAdminCredentialProvider::verify(const QString &password) const
{
    if (!isConfigured()) {
        // No credential to check against. Refuse rather than accept — an unconfigured
        // gate that lets everyone through is worse than no gate, because it looks like one.
        return false;
    }

    const AppSettings *settings = AppSettings::instance();
    const QByteArray salt = QByteArray::fromHex(
        settings->value(QLatin1String(AppKey::adminPasswordSalt)).toString().toLatin1());
    const QByteArray stored = QByteArray::fromHex(
        settings->value(QLatin1String(AppKey::adminPasswordHash)).toString().toLatin1());

    const QByteArray candidate = hashWith(salt, password);

    if (candidate.size() != stored.size()) {
        return false;
    }

    // Constant-time over the digest. The timing signal on a local password dialog is not a
    // realistic attack; a variable-time compare on a credential is the kind of detail that
    // gets copied into somewhere it does matter.
    quint8 diff = 0;
    for (int i = 0; i < stored.size(); ++i) {
        diff |= static_cast<quint8>(stored.at(i) ^ candidate.at(i));
    }
    return diff == 0;
}

/// Stores a new salt and hash for `password`.
bool SettingsAdminCredentialProvider::setPassword(const QString &password)
{
    if (password.isEmpty()) {
        // An empty admin password would make the gate a formality that still looks like a
        // gate. Refuse it here rather than relying on every caller to check.
        return false;
    }

    const QByteArray salt = makeSalt();
    AppSettings *settings = AppSettings::instance();
    settings->setValue(QLatin1String(AppKey::adminPasswordSalt),
                       QString::fromLatin1(salt.toHex()));
    settings->setValue(QLatin1String(AppKey::adminPasswordHash),
                       QString::fromLatin1(hashWith(salt, password).toHex()));
    return true;
}

/// Returns SHA-256 over the salt followed by the UTF-8 bytes of `password`.
QByteArray SettingsAdminCredentialProvider::hashWith(const QByteArray &salt,
                                                     const QString &password)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(salt);
    hash.addData(password.toUtf8());
    return hash.result();
}

/// Generates a fresh salt from the system's cryptographic generator.
///
/// `QRandomGenerator::system()` rather than `global()`: the global generator is seeded
/// once and is a PRNG, which is fine for jitter and wrong for a credential.
QByteArray SettingsAdminCredentialProvider::makeSalt()
{
    static_assert(kSaltBytes % 4 == 0,
                  "fillRange writes whole quint32 words, so the salt length must be a "
                  "multiple of 4");

    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()),
                                          kSaltBytes / 4);
    return salt;
}

}  // namespace vc::auth
