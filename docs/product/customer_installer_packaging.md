# Customer Installer Packaging

**Status:** Open release risk, ON HOLD as of 2026-07-28  
**Last updated:** 2026-07-28  
**Scope:** customer-facing installer or deployment package, not developer build folders

Phase 4 is deferred, so this checklist is not active work. It stays accurate as
a payload reference for whenever the release track resumes; nothing here should
be started, and nothing else should be blocked on it. See the hold notice in
[phase4_product_release_plan.md](phase4_product_release_plan.md).

See also [phase4_product_release_plan.md](phase4_product_release_plan.md) for
the first-release product shape and release gate.

## Why This Exists

The Debug build folder now deploys the RobotKinematics runtime DLLs and
`robot_assets/` next to `ncr_picking.exe`, but that only proves local developer
execution. Customer packaging is a separate release concern: a clean target
machine must run without relying on source-tree paths, developer PATH entries,
Qt Creator kits, or previously installed third-party DLLs.

> **Update 2026-08-21 (Phase 6).** The payload is no longer hypothetical: the product now
> ships **two** executables, and `scripts/make_dist.ps1` stages both over one shared
> runtime set into `dist/`. That staged layout — and the reasoning behind one folder rather
> than two — is written down in [install_image.md](install_image.md), which is the concrete
> manifest this checklist described in the abstract. The remaining gaps below (GenTL
> producers, VC++ redist, a real installer, clean-machine smoke) are still open.

## Installer Payload Checklist

Ship these beside the installed executable or in a layout that the executable
can resolve deterministically:

- **Both** application shells — `ncr_picking.exe` and `ncr_runtime.exe` — in the
  same folder, over one copy of the runtime. Two folders means two copies that
  can drift to different Qt or Pylon versions, and a future editor/runtime switch
  locates its sibling by `applicationDirPath()`. See
  [install_image.md](install_image.md).
- Any other project-owned helper executables.
- Qt runtime DLLs and plugins, including at minimum `platforms/`, `imageformats/`,
  and any SQL/style/plugin folders used by the app. Prefer `windeployqt` as an
  initial collector, then audit the result manually.
- OpenCV runtime DLLs matching the build configuration and ABI.
- Basler Pylon runtime DLLs or a documented prerequisite installer.
- Advanced Docking System runtime DLLs if linked dynamically.
- RobotKinematics runtime DLLs currently required by build-folder deploy:
  - `coal.dll`
  - `assimp-vc143-mt.dll`
  - `boost_serialization-vc143-mt-x64-1_87.dll`
  - `boost_filesystem-vc143-mt-x64-1_87.dll`
- RobotKinematics mesh assets under `robot_assets/Nachi/MZ04`, including the
  `simplified/` mesh profile.
- Third-party license notices for Qt, OpenCV, Basler, Coal, Assimp, Boost, and
  any transitive libraries shipped in the installer.

## Clean-Machine Verification

Run this before a customer release:

- Install on a machine or VM that has no Qt Creator kit, no project source tree,
  and no developer PATH additions.
- Launch the app directly from the installed shortcut and from the installed
  executable path.
- Open or create a Localization project.
- Add/configure the first-release device set: Basler GigE camera, Mitsubishi MC
  PLC, VisionOutput TCP/IP, and RobotKinematics advisory checking.
- Confirm `robot_assets/Nachi/MZ04` resolves from the install directory and the
  mesh-collision profile loads without falling back to source-tree discovery.
- Run a simulated or real Localization cycle and confirm PLC outputs,
  VisionOutput send, dashboard lamps/faults, result table, and task log.
- Capture missing-DLL failures with a dependency scanner before changing PATH.

## Field Machine Configuration

Beyond copying files, a field station needs three things set up. Full rationale and the
exact Task Scheduler settings are in
[../domains/runtime_app/runtime_shell.md](../domains/runtime_app/runtime_shell.md) →
"Starting With Windows"; the traps worth repeating here, because an installer author will
meet them:

- **Boot start via Task Scheduler**, trigger "At log on" with a delay, running **as the
  operator's account** with **"Run only when user is logged on"**. Selecting "whether user
  is logged on or not" starts the runtime in a session with no desktop: it takes the
  camera and the PLC socket and displays nothing, so the station looks dead while working.
- **Same account as the interactive login.** The single-instance guard is per-user; a task
  running under a different account cannot see the interactive instance, and both start.
- **Windows auto-login**, if the station must reach the runtime with nobody present.

An installer that writes the scheduled task should verify these three rather than assume
them — each one fails silently in a way that looks like a different problem.

## Current Boundary

`components/RobotKinematics/robotkinematics.pri` is allowed to copy DLLs/assets
for build-folder runs. The customer installer must not depend on
`QMAKE_POST_LINK` as the only deployment mechanism; packaging needs its own
explicit payload manifest and clean-machine smoke result.
