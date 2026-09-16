# Technical Debt And Next Steps

**Date:** 2026-06-24 (release-track status revised 2026-07-28)  
**Status:** Active implementation backlog after restructure closeout

## Release Track Status - Phase 4 On Hold (2026-07-28)

**Decision:** Phase 4 is deferred and is not being executed. The product needs
further development before a first release is meaningful, so the release track
(operator runtime validation as a release gate, customer installer, release
candidate hardening) is parked rather than in progress.

What this changes:

- Stage A / Stage B / Stage C below are **on hold**. Do not open them, and do
  not treat their exit gates as active acceptance criteria.
- The release gate in
  [../product/phase4_product_release_plan.md](../product/phase4_product_release_plan.md)
  is not a current target. Nothing should be blocked on it.
- Nothing already decided is reversed. The single-app shape with explicit
  Commission/Runtime modes remains the recorded release shape for whenever the
  release track resumes.
- Feature and engineering work continues normally. Debt items outside the
  release track stay active.

Resuming Phase 4 is a product decision by the user, not something an agent
should infer from the backlog being otherwise clear.

## Carried Out Of Phase 9 (closed 2026-09-16)

Phase 9 (the localization-runtime backlog plan,
[../history/plan/phase_9_implementation_plan.md](../history/plan/phase_9_implementation_plan.md))
closed on 2026-09-16 **with carried items, not clean**: the umbrella build was clean, every suite
green at the counts recorded at Checkpoint Z, and Z1–Z3 landed — but thirteen items were held by
owner-run checks, one blocked tool and one field defect. Each is listed here with a concrete
destination, a Phase 10 work package (Phase 10 charter §3; the charter moves to
`docs/plan/phase_10/` at Checkpoint 0) or a backlog item in
[later_todo_list.md](later_todo_list.md), so a reader of *this* file does not mistake "Phase 9
closed" for "all of it is proven". The same table is recorded at Checkpoint Z of the plan; the owner
approved the carry on 2026-09-16 (charter §6).

| Item | What is unfinished | Destination |
|---|---|---|
| **Phase G / item 54** — a held `bExecuteTrigger` at runtime start | Phase G not started (owner-gated), so G2's held-trigger behaviour was never run and item 54's decision is still open | **WP-50** — Stage 5 pilot on the new runtime core, reusing the G2 design under the DR that answers PQ-1. Item 54 stays open until WP-50 lands |
| **E4 on hardware** — a real PLC's refusal aborting the cycle with `301 PlcWriteFailed` | Blocked: the owner has no way to make a healthy PLC refuse a write; two untried recipes are recorded on the item. Bench-proven only | **Backlog 69** — non-blocking; not a Phase 10 gate |
| **D1 field check** — vision-output reconnect without a restart | Observed failing on the cell 2026-09-09: a `VisionTcpipClientDevice` stays Recovering when only its heartbeat link comes back; the reconnect-is-a-no-op hypothesis is on the item, investigation paused by the owner | **Backlog 70** for the device-side defect, fixed on its own; **WP-32** (role-health rows of the new core) must carry a row for scenario S-06 |
| **Owner review of `plc_signal_contract.md`** (the Z2 rewrite) | Not done | Replaced, not rescheduled: the owner reviews the contract **table** at Checkpoint 2 — **WP-21** (to-be table) — instead of the prose |
| **Documentation build** | Doxygen / Graphviz / PlantUML are absent on this machine, so the generated reference was not rebuilt after Z2 | **Backlog 44** — non-blocking |
| **51, device half** — `IDevice::errorOccurred` | No device emits it; the emit-or-delete decision is open (the runner half closed with B1) | **WP-03** triage — close, merge, keep, or absorb into the Phase 10 redesign; stays item 51 |
| **56** — MC 1C/3C on a real C24 | The 1C/3C command set on a real module, and the `tools/mc_protocol_bench` numbers (write completion closed by E1) | **WP-03** triage; stays item 56 |
| **58, part B** — the true power-up selection | A mapped-but-never-written register as its own fault; MC / Modbus-client first-poll adoption for every non-index signal (part A closed by C2 + C3 + C6) | **WP-03** triage; stays item 58 |
| **63** — fault code 400 for content-invalid pattern faults | Awaits the owner allocating `PatternInvalid = 402` | **WP-03** triage; stays item 63 |
| **64** — dual-role Modbus publish/poll collision | The result publish dispatched during an in-flight poll aborts the cycle (2 of 85 cycles on 2026-09-14); filed by Z3, not fixed | **WP-03** triage; stays item 64 |
| **66** — two editors for the robot pick check | The task's editor and each vision-output device's can disagree silently (open question O-2); removing one is a deprecation with a migration question | **WP-03** triage; stays item 66 |
| **67** — recovery policies not settable | `setRecoveryPolicies()` has no production caller and the values are not persisted (deferred by D5) | **WP-03** triage; stays item 67 |
| **68** — two-position cap and a debug print | `buildVisionOutputPositions()` caps at two positions with no reason in the row, and `handlePlcValues()` keeps a temp-debug print; owner to confirm whether the cap is deliberate | **WP-03** triage; stays item 68 |
| **A1 on hardware** — `bExecuteTrigger` delivery on an M-only MC PLC | Ships on unit evidence only (open question O-3): no M-only station was identified, so the field half of A1 was never run. Not a defect, an unverified claim | **Backlog 71** (filed 2026-09-16); non-blocking, re-opened when an M-only station exists |

