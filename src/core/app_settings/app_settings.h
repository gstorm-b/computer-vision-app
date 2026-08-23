#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

/**
 * @file app_settings.h
 * @brief AppSettings — singleton that persists application-wide preferences to a
 *        binary, integrity-checked file in the OS app-data directory, plus the
 *        AppKey:: namespace of compile-time setting-key constants.
 *
 * File format (all multi-byte integers big-endian):
 *   Offset  Size   Field
 *   0       4      Magic: 0x4E435253 ("NCRS")
 *   4       4      Schema version (uint32)
 *   8       32     SHA-256 of the plain (pre-obfuscation) CBOR payload
 *   40      N      XOR-obfuscated CBOR-encoded QVariantMap
 *
 * Extending:
 *   1. Add a new key constant to AppKey:: below.
 *   2. Add a default value in the AppSettings constructor.
 *   3. Add a typed getter/setter pair in the "Known settings" section.
 *   If a schema change is not backward-compatible, increment kVersion and
 *   add a migration branch in decode().
 */

/// Compile-time keys for all known settings.
/// Always use these constants — never raw string literals — to prevent typos
/// and to make key renames a single-point change.
namespace AppKey {
    inline constexpr char Theme[]    = "app.theme";  ///< Key for the persisted UI theme id.
    inline constexpr char Language[] = "app.language";  ///< Key for the persisted UI locale code.
    inline constexpr char lastFolderAccessDir[] = "app.lastFolderAccessDir";  ///< Key for the last folder path used in a generic folder-browse dialog.
    inline constexpr char lastImageAccessDir[] = "app.lastImageAccessDir";  ///< Key for the last folder path used in an image-file dialog.
    inline constexpr char lastRuntimeProjectPath[] = "app.lastRuntimeProjectPath";  ///< Key for the project file the operator runtime shell last ran.
    inline constexpr char adminPasswordSalt[] = "auth.adminPasswordSalt";  ///< Hex-encoded random salt for the admin credential; never the password itself.
    inline constexpr char adminPasswordHash[] = "auth.adminPasswordHash";  ///< Hex-encoded SHA-256 of salt + admin password. Storing the hash means copying this file does not hand over the password.
}

/**
 * @class AppSettings
 * @brief Singleton that persists application-wide preferences to a binary,
 *        integrity-checked file in the OS app-data directory. The payload is
 *        XOR-obfuscated with a compile-time key and protected by a SHA-256
 *        checksum; a failed check silently resets all values to their defaults.
 */
class AppSettings : public QObject {
    Q_OBJECT

public:
    /// Returns the process-wide singleton, lazily constructing it (parented to qApp) on first call.
    static AppSettings* instance();

    // --- Known settings (type-safe accessors) ---

    /// Returns the persisted UI theme id, defaulting to "light" if never set.
    QString theme()    const;
    /// Returns the persisted UI locale code, defaulting to "en" if never set.
    QString language() const;
    /// Returns the last folder path used in a generic folder-browse dialog.
    QString lastFolderAccessDir() const;
    /// Returns the last folder path used in an image-file dialog.
    QString lastImageAccessDir() const;
    /// Returns the absolute path of the project file the operator runtime shell last ran
    /// successfully, or an empty string if it has never run one. Used by
    /// `ncr_runtime.exe` to skip the project-select page on startup.
    QString lastRuntimeProjectPath() const;

    /// Persists `styleId` as the current theme (via setValue()); no-op if unchanged.
    void setTheme(const QString& styleId);
    /// Persists `localeCode` as the current UI language (via setValue()); no-op if unchanged.
    void setLanguage(const QString& localeCode);
    /// Persists `dir` as the last folder-browse-dialog path (via setValue()); no-op if unchanged.
    void setLastFolderAccessDir(const QString& dir);
    /// Persists `dir` as the last image-file-dialog path (via setValue()); no-op if unchanged.
    void setLastImageAccessDir(const QString& dir);
    /// Persists `path` as the project the operator runtime shell last ran successfully.
    /// Written only after a load succeeds, so a corrupt or missing file never becomes the
    /// remembered one.
    void setLastRuntimeProjectPath(const QString& path);

