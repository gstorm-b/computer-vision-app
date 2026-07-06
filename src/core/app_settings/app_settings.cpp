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

static constexpr quint32 kMagic      = 0x4E435253u;   // "NCRS"
static constexpr quint32 kVersion    = 1u;
static constexpr int     kHeaderSize = 4 + 4 + 32;    // magic + version + SHA-256

// ---------------------------------------------------------------------------
//  Obfuscation key (64 bytes, compile-time constant).
//
//  NOTE: Changing this key invalidates all existing settings files on disk —
//  users will receive a clean defaults reset on next launch.  Increment
//  kVersion as well when doing so.
// ---------------------------------------------------------------------------
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

AppSettings* AppSettings::instance() {
    if (!s_instance)
        s_instance = new AppSettings(qApp);
    return s_instance;
}

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

QString AppSettings::theme() const {
    return m_data.value(AppKey::Theme).toString();
}

QString AppSettings::language() const {
    return m_data.value(AppKey::Language).toString();
}

QString AppSettings::lastFolderAccessDir() const {
    return m_data.value(AppKey::lastFolderAccessDir).toString();
}

QString AppSettings::lastImageAccessDir() const {
    return m_data.value(AppKey::lastImageAccessDir).toString();
}

void AppSettings::setTheme(const QString& styleId) {
    setValue(AppKey::Theme, styleId);
}

void AppSettings::setLanguage(const QString& localeCode) {
    setValue(AppKey::Language, localeCode);
}

void AppSettings::setLastFolderAccessDir(const QString& dir) {
    setValue(AppKey::lastFolderAccessDir, dir);
}

void AppSettings::setLastImageAccessDir(const QString& path) {
    setValue(AppKey::lastImageAccessDir, path);
}

// ---------------------------------------------------------------------------
//  Generic API
// ---------------------------------------------------------------------------

QVariant AppSettings::value(const QString& key, const QVariant& fallback) const {
    return m_data.value(key, fallback);
}

void AppSettings::setValue(const QString& key, const QVariant& val) {
    if (m_data.value(key) == val) return;   // no-op when unchanged
    m_data[key] = val;
    save();
    emit settingChanged(key, val);
}

// ---------------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------------

QString AppSettings::filePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/settings.dat");
}

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

QByteArray AppSettings::obfuscate(const QByteArray& data) {
    static constexpr int keyLen = static_cast<int>(sizeof(kObfKey));
    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result[i] = char(quint8(result[i]) ^ kObfKey[i % keyLen]);
    return result;
}