## Carried Out Of Phase 8 (closed 2026-09-07)

Phase 8 delivered the MC 1C/3C frames, both Modbus TCP devices, the JAI GigE camera,
and the commissioning fixes that followed. It closed with **three items carried
rather than ticked**. They live in
[later_todo_list.md](later_todo_list.md); they are listed here so a reader of *this*
file does not mistake "Phase 8 closed" for "all of it is proven".

| # | Item | Needs |
|---|---|---|
| **56** | MC 1C/3C **read/write command coverage** on a real C24, plus `tools/mc_protocol_bench` numbers for 3E/1C/3C | **Partly closed by Phase 9 / E1 (2026-09-09):** write completion is proven on the real C24, and a device-level MC harness now exists. Still open: the 1C/3C command set on a real module, and the bench numbers |
| ~~**57**~~ | ~~Robot pick check verified **active** on the dual-role Modbus binding~~ | **CLOSED 2026-09-14** by Phase 9 / F1 — the check became a task setting, and the owner observed it reject unreachable poses on the dual-role Modbus binding |
| **54** | Decide what a **held** `bExecuteTrigger` should do when the runtime starts | Still open — owner-gated as Phase 9 / Phase G, not started. The PLC signal contract holds that section back with a pointer to the item |

Two smaller audit findings from the same phase were open and are now **closed by Phase 9**: **55**
(`setup()` reported an unregistered active camera as a calibration problem — C2 + C7) and the
pre-existing **32** (flaky `test_disconnect_notice_on_graceful_close` — D2, 0 / 20 on both suites).

**Found after Phase 8 closed, owner-scheduled into Phase 9: item 58.** At runtime
startup the task reaches Ready with the PLC's selection registers at 0, and will
run a trigger, because `setup()` takes the active indices from the project's
bindings and bypasses every gate the setters carry — while a register that is 0
from power-up and never written is never delivered at all. The runtime also never
publishes its own selection, so "nobody wrote anything" and "0 was commanded" look
identical on the signal monitor. Item **59** carries three unrelated things found
on the way. Confirmed on hardware and by a 13-agent adversarial pass;
`plc_signal_contract.md` had claimed the opposite and has been corrected.

**Resolved by Phase 9 (part A, 2026-09-09):** C2 validates the startup selection, C3 announces it on
two status outputs, and C6 reads it from the PLC before the camera is bound; the owner confirmed that a
master holding 0 now faults. Part B's remainder stays open in item 58, and item 59 closed.

**Resolved 2026-09-07: item 43** — a virtual PLC's inputs can now be driven, so a
hardware-free project runs a full cycle instead of stopping at Ready. This was the
one thing blocking hardware-free verification of the runtime path, which matters
beyond the item itself: every Phase F defect was found by the owner running the
cell, because no suite could reach that path. See
[later_todo_list.md](later_todo_list.md) item 43.