    // --- Generic API for future or one-off settings ---

    /**
     * @brief Generic lookup for one-off or future settings keys not covered by a typed accessor.
     * @param[in] key      settings key to look up (prefer AppKey:: constants when one exists)
     * @param[in] fallback value returned when `key` has never been set
     * @return the stored value, or `fallback` if `key` is absent
     */
    QVariant value(const QString& key, const QVariant& fallback = {}) const;
    /**
     * @brief Generic setter: stores `val` under `key`, persists to disk, and emits
     *        settingChanged() — but only when `val` differs from the current value.
     * @param[in] key settings key to store (prefer AppKey:: constants when one exists)
     * @param[in] val new value to persist
     */
    void     setValue(const QString& key, const QVariant& val);

signals:
    /**
     * @brief Emitted after a value has changed and been persisted to disk.
     * @param[in] key      the settings key that changed
     * @param[in] newValue the newly stored value
     */
    void settingChanged(const QString& key, const QVariant& newValue);

private:
    /// Constructs the store: seeds compile-time defaults for known keys, then calls load()
    /// to overlay any values persisted on disk.
    explicit AppSettings(QObject* parent = nullptr);

    /// Reads the settings file from disk, verifies it via decode(), and merges recovered
    /// values over the compile-time defaults; leaves defaults untouched if the file is
    /// missing or fails validation.
    void load();
    /// Serializes the current settings map via encode() and overwrites the settings file on
    /// disk, creating the containing directory first if needed.
    void save() const;

    /**
     * @brief Encodes `map` as CBOR, hashes it (SHA-256), XOR-obfuscates the CBOR bytes, and
     *        prepends the magic/version/hash header described in the class comment.
     * @param[in] map settings map to serialize
     * @return the full on-disk byte layout (header + obfuscated payload)
     */
    static QByteArray encode(const QVariantMap& map);
    /**
     * @brief Validates `raw` against the file format (magic, size, SHA-256 of the
     *        de-obfuscated payload) and, on success, decodes the CBOR payload into `out`.
     * @param[in]  raw raw file bytes as read from disk
     * @param[out] out receives the decoded settings map; only written on success
     * @return true if the header, hash, and CBOR all validated; false otherwise (`out` left untouched)
     */
    static bool       decode(const QByteArray& raw, QVariantMap& out);
    /// XORs `data` with the compile-time obfuscation key byte-by-byte; symmetric, so calling
    /// it a second time reverses the obfuscation.
    static QByteArray obfuscate(const QByteArray& data);  // XOR — symmetric

    QVariantMap         m_data;  ///< In-memory cache of all settings key/value pairs; the source of truth behind value() and the typed getters.
    static AppSettings* s_instance;  ///< Backing storage for the lazily-created singleton returned by instance().

public:
    /// Folder name under the OS app-data directory that holds this PRODUCT's settings.
    /// Shared by both application shells on purpose — see filePath().
    static constexpr char kProductFolder[] = "NCRN Pick";

    /**
     * @brief Returns the absolute path to the shared settings file.
     *
     * "settings.dat" under a product-scoped folder. **Deliberately independent of
     * QCoreApplication::applicationName()** — the commissioning shell and the operator
     * runtime set different application names, and deriving the path from one gave them a
     * settings file each, so a theme or language chosen in one was invisible to the other.
     *
     * Public so the architecture contract test can assert that property directly:
     * changing the application name must not move this path.
     */
    static QString filePath();

    /**
     * @brief Returns the pre-Phase-7 application-scoped path for the current executable.
     *
     * Where a shell whose application name differs from the product folder used to write.
     * Read once as a fallback by load() when the shared file is absent, so those settings
     * survive the unification; never written to. Unlike filePath(), this path **does**
     * move with the application name — that is what makes it the legacy one.
     */
    static QString legacyFilePath();

};

#endif // APP_SETTINGS_H
