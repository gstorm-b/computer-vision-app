#include "core/utils/translation_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

namespace vc {
namespace core {

namespace {

/// Resolves the persisted AppSettings value to a locale, or returns false when no translation
/// should be installed at all ("en", or anything unrecognized — Qt's own text is English
/// already, so installing nothing is the correct English behaviour, not a fallback).
bool localeForSetting(const QString &language, QLocale *out)
{
    if (language == QLatin1String("ja_JP")) {
        *out = QLocale(QStringLiteral("ja_JP"));
        return true;
    }
    if (language == QLatin1String("system")) {
        // Legacy setting value: follow the OS. QTranslator's QLocale overload walks that
        // locale's uiLanguages() itself, which is what the hand-written loop here used to do.
        *out = QLocale::system();
        return true;
    }
    return false;
}

/// Directories that may hold Qt's catalogs, in the order they should be tried.
///
/// The deployed folder comes first because it is the one that is guaranteed to match the Qt
/// the executable is actually running against; a developer machine falls through to the Qt
/// installation, which is normally the same Qt anyway.
QStringList qtCatalogDirectories()
{
    QStringList dirs;
    dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/translations");

    const QString installed = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (!installed.isEmpty() && !dirs.contains(installed))
        dirs << installed;

    return dirs;
}

/// Loads Qt's own catalog into @p translator. Returns true when one was found; the file that
/// won is then QTranslator::filePath().
///
/// Two names are tried, because neither one exists in both places this can run from:
///
///   qtbase_<locale>.qm  the catalog itself. Present in a Qt installation. windeployqt does
///                       NOT copy it into a deployed folder.
///   qt_<locale>.qm      present in both, and it is a different file in each. In a deployed
///                       folder windeployqt writes the merged catalog under this name. In a
///                       Qt installation it is an 84-byte *dependency manifest* naming
///                       qtbase_<locale> and qtmultimedia_<locale>, which QTranslator then
///                       loads from the same directory — so it works too, indirectly.
///
/// qtbase is tried first only because it is the direct hit and needs no dependency
/// resolution; either name gets there. isEmpty() is a cheap sanity check against a genuinely
/// empty or corrupt file, not the thing that makes this correct.
///
/// The winning path is read back from the translator rather than rebuilt from the locale
/// name: load() falls back from "ja_JP" to "ja", so a reconstructed path names a file that
/// does not exist and sends whoever reads the log looking for the wrong thing.
bool loadQtCatalog(QTranslator *translator, const QLocale &locale)
{
    const QStringList dirs = qtCatalogDirectories();
    const QStringList names = { QStringLiteral("qtbase"), QStringLiteral("qt") };

    for (const QString &name : names) {
        for (const QString &dir : dirs) {
            if (!QDir(dir).exists())
                continue;
            if (!translator->load(locale, name, QStringLiteral("_"), dir))
                continue;
            if (translator->isEmpty())
                continue;
            return true;
        }
    }
    return false;
}

}  // namespace

/// Installs the application catalog from the embedded :/i18n resource and Qt's own catalog
/// from disk. Both translators are parented to @p app, so they live exactly as long as the
/// application does — a translator that goes out of scope silently stops translating.
TranslationLoadResult installTranslations(QCoreApplication &app, const QString &language)
{
    TranslationLoadResult result;

    QLocale locale;
    if (!localeForSetting(language, &locale))
        return result;

    // --- This product's own strings, compiled into the executable by embed_translations.
    auto *appTranslator = new QTranslator(&app);
    if (appTranslator->load(locale, QStringLiteral("ncr_picking"), QStringLiteral("_"),
                            QStringLiteral(":/i18n"))) {
        result.applicationLoaded = app.installTranslator(appTranslator);
    }
    if (result.applicationLoaded)
        result.applicationTranslator = appTranslator;
    else
        delete appTranslator;

    // --- Qt's own strings: the buttons in every QMessageBox and QDialogButtonBox. These are
    // --- not in this product's .ts and never will be; they ship with Qt.
    auto *qtTranslator = new QTranslator(&app);
    if (loadQtCatalog(qtTranslator, locale)) {
        result.qtLoaded = app.installTranslator(qtTranslator);
        if (result.qtLoaded) {
            result.qtCatalog = qtTranslator->filePath();
            result.qtTranslator = qtTranslator;
        }
    }
    if (!result.qtLoaded)
        delete qtTranslator;

    return result;
}

}  // namespace core
}  // namespace vc