Also still open and **not** a Phase 8 deliverable: the Japanese UI has **465 untranslated strings**
(sweep of 2026-09-14: 1324 source texts — 860 finished, 465 unfinished, 33 vanished). Every Phase 9
task swept the catalogue with 0 newly vanished, but the rendering itself has never been verified.

## Highest Priority Debt

### Runtime/Task Safety

- **Done (2026-06-24):** ~~Fix `TaskLocalization` shared-pointer ownership bugs
  where raw device pointers are reset into independent `std::shared_ptr` control
  blocks.~~ Audit found this already fixed — no `.reset(raw)` remains, and device
  typing uses `std::dynamic_pointer_cast`, so every owner shares the single
  `DeviceManager` control block. See `docs/backlog/later_todo_list.md` item 22.1.
- **Done (2026-06-24):** ~~Finish runner-based active-camera switching.~~
  Reviewed the full `TaskLocalization::setCameraNumber` →
  `LocalizationRuntimeController::setActiveCameraNumber` path: it is fully
  runner-coordinated (previous camera disconnects via `requestDisconnect()`, new
  via `requestConnect()`, per-cycle grab/command connections always target the
  active `CameraRunner`, mid-cycle changes are guarded). Fixed one latent
  null-deref/stale-workspace defect on the camera-switch path. See
  `phase2_phase3_runtime_hardening.md` → "Still Open".
- **Done (2026-06-24):** ~~Decide whether `LocalizationRuntimeController` must
  move into `TaskRunner::m_runtimeThread`.~~ Decision recorded: the controller
  runs on the coordinator thread, devices keep per-device threads, and matching
  is offloaded to `matchingRunner`. This is the supported design; revisit only on
  measured latency evidence. See `phase2_phase3_runtime_hardening.md` →
  "Runtime Threading Model Decision (2026-06-24)".
- **Done (2026-06-24):** ~~Snapshot `MatchGroup` data before worker-thread
  matching so GUI mutation cannot race the matching worker.~~ Both paths now hand
  the worker an isolated deep copy via `TaskLocalization::snapshotPatternGroup()`:
  the runtime path in `buildRuntimeContext()`, and the commission path in
  `startCommissionMatching()` (which previously passed the live
  `PatternGroupManager` group straight to the matching thread). Verified by the
  architecture contract suite (38 passed). See `docs/backlog/later_todo_list.md` item 22.6.

### Product Verification

- **On hold (2026-07-28, Phase 4 deferred):** Run an operator UI pass against a
  real or simulated Localization cycle: dashboard lamps, fault panel, KPIs,
  result table, task-local log, read-only dashboard behavior, and recovery
  messaging. Still useful as development validation whenever the runtime path is
  touched, but it is no longer a release gate.
- **Done for cycle latency (2026-09-14, Phase 9 / F3); UI responsiveness not measured.** Every
  `CycleResult` now carries a per-stage breakdown from one monotonic clock. 85 cycles on the
  dual-role Modbus cell: median **304 ms** end to end, of which matching is **241 ms (79 %)**, grab
  55 ms (on a virtual camera — the Basler measured ~200 ms on 2026-09-11), Modbus send 4 ms. The
  threading-model revisit criterion in `phase2_phase3_runtime_hardening.md` was evaluated against that:
  matching already runs on its own thread and dominates, so **no revisit**. Distribution and caveats:
  Phase 9 plan, Checkpoint F. UI responsiveness was not part of F3 and is still unmeasured.
- **On hold (2026-07-28, Phase 4 deferred):** Implement customer installer
  packaging and run clean-machine smoke verification.

### Persistence And Schema

- **Done (2026-06-24):** ~~Add schema/version validation to
  `TaskLocalizeConfig::toJson()` / `fromJson()`.~~ Added `version` /
  `kSchemaVersion`; documents from a newer app are refused. See
  `later_todo_list.md` 22.8.
- **Done (2026-06-24):** ~~Validate imported binding data, especially camera
  numbers and device-id strings.~~ `TaskDeviceBinding::fromJson()` now
  range-checks camera numbers (1..16) and caps device-id length. See 22.9.
