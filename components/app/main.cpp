#include "mainwindow.h"
#include "core/app_settings/app_settings.h"
#include "core/app_version.h"
#include "core/utils/shell_handoff.h"
#include "core/utils/single_instance_guard.h"
#include "core/utils/theme_manager.h"
#include "ui/forms/shell_startup.h"

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

#include "core/logger/app_logger.h"

/// Which of the two shells this executable is. Everything that differs between them —
/// application name, which sibling to hand off to, whether entering it needs Admin — is
/// derived from this one constant rather than repeated.
static constexpr vc::shell::ShellKind kShellKind = vc::shell::ShellKind::Commissioning;

/// Application entry point: initializes the Basler Pylon runtime, constructs the QApplication,
/// loads the saved UI language and installs the matching translator (falling back to the OS
/// locale for the legacy "system" setting, or to English for "en"/unrecognized values),
/// initializes AppSettings and ThemeManager (before any widgets are created), then creates and
/// shows the MainWindow and enters the Qt event loop.
/// @param argc argument count forwarded to QApplication
/// @param argv argument vector forwarded to QApplication
/// @return the Qt event loop's exit code (QApplication::exec() return value)
int main(int argc, char *argv[]) {
    // pylon runtime initialize
    Pylon::PylonAutoInitTerm autoInitTerm;

    QApplication a(argc, argv);
    // NOT wrapped in tr(): the application name is an identity, not UI text. QLockFile
    // records it, and a losing launch compares it to work out which shell is running — a
    // name that changed with the UI language would break that in Japanese and nowhere else.
    a.setApplicationName(vc::shell::ShellHandoff::applicationName(kShellKind));
    a.setApplicationVersion(vc::version::applicationVersion());
    // a.setQuitOnLastWindowClosed(true);
    a.setWindowIcon(QIcon(":/resrc/icon/software_icon.svg"));

    // One instance of the PRODUCT, not of this executable: the two shells own the same
    // camera, PLC socket and vision-output port exclusively, so they exclude each other as
    // well as themselves. Switching between them is the ordered hand-off in ShellHandoff,
    // never two processes overlapping.
    SingleInstanceGuard guard{ QLatin1String(vc::shell::ShellHandoff::kInstanceKey) };
    switch (vc::ui::resolveSingleInstance(guard, kShellKind)) {
    case vc::ui::StartupDecision::Proceed:
        break;
    case vc::ui::StartupDecision::ExitQuietly:
        return 0;  // Not an error: the application the user asked for is already up.
    case vc::ui::StartupDecision::ExitWithError:
        return 1;
    }

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
    // show() + raise the window when this process was started to replace the runtime
    // shell; a hand-off launch does not get the foreground for free.
    vc::ui::presentShellWindow(&w);

    QObject::connect(&guard, &SingleInstanceGuard::raiseRequested, &w, [&w]() {
        // Clear a minimised state before raising, otherwise the window is restored to the
        // taskbar rather than to the user.
        w.setWindowState((w.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        w.show();
        w.raise();
        w.activateWindow();
    });

    // The "Application starting." line, with the shell name and version, is written by
    // resolveSingleInstance() before the lock is taken — so a launch that is refused
    // because the other shell is running is in the log too, which is the case someone
    // reading the log afterwards is most likely trying to explain.
    LOG_USER_INFO << "Commissioning shell ready.";
    return a.exec();
}
