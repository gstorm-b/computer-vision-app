#pragma once

/**
 * @file translation_loader.h
 * @brief Installs the application's translations AND Qt's own, for both application shells.
 */

#include <QString>

class QCoreApplication;
class QTranslator;

namespace vc {
namespace core {

/**
 * @struct TranslationLoadResult
 * @brief What installTranslations() managed to load.
 *
 * The two catalogs are reported separately because they fail separately, and one of the
 * failures is invisible in the UI until someone opens a dialog: the application catalog
 * covers this product's own strings, Qt's catalog covers the standard button and dialog
 * text ("OK", "Cancel", "Close") that no .ts file here will ever contain.
 */
struct TranslationLoadResult {
    bool applicationLoaded = false;  ///< :/i18n/ncr_picking_<locale> was installed.
    bool qtLoaded = false;           ///< A non-empty Qt catalog was installed.
    QString qtCatalog;               ///< Absolute path of the Qt catalog that won, for the log.

    /// The installed translators, or nullptr. Owned by the application they were installed
    /// into — do not delete them. They are handed back so a caller that installed
    /// translations into a long-lived application can take them out again, which is what
    /// switching language without a restart would need, and what a test needs to avoid
    /// leaving the rest of a suite running in Japanese.
    QTranslator *applicationTranslator = nullptr;
    QTranslator *qtTranslator = nullptr;
};

/**
 * @brief Installs the UI translations for @p language, application-side and Qt-side.
 *
 * Call once, before any widget exists — a translator installed later does not re-translate
 * text that is already on screen, which is why the language menu says "next start".
 *
 * @param app       Receives the translators and owns them (they are parented to it).
 * @param language  The persisted AppSettings value: "ja_JP", "en", or the legacy "system".
 *                  "en" and anything unrecognized install nothing; Qt is English already.
 * @return What was loaded. Log it — a missing Qt catalog is otherwise silent until an
 *         operator opens a message box and finds an English button in a Japanese UI.
 */
TranslationLoadResult installTranslations(QCoreApplication &app, const QString &language);

}  // namespace core
}  // namespace vc
