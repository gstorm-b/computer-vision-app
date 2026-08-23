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

**Build layout and deploy layout are different questions.** Intermediates must be
kept apart — two qmake projects in one directory collide on `Makefile`,
`.qmake.stash`, `ui_*.h` and the `moc_*.cpp` generated from the same shared `src/`
headers. Linked binaries must be brought *together*, because the runtime beside
them is ~40 DLLs plus the Qt plugin tree and two copies of that can drift to
different Qt or Pylon versions.

So intermediates follow the `.pro` file; **both executables land in one place**.

### Where the executables go

`build\bin\<config>` — `build\bin\release`, `build\bin\debug`. Both shells, one
folder, one runtime set, plus `robot_assets\`. This is `DESTDIR`, set once in
[`qmake/app_common.pri`](../../qmake/app_common.pri).

It is a **fixed** location, not one derived from the build directory, so the
umbrella build and the two standalone builds all produce the same folder rather
than three. Same reasoning as `ncr_shared.lib` below.

The build produces this folder complete — `qmake/deploy_dependencies.pri` and
`components/RobotKinematics/robotkinematics.pri` both resolve their destination
from `DESTDIR`. `scripts\make_dist.ps1` only verifies and copies it; it no longer
assembles anything.

### Where the intermediates go

Build output location follows the owner of the `.pro` file:

- **Both shells at once (preferred):** `ncr_picking_all.pro` →
  `build\all\<build-name>` (e.g. `build\all\Release`).
- Shared static library: `src\src.pro` → `build\shared_lib\<config>` for
  `ncr_shared.lib` itself (a fixed location so both shells name it the same way),
  with its objects in whichever build directory it was configured with.
- Application shells built individually: **one subfolder per target** under
  `%NCR_PICKING_ROOT%\build\`.
  - `ncr_picking.pro` → `build\<build-name>` (e.g. `build\Release`).
  - `runtime_app\ncr_runtime.pro` → `build\runtime_build\<build-name>`.

  A shell **links** `ncr_shared` and cannot build it — qmake has no way to build
  another project from an app template. Build `src\src.pro` first, or use the
  umbrella, which does it for you.

> Building one shell alone overwrites only its own executable in `build\bin\`. The
> other shell's stays as it was — which is exactly the stale-pair hazard the
> umbrella exists to prevent, and why `make_dist.ps1` refuses a version-mismatched
> pair rather than trusting that both were built together.
- Test build: use a `build\<build-name>` folder next to the test `.pro`, for
  example `%NCR_PICKING_ROOT%\tests\architecture_contract_test\build\msvc_debug`.
- Example or component build: use a `build\<build-name>` folder next to that
  example/component `.pro`.

Two shells must never share one build directory **when built individually**. They
collide on `Makefile`, `.qmake.stash`, the generated `ui_*.h`, and the
`moc_*.cpp` produced from the same shared `src/` headers — the second `qmake` run
overwrites the first's Makefile, and `nmake` then builds only one of them.

The umbrella is the safe way to put them in one directory: qmake gives each
subproject its own uniquely named makefile (`Makefile.ncr_picking`,
`runtime_app\Makefile.ncr_runtime`) under the umbrella's own `Makefile`, so
nothing overwrites anything.

## Building Both Shells At Once

```powershell
# from build\all\Release
qmake ..\..\..\ncr_picking_all.pro -spec win32-msvc "CONFIG+=release"
nmake /nologo
```

Use **the same directory from the CLI and from Qt Creator** (Projects → Build
Settings → Build directory → `build\all\Release`). One object set and one
incremental state means a build started in the IDE is not redone from the command
line, and vice versa.

### Parallel compilation (opt-in)

Add `CONFIG+=multicore` at qmake time to enable MSVC `/MP`:

```powershell
# from build\all\Release
qmake ..\..\..\ncr_picking_all.pro -spec win32-msvc "CONFIG+=release" "CONFIG+=multicore"
nmake /nologo
```

Measured on a 16-core / 32-thread machine, clean full rebuild with `nmake`:

| | Wall clock |
|---|---|
| default | 10 min 50 s |
| `CONFIG+=multicore` | **4 min 08 s** (2.6×) |

**Off by default on purpose.** `/MP` runs one `cl.exe` per job, each with its own
working set; on a machine with less RAM than cores it starts swapping and the
build gets *slower*. Whether it pays is a property of the machine, not of the
project.

Cap it when needed — on the qmake line or from the environment:

```powershell
qmake ... "CONFIG+=multicore" "NCR_JOBS=8"
```

> **Using jom (Qt Creator's default make tool for MSVC kits)?** jom already runs
> several compile jobs in parallel at the *makefile* level. `/MP` on top multiplies
> the two and can oversubscribe the machine several times over. With jom, either
> leave `multicore` off or set `NCR_JOBS` to 2–4.

Owned by [`../../qmake/multicore.pri`](../../qmake/multicore.pri), included by
`qmake/common_deps.pri` (the shared library and both shells) and by
`tests/architecture_contract_test/architecture_contract_test.pro`.

Layout produced inside it:

```text
build\all\Release\
  Makefile                          umbrella
  src\Makefile                      shared static library
  src\release\                      all 262 src/ objects
  Makefile.ncr_picking              editor shell
  release\                          editor objects (9)
  runtime_app\Makefile.ncr_runtime
  runtime_app\release\              runtime objects (8)
