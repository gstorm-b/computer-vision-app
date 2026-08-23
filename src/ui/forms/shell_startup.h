#ifndef SHELL_STARTUP_H
#define SHELL_STARTUP_H

#include <functional>

#include "core/utils/shell_handoff.h"

class QWidget;
class SingleInstanceGuard;

/**
 * @file shell_startup.h
 * @brief Startup and shell-switch flows shared by both application shells.
 */

namespace vc::ui {

/**
 * @enum StartupDecision
 * @brief What `main()` should do after the single-instance check.
 */
enum class StartupDecision {
    Proceed,        ///< This process owns the instance lock; carry on and show a window.
    ExitQuietly,    ///< Another instance is already serving the user. Exit 0 — not an error.
    ExitWithError   ///< A hand-off launch could not take over. Exit non-zero; the user has been told why.
};

/**
 * @brief Runs the product-wide single-instance check and reports the outcome to the user.
 *
 * Lives here rather than in each `main()` because both shells need identical behaviour and
 * a copy in two files drifts — and the case that drifts is the one nobody exercises: the
 * cross-shell launch.
 *
 * Three outcomes, three different things to say:
 * - **lock taken** → proceed;
 * - **held by the SAME shell** → silently raise the running window. The user asked for
 *   this application and it is already there; a dialog would only be in the way;
 * - **held by the OTHER shell** → say which one is running and how to switch, *then*
 *   raise it. Clicking the editor icon and having the runtime appear with no explanation
 *   reads as a bug.
 *
 * A launch flagged as a hand-off waits for the outgoing sibling instead of giving up
 * immediately, and if the wait expires it says so rather than vanishing — an application
 * that exits with no window is indistinguishable from one that crashed.
 *
 * @param[in,out] guard the (not yet acquired) guard for this process
 * @param[in]     self  which shell this process is
 * @return what `main()` should do next
 */
StartupDecision resolveSingleInstance(SingleInstanceGuard &guard, vc::shell::ShellKind self);

/**
 * @brief Confirms, authorises and performs a switch to the other shell.
 *
 * The sequence, and the order matters:
 * 1. **authorise** — entering commissioning requires Admin; entering the operator runtime
 *    does not, because dropping into the operator view is not an escalation;
 * 2. **confirm** — in both directions, including when the user already holds Admin. The
 *    password answers "are you allowed?"; this answers "did you mean to?", and a mis-tap
 *    by an authorised person still stops a production line;
 * 3. **release** — `releaseResources` stops this shell's work and hands back the camera,
 *    the PLC socket and the output port. If it returns false the switch is abandoned:
 *    handing off into a half-released camera is worse than not switching;
 * 4. **launch** the sibling, which waits for this process's instance lock;
 * 5. **quit**, releasing that lock.
 *
 * @param[in] parent           parent widget for the dialogs
 * @param[in] self             which shell is handing off
 * @param[in] releaseResources stops this shell's work; returns false to abandon the switch
 * @return true if the switch is under way and this process is quitting
 */
bool requestShellSwitch(QWidget *parent,
                        vc::shell::ShellKind self,
                        const std::function<bool()> &releaseResources);

/**
 * @brief Shows `window` and, after a hand-off, forces it to the foreground.
 *
 * A shell started by the *other* shell does not get the foreground for free: Windows keeps
 * it behind whatever is on screen and merely flashes its taskbar button. From the
 * operator's side that looks like the switch half-worked — the old application is gone and
 * the new one has to be hunted for in the taskbar.
 *
 * The outgoing process grants the right (`AllowSetForegroundWindow`, in
 * `ShellHandoff::launchSibling`); this is the other half, where the incoming process
 * actually claims it. Both are needed.
 *
 * On a normal launch this is just `show()` — a user-initiated start already comes forward,
 * and forcing the foreground unasked is the behaviour that makes applications obnoxious.
 *
 * @param[in] window the shell's main window
 */
void presentShellWindow(QWidget *window);

}  // namespace vc::ui

#endif // SHELL_STARTUP_H
