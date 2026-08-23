#ifndef SHELL_HANDOFF_H
#define SHELL_HANDOFF_H

#include <QString>
#include <QStringList>

/**
 * @file shell_handoff.h
 * @brief ShellKind and ShellHandoff — the one place that knows there are two application
 *        shells, what they are called, and how one replaces the other.
 */

namespace vc::shell {

/**
 * @enum ShellKind
 * @brief Which of the two executables a process is.
 */
enum class ShellKind {
    Commissioning,   ///< `ncr_picking.exe` — devices, calibration, patterns, signals. Writes project files.
    OperatorRuntime  ///< `ncr_runtime.exe` — runs tasks and shows dashboards. Never writes a project file.
};

/**
 * @class ShellHandoff
 * @brief Locates the sibling executable and starts it as this process's replacement.
 *
 * **The two shells never run at the same time.** Both own the same hardware exclusively —
 * the Basler camera (Pylon grants exclusive access), the MC PLC socket and the
 * vision-output listen port — so switching is a *hand-off*, not a launch: stop the work,
 * release the devices, start the sibling, exit. Both shells therefore take the **same**
 * single-instance key (kInstanceKey), which is what makes "one app at a time" a fact
 * rather than an operating instruction.
 *
 * That creates one hazard worth naming, because its failure mode is silence: the outgoing
 * process must release the lock before the incoming one takes it, and the incoming one
 * starts first. A shell launched for a hand-off therefore waits (kHandoffAcquireTimeoutMs)
 * instead of giving up immediately — see wasStartedForHandoff().
 *
 * @note The sibling is found via QCoreApplication::applicationDirPath() plus a fixed
 *       filename. That is why both executables must ship in one folder, which the Phase 6
 *       install image already guarantees (docs/product/install_image.md).
 */
class ShellHandoff {
public:
    /**
     * @brief The single-instance key BOTH shells take.
     *
     * One key, deliberately. Phase 6 gave each shell its own so they would not block each
     * other; Phase 7 reverses that, because two processes reaching for one camera produce
     * symptoms indistinguishable from hardware faults.
     */
    static constexpr char kInstanceKey[] = "ncr_picking_suite";

    /// Command-line flag marking a launch that is replacing an exiting sibling.
    static constexpr char kHandoffFlag[] = "--handoff";

    /**
     * @brief How long a hand-off launch waits for the outgoing shell to release the lock.
     *
     * Generous on purpose. The outgoing shell still has to stop device runners and let
     * their threads finish, and the cost of waiting too long is a few seconds of splash;
     * the cost of waiting too little is an application that appears not to start at all.
     */
    static constexpr int kHandoffAcquireTimeoutMs = 15000;

    /// Returns the other shell.
    static ShellKind siblingOf(ShellKind kind);

    /**
     * @brief Canonical, **untranslated** application name for `kind`.
     *
     * Not run through tr(). This is an identity, not UI text: it is what QLockFile records
     * in the lock file, and it is how a losing launch works out which shell is already
     * running. A name that changes with the UI language would make that comparison fail in
     * Japanese and nowhere else.
     */
    static QString applicationName(ShellKind kind);

    /// File name of the executable for `kind`, including the platform suffix.
    static QString executableName(ShellKind kind);

    /**
     * @brief Absolute path of the sibling executable, next to this one.
     * @param[in] self the shell asking
     * @return the path, which is not checked for existence here — see launchSibling()
     */
    static QString siblingPath(ShellKind self);

    /// True when `arguments` carries kHandoffFlag, i.e. this process is replacing a sibling.
    static bool wasStartedForHandoff(const QStringList &arguments);

    /**
     * @brief Starts the sibling executable detached, flagged as a hand-off.
     *
     * Does **not** stop this process. The caller must have already released its devices,
     * and must exit only after this returns true — the sibling is waiting for the lock
     * this process still holds.
     *
     * @param[in]  self  the shell handing off
     * @param[out] error receives a user-readable reason when the launch fails
     * @return true if the sibling process was started
     */
    static bool launchSibling(ShellKind self, QString *error);
};

}  // namespace vc::shell

#endif // SHELL_HANDOFF_H