```

Neither executable is in that tree. Both are linked into `build\bin\<config>`
together with one shared runtime set, and `ncr_shared.lib` lands in
`build\shared_lib\<config>` — see the Build Directory Policy for why both are
fixed locations rather than being derived from the build directory.

```text
build\bin\Release\
  ncr_picking.exe  ncr_runtime.exe
  <40 runtime DLLs>                 Qt, OpenCV, Pylon, ADS, Coal/Assimp/Boost
  platforms\ sqldrivers\ imageformats\ iconengines\ styles\ tls\
  networkinformation\ generic\ translations\
  robot_assets\                     Nachi MZ04D meshes
```

**What still happens twice:** linking the two executables, and each shell's own
handful of objects (9 for the editor, 8 for the runtime — its sources plus the
four `qrc_*.cpp`). The `.qrc` files stay at shell level on purpose: a resource
compiled into a static library is dropped by the linker and its content silently
disappears at runtime. See `src\src.pro`.

Measured full rebuilds (`nmake`, clean directory, Release): **~25 min** when both
shells compiled `src/` themselves, **18.6 min** since `src/` became one library
(2026-08-21).

> Root `build\` previously said "reserved for the root application". That rule is
> replaced by the per-target-subfolder rule above, because there are now two
> shells. What it was protecting against — build outputs landing on top of each
> other — is still enforced, just by subfolder rather than by exclusion.

**An intermediates directory is not shippable, and is no longer mistakable for the
image.** Before Phase 6 / E7b each shell's `release\` folder mixed ~270 `.obj` and
~130 generated `.cpp` in with the runtime DLLs, so "the folder with the exe in it"
was both the build output and very nearly the install image. `build\bin\` now holds
only linked binaries and their runtime, and the intermediates stay behind.

## Producing An Install Image

The build already produces one complete folder — `build\bin\<config>` above. What
`dist\` adds is the *checks*, which a build step cannot do: both shells present,
matching non-placeholder versions, no intermediates, no duplicated DLL.

```powershell
.\scripts\make_dist.ps1
```

Defaults to `-BinDir build\bin\release`; pass `-BinDir build\bin\debug` for a debug
image. There is nothing to point at per shell any more, because there is no longer
a per-shell output folder to point at.

It refuses to stage a version-mismatched pair, a build with no file version at all,
and an image containing a duplicated DLL.

Set `OPENCV_BIN` and `PYLON_RUNTIME_DIR` before building, or `deploy_deps` skips
those DLL families with a warning and the image comes out incomplete.

Manifest and rationale: [`../product/install_image.md`](../product/install_image.md).

## Two Application Shells

As of Phase 6 the repository builds two executables from the same `src/` modules:

| Target | `.pro` | Purpose |
|---|---|---|
| `ncr_picking.exe` | `ncr_picking.pro` | Commissioning; the only shell that writes a project file |
| `ncr_runtime.exe` | `runtime_app\ncr_runtime.pro` | Operator runtime; loads and runs, never saves |

Everything they share is split in two files rather than copied into each `.pro`:

| File | Holds | Included by |
|---|---|---|
| `qmake\common_deps.pri` | what is needed to **compile** repository code: Qt modules, include roots, OpenCV, Pylon, ADS, the vendored property browser, RobotKinematics | `src\src.pro` **and** both shells |
| `qmake\app_common.pri` | what only an **executable** can have: resources, the file version, `DESTDIR`, the link against `ncr_shared`, dependency deployment | both shells |

A shell `.pro` adds only its own shell `.pri`, icon and install rules. **Do not
copy either file's contents into a shell `.pro`**: a copy drifts the first time a
module is added, and the drift surfaces as a link error in whichever shell was
forgotten.

Verify a change that touches `src/` by building **both** shells; the architecture
contract test asserts both `.pro` files include the shared config and that neither
shell has taken the resources into the library, but only a build proves both still
link.

### Where A New Source File Registers

In its **module `.pri`** — `src/<module>/<module>.pri` — and nowhere else.
`src\src.pro` includes the seven module `.pri` files and compiles them once into
`ncr_shared.lib`; no shell `.pro` lists module sources any more, so a file added
to a shell `.pro` is not built at all.

### Why The `.qrc` Files Stay In The Shells

`resrc.qrc` (icons, QSS themes), `ads.qrc` and `qtpropertybrowser.qrc` are listed
in `qmake\app_common.pri` — at **shell** level, never in a module `.pri` and never
in `src\src.pro`.

Qt registers a resource through a static initialiser in the generated
`qrc_*.cpp`. Inside a static library nothing references a symbol in that object
file, so the linker drops it and **the resource does not exist at runtime**:
icons, the QSS theme and `:/i18n` all resolve to nothing, with **no build error
and no warning**. `Q_INIT_RESOURCE()` in every consumer is the workaround, and it
is one that gets forgotten. Compiling four small `qrc_*.cpp` twice costs a second
or two; this is the trade, and it is why the static library was deferred as long
as it was.

Library code may still *use* those resources — the Qt resource system is
process-global, so what the executable registers is visible everywhere in the
process. Only the registration must stay in the shells. `src\src.pro` ends with a
bare `RESOURCES =` that takes back any `.qrc` an included `.pri` added on its own;
that line is load-bearing, not tidying.

**This failure is invisible to the build, so it has two guards:** the contract test
asserts the split, and only *running* each shell proves the icon, theme and
Japanese translation actually loaded.

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

One shell on its own. The library step is not optional: `ncr_picking.pro` links
`ncr_shared` and cannot build it. To build both shells instead, use the umbrella
above — it covers the library for you.

```bat
if not defined NCR_PICKING_ROOT exit /b 1
if not defined QT_MSVC_DIR exit /b 1
if not defined VCVARS exit /b 1
if not defined OPENCV_BIN exit /b 1
if not defined PYLON_RUNTIME_DIR exit /b 1

