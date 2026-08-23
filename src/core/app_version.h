#ifndef APP_VERSION_H
#define APP_VERSION_H

#include <QString>

/**
 * @file app_version.h
 * @brief Single definition of the application version, shared by every shell.
 *
 * Two executables ship in one folder (`ncr_picking.exe` and `ncr_runtime.exe`) and must
 * come from the same build. The failure this guards against is concrete: the project
 * file carries `TaskLocalizeConfig::kSchemaVersion`, so a newer editor writing a project
 * that an older runtime refuses to load is exactly what "one folder, two exes" makes
 * possible. Nothing corrupts — the schema gate refuses loudly — but the operator's report
 * is "the runtime will not open what the editor just saved", and the diagnosis should be
 * one glance rather than a log hunt.
 *
 * Both shells therefore read the version from here and publish it through
 * QCoreApplication::applicationVersion().
 */

namespace vc::version {

/// The application version, in MAJOR.MINOR.PATCH form.
///
/// Defined by `qmake/version.pri`, which is the single place the value is written: it
/// feeds both this constant and the Windows file-resource `VERSION`, so an executable's
/// Properties dialog and its startup log can never disagree.
///
/// The fallback below applies only to builds that do not include `qmake/app_common.pri`
/// — currently `tests/architecture_contract_test`, which lists src sources individually.
/// It is deliberately marked `-dev` rather than carrying a plausible number: a stale but
/// believable version is worse than an obviously unreleased one.
#ifdef NCR_APP_VERSION_STR
inline constexpr char kApplicationVersion[] = NCR_APP_VERSION_STR;
#else
inline constexpr char kApplicationVersion[] = "0.0.0-dev";
#endif

/// @return the version as a QString, for UI and log use.
inline QString applicationVersion()
{
    return QString::fromLatin1(kApplicationVersion);
}

} // namespace vc::version

#endif // APP_VERSION_H
