# Phase 4 Product Release Plan

**Date:** 2026-06-24 (status revised 2026-07-28)  
**Status:** ON HOLD - plan defined, execution deferred

## Hold Notice (2026-07-28)

Phase 4 is deferred. The product needs further development before a first
release is meaningful, so none of the packaging or release-gate work in this
document is being executed.

Read this document as a recorded plan, not as active work:

- The release shape decision below still stands and should not be re-litigated.
- The packaging work items and the release gate are not current targets. Do not
  block other work on them, and do not treat the gate as acceptance criteria.
- Resuming Phase 4 is a product decision by the user. An agent should not start
  installer or release-candidate work because the rest of the backlog looks
  clear.

Backlog counterpart: `../backlog/technical_debt_and_next_steps.md` →
"Release Track Status - Phase 4 On Hold".

## Release Shape Decision

> ⚠️ **Superseded on 2026-08-19 by the project owner's Phase 6 decision.** The
> two-executable split described below as "a later product decision" was taken:
> `ncr_picking.exe` (commissioning) and `ncr_runtime.exe` (operator runtime) now
> both build from the same `src/` modules via `qmake/app_common.pri`. The section
> is kept because its reasoning still explains what the split costs, and the costs
> it names are now real obligations rather than hypotheticals. See
> [../domains/runtime_app/runtime_shell.md](../domains/runtime_app/runtime_shell.md)
> and `docs/history/plan/phase_6_implementation_plan.md` → Phase D.

The first commercial release should ship as **one Qt application with explicit
Commission and Runtime modes**.

Do not split into separate configuration and runtime executables for the first
release. The current codebase, runtime controller, task runner lifecycle, and
architecture contract tests are already organized around one application with
phase transitions. A two-executable split is a later product decision after the
single-app operator flow is validated.

## Why Single App First

- The current `TaskLocalization` lifecycle already models Commission and
  Runtime explicitly.
- The Phase 1/2 verification baseline is built around `ncr_picking.pro` and the
  architecture contract test suite.
- Splitting now would add project-file compatibility, IPC/shared-service
  boundaries, installer payload variants, and operator permission rules before
  the Runtime UI has completed a real or simulated operator pass.
- The project has not shipped to customers yet, so the first release should
  reduce moving parts and validate the critical Localization runtime path.

## Product Layout For First Release

The installed package should contain:

- `ncr_picking.exe`.
- Qt runtime DLLs and required plugin directories.
- OpenCV runtime DLLs matching the MSVC/Qt kit.
- Basler Pylon runtime dependency, either bundled if licensing permits or listed
  as a prerequisite installer.
- Advanced Docking System runtime DLLs if the final build links it dynamically.
- RobotKinematics transitive runtime DLLs and mesh assets, as detailed in
  [customer_installer_packaging.md](customer_installer_packaging.md).
- License notices for shipped third-party components.
- A release smoke-test checklist and known-risk notes.

## Runtime Operator Flow

The first-release operator path should remain:

1. Open the single app.
2. Load or create a project.
3. Enter Commission mode to configure devices, camera mappings, calibration,
   pattern groups, VisionOutput, and RobotKinematics advisory checking.
4. Start Runtime mode.
5. Confirm task ready/fault outputs and dashboard state.
6. Let PLC trigger Localization cycles.
7. Recover or stop through the existing task/runtime lifecycle.

Runtime screens must stay operator-safe:

- Dashboard is read-only.
- No manual PLC writes, trigger buttons, or destructive configuration controls
  are exposed in the Runtime dashboard.
- Fault state must show the stable fault code and readable reason.
- Device reconnect/recovery behavior must be visible enough for support.

## Packaging Work Items

Use [customer_installer_packaging.md](customer_installer_packaging.md) as the
payload checklist. The next implementation slice should:

- Choose installer technology: Qt Installer Framework, WiX, Inno Setup, or a
  company-standard packager.
- Create a packaging manifest that is separate from `QMAKE_POST_LINK`.
- Add a `windeployqt` collection step, then audit/copy non-Qt dependencies.
- Add RobotKinematics DLL and `robot_assets/` install rules.
- Add third-party license files to the installer payload.
- Create a clean-machine smoke-test record template.
- Decide whether Basler Pylon is bundled or documented as a prerequisite.

## Release Gate

Deferred as of 2026-07-28 — see the hold notice above. Kept for whenever the
release track resumes. Do not mark Phase 4 complete for shipment until:

- Full Debug or Release build succeeds from the documented qmake flow.
- Architecture contract test suite passes.
- The app launches on a clean machine without source-tree or Qt Creator PATH
  dependencies.
- The installed app can find `robot_assets/Nachi/MZ04` from the install
  directory.
- A simulated or real Localization runtime cycle verifies PLC outputs,
  VisionOutput send, dashboard lamps/faults, result table, and task log.
- Missing-DLL analysis is captured before changing PATH on the target machine.

## Deferred Product Split

A future two-executable split may still be valuable, but only after the
first-release runtime is validated. Revisit it when there is a concrete need for
one of these:

- locked-down operator station with no configuration access;
- separate deployment/update cadence for configuration tools and runtime;
- headless or service-style runtime;
- remote project authoring with local machine runtime.

Until then, keep shared project files, device modules, and runtime services in
the single app.

