#include "single_instance_guard.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "core/logger/app_logger.h"

namespace {

/// Bytes the losing process writes to ask the running one to come forward. The content is
/// irrelevant — the connection itself is the message — but a named constant keeps the two
/// ends from drifting into different expectations.
const char kRaiseRequest[] = "raise";

/// How long the losing process waits for the running one to connect/acknowledge, in ms.
/// Short on purpose: this runs while the user is staring at a double-clicked icon, and a
/// running instance that cannot answer within a moment is better reported than waited on.
constexpr int kNotifyTimeoutMs = 500;

} // namespace

/// Stores the key; nothing is acquired until tryAcquire().
SingleInstanceGuard::SingleInstanceGuard(const QString &instanceKey, QObject *parent)
    : QObject(parent)
    , m_instanceKey(instanceKey)
{
}

/// Releases the lock file and stops the raise listener.
SingleInstanceGuard::~SingleInstanceGuard()
{
    if (m_server != nullptr) {
        m_server->close();
    }
    if (m_lockFile) {
        m_lockFile->unlock();
    }
}

/// Lock file lives beside other per-user temporary state, named after the instance key.
QString SingleInstanceGuard::lockFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(dir).absoluteFilePath(m_instanceKey + QStringLiteral(".lock"));
}

/// Raise channel shares the instance key so both ends derive the same name.
QString SingleInstanceGuard::serverName() const
{
    return m_instanceKey + QStringLiteral(".raise");
}

/// Reads the lock holder's application name without taking the lock.
QString SingleInstanceGuard::runningInstanceName() const
{
    QLockFile probe(lockFilePath());
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (!probe.getLockInfo(&pid, &hostname, &appname)) {
        return QString();
    }
    return appname;
}

/// Takes the lock file, then starts the raise listener.
bool SingleInstanceGuard::tryAcquire(int timeoutMs)
{
    m_lockFile = std::make_unique<QLockFile>(lockFilePath());

    // Stale-lock handling is left at QLockFile's default on purpose. A lock whose recorded
    // PID is no longer running is reclaimed by QLockFile regardless, which is the crash
    // case that matters; the stale *time* is only the secondary guard for a recycled PID.
    // setStaleLockTime(0) would read like "reclaim immediately" but means the opposite —
    // never stale by age — and would give up that secondary guard.
    //
    // `timeoutMs` is the tryLock TIMEOUT, not the stale time. It is 0 for a normal launch
    // — that decision is made while the user waits on a double-clicked icon, so it must
    // not block — and a real duration only when this process was started to REPLACE an
    // exiting sibling, whose lock is still held while it shuts its devices down.
    // QLockFile does the waiting and the retrying itself; a hand-rolled retry loop here
    // would duplicate it and get the stale-lock handling subtly different.
    if (!m_lockFile->tryLock(timeoutMs)) {
        qint64 pid = 0;
        QString hostname;
        QString appname;
        if (m_lockFile->getLockInfo(&pid, &hostname, &appname)) {
            LOG_USER_INFO << "Another instance is already running."
                          << "key=" << m_instanceKey
                          << "pid=" << pid
                          << "app=" << appname;
        } else {
            LOG_USER_INFO << "Another instance is already running." << m_instanceKey;
        }
        m_lockFile.reset();
        return false;
    }

    startRaiseListener();
    return true;
}

/// Starts the local server used to receive raise requests. Failure is deliberately not
/// fatal: the process has already won the lock and is the legitimate single instance.
/// Refusing to launch because a named pipe could not be created would trade a small
/// convenience for the whole application.
void SingleInstanceGuard::startRaiseListener()
{
    m_server = new QLocalServer(this);

    // Clear a socket left behind by a crash; on platforms where the endpoint survives the
    // process, listen() would otherwise fail for a name nobody is using.
    QLocalServer::removeServer(serverName());

    if (!m_server->listen(serverName())) {
        LOG_DEV_ERR << "Single-instance raise listener could not start."
                    << "key=" << m_instanceKey
                    << "error=" << m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *client = m_server->nextPendingConnection()) {
            connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
            client->write(kRaiseRequest, static_cast<qint64>(sizeof(kRaiseRequest) - 1));
            client->flush();
            client->disconnectFromServer();
        }
        LOG_USER_INFO << "Another launch was blocked; bringing the existing window forward.";
        emit raiseRequested();
    });
}

/// Grants foreground rights to the running instance (Windows), then pokes its raise
/// channel.
bool SingleInstanceGuard::notifyExistingInstance()
{
#ifdef Q_OS_WIN
    // Without this, Windows refuses to let a background process take the foreground and
    // only flashes its taskbar button — which reads to the operator as "clicking the icon
    // did nothing". The PID comes from the lock file the running instance wrote.
    QLockFile probe(lockFilePath());
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (probe.getLockInfo(&pid, &hostname, &appname) && pid > 0) {
        ::AllowSetForegroundWindow(static_cast<DWORD>(pid));
    }
#endif

    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(kNotifyTimeoutMs)) {
        LOG_DEV_ERR << "Could not reach the running instance to raise it."
                    << "key=" << m_instanceKey
                    << "error=" << socket.errorString();
        return false;
    }

    // Wait for the acknowledgement rather than just the connection: on Windows the
    // foreground handover needs the running process to have actually reacted before this
    // process exits and gives up its own foreground claim.
    const bool acknowledged = socket.waitForReadyRead(kNotifyTimeoutMs);
    socket.disconnectFromServer();
    return acknowledged;
}
