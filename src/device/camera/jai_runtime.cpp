/**
 * @file jai_runtime.cpp
 * @brief Implementation of the eBUS GenICam runtime preparation. See the header for why.
 */

#include "device/camera/jai_runtime.h"

#include "core/logger/app_logger.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace vc::device::jai {

namespace {

/// The GenApi DLL family eBUS delay-loads. Globbed rather than named exactly: the version
/// suffix (`_v3_4`) belongs to the installed SDK, and pinning it here would turn a routine SDK
/// upgrade into a crash identical to the one this file exists to prevent.
constexpr const char *kGenApiGlob = "GenApi_MD_*.dll";

/// Returns the candidate directories, most-specific first. All come from the environment.
QStringList candidateDirectories()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList candidates;

    const QString explicitDir = env.value(QStringLiteral("EBUS_GENICAM_BIN"));
    if (!explicitDir.isEmpty()) {
        candidates << QDir::cleanPath(explicitDir);
    }

    const QString commonFiles = env.value(QStringLiteral("CommonProgramFiles"));
    if (!commonFiles.isEmpty()) {
        candidates << QDir::cleanPath(commonFiles
                                      + QStringLiteral("/Pleora/eBUS SDK/GenICam/bin/Win64_x64"));
    }

    const QString sdkRoot = env.value(QStringLiteral("PUREGEV_ROOT"));
    if (!sdkRoot.isEmpty()) {
        candidates << QDir::cleanPath(sdkRoot + QStringLiteral("/GenICam/bin/Win64_x64"));
    }

    candidates.removeDuplicates();
    return candidates;
}

/// Returns the first candidate that actually holds a GenApi DLL, or an empty string.
QString findGenICamBinDir(QStringList *searched)
{
    for (const QString &candidate : candidateDirectories()) {
        searched->append(candidate);
        const QDir dir(candidate);
        if (!dir.exists()) {
            continue;
        }
        if (!dir.entryList({QString::fromLatin1(kGenApiGlob)}, QDir::Files).isEmpty()) {
            return candidate;
        }
    }
    return QString();
}

/// Loads the GenApi DLL by full path — both the proof and half the fix.
///
/// It verifies: a handle means the very DLL whose absence killed the process is resolvable, so
/// the outcome is measured rather than assumed. Adding a directory to PATH and hoping is how
/// this defect would come back silently.
///
/// It also fixes: the module stays loaded for the life of the process, and a later delay-load
/// asking for the bare name `GenApi_MD_VC141_v3_4.dll` matches an already-loaded module by base
/// name without searching at all. Which is why this runs AFTER the PATH edit and not instead of
/// it — GenApi has its own siblings in that folder (GCBase, Log, XmlParser…), and those resolve
/// through the normal search order, i.e. through PATH. Removing either half breaks the other's
/// assumption.
bool verifyLoadable(const QString &dir, QString *loadedName)
{
#ifdef Q_OS_WIN
    const QDir binDir(dir);
    const QStringList matches =
        binDir.entryList({QString::fromLatin1(kGenApiGlob)}, QDir::Files);
    if (matches.isEmpty()) {
        return false;
    }

    const QString fullPath = QDir::toNativeSeparators(binDir.absoluteFilePath(matches.first()));
    const HMODULE handle =
        LoadLibraryW(reinterpret_cast<const wchar_t *>(fullPath.utf16()));
    if (handle == nullptr) {
        return false;
    }
    *loadedName = matches.first();
    return true;
#else
    Q_UNUSED(dir);
    Q_UNUSED(loadedName);
    return false;
#endif
}

struct RuntimeState {
    QMutex mutex;
    bool resolved{false};
    bool ok{false};
    QString detail;
};

RuntimeState &state()
{
    static RuntimeState instance;
    return instance;
}

} // namespace

bool ensureGenICamRuntime(QString *detail)
{
    RuntimeState &s = state();
    QMutexLocker locker(&s.mutex);

    if (s.resolved) {
        if (detail) {
            *detail = s.detail;
        }
        return s.ok;
    }
    s.resolved = true;

#ifndef Q_OS_WIN
    s.ok = false;
    s.detail = QObject::tr("The JAI camera requires the Windows eBUS SDK runtime.");
    if (detail) {
        *detail = s.detail;
    }
    return false;
#else
    QStringList searched;
    const QString binDir = findGenICamBinDir(&searched);

    if (binDir.isEmpty()) {
        s.ok = false;
        s.detail = QObject::tr(
                       "The eBUS GenICam runtime was not found, so the JAI camera cannot be "
                       "opened. Install the JAI/Pleora eBUS SDK, or set EBUS_GENICAM_BIN to the "
                       "folder holding GenApi_MD_*.dll. Looked in: %1")
                       .arg(searched.join(QStringLiteral("; ")));
        LOG_USER_ERR << s.detail;
        if (detail) {
            *detail = s.detail;
        }
        return false;
    }

    // Prepended, not appended. The directory holds nothing but GenICam runtime DLLs, all
    // carrying a vendor-and-version suffix, so it cannot shadow anything of ours — while
    // appending would lose to another vendor's `_v3_4` build if one were ever earlier on PATH,
    // and a mismatched GenICam is a far worse failure than a missing one.
    const QByteArray currentPath = qgetenv("PATH");
    const QByteArray nativeDir = QDir::toNativeSeparators(binDir).toLocal8Bit();
    if (!currentPath.contains(nativeDir)) {
        qputenv("PATH", nativeDir + ';' + currentPath);
    }

    QString loadedName;
    if (!verifyLoadable(binDir, &loadedName)) {
        s.ok = false;
        s.detail = QObject::tr("The eBUS GenICam runtime at %1 could not be loaded. It is "
                               "probably the wrong architecture (a 64-bit build is required) "
                               "or an incomplete installation.")
                       .arg(binDir);
        LOG_USER_ERR << s.detail;
        if (detail) {
            *detail = s.detail;
        }
        return false;
    }

    s.ok = true;
    s.detail.clear();
    LOG_USER_INFO << QObject::tr("eBUS GenICam runtime ready: %1 (%2)")
                         .arg(loadedName, binDir);
    if (detail) {
        detail->clear();
    }
    return true;
#endif
}

} // namespace vc::device::jai
