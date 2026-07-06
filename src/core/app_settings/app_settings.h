#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

// ---------------------------------------------------------------------------
//  Compile-time keys for all known settings.
//  Always use these constants — never raw string literals — to prevent typos
//  and to make key renames a single-point change.
// ---------------------------------------------------------------------------
namespace AppKey {
    inline constexpr char Theme[]    = "app.theme";
    inline constexpr char Language[] = "app.language";
    inline constexpr char lastFolderAccessDir[] = "app.lastFolderAccessDir";
    inline constexpr char lastImageAccessDir[] = "app.lastImageAccessDir"; 
}

// ---------------------------------------------------------------------------
//  AppSettings
//
//  Singleton that persists application-wide preferences to a binary,
//  integrity-checked file in the OS app-data directory.  The payload is
//  XOR-obfuscated with a compile-time key and protected by a SHA-256
//  checksum; a failed check silently resets all values to their defaults.
//
//  File format  (all multi-byte integers big-endian):
//    Offset  Size   Field
//    0       4      Magic: 0x4E435253 ("NCRS")
//    4       4      Schema version (uint32)
//    8       32     SHA-256 of the plain (pre-obfuscation) CBOR payload
//    40      N      XOR-obfuscated CBOR-encoded QVariantMap
//
//  Extending:
//    1. Add a new key constant to AppKey:: above.
//    2. Add a default value in the AppSettings constructor.
//    3. Add a typed getter/setter pair in the "Known settings" section.
//    If a schema change is not backward-compatible, increment kVersion and
//    add a migration branch in decode().
// ---------------------------------------------------------------------------
class AppSettings : public QObject {
    Q_OBJECT

public:
    static AppSettings* instance();

    // --- Known settings (type-safe accessors) ---

    QString theme()    const;
    QString language() const;
    QString lastFolderAccessDir() const;
    QString lastImageAccessDir() const;

    void setTheme(const QString& styleId);
    void setLanguage(const QString& localeCode);
    void setLastFolderAccessDir(const QString& dir);
    void setLastImageAccessDir(const QString& dir);

    // --- Generic API for future or one-off settings ---

    QVariant value(const QString& key, const QVariant& fallback = {}) const;
    void     setValue(const QString& key, const QVariant& val);

signals:
    // Emitted after a value has changed and been persisted to disk.
    void settingChanged(const QString& key, const QVariant& newValue);

private:
    explicit AppSettings(QObject* parent = nullptr);

    void load();
    void save() const;

    static QByteArray encode(const QVariantMap& map);
    static bool       decode(const QByteArray& raw, QVariantMap& out);
    static QByteArray obfuscate(const QByteArray& data);  // XOR — symmetric

    static QString filePath();

    QVariantMap         m_data;
    static AppSettings* s_instance;
};

#endif // APP_SETTINGS_H
