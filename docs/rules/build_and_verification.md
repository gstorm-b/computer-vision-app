# Build And Verification Guide

**Last updated:** 2026-06-24

## Rule Of Thumb

This repository is a qmake project. Always generate the Makefile with `qmake`
from the build directory before compiling. `qmake` is the project generator;
the actual compiler driver is the make tool selected by the Qt kit.

For MSVC command-line verification from Codex, use the same pattern as
`components/RobotKinematics/scripts/build_msvc*.bat`:

1. Call `vcvars64.bat` directly. Do **not** redirect its output; the component
   scripts explicitly warn that redirecting can break SDK setup.
2. Verify `VCToolsInstallDir` and `INCLUDE` are set.
3. Put the Qt kit `bin` directory and required runtime DLL directories on PATH.
4. Run `qmake` in the build directory.
5. Run `nmake /nologo` as the reliable CLI fallback.

Qt Creator can use `jom` for MSVC builds. During Phase 1 verification from the
Codex shell, `jom` did not inherit the PATH modified inside the `cmd` wrapper and
failed to find `cl`, even though `where cl` succeeded immediately before
launching `jom`. Use `jom` when launching through Qt Creator's configured kit;
otherwise prefer the `nmake` fallback below.

## Build Directory Policy

Build output location follows the owner of the `.pro` file:

- Root application build: use `%NCR_PICKING_ROOT%\build\<build-name>` for
  `ncr_picking.pro`.
- Test build: use a `build\<build-name>` folder next to the test `.pro`, for
  example `%NCR_PICKING_ROOT%\tests\architecture_contract_test\build\msvc_debug`.
- Example or component build: use a `build\<build-name>` folder next to that
  example/component `.pro`.

Do not put test, example, or component build folders under the repository root
`build\` directory. Root `build\` is reserved for the root application and
root-level release/package verification only.

Legacy root-level folders such as `build\phase1_architecture_contract_*` and
`build_contract_test` are not canonical. Treat them as disposable generated
artifacts and clean them only when the user explicitly approves cleanup.

## Local Environment Variables

Machine-local paths must come from environment variables in docs, scripts, and
project files. Do not add new hard-coded absolute paths such as `C:\Qt\...`,
`C:\opencv\...`, or `C:\Program Files\Basler\...`.

Use these names for local configuration:

- `NCR_PICKING_ROOT`: repository root.
- `QT_MSVC_DIR`: Qt MSVC kit root, for example the directory that contains
  `bin\qmake.exe`.
- `VCVARS`: full path to Visual Studio `vcvars64.bat`.
- `OPENCV_ROOT`, `OPENCV_BIN`, `OPENCV_INCLUDE_DIR`, `OPENCV_LIB_DIR`: OpenCV
  install/build paths.
- `OPENCV_WORLD_RELEASE`, `OPENCV_WORLD_DEBUG`: optional OpenCV world library
  names when the local OpenCV version differs from the qmake default
  (`opencv_world4120`, `opencv_world4120d`).
- `PYLON_ROOT`, `PYLON_RUNTIME_DIR`, `PYLON_INCLUDE_DIR`, `PYLON_LIB_DIR`:
  Basler Pylon paths.
- `PYLON_BASE_LIB`: optional Basler Pylon import-library name when the local
  Pylon SDK differs from the qmake default (`PylonBase_v11`).
- `VCTOOLS_DEBUG_CRT_DIR`: optional Debug CRT runtime directory when running
  Debug test binaries outside Visual Studio.
- RobotKinematics and third-party dependencies should follow the component
  scripts' pattern: define the dependency root once, then derive include, lib,
  bin, and runtime folders from that root.

qmake projects consume local dependency paths through the shared includes under
`qmake/`:

- `qmake/opencv_dependency.pri` for OpenCV-only targets;
- `qmake/pylon_dependency.pri` for Pylon-only targets;
- `qmake/local_dependencies.pri` for targets that need both OpenCV and Pylon.

Do not add per-target hard-coded local dependency paths.

## MSVC App Build

```bat
if not defined NCR_PICKING_ROOT exit /b 1
if not defined QT_MSVC_DIR exit /b 1
if not defined VCVARS exit /b 1
if not defined OPENCV_BIN exit /b 1
if not defined PYLON_RUNTIME_DIR exit /b 1

