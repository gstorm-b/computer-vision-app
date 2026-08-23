#ifndef SINGLE_INSTANCE_GUARD_H
#define SINGLE_INSTANCE_GUARD_H

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;
class QLockFile;

/**
 * @file single_instance_guard.h
 * @brief SingleInstanceGuard — prevents a second copy of an application shell from
 *        starting, and hands the raise request to the copy already running.
 */

/**
 * @class SingleInstanceGuard
 * @brief Ensures only one process per instance key runs, and lets a second launch bring
 *        the running window forward instead of starting a rival process.
 *
 * Both application shells own hardware exclusively: the Basler camera (Pylon grants
 * exclusive access), the MC PLC socket, and the vision-output TCP listen port. A second
 * process fails to take any of them, and the symptoms it produces — failed grabs,
 * "device removed", a port that will not bind — are indistinguishable from real hardware
 * faults. That is what makes this worth a guard rather than an operating instruction:
 * on a field machine the runtime starts at boot *and* an operator may click its icon, so
 * two instances is the normal case, not an edge case.
 *
 * Mechanism: a QLockFile provides the mutual exclusion and carries the owner's PID; a
 * QLocalServer on the same key carries the "please come forward" message.
 *
 * @note Crash recovery is inherited from QLockFile: a lock whose recorded PID is no
 *       longer running is reclaimed automatically, so a killed process does not lock the
 *       application out of its own machine.
 * @note Scope is the current user. Two different Windows accounts can each run one
 *       instance — which matters if boot-start is configured under a service account
 *       while an operator is logged in interactively. Run the scheduled task as the
 *       operator's account (see docs/domains/runtime_app/runtime_shell.md).
 * @note **Both shells share one key** (`vc::shell::ShellHandoff::kInstanceKey`), so the
 *       commissioning app and the operator runtime exclude each other as well as
 *       themselves. Phase 6 gave them separate keys deliberately, on the grounds that
 *       stopping them co-running needed an ordered device hand-off rather than a refusal
 *       to start; Phase 7 built that hand-off, so the refusal is now the right default.
 *       runningInstanceName() is how a losing launch tells the user *which* shell is up.
 */
class SingleInstanceGuard : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Creates a guard for `instanceKey` without acquiring anything yet.
     * @param[in] instanceKey stable per-application key (e.g. "ncr_runtime"); two shells
     *            must use different keys or each would block the other
     * @param[in] parent optional QObject parent
     */
    explicit SingleInstanceGuard(const QString &instanceKey, QObject *parent = nullptr);

    /// Releases the lock file and stops listening.
    ~SingleInstanceGuard() override;

    /**
     * @brief Attempts to become the single running instance.
     * @param[in] timeoutMs how long to keep trying, in milliseconds. **0 (the default) is
     *            correct for a normal launch**: the decision is made while the user stares
     *            at a double-clicked icon, so it must not block. A launch that is
     *            *replacing* an exiting sibling passes a real timeout, because the lock it
     *            wants is still held by a process that is on its way out — see
     *            vc::shell::ShellHandoff.
     * @return true if this process now owns the instance key; false if another live
     *         process still holds it when the timeout expires
     * @note Returning true also starts the raise-request listener. If that listener
     *       cannot start, this still returns true: losing the ability to raise a window
     *       is a far smaller problem than refusing to launch the runtime at all.
     */
    bool tryAcquire(int timeoutMs = 0);

    /**
     * @brief Returns the application name recorded by whichever process holds the lock.
     *
     * Readable without holding the lock, which is the point: a losing launch uses it to
     * say *which* shell is already running instead of raising an unexpected window with no
     * explanation. Matches the canonical names in `vc::shell::ShellHandoff` because those
     * are untranslated for exactly this comparison.
     *
     * @return the holder's application name, or an empty string if the lock is free or its
     *         information cannot be read
     */
    QString runningInstanceName() const;

    /**
     * @brief Asks the instance already running to bring its window forward.
     *
     * Called by the losing process just before it exits. On Windows this first grants the
     * running process permission to take the foreground, without which the operating
     * system would only flash its taskbar button — from the operator's side that looks
     * like the second launch did nothing at all.
     *
     * @return true if the running instance acknowledged the request
     */
    bool notifyExistingInstance();

signals:
    /// Emitted in the running instance when another launch asked it to come forward.
    void raiseRequested();

private:
    /// Absolute path of the lock file backing this key.
    QString lockFilePath() const;
    /// Local-socket/named-pipe name used for the raise channel.
    QString serverName() const;
    /// Starts the raise-request listener; failure is logged and tolerated.
    void startRaiseListener();

    QString m_instanceKey;                   ///< Per-application key this guard protects.
    std::unique_ptr<QLockFile> m_lockFile;   ///< Mutual exclusion; also records the owner PID.
    QLocalServer *m_server{nullptr};         ///< Raise-request listener; null when not acquired.
};

#endif // SINGLE_INSTANCE_GUARD_H
