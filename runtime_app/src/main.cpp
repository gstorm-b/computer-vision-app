#include "runtime_app/ui/runtime_shell_window.h"

#include "core/app_settings/app_settings.h"
#include "core/app_version.h"
#include "core/logger/app_logger.h"
#include "core/utils/shell_handoff.h"
#include "core/utils/single_instance_guard.h"
#include "core/utils/theme_manager.h"
#include "ui/forms/shell_startup.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QStringList>
#include <QTimer>
#include <QTranslator>
#include <pylon/PylonIncludes.h>

/// Which of the two shells this executable is. Everything that differs between them is
/// derived from this one constant rather than repeated.
static constexpr vc::shell::ShellKind kShellKind = vc::shell::ShellKind::OperatorRuntime;

/// Entry point of the operator runtime executable (`ncr_runtime.exe`).
///
/// Mirrors the commissioning shell's startup order — Pylon runtime, QApplication,
/// translator, AppSettings, ThemeManager, then the window — because both shells build on
/// the same singletons and the same ordering constraint: settings must exist before the
/// theme, and the theme before any widget.
///
/// The only difference is which window is shown, and that this one loads a project and
/// enters runtime by itself rather than waiting for an operator to open anything.
/// @param argc argument count forwarded to QApplication
/// @param argv argument vector forwarded to QApplication
/// @return the Qt event loop's exit code
int main(int argc, char *argv[]) {
    // pylon runtime initialize
    Pylon::PylonAutoInitTerm autoInitTerm;

    QApplication a(argc, argv);
    // NOT wrapped in tr(): the application name is an identity, not UI text. QLockFile
    // records it, and a losing launch compares it to work out which shell is running.
    a.setApplicationName(vc::shell::ShellHandoff::applicationName(kShellKind));
    a.setApplicationVersion(vc::version::applicationVersion());
    a.setWindowIcon(QIcon(":/resrc/icon/software_icon.svg"));

    // Before anything touches a device. On a field machine this executable starts at boot
    // AND an operator may click its icon, so a second launch is routine — and two
    // processes competing for the same camera, PLC socket and vision-output port produce
    // symptoms that look exactly like hardware faults.
    //
    // The key is the PRODUCT's, not this executable's: the commissioning shell owns the
    // same devices, so it must be excluded too. Switching between them is the ordered
    // hand-off in ShellHandoff.
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

    RuntimeShellWindow w;
    // show() + raise the window when this process was started to replace the editor
    // shell; a hand-off launch does not get the foreground for free.
    vc::ui::presentShellWindow(&w);

    // Load the project only after the window exists and has claimed the foreground.
    //
    // The order of these two statements is the whole point, and it is why this is posted
    // from here rather than from the window's constructor: presentShellWindow() queues its
    // foreground claim on a zero timer, and a zero timer posted from the constructor would
    // be queued AHEAD of it — putting the seconds of project load and device startup back
    // in front of the raise, which is the bug this exists to fix. Posted here it lands
    // behind the claim.
    QTimer::singleShot(0, &w, [&w]() { w.beginStartup(); });

    QObject::connect(&guard, &SingleInstanceGuard::raiseRequested, &w, [&w]() {
        // Clear a minimised state before raising, otherwise the window is restored to the
        // taskbar rather than to the operator.
        w.setWindowState((w.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        w.show();
        w.raise();
        w.activateWindow();
    });

    // The "Application starting." line, with the shell name and version, is written by
    // resolveSingleInstance() before the lock is taken. When a field report says the
    // runtime will not open what the editor just saved, that version is the first thing
    // worth knowing — and a launch refused because the editor was open is in the log too.
    LOG_USER_INFO << "Operator runtime shell ready.";
    return a.exec();
}
