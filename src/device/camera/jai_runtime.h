#ifndef JAI_RUNTIME_H
#define JAI_RUNTIME_H

/**
 * @file jai_runtime.h
 * @brief One-time preparation of the Pleora eBUS SDK's GenICam runtime.
 */

#include <QString>

namespace vc::device::jai {

/**
 * @brief Makes the eBUS GenICam runtime loadable, once per process, and reports whether it is.
 *
 * @warning **Without this the application does not fail — it dies.**
 *
 * `PvDevice64.dll` and `PvGenICam64.dll` **delay-load** `GenApi_MD_VC141_v3_4.dll` and
 * `GCBase_MD_VC141_v3_4.dll`. Those live in a *subdirectory* of the eBUS runtime folder
 * (`GenICam\bin\Win64_x64`) which the installer does **not** put on PATH — only the parent.
 * So every eBUS call that needs no node map works, and the first one that does raises the
 * VC++ delay-load exception `0xC06D007E` (ERROR_MOD_NOT_FOUND). That is a Windows SEH
 * exception, not a C++ one: `catch (...)` never sees it, the process is killed, and the log
 * ends mid-sentence with nothing written about the failure.
 *
 * This is exactly the shape it had on the first station it ran on: the camera-select dialog
 * listed cameras happily (`PvSystem64.dll` delay-loads nothing) and pressing Connect closed
 * the application with an empty log.
 *
 * Note that a GenICam runtime being on PATH is not enough — it has to be **this** one. A
 * machine with the JAI SDK installed carries `GenApi_MD_VC80_JAI_v2_4.dll` on PATH, and Pylon
 * carries `GenApi_MD_VC141_v3_1_Basler_pylon.dll`; neither can satisfy a `_v3_4` import.
 *
 * Idempotent and safe to call from any thread. The first call locates the directory, prepends
 * it to the process PATH, verifies by actually loading the GenApi DLL, and logs the outcome;
 * later calls return the cached answer.
 *
 * Search order, all derived from the environment — no absolute path is compiled in:
 *   1. `EBUS_GENICAM_BIN` (explicit override for a non-standard install)
 *   2. `%CommonProgramFiles%\Pleora\eBUS SDK\GenICam\bin\Win64_x64` (where the installer puts it)
 *   3. `%PUREGEV_ROOT%\GenICam\bin\Win64_x64` (a self-contained/portable SDK copy)
 *
 * @param[out] detail on failure, a message naming what was looked for and where; may be null
 * @return true when the GenICam runtime is loadable and eBUS calls are safe to make
 */
bool ensureGenICamRuntime(QString *detail = nullptr);

} // namespace vc::device::jai

#endif // JAI_RUNTIME_H
