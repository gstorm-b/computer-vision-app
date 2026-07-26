#include "app_settings.h"

#include <QApplication>
#include <QCborParserError>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

// ---------------------------------------------------------------------------
//  File format constants
// ---------------------------------------------------------------------------

static constexpr quint32 kMagic      = 0x4E435253u;   ///< File magic number, ASCII "NCRS".
static constexpr quint32 kVersion    = 1u;            ///< Settings file schema version.
static constexpr int     kHeaderSize = 4 + 4 + 32;    ///< magic + version + SHA-256 header size, in bytes.

/// Obfuscation key (64 bytes, compile-time constant).
/// @note Changing this key invalidates all existing settings files on disk — users
/// will receive a clean defaults reset on next launch. Increment kVersion as well
/// when doing so.
static constexpr quint8 kObfKey[] = {
    0xA3, 0x7F, 0x2C, 0xE1, 0x58, 0x94, 0x0B, 0xD6,
    0x3A, 0xF2, 0x71, 0xCC, 0x45, 0x89, 0x1E, 0xB0,
    0x67, 0x23, 0xFB, 0x4D, 0x90, 0x5C, 0xA8, 0x16,
    0xDE, 0x82, 0x3F, 0x60, 0xB7, 0x4E, 0xC9, 0x05,
    0x78, 0xAD, 0x31, 0xF4, 0x6B, 0x9E, 0x52, 0xC7,
    0x10, 0x8F, 0xA4, 0x2D, 0xE6, 0x73, 0xBB, 0x49,
    0x1C, 0x95, 0xD0, 0x6E, 0x47, 0xF9, 0x83, 0x2A,
    0xC5, 0x54, 0x0D, 0xE8, 0x3B, 0x76, 0xA1, 0x5F
};

// ---------------------------------------------------------------------------
//  Singleton
// ---------------------------------------------------------------------------

AppSettings* AppSettings::s_instance = nullptr;

/// Returns the process-wide AppSettings singleton, lazily constructing it (parented
/// to qApp) on first call.
AppSettings* AppSettings::instance() {
    if (!s_instance)
        s_instance = new AppSettings(qApp);
    return s_instance;
}

/// Seeds compile-time defaults for every known key, then overlays any values found on
/// disk via load(). Defaults are applied first so that keys added in future schema
/// versions always have a valid fallback value even against an older settings file.
AppSettings::AppSettings(QObject* parent) : QObject(parent) {
    // Compile-time defaults — applied before load() so that keys added in
    // future schema versions always have a valid fallback value.
    m_data[AppKey::Theme]    = QStringLiteral("light");
    m_data[AppKey::Language] = QStringLiteral("en");

    load();
}

// ---------------------------------------------------------------------------
//  Known settings — typed accessors
// ---------------------------------------------------------------------------

/// Returns the current UI theme id (e.g. "light"/"dark"); defaults to "light" until changed.
QString AppSettings::theme() const {
    return m_data.value(AppKey::Theme).toString();
}

/// Returns the current UI locale code (e.g. "en"); defaults to "en" until changed.
QString AppSettings::language() const {
    return m_data.value(AppKey::Language).toString();
}

/// Returns the last folder path the user browsed to in a folder-picker dialog.
QString AppSettings::lastFolderAccessDir() const {
    return m_data.value(AppKey::lastFolderAccessDir).toString();
}

/// Returns the last folder path the user browsed to in an image-picker dialog.
QString AppSettings::lastImageAccessDir() const {
    return m_data.value(AppKey::lastImageAccessDir).toString();
}

/// Sets the UI theme id and persists it (no-op if unchanged; see setValue).
void AppSettings::setTheme(const QString& styleId) {
    setValue(AppKey::Theme, styleId);
}

/// Sets the UI locale code and persists it (no-op if unchanged; see setValue).
void AppSettings::setLanguage(const QString& localeCode) {
    setValue(AppKey::Language, localeCode);
}

/// Sets the last folder-picker access directory and persists it (no-op if unchanged; see setValue).
void AppSettings::setLastFolderAccessDir(const QString& dir) {
    setValue(AppKey::lastFolderAccessDir, dir);
}

/// Sets the last image-picker access directory and persists it (no-op if unchanged; see setValue).
void AppSettings::setLastImageAccessDir(const QString& path) {
    setValue(AppKey::lastImageAccessDir, path);
}

// ---------------------------------------------------------------------------
//  Generic API
// ---------------------------------------------------------------------------