set "BUILD=%NCR_PICKING_ROOT%\build\msvc_debug"

call "%VCVARS%"
if not defined VCToolsInstallDir exit /b 1
if not defined INCLUDE exit /b 1

set "PATH=%QT_MSVC_DIR%\bin;%BUILD%\debug;%OPENCV_BIN%;%PYLON_RUNTIME_DIR%;%PATH%"

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%" || exit /b 1

qmake -o Makefile "%NCR_PICKING_ROOT%\ncr_picking.pro" -spec win32-msvc CONFIG+=debug || exit /b 1
nmake /nologo || exit /b 1
```

## Architecture Contract Test

The architecture contract target contains an inline `main.moc`, so run the moc
target before the normal build when using command-line qmake/nmake:

```bat
if not defined NCR_PICKING_ROOT exit /b 1
if not defined QT_MSVC_DIR exit /b 1
if not defined VCVARS exit /b 1
if not defined OPENCV_BIN exit /b 1
if not defined PYLON_RUNTIME_DIR exit /b 1

set "TEST_DIR=%NCR_PICKING_ROOT%\tests\architecture_contract_test"
set "BUILD=%TEST_DIR%\build\msvc_debug"

call "%VCVARS%"
if not defined VCToolsInstallDir exit /b 1
if not defined INCLUDE exit /b 1

set "PATH=%QT_MSVC_DIR%\bin;%BUILD%\debug;%OPENCV_BIN%;%PYLON_RUNTIME_DIR%;%PATH%"

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%" || exit /b 1

qmake -o Makefile "%TEST_DIR%\architecture_contract_test.pro" -spec win32-msvc CONFIG+=debug || exit /b 1
nmake /nologo -f Makefile.Debug compiler_moc_source_make_all || exit /b 1
nmake /nologo || exit /b 1
```

Run the test executable with Qt, OpenCV, Basler, the build output directory, and
Debug CRT on PATH:

```powershell
$env:QT_QPA_PLATFORM = 'minimal'
$testBuild = Join-Path $env:NCR_PICKING_ROOT 'tests\architecture_contract_test\build\msvc_debug\debug'
$env:Path = "$testBuild;$($env:QT_MSVC_DIR)\bin;$($env:OPENCV_BIN);$($env:PYLON_RUNTIME_DIR);$($env:VCTOOLS_DEBUG_CRT_DIR);C:\Windows\System32;" + $env:Path
Set-Location $testBuild
.\architecture_contract_test.exe -silent
```

Expected success signal: process exit code `0`.

## Dependency Deployment (`CONFIG+=deploy_deps`)

The root app can copy its third-party **runtime DLLs** next to the built binary
so it runs from the build folder without those directories on `PATH`. This is
**opt-in and off by default**; enable it at qmake time:

```bat
qmake -o Makefile "%NCR_PICKING_ROOT%\ncr_picking.pro" -spec win32-msvc CONFIG+=deploy_deps
```

Owned by [qmake/deploy_dependencies.pri](../../qmake/deploy_dependencies.pri).
Scope (Windows only), each file copied only when missing from the target
(`if not exist`), so existing files are left untouched:

- **ADS docking** — `qtadvanceddocking[d].dll` from the repo-bundled
  `3rdparty/advance_docking/bin`.
- **OpenCV world** — `$OPENCV_WORLD_{RELEASE,DEBUG}.dll` from `OPENCV_BIN`.
- **Basler Pylon** — every top-level `*.dll` in `PYLON_RUNTIME_DIR`.
- **Qt runtime** — Qt DLLs + plugins via `windeployqt` (`--release`/`--debug`),
  resolved from `$$[QT_INSTALL_BINS]`.

Not handled here: the RobotKinematics/Coal/Assimp set (already copied by
`components/RobotKinematics/robotkinematics.pri`). A missing `OPENCV_BIN` /
`PYLON_RUNTIME_DIR` / `windeployqt` only warns and skips that family — it never
fails the build.

This is a convenience for build-folder QA, **not** a customer installer. Full
Basler deployment (GenTL producers + `GENICAM_GENTL64_PATH`) and the installer
manifest remain tracked in
[later_todo_list.md](../backlog/later_todo_list.md) #27.
