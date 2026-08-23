#include "core/utils/shell_handoff.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "core/logger/app_logger.h"

namespace vc::shell {

/// Returns the other shell.
ShellKind ShellHandoff::siblingOf(ShellKind kind)
{
    return kind == ShellKind::Commissioning ? ShellKind::OperatorRuntime
                                            : ShellKind::Commissioning;
}

/// Canonical untranslated application name. See the header for why tr() is not used.
QString ShellHandoff::applicationName(ShellKind kind)
{
    switch (kind) {
    case ShellKind::OperatorRuntime:
        return QStringLiteral("NCRN Pick Runtime");
    case ShellKind::Commissioning:
        break;
    }
    return QStringLiteral("NCRN Pick");
}

/// Executable file name for `kind`.
QString ShellHandoff::executableName(ShellKind kind)
{
#ifdef Q_OS_WIN
    const QString suffix = QStringLiteral(".exe");
#else
    const QString suffix;
#endif
    switch (kind) {
    case ShellKind::OperatorRuntime:
        return QStringLiteral("ncr_runtime") + suffix;
    case ShellKind::Commissioning:
        break;
    }
    return QStringLiteral("ncr_picking") + suffix;
}

/// Absolute path of the sibling, resolved next to the running executable.
QString ShellHandoff::siblingPath(ShellKind self)
{
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(executableName(siblingOf(self)));
}

/// True when the command line carries the hand-off flag.
bool ShellHandoff::wasStartedForHandoff(const QStringList &arguments)
{
    return arguments.contains(QLatin1String(kHandoffFlag));
}

/// Starts the sibling detached, flagged so it waits for this process to release the lock.
bool ShellHandoff::launchSibling(ShellKind self, QString *error)
{
    const QString path = siblingPath(self);

    // Checked before launching rather than relying on startDetached's return: an install
    // missing one of the two executables is a real field case (a partial copy), and
    // "ncr_runtime.exe was not found next to this application" is a diagnosis, whereas
    // "could not start the other application" is not.
    if (!QFileInfo::exists(path)) {
        if (error != nullptr) {
            *error = QCoreApplication::translate(
                         "ShellHandoff",
                         "%1 was not found next to this application:\n%2\n\n"
                         "Both executables must be installed in the same folder.")
                         .arg(executableName(siblingOf(self)), path);
        }
        LOG_USER_ERR << "Shell hand-off failed: sibling executable is missing." << path;
        return false;
    }

    qint64 pid = 0;
    const bool started = QProcess::startDetached(
        path,
        QStringList{ QLatin1String(kHandoffFlag) },
        QCoreApplication::applicationDirPath(),
        &pid);

    if (!started) {
        if (error != nullptr) {
            *error = QCoreApplication::translate(
                         "ShellHandoff",
                         "%1 could not be started:\n%2")
                         .arg(executableName(siblingOf(self)), path);
        }
        LOG_USER_ERR << "Shell hand-off failed: sibling could not be started." << path;
        return false;
    }

#ifdef Q_OS_WIN
    // Hand the foreground to the incoming process before this one gives it up.
    //
    // Windows only lets the process that currently owns the foreground grant that right to
    // another. Without this the new shell comes up BEHIND whatever is on screen and only
    // flashes its taskbar button — the operator switches applications and appears to get
    // nothing, then has to hunt for the window. The permission is granted to the process
    // and survives until it is used, so it does not matter that the child has no window
    // yet. The incoming shell still has to ask for the foreground; see
    // vc::ui::presentShellWindow().
    if (pid > 0) {
        ::AllowSetForegroundWindow(static_cast<DWORD>(pid));
    }
#endif

    LOG_USER_INFO << "Shell hand-off: sibling started."
                  << "target=" << applicationName(siblingOf(self))
                  << "pid=" << pid;
    return true;
}

}  // namespace vc::shell
