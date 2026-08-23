# Install Image (`dist/`)

What ships to a field machine, and why it is one folder.

Produced by [`scripts/make_dist.ps1`](../../scripts/make_dist.ps1). This document is the
manifest — the script implements it, and the two must agree.

Build both shells first, ideally through the umbrella `ncr_picking_all.pro` so neither can
be stale against the other:

```powershell
.\scripts\make_dist.ps1
```

**Since Phase 6 / E7b the build itself produces the folder.** Both shells share one
`DESTDIR` — `build\bin\<config>`, set in [`qmake/app_common.pri`](../../qmake/app_common.pri) —
and both the Qt/OpenCV/Pylon/ADS deployment and the RobotKinematics DLL/asset copy resolve
their destination from it. There is exactly one definition of what ships: the build's own
deploy step. The script no longer assembles anything; it **verifies** that output and
copies it.

That distinction matters for reading a failure. Something missing from `dist\` is missing
from `build\bin\` too, and the fix belongs in the qmake deploy step — not in the script.

Measured on 2026-08-23: 2 shells, 40 DLLs (62 including plugin DLLs), 10 plugin/asset
folders, 43 top-level files, zero build intermediates, zero duplicated DLLs.

## Layout

```text
dist/
  ncr_picking.exe          commissioning shell
  ncr_runtime.exe          operator runtime shell
  vc_redist.x64.exe        present when the build directory has it (Basler ships one);
                           an installer can chain it — see #27
  Qt6*.dll                 Qt runtime (via windeployqt)
  opencv_world4NNN.dll     OpenCV — the exact name follows the local install, see
                           OPENCV_WORLD_RELEASE in qmake/opencv_dependency.pri
  Pylon*.dll, GC*.dll      Basler Pylon runtime
  qtadvanceddocking.dll    ADS docking
  coal.dll, assimp-*.dll   RobotKinematics mesh collision
  platforms/               Qt platform plugins (qwindows.dll)
  sqldrivers/              Qt SQL drivers (qsqlite.dll — the project file is SQLite)
  imageformats/  iconengines/  styles/  tls/  networkinformation/  generic/
  translations/            Qt's own .qm files
  robot_assets/            Nachi MZ04D meshes for the kinematic check
```

## Why Both Executables Live In One Folder

- **One runtime set.** The two shells share ~38 DLLs plus the whole plugin tree. Two
  install folders means two copies — and two copies that can drift to *different* Qt or
  Pylon versions. That mismatch does not show up in the office; it shows up on the
  customer's machine.
- **The future mode switch needs it.** When the shells can hand off to each other, one
  locates the other with `QCoreApplication::applicationDirPath()` plus a fixed filename.
  Same folder turns that from configuration into a fact.
- **One writer, one reader.** Shipping them together does not create a project-file
  compatibility problem, because only `ncr_picking.exe` ever writes one.

## Why `dist/` Still Exists Now That `bin/` Is Complete

Before E7b each shell deployed a full runtime set next to its own intermediates, so
`dist/` had to *select* what shipped out of ~270 `.obj` and ~130 generated `.cpp`, and
merge two folders into one. It no longer does either: `build\bin\<config>` is already the
shippable set.

What `dist/` adds is the set of checks a build step cannot make, because a build step
cannot fail on something it was never asked to produce:

- **both** shells are present — a one-shell image is not an install image, and building
  one shell alone leaves the other's executable in `bin\` untouched and stale;
- their file versions **match** and are not the `0.0.0.0` placeholder;
- nothing that is a build intermediate leaked in;
- no DLL appears twice.

It also gives release a stable artefact that a rebuild cannot silently change underneath
it.

The copy still excludes intermediates **by extension** rather than listing what to keep,
so a newly added DLL family lands in the image automatically instead of being silently
absent — the failure mode of a keep-list is a missing file nobody notices until the
customer's machine. In `bin/` that filter should catch nothing; if it catches something it
**warns with the file names** rather than dropping them quietly, because a `.obj` in a
`DESTDIR` means something else is wrong.

## Version Pairing

Both executables read their version from `src/core/app_version.h` and publish it through
`QCoreApplication::applicationVersion()`; both log it at startup and the runtime shows it
on its project-select page.

This matters because the project file carries schema versions
(`TaskLocalizeConfig::kSchemaVersion` is 2 as of Phase 6). A newer editor writing a
project an older runtime refuses to load is precisely what "two exes in one folder" makes
possible. Nothing corrupts — the schema gate refuses loudly — but the field report is *"the
runtime will not open what the editor just saved"*, and the version line turns that into a
one-glance diagnosis.

`qmake/version.pri` is the one place the number is written. It feeds **both** the Windows
file-resource `VERSION` (readable from Explorer's Properties dialog without launching
anything) and `NCR_APP_VERSION_STR`, which `src/core/app_version.h` compiles in.

`scripts/make_dist.ps1` **refuses to stage** when the two executables report different
versions, and also when they report `0.0.0.0` — a build predating `version.pri` would
otherwise produce an image whose version pairing cannot be checked at all, which is worse
than a mismatch because it looks like a passing check.

> Builds that do not include `qmake/app_common.pri` — currently only
> `tests/architecture_contract_test` — fall back to `0.0.0-dev`. The fallback is
> deliberately not a plausible number: a stale but believable version is worse than an
> obviously unreleased one.

## Verification

Run **both** executables from `dist\` on a machine with the dependency directories removed
from `PATH`. Neither may fall back to a developer machine's Qt / OpenCV / Pylon install —
catching that fallback is the entire purpose of the check.

**Both executables were verified this way on 2026-08-23**, from `dist\` and from
`build\bin\release`, with `PATH` reduced to `system32;Windows;Wbem`. Each stayed up and
reached its main window — `NCR-PICKING (No Project opened)` and `NCRN Pick — Runtime`.

`ncr_runtime.exe` had been deferred until then, on the grounds that it auto-loads the
remembered project and would reach for real hardware. That is true **only once a project
has been run**: with `lastRuntimeProjectPath` empty it stops on the project-select page and
touches no device, which is what made the check safe here. On a machine that has already
run a project, the same launch does open the camera and the PLC socket — check
`%APPDATA%\NCRN Pick Runtime\settings.dat` before assuming it is a harmless smoke test.

> A start-and-stay-up check proves the DLL set is complete and self-contained. It does not
> prove the `.qrc` resources resolved — a missing icon, QSS theme or `:/i18n` translation
> produces no error at all, only a plain-looking window. That one has to be looked at.

## After Installing

Copying `dist/` is not the whole install. A field station also needs boot-start
configured, and there are three settings that fail silently if chosen wrong —
Task Scheduler settings, the account it runs as, and auto-login. See
[../domains/runtime_app/runtime_shell.md](../domains/runtime_app/runtime_shell.md) →
"Starting With Windows".

## Not In Scope Here

Still tracked in [`../backlog/later_todo_list.md`](../backlog/later_todo_list.md) #27:

- an actual installer (MSI/NSIS) and its uninstall story;
- Basler **GenTL producer** deployment and `GENICAM_GENTL64_PATH` setup — a GigE camera
  needs more than the top-level Pylon DLLs;
- Visual C++ redistributable handling;
- a clean-machine smoke test.

`dist/` is the input an installer consumes. Having it means the installer can be written
later without re-deriving what ships.