- Decide whether `docs/generated/architecture_docs/` is regenerated, hand-maintained, or
  removed as stale API reference. Phase 9 / Z2 hand-rewrote `LocalizationRuntimeController.md`, whose
  signals, fault codes and recovery model had drifted into fiction; the decision itself is still
  open.

## Medium Priority Debt

- **Open (2026-08-27):** **Unify `BaslerCameraWidget` and `JaiCameraWidget`.** Phase 8 / C4
  added the JAI panel as a near-sibling of the Basler one. They now differ in only three
  things — the config type, the exposure enum, and the camera-select dialog — while the
  device-info browser, the connect/trigger/save controls and the *entire* calibration
  workflow (board setup, threshold tuning, corner detect, apply) are generic over
  `CameraDevice` / `CameraCfg` and exist twice, ~450 duplicated lines.
  It was written as a sibling on purpose: unifying would have rewritten the panel of a
  camera already running in production, with no widget-level test to catch a regression, in
  the same change that introduced a second camera family. Now that a second implementation
  exists the shared shape is visible and the extraction is safe to plan.
  **Do it before a third camera family is added**, and note that the JAI copy already fixed
  four defects the Basler original still has, so the unified version must keep the JAI
  behaviour, not the Basler one:
  - `onCameraConnected()` sets `connectionState` to `"disconnected"`, so the Basler lamp can
    never turn green whatever the QSS says;
  - `grabSingleShot()` shadows its `GrabResult` in the success branch, so the value it
    *returns* always reports failure while the signal it emits reports success;
  - `btn_save_image_clicked()` is an empty body, so the Save button silently does nothing;
  - `BaslerCamSelectDialog::tableViewSelectionChanged()` tests `(row < 0) && (row >= size)`,
    which is never true, so the confirm button enables with nothing selected.
- **Done (2026-06-24):** ~~Extract shared gadget meta-property helper logic if
  property-browser dispatch keeps growing.~~ Added `vc::gadget_meta` in
  `qgadget_macro.h`; setting + vision widgets route through it. See
  `later_todo_list.md` 22.17.
- **Partially done (2026-06-24):** Finish QSS token mechanism and migrate
  remaining hardcoded theme colors. Added a runtime token resolver: `.qss` files now
  reference `docs/rules/ui_theme_tokens.md` design tokens as `@{group.token}`
  placeholders, and `ThemeManager::resolveTokens()` substitutes them from the
  canonical `tokenTable()` in `theme_manager.cpp` on every sheet load (global apply +
  per-form reload). Migrated all 18 sheets (1054 token references); the sweep is
  provably non-visual-changing — the C++ table reproduces every original colour
  byte-for-byte (modulo case/whitespace). A follow-up handoff sweep then added
  `device.*`, `state.*.bright`, `state.error.deep`, `accent.pressed.deep`, and
  `overlay.*` tokens and migrated the affected QSS selectors onto them. The
  sweep scope is now acceptance-clean except for the approved raw `#7a1010`
  single-use shadow; see `docs/rules/ui_design_rules.md` §3.7 and
  `docs/rules/ui_theme_tokens.md`.
- **Done (2026-06-24):** ~~Finish the remaining UI token closeout for
  `DevicesMonitorWidget` inside `MitsubishiMcDeviceWidget` and
  `SystemLogForm`.~~ Both surfaces now follow the active theme: the PLC monitor
  widget reloads token-backed QSS and delegate painting on theme changes, and
  `SystemLogForm` uses explicit dark/light styling plus visible-entry
  re-rendering so existing log lines recolour after a theme toggle.
- **Done (2026-06-24):** ~~Replace remaining `ConnectStatus` `default:` switches
  where explicit enum handling would catch future states.~~ All `ConnectStatus`
  switches are now exhaustive. See 22.16.
- **Done (2026-06-24):** ~~Improve user-facing feedback for device rename/save
  failures.~~ Rename failures (4 widgets) and the connected-device save block
  now emit a user warning. See 22.21/22.22.
