#include "mainwindow.h"
#include "src/app_settings/app_settings.h"
#include "src/utils/theme_manager.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFontDatabase>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QtGlobal>
#include <QLoggingCategory>
#include <QtAssert>
#include <pylon/PylonIncludes.h>

#include "logger/app_logger.h"

int main(int argc, char *argv[]) {
    // pylon runtime initialize
    Pylon::PylonAutoInitTerm autoInitTerm;

    QApplication a(argc, argv);
    a.setApplicationName(a.tr("NCRN Pick"));
    // a.setQuitOnLastWindowClosed(true);
    a.setWindowIcon(QIcon(":/resrc/icon/software_icon.svg"));

    QTranslator translator;
    const QString savedLang = AppSettings::instance()->language();
    if (savedLang == QLatin1String("ja_JP")) {
        if (translator.load(QStringLiteral(":/i18n/ncr_picking_ja_JP")))
            a.installTranslator(&translator);
    } else if (savedLang == QLatin1String("system")) {
        // legacy value: follow OS locale
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            if (translator.load(":/i18n/ncr_picking_" + QLocale(locale).name())) {
                a.installTranslator(&translator);
                break;
            }
        }
    }
    // "en" (or unrecognized) → no translator; Qt defaults to English

    // Load persisted settings first — other singletons depend on it
    AppSettings::instance();

    // Initialize theme before creating any widgets so the palette is applied first
    ThemeManager::instance();

    MainWindow w;
    w.show();
    LOG_USER_INFO << "Sytem startup...";
    return a.exec();
}