set "LIBBUILD=%NCR_PICKING_ROOT%\build\shared_lib_msvc_debug"
set "BUILD=%NCR_PICKING_ROOT%\build\msvc_debug"

call "%VCVARS%"
if not defined VCToolsInstallDir exit /b 1
if not defined INCLUDE exit /b 1

set "PATH=%QT_MSVC_DIR%\bin;%BUILD%\debug;%OPENCV_BIN%;%PYLON_RUNTIME_DIR%;%PATH%"

rem --- shared library first ---
if not exist "%LIBBUILD%" mkdir "%LIBBUILD%"
cd /d "%LIBBUILD%" || exit /b 1
qmake -o Makefile "%NCR_PICKING_ROOT%\src\src.pro" -spec win32-msvc CONFIG+=debug || exit /b 1
nmake /nologo || exit /b 1

rem --- then the shell ---
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

Expected success signal: process exit code `0`. On failure the exit code is the **number of
failed test functions**.

> ⚠️ **Nothing reaches `stdout`, so redirecting it captures an empty file.** Measured on
> 2026-08-23: `architecture_contract_test.exe > out.txt 2>&1` produces **0 bytes** whether
> or not `-silent` is passed, while the same binary with `-o file,txt` writes the full
> report. `CONFIG += console` *is* set in the `.pro`, so this is not a subsystem problem —
> QTestLib's plain logger routes to the Windows debug output channel rather than `stdout`
> unless output has been redirected to a file. An empty capture reads exactly like "the exe
> failed to start"; it is not. **To see which test failed and why, always use the file
> logger:**
>
> ```powershell
> .\architecture_contract_test.exe -o results.txt,txt
> Select-String -Path results.txt -Pattern 'FAIL!|Totals'
> ```

> **`test_both_shells_take_the_same_instance_key` takes the real product lock.** It acquires
> `ShellHandoff::kInstanceKey` — the same key `ncr_picking.exe` and `ncr_runtime.exe` use —
> so it fails if either shell is running, including one left over from a manual check. Close
> both before running the suite.
>
> It is also **flaky when the suite is run back-to-back**: three consecutive `cmd /c` runs
> failed on it while the same binary passed 58/58 whenever a run stood alone (measured
> 2026-08-23). A lone failure in this one test with everything else green is almost always
> that, not a regression — re-run it on its own before investigating. Recorded as backlog
> item #39.

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