- **Done (2026-06-24):** ~~Review `TaskLocalization` public limit constants and
  dead members after runtime tests are stable.~~ Limits are now
  `static constexpr kLimit*`; dead members removed. See 22.18/22.14.
- **Done (2026-06-27):** ~~Migrate hard-coded local dependency paths in qmake
  files and helper scripts to environment variables.~~ Added shared qmake
  dependency includes under `qmake/` for OpenCV/Pylon, migrated
  `ncr_picking.pro`, the architecture contract test, and calibration tests,
  and removed hard-coded Qt/Visual Studio/VTK defaults from helper scripts.
  `scripts/build_test.bat` now builds the architecture contract test beside its `.pro`
  under `tests\architecture_contract_test\build\msvc_debug`. New work must
  follow `docs/rules/build_and_verification.md`: root app builds under root
  `build\`, tests/examples/components build beside their `.pro`, and
  Qt/OpenCV/Basler/Visual Studio paths come from environment variables.
- **Done (2026-07-28):** ~~Mask-based gripper collision check.~~ Confirmed by
  the user against real picking runs with no observed failures, so the new path
  is the proven one. Follow-up (1) validation is closed by that result.
  Follow-up (2) is decided: nested holes stay filled solid — the conservative
  behaviour is accepted, `RETR_TREE` hierarchy carving is not pursued. Follow-up
  (3) remains as cleanup only: the deprecated `checkCollisionObject` is still
  present in `src/matching/match_object.h` and is now dead (no caller); tracked
  in `later_todo_list.md` item 29. Original description below for traceability.
  `MatchedObject::checkCollisionObject2` (in
  `src/matching/match_object.h`) replaces the point-in-polygon test at
  `ImageMatcher::matching` (`src/matching/image_matcher.cpp`). The old vertex
  test reported no collision when a jaw sat wholly inside a large part (no
  contour vertex fell inside the jaw); the new test rasterises the two jaws and
  the filled contour footprints and intersects their areas, classifying each
  object as `Outside` / `Inside` / `Collision`. Only `Outside` is treated as
  pickable (`hasCollision = state != Outside`). The filled-contour mask is built
  once per frame in `matching()` and shared; per object the intersection runs
  only inside the jaw union bounding box for speed. The previous
  `checkCollisionObject` is retained but no longer called.

## Product/Packaging Next Stages

**All three stages below are ON HOLD as of 2026-07-28** — see "Release Track
Status" at the top of this document. They are kept in full so the track can be
resumed without re-planning, but none of them is active work and none of their
exit gates is a current acceptance criterion.

### Stage A - Operator Runtime Validation (on hold)

Goal: prove the current single-app Runtime mode before packaging.

Work:

- Build a repeatable simulated cycle scenario if hardware is unavailable.
- Capture screenshots/logs of ready, running, success, and fault states.
- Confirm PLC outputs match `docs/domains/task_localization/plc_signal_contract.md`.
- Record failures in `docs/backlog/later_todo_list.md` or dedicated issue docs.

Exit gate:

- Operator runtime smoke result is written to docs.
- Any blocking runtime defects have focused tests or reproduction steps.

### Stage B - Installer Prototype (on hold)

Goal: create an installer or install folder that does not rely on developer
PATH/source-tree state.

Work:

- Pick installer technology.
- Add packaging manifest.
- Add Qt/OpenCV/Basler/RobotKinematics payload rules.
- Include license notices.
- Run missing-DLL analysis on the install folder.

Exit gate:

- Installed app launches on a clean VM.
- `robot_assets/Nachi/MZ04` loads from the install directory.

### Stage C - Release Candidate Hardening (on hold)

Goal: turn the prototype into a repeatable release candidate.

Work:

- Run architecture contract tests and app build from clean checkout.
- Run operator smoke.
- Run clean-machine install smoke.
- Record dependency versions and rollback plan.

Exit gate:

- Release checklist is complete and does not rely on tribal knowledge.

## How To Track New Debt

- Use `docs/backlog/later_todo_list.md` for localized deferred work.
- Use this document for cross-cutting debt that affects release planning.
- Do not mark debt resolved until code, docs, and verification evidence are all
  updated.
