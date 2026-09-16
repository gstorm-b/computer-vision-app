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

Beyond copying files, a field station needs four things set up. Full rationale and the
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
- **Windows Defender Firewall inbound allowance** — see below.

An installer that writes the scheduled task should verify these four rather than assume
them — each one fails silently in a way that looks like a different problem.

### Windows Defender Firewall: the app needs an inbound allowance

> ⚠️ Verified on a real station, 2026-08-26. Without this, a Modbus TCP **server** device
> binds and listens successfully, the panel shows *Connected*, and no master on the network
> can ever reach it. Nothing in the log says why, because from the application's side
> nothing happened: Windows drops the SYN before it reaches the socket.

Add both `ncr_picking.exe` and `ncr_runtime.exe` to **Control Panel → Windows Defender
Firewall → Allow an app or feature through Windows Defender Firewall**, ticking the
profile the plant network actually uses — **Private** and **Domain** are usually right, and
a plant LAN that Windows has classified as *Public* needs Public ticked or the rule does
nothing. Check the classification rather than assuming it; an unidentified network defaults
to Public.

This is **inbound only**, and it matters for exactly the cases where something on the
network has to reach *us*:

| Feature | Why it needs inbound |
|---|---|
| Modbus TCP **server** device | A master opens the connection to us (default port 502) |
| GigE Vision camera discovery | Cameras answer a broadcast; the reply is unsolicited inbound UDP |
| VisionOutput TCP/IP **server** mode | The robot/PLC controller connects to us |

Outbound-only roles — the Modbus TCP **client**, the Mitsubishi MC device, VisionOutput in
client mode — work without any rule, which is what makes this so confusing to diagnose: on
the same station, in the same project, the client half of a Modbus pair connects and the
server half is unreachable.

Two things that look like a fix and are not: allowing the *port* rather than the app breaks
the moment the port is changed in a project, and Windows' own "do you want to allow…" popup
on first launch is missed entirely when the app is started by the scheduled task, since that
session has no desktop to show it on. An installer should write the rule explicitly (for
example `netsh advfirewall firewall add rule ... dir=in action=allow program=...`) rather
than rely on the operator answering a prompt.

## Current Boundary

`components/RobotKinematics/robotkinematics.pri` is allowed to copy DLLs/assets
for build-folder runs. The customer installer must not depend on
`QMAKE_POST_LINK` as the only deployment mechanism; packaging needs its own
explicit payload manifest and clean-machine smoke result.