/// Returns the raw value stored under `key`, or `fallback` if the key is unset.
QVariant AppSettings::value(const QString& key, const QVariant& fallback) const {
    return m_data.value(key, fallback);
}

/// Sets the raw value for `key`, persists it to disk via save(), and emits
/// settingChanged. Does nothing (no save, no signal) if `val` equals the current value.
void AppSettings::setValue(const QString& key, const QVariant& val) {
    if (m_data.value(key) == val) return;   // no-op when unchanged
    m_data[key] = val;
    save();
    emit settingChanged(key, val);
}

// ---------------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------------

/// Returns the absolute path of the settings file ("settings.dat" in the OS
/// application-data directory for this app).
QString AppSettings::filePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/settings.dat");
}

/// Reads and decodes the settings file at filePath(), merging decoded values on top of
/// the current (default) m_data. Leaves m_data untouched (i.e. all defaults) if the
/// file is missing, unreadable, or fails integrity/format checks in decode().
void AppSettings::load() {
    QFile f(filePath());
    if (!f.open(QFile::ReadOnly)) return;

    QVariantMap loaded;
    if (!decode(f.readAll(), loaded)) return;  // corrupt or wrong magic → keep defaults

    // Merge: values from disk override defaults, but unknown new-version keys
    // that don't exist in the file retain their default values.
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it)
        m_data[it.key()] = it.value();
}

/// Encodes m_data and writes it (truncating any existing file) to filePath(), creating
/// the parent directory first if needed. Silently does nothing if the file can't be opened.
void AppSettings::save() const {
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (f.open(QFile::WriteOnly | QFile::Truncate))
        f.write(encode(m_data));
}

// ---------------------------------------------------------------------------
//  Encode / Decode
// ---------------------------------------------------------------------------

/// Serializes `map` to the on-disk settings format: CBOR-encodes it, computes a
/// SHA-256 of the plain CBOR payload, then prepends the magic/version/hash header to
/// the XOR-obfuscated CBOR bytes (see the file-format layout documented on
/// AppSettings in app_settings.h).
QByteArray AppSettings::encode(const QVariantMap& map) {
    const QByteArray cbor = QCborValue::fromVariant(map).toCbor();
    const QByteArray hash = QCryptographicHash::hash(cbor, QCryptographicHash::Sha256);
    const QByteArray obf  = obfuscate(cbor);

    QByteArray out;
    out.reserve(kHeaderSize + obf.size());

    auto appendU32 = [&](quint32 v) {
        out.append(char((v >> 24) & 0xFF));
        out.append(char((v >> 16) & 0xFF));
        out.append(char((v >>  8) & 0xFF));
        out.append(char( v        & 0xFF));
    };

    appendU32(kMagic);
    appendU32(kVersion);
    out.append(hash);   // 32 bytes SHA-256
    out.append(obf);
    return out;
}

/// Parses and validates a settings file previously produced by encode(): checks the
/// magic number, de-obfuscates the payload, verifies its SHA-256 against the stored
/// hash, then CBOR-decodes it into `out`.
bool AppSettings::decode(const QByteArray& raw, QVariantMap& out) {
    if (raw.size() < kHeaderSize) return false;

    const quint32 magic = (quint8(raw[0]) << 24) | (quint8(raw[1]) << 16)
                        | (quint8(raw[2]) <<  8) |  quint8(raw[3]);
    if (magic != kMagic) return false;

    // Version field reserved for future migration branches:
    // const quint32 fileVersion = (quint8(raw[4]) << 24) | ... | quint8(raw[7]);

    const QByteArray storedHash = raw.mid(8, 32);
    const QByteArray cbor       = obfuscate(raw.mid(kHeaderSize));  // XOR is symmetric

    // Integrity check — any bit flip or manual edit will fail here.
    if (QCryptographicHash::hash(cbor, QCryptographicHash::Sha256) != storedHash)
        return false;

    QCborParserError err;
    const QCborValue val = QCborValue::fromCbor(cbor, &err);
    if (err.error != QCborError::NoError || !val.isMap()) return false;

    out = val.toVariant().toMap();
    return true;
}

/// XORs every byte of `data` against the repeating compile-time key kObfKey.
/// Symmetric: calling this again on the result recovers the original bytes.
QByteArray AppSettings::obfuscate(const QByteArray& data) {
    static constexpr int keyLen = static_cast<int>(sizeof(kObfKey));
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result[i] = char(quint8(result[i]) ^ kObfKey[i % keyLen]);
    return result;
}
