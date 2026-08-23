#include "ui/forms/shell_startup.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "core/logger/app_logger.h"
#include "core/utils/single_instance_guard.h"
#include "ui/forms/admin_login_dialog.h"

using vc::shell::ShellHandoff;
using vc::shell::ShellKind;

namespace vc::ui {

/// Runs the single-instance check and reports the outcome.
StartupDecision resolveSingleInstance(SingleInstanceGuard &guard, ShellKind self)
{
    const bool handoff = ShellHandoff::wasStartedForHandoff(QCoreApplication::arguments());

    LOG_USER_INFO << "Application starting."
                  << "shell=" << ShellHandoff::applicationName(self)
                  << "version=" << QCoreApplication::applicationVersion()
                  << "handoff=" << handoff;

    if (guard.tryAcquire(handoff ? ShellHandoff::kHandoffAcquireTimeoutMs : 0)) {
        return StartupDecision::Proceed;
    }

    // Read the holder BEFORE notifying it: once it comes forward it may exit, and then
    // there is nothing left to name in the message.
    const QString holder = guard.runningInstanceName();
    const QString ownName = ShellHandoff::applicationName(self);

    if (handoff) {
        // The sibling this process was launched to replace is still holding the lock after
        // the full timeout. Do not exit silently — from the user's side that is
        // indistinguishable from the switch doing nothing at all.
        LOG_USER_ERR << "Shell hand-off failed: the previous application did not exit."
                     << "holder=" << holder;
        QMessageBox::warning(
            nullptr,
            ownName,
            QCoreApplication::translate(
                "ShellStartup",
                "The previous application is still shutting down, so %1 could not start.\n\n"
                "Wait a few seconds and start it again.")
                .arg(ownName));
        return StartupDecision::ExitWithError;
    }

    if (!holder.isEmpty() && holder != ownName) {
        // The OTHER shell holds the lock. Say so before raising it, or the user clicks one
        // icon and a different application appears.
        LOG_USER_INFO << "Launch blocked: the other shell is running." << "holder=" << holder;
        QMessageBox::information(
            nullptr,
            ownName,
            QCoreApplication::translate(
                "ShellStartup",
                "%1 is already running, and only one of the two applications can run at a "
                "time — they share the camera, the PLC connection and the output port.\n\n"
                "To switch, use Project → Open Editor (or Open Runtime) in the running "
                "application.")
                .arg(holder));
    }

    // Same shell, or an unreadable lock: bring the running window forward and go away.
    guard.notifyExistingInstance();
    return StartupDecision::ExitQuietly;
}

/// Shows `window`, and claims the foreground when this process replaced a sibling.
void presentShellWindow(QWidget *window)
{
    if (window == nullptr) {
        return;
    }

    window->show();

    if (!ShellHandoff::wasStartedForHandoff(QCoreApplication::arguments())) {
        return;  // A user-initiated launch comes forward on its own.
    }

    // Claim the foreground twice: once now, once on the first turn of the event loop.
    //
    // Not belt-and-braces. show() creates and maps the native window synchronously, so the
    // immediate claim normally succeeds — and taking it here means the claim happens before
    // anything else this process posts, rather than behind a queue that a shell with
    // running device threads fills up. But a claim made before the window manager has
    // finished with the new window can still be dropped, which is what the deferred turn
    // covers. Asking twice costs nothing: the second call on an already-active window is a
    // no-op, and Windows revokes the hand-off grant on user input, not on a spent request.
    const auto claimForeground = [](QWidget *w) {
        // Clear a minimised state first, or "restore" puts the window back on the taskbar
        // rather than in front of the operator — the same ordering the single-instance
        // raise path needs.
        w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        w->raise();
        w->activateWindow();
    };

    claimForeground(window);
    QTimer::singleShot(0, window, [window, claimForeground]() {
        claimForeground(window);
        // Logged as "asked", not "succeeded": activateWindow() returns void and Windows
        // silently downgrades a refused request to a flashing taskbar button, so this line
        // prints in the failing case too. It marks the attempt, not the outcome.
        LOG_DEV_INFO << "Hand-off launch: foreground claimed for the new shell window.";
    });
}

/// Confirms, authorises and performs a switch to the other shell.
bool requestShellSwitch(QWidget *parent,
                        ShellKind self,
                        const std::function<bool()> &releaseResources)
{
    const ShellKind target = ShellHandoff::siblingOf(self);
    const QString targetName = ShellHandoff::applicationName(target);

    // 1. Authorise. Only entering commissioning needs it: going the other way hands
    //    authority back, and nobody needs permission to give up permission.
    if (target == ShellKind::Commissioning && !AdminLoginDialog::ensureAdmin(parent)) {
        LOG_USER_INFO << "Shell switch cancelled: administrator access was not granted.";
        return false;
    }

    // 2. Confirm — in both directions, and even when the user already holds Admin. This
    //    guards against a mis-tap, which authorisation does not.
    const QMessageBox::StandardButton answer = QMessageBox::question(
        parent,
        QCoreApplication::translate("ShellStartup", "Switch to %1").arg(targetName),
        QCoreApplication::translate(
            "ShellStartup",
            "This will stop all running tasks and release the camera, the PLC connection "
            "and the output port, then close this application and start %1.\n\n"
            "Continue?")
            .arg(targetName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        LOG_USER_INFO << "Shell switch cancelled by the user." << "target=" << targetName;
        return false;
    }

    LOG_USER_INFO << "Shell switch requested." << "target=" << targetName;

    // 3. Release. If this fails the switch is abandoned: the sibling would come up against
    //    devices this process has not let go of, and the symptoms of that look exactly
    //    like a hardware fault.
    if (releaseResources && !releaseResources()) {
        LOG_USER_ERR << "Shell switch abandoned: this application could not release its "
                        "devices.";
        QMessageBox::critical(
            parent,
            QCoreApplication::translate("ShellStartup", "Switch to %1").arg(targetName),
            QCoreApplication::translate(
                "ShellStartup",
                "This application could not stop its running work, so the switch was "
                "cancelled. Nothing has changed."));
        return false;
    }

    // 4. Launch the sibling. It waits for the instance lock this process still holds.
    QString error;
    if (!ShellHandoff::launchSibling(self, &error)) {
        QMessageBox::critical(
            parent,
            QCoreApplication::translate("ShellStartup", "Switch to %1").arg(targetName),
            error);
        // Deliberately does NOT quit: this shell has stopped its work, but it is still the
        // only thing on screen. Leaving the operator with a running application they can
        // restart from beats leaving them with nothing.
        return false;
    }

    // 5. Quit, which releases the lock the sibling is waiting on.
    LOG_USER_INFO << "Shell switch: exiting so the sibling can take over.";
    QCoreApplication::quit();
    return true;
}

}  // namespace vc::ui
