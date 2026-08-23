# Phase 6 Implementation Plan — Fault Acknowledge, Unlimited Reconnect, Runtime Executable

**Date:** 2026-08-19
**Status:** **Phase A ✅ DONE (2026-08-20)** — A1–A7 implemented; Checkpoint A benched by
the user, fault and acknowledge behaved as expected.
**Phase B ✅ DONE (2026-08-20)** — B1–B6 implemented; Checkpoint B benched, found five
defects the old escalation path had been masking (fixed as **B7**), and the re-bench
confirmed expected behaviour.
**Phase C ✅ DONE (2026-08-20)** — C2 landed early inside B7; C1 documentation complete.
**Phase D ✅ CODE COMPLETE (2026-08-20)** — D1–D8 implemented. Both shells build,
contract test 46 passed / 0 failed. **Checkpoint D is user-owned**: every acceptance item
except the build and the contract test needs a real project file and a real machine.
**Phase E ✅ CODE COMPLETE (2026-08-23)** — E1–E6 ✅ (2026-08-20/21); **E7a** ✅ (2026-08-21),
**E7b / E7c ✅ (2026-08-23)**. E7 was opened because E6's known cost showed up as a ~25 min
full rebuild once the user ran it; E7a brought that to 18.6 min. Checkpoint E items 1 and 2
(double-launch, kill-and-relaunch) and E6 confirmed by the user on 2026-08-21.
Contract test **51 passed / 0 failed**. Both shells now link into one `build/bin/<config>`
over a single runtime set, and **both** were verified to run from `dist/` with the
dependency directories stripped from `PATH`.

> ### ⚠️ 2026-08-23 — `runtime_app/` was lost and restored
>
> The folder was found **missing from disk** at the start of the session. It had never been
> committed, so git could not recover it: no history, no stash. The project owner restored
> it from their own copy, and it was then verified by clean rebuild — it reproduces every
> number E7a recorded (262 objects compiled once, 0 `qrc_*.cpp` in the library, 4 per
> shell, `ncr_runtime.exe` at 1.77 MB), which is the strongest available evidence that the
> restored copy is the final revision and not an older one.
>
> **The whole of Phase 6 is still uncommitted**, including this plan file. Everything the
> phase produced is exposed to the same loss. Committing it is a project-owner decision and
> has not been done.

**All of Phase 6 is code complete.** What remains is user-owned verification: Checkpoint D
(runtime shell on a real project), the Phase E manual checks — double-launch and
crash-recovery for the guard, and a scheduled-task reboot — and Checkpoint E7's resource
check, which is the one item nothing in the repository can substitute for: a missing `.qrc`
produces no build error, so only *looking at* both windows proves the icon, the QSS theme
and the Japanese translation still load.

> **B found a latent hazard that B itself would have created.** Losing a role outside a
> running cycle never published `bTaskReady = false` and never left
> `CycleState::ReadyForTrigger`. The escalation path covered for it: after 10 failed
> retries it published `bTaskFault` and the PLC saw *something*. With escalation deleted
> there is no backstop, so an unreachable camera would have advertised "ready, send me a
> trigger" forever. Entering recovery now withdraws readiness — see B2b. The plan's own
> Phase B text already described this as existing behaviour; it was not.
**Source request:** [../request/phase_6_request.md](../request/phase_6_request.md)

> ⚠️ **Phase 6 reverses a standing product decision.** `AGENT.md` and
> `app/AGENTS.md` both state the first release ships **one** executable with explicit
> Commission and Runtime modes, "no separate runtime app until the operator flow is
> validated". The user chose a **separate `ncr_runtime.exe`** for this phase (see D1).
> Both scope cards and the Phase 4 release-shape section must be amended as part of
> Task D1 — otherwise the next agent reads the old invariant and treats the new
> executable as a violation to clean up.

> **Location note.** This file sits under `docs/history/`, which `AGENT.md` declares
> traceability-only. Same exception as the Phase 5 plan: this is the CURRENT plan for
> Phase 6, not history. The Phase 5 plan already recommended moving this folder to
> `docs/plan/` and linking it from `docs/README.md`; that recommendation still stands
> and is now twice as load-bearing.

## Overview

Phase 6 has four independent tracks. Only D depends on A/B/C, and only loosely (it
displays what they produce).

1. **Fault acknowledge, plus an automatic way out.** A new `bErrorReset` PLC input whose
   rising edge clears `bTaskFault` and re-arms the runtime — and, when no acknowledge
   arrives, a 2-second timer that does the same thing by itself. Today there is no way
   back from a cycle fault except restarting the task.
2. **Recovery becomes unlimited.** A lost device is retried until the runtime ends,
   instead of escalating to a hard fault after 10 attempts. Recovery logging is
   de-duplicated so an outage does not bury the operator's event log.
3. **Camera grab retry gets fixed and documented.** The behaviour already exists in
   `CameraRunner` and only the docs were missing — but auditing it for the doc showed
   the retry chain mostly does not run. Both the fix and the docs land in this phase.
4. **A separate runtime executable.** `ncr_runtime.exe`: pick a project file (or
   auto-load last), enter runtime automatically, show one dashboard per task in a
   dock/float layout of 1/2/4/6/8 tiles, max 8.
5. **Shipping that executable to a field machine.** Both exes in one install folder over
   one shared runtime set, a single-instance guard, matched versions, and a boot-start
   mechanism that survives a cold network. Added after Phase D from the packaging review;
   tracked as Phase E.

## Resolved Decisions

| # | Decision | Consequence |
|---|---|---|
| **D1** | **Runtime ships as a separate executable, `ncr_runtime.exe`.** Chosen by the user on 2026-08-19 over a mode inside `ncr_picking.exe`. | Reverses the Phase 4 "one executable" shape. Needs a second `.pro`, a shared qmake include, a second deploy manifest, amendments to `AGENT.md` + `app/AGENTS.md` + `docs/product/phase4_product_release_plan.md`, and a new module in the architecture-contract layering test (which today hard-codes the module list and would silently skip a new top-level folder). |
| **D2** | **`bErrorReset` clears the fault AND re-arms the runtime.** Rising edge → `bTaskFault=false`, `nFaultCode=0`, then attempt `markRuntimeReady()`. | Requires a `Faulted → Ready` edge in `canTransitionTaskState()`, which today only permits `Faulted → Stopping`. Without that edge the PLC flag would clear while the task UI stayed Faulted forever — worse than not implementing it. |
| **D3** | **Signal is named `bErrorReset`, not `nErrorReset`.** | The request wrote `nErrorReset (bool)`; all 9 existing bool signals use the `b` prefix and all 4 numbers use `n`. The name is a persisted project-file key, so it is expensive to change later. `SignalsMapWidget::Type` is declared explicitly per row in `kSignalRows`, so the prefix is convention, not mechanism — but the convention is unbroken today and worth keeping. |
| **D4** | **Clearing the latched flags is unconditional; re-arming is not.** The acknowledge always clears `bTaskFault`/`nFaultCode`; it only reaches `ReadyForTrigger` if the runtime is valid, all roles are healthy, and pattern/calibration validate. | That is what makes it an *acknowledge* rather than a *repair*. If the cause persists, the next cycle re-raises the fault. The alternative — refusing to clear while unhealthy — leaves the PLC unable to distinguish "operator has seen this" from "still broken". |
| **D5** | **The retry budget is deleted, not set to infinity.** `maxRetries`, `canRetry()`, `LocalizationRecoveryAction::EscalateFault`, `raiseRoleFault()` and `buildRecoveryFaultMessage()` are removed outright. | Per the `AGENT.md` no-compat-shims rule. Deleting the enum value makes the compiler point at every site that assumed escalation existed, which is the point; a `maxRetries = INT_MAX` sentinel would leave dead branches that read as live policy. |
| **D6** | **A latched cycle fault self-clears after 2000 ms.** Added by the user on 2026-08-19. If no `bErrorReset` rising edge arrives within that window, the runtime performs the same acknowledge by itself. Scope is **cycle faults only** — setup-failure faults are excluded. | The line never halts on a transient fault, which is the point. Two consequences that must be designed for, not discovered: `bTaskFault` becomes a **2-second pulse** rather than a latched state (see R7), and `bErrorReset` is demoted from "the only way out" to "the fast way out" (see R8). Setup faults are excluded because auto-clearing an invalid pattern or calibration would produce a re-trigger loop against a cause that cannot resolve itself. |

## Assumptions (stated, not asked)

- **A-i.** The runtime shell runs **every** `TaskLocalization` in the loaded project, in
  project order, capped at 8. A project with more than 8 tasks shows the first 8 and
  logs which ones were dropped — no silent truncation.
- **A-ii.** The runtime shell shows **no** Commission affordance at all: no pattern
  editing, no device config, no manual trigger beyond what the dashboard already has.
  `LocalizationDashboardWidget` is already read-only and is reused as-is.
- **A-iii.** `ncr_runtime.exe` and `ncr_picking.exe` are never expected to hold the same
  project file open simultaneously. The project file is SQLite; concurrent access is not
  designed for and is not guarded against here.
- **A-iv.** Acknowledge has no UI affordance — it is a PLC input or the 2-second timer.
  See Risk R3.
- **A-v.** 2000 ms is a fixed constant, not a configurable setting. The user named the
  value; no second use case exists to justify exposing it. See Risk R8.

## Architecture Decisions

- **The new executable is a second shell over the same `src/` modules.** Nothing moves
  out of `src/`. `runtime_app/` sits beside `app/` at the same level: it may include
  everything, and nothing may include it. `app/` and `runtime_app/` must not include
  each other — two peers, not a hierarchy.
- **Shared qmake configuration is extracted, not copied.** `ncr_picking.pro` today
  carries the module includes, INCLUDEPATH, ADS/property-browser wiring, resources and
  deploy hooks. Copying that into a second `.pro` guarantees drift the first time a
  module is added. It moves to `qmake/app_common.pri`, included by both.
- **The runtime shell reuses `LocalizationDashboardWidget` directly, never
  `LocalizationTaskWidget`.** The task widget's `initWidget()` *starts Commission* on
  the task; reusing it would put the runtime app into commission on launch. The
  dashboard already takes `(task, ads::CDockWidget*, parent)` and refreshes purely from
  task signals.
- **Tile layout is a controller, not a widget.** ADS has floating and docking but no
  "tile into N" primitive. A `RuntimeLayoutController` translates a tile count into a
  fixed sequence of `addDockWidget()` placements. Keeping it separate from the shell
  window means the layout rules are testable and the window stays thin.
- **Recovery logging keeps full detail at DEV level and de-duplicates only USER level.**
  The request is about the operator's event log. Throwing away the per-attempt record
  entirely would make a flapping link undiagnosable after the fact.
- **No new abstraction for "shell".** `MainWindow` and `RuntimeShellWindow` share the
  `src/` modules and nothing else. Per `AGENT.md`, no common base class until a third
  shell proves the shape.

## Current State (verified against source)

| Entity | Location | Today |
|---|---|---|
| Signal properties | [task_localization_config.h:67-93](../../../src/model/task_localization_config.h#L67-L93) | 13 signals; `kSchemaVersion = 1` |
| Mapper field list | [localization_signal_mapper.cpp:15-29](../../../src/model/localization_signal_mapper.cpp#L15-L29) | 13 entries in `kSignalFields` |
| Settings row table | [localization_setting_widget.cpp:43-59](../../../src/ui/forms/task/localization_setting_widget.cpp#L43-L59) | 13 entries in `kSignalRows`, type declared per row |
| PLC edge handling | [localization_runtime_controller.cpp:344-392](../../../src/model/localization_runtime_controller.cpp#L344-L392) | Handles `nActiveCamera`, `nActivePatternGroup`, `bExecuteTrigger` only |
| Retry budget | [localization_recovery_policy.h:34-57](../../../src/model/localization_recovery_policy.h#L34-L57) | `maxRetries{10}`, `canRetry()`, `EscalateFault` |
| Escalation | [localization_runtime_controller.cpp:1076-1080](../../../src/model/localization_runtime_controller.cpp#L1076-L1080) | `EscalateFault` → `raiseRoleFault()` → `CycleState::Faulted` |
| Per-attempt logging | [localization_runtime_controller.cpp:1100-1105](../../../src/model/localization_runtime_controller.cpp#L1100-L1105), [1161-1165](../../../src/model/localization_runtime_controller.cpp#L1161-L1165) | Two `LOG_USER_WARN` per attempt, unconditional |
| `runtimeRecovering` emit | [localization_runtime_controller.cpp:1067-1071](../../../src/model/localization_runtime_controller.cpp#L1067-L1071) | Emitted on **every** retry |
| State machine | [task_state_machine.h:70-71](../../../src/model/task_state_machine.h#L70-L71) | `Faulted` → `Stopping` only |
| Grab retry | [camera_runner.h:283-312](../../../src/runtime/camera_runner.h#L283-L312) | Re-emits `sig_singleShot()` while `m_grabFailedCount <= 5` |
| Contract test | [main.cpp:1104-1148](../../../tests/architecture_contract_test/main.cpp#L1104-L1148), [1633-1676](../../../tests/architecture_contract_test/main.cpp#L1633-L1676) | Asserts `maxRetries == 10` and `EscalateFault`; module list is hard-coded |
| App entry | [main.cpp:27-63](../../../app/main.cpp#L27-L63) | Pylon init → translator → AppSettings → ThemeManager → `MainWindow` |
| Settings keys | [app_settings.h:33-38](../../../src/core/app_settings/app_settings.h#L33-L38) | 4 keys; no last-project key. `MainWindow::m_recentPaths` is **not** persisted |
| Root build | [ncr_picking.pro:12-66](../../../ncr_picking.pro#L12-L66) | Module includes + ADS + property browser + resources + deploy, all inline |

---

## Phase A — Fault acknowledge (`bErrorReset`) ✅ DONE 2026-08-20

**Outcome.** All seven tasks landed as planned; no design change was needed during
implementation. Two things worth recording:

- The auto-recovery timer's two arming points behaved exactly as reasoned. The contract
  test log shows `Fault auto-recovery armed. delayMs=2000` followed by
  `Fault auto-recovery cancelled.` in the acknowledge test, and armed → fired at exactly
  `+2000 ms` in the no-acknowledge test. The race between acknowledge and timer resolves
  in the acknowledge's favour, as designed.
- A7 grew beyond the planned unit assertions. The contract test already had a
  `LocalizationRuntimeFixture` with fake PLC/camera/vision-output devices, so both
  recovery paths are covered by **behavioural** tests that drive a real cycle to a
  `VisionOutputSendFailed` fault and then clear it — one via `bErrorReset`, one by
  waiting. The shared setup lives in `faultCycleAndReleaseTrigger()` so the two tests
  differ only in how the fault is cleared, which is the only thing under test.

**Verification:** root app `=== BUILD OK ===`; contract test **44 passed, 0 failed**
(3838 ms). Not verified: anything requiring hardware — see Checkpoint A.

### A1. Add the signal to the config

**Files:** `src/model/task_localization_config.h`

- Add `QString m_bErrorReset;` to `TaskLocalizeConfigPrivate` in the bool block.
- Add `P_PROPERTY_STRING_READWRITE(QString, bErrorReset, "Error reset")`.
- Serialize/deserialize `bErrorReset` alongside the other bools.
- **Bump `kSchemaVersion` 1 → 2.** A v1 project loads fine (missing key → empty tag →
  signal unmapped, which is correct). A v2 project on a v1 build is refused by the
  existing version gate rather than silently loading with the acknowledge unbound.

### A2. Register it in both signal tables

**Files:** `src/model/localization_signal_mapper.cpp`,
`src/ui/forms/task/localization_setting_widget.cpp`

- `kSignalFields`: append `"bErrorReset"`.
- `kSignalRows`: append `{ "bErrorReset", SignalsMapWidget::Type::Bool }` at the end of
  the bool block.

> Two separate tables must agree. `kSignalFields` drives PLC mapping; `kSignalRows`
> drives whether the operator can *see* the row to map it. A signal in one but not the
> other fails silently in opposite directions — the plan touches both in one task on
> purpose.

### A3. Rising-edge handling in the runtime controller

**Files:** `src/model/localization_runtime_controller.h/.cpp`

- Add `bool m_lastErrorReset{false};` beside `m_lastExecuteTrigger`.
- Add `void acknowledgeFault();` (private).
- In `handlePlcValues()`, add a branch for `bErrorReset` mirroring the `bExecuteTrigger`
  edge detection; call `acknowledgeFault()` on the rising edge only.
- `acknowledgeFault()`:
  1. Log at INFO via `appendTaskLog()` — the acknowledge is an operator action and
     belongs in the event log even when it changes nothing.
  2. `publishBoolSignal("bTaskFault", false)` and
     `publishNumberSignal("nFaultCode", None)`.
  3. Clear `m_pendingCycleResult.faulted`.
  4. If `m_cycleState` is `Faulted` or `Recovering`, call
     `markRuntimeReady("Fault acknowledged. Runtime ready.")`. That call already
     self-guards on `m_valid`, role health and pattern/calibration validity, so an
     acknowledge during a live outage clears the flags and leaves the state alone (D4).
- Do **not** act on the falling edge.

### A4. Allow the task to leave `Faulted`

**Files:** `src/model/task_state_machine.h`, `src/model/task_localization.cpp`

- `canTransitionTaskState()`: `Faulted` gains `→ Ready` (keeping `→ Stopping`).
- `TaskLocalization::onRuntimeReady()` currently refuses to act when the task is
  `Faulted`, `Stopping` or `Idle`. Drop `Faulted` from that guard; keep `Stopping` and
  `Idle` (a ready event must not resurrect a task being torn down).

> This is the load-bearing half of D2, and it is the half that is easy to skip: the PLC
> flags would clear, the dashboard lamp would stay red, and the runtime would look
> acknowledged to the robot and faulted to the operator.

### A5. Automatic fault recovery after 2 seconds

**Files:** `src/model/localization_runtime_controller.h/.cpp`

The safety net for D6: a cycle fault must never park the line indefinitely waiting for an
operator. `bErrorReset` stays the fast, explicit way out; this is the automatic one.

- Add `QTimer m_faultRecoverTimer;` (single-shot, parented to the controller) and
  `static constexpr int kFaultAutoRecoverMs = 2000;`.
- **Arm it at exactly the two points where the fault currently gets stuck**, not at the
  moment of failure:
  1. In `abortCycle()`, when the resulting state is `Recovering` (the execute trigger was
     already low);
  2. In `handlePlcValues()`, in the `bExecuteTrigger` falling-edge branch that today does
     `m_cycleState = CycleState::Recovering` when `m_pendingCycleResult.faulted`
     ([localization_runtime_controller.cpp:385](../../../src/model/localization_runtime_controller.cpp#L385)).

  Arming at `abortCycle()` unconditionally would re-arm the runtime while the PLC still
  holds the trigger high, which breaks the rising-edge handshake.
- On timeout, run the **same** recovery body as `acknowledgeFault()` — factor it into a
  shared private `recoverFromFault(const QString &reason)` so the two entry points cannot
  drift. The only difference is the log line: the automatic path records
  `"Fault auto-cleared after 2000 ms (no bErrorReset received)."` so the event log
  distinguishes an operator-confirmed fault from a self-cleared one.
- **Stop the timer** in `acknowledgeFault()` (an acknowledge that arrives first wins), in
  `startCycle()`, and in `resetRuntimeBindings()`. Use the `QTimer` member rather than
  `QTimer::singleShot` precisely so it is cancellable — the same lifetime reasoning as B4.
- **Excluded: setup-failure faults.** The `bTaskFault` publishes in `setup()`
  ([localization_runtime_controller.cpp:133](../../../src/model/localization_runtime_controller.cpp#L133),
  [155](../../../src/model/localization_runtime_controller.cpp#L155),
  [193](../../../src/model/localization_runtime_controller.cpp#L193),
  [280](../../../src/model/localization_runtime_controller.cpp#L280),
  [287](../../../src/model/localization_runtime_controller.cpp#L287)) do not arm the
  timer. Those causes cannot resolve on their own.
- **Fault-storm guard.** For `PatternInvalid` / `CalibrationInvalid` the existing
  validation inside `markRuntimeReady()` already refuses to re-arm, so a persistent
  configuration fault does not loop. For genuinely repeating transient faults
  (`CameraGrabTimeout`, `VisionOutputSendFailed`) the runtime *will* re-arm, the PLC will
  re-trigger, and it will fault again — which is the requested behaviour, but it must not
  bury the event log. Track consecutive auto-clears; log each one at DEV level and emit a
  single `LOG_USER_WARN` every `kAutoRecoverWarnStride` (proposed: 5) with the repeating
  fault code. Reset the counter on any successful cycle.

### A6. Documentation

**Files:** `docs/domains/task_localization/plc_signal_contract.md`,
`docs/domains/task_localization/task_localization.md`

- Signal table: add `bErrorReset | bool | PLC -> task | Rising-edge fault acknowledge.`
- New "Fault Acknowledge And Auto-Recovery" section under "Handshake Behavior": the edge
  semantics, what clears unconditionally, what is conditional, the 2000 ms automatic
  path, and the explicit statement that neither path repairs the cause.
- **State the sampling rule for the PLC, prominently.** Because `bTaskFault` now clears
  itself after 2 s, a PLC that polls slowly can miss the fault entirely. `bMatchingFinished`
  goes true on the faulted cycle and stays true until the trigger is reset, so it is the
  correct edge on which to sample `bTaskFault` and `nFaultCode`. A PLC program that wants
  a persistent fault record must latch it on its own side.
- Note the `kSchemaVersion` bump to 2 wherever the config schema is described.

### A7. Tests

**Files:** `tests/architecture_contract_test/main.cpp`

- Assert `canTransitionTaskState(Faulted, Ready)` is true and
  `canTransitionTaskState(Faulted, RunningCycle)` is still false.
- Assert `TaskLocalizeConfig::kSchemaVersion == 2` and that a round-trip
  `toJson()`/`fromJson()` preserves `bErrorReset`.
- Assert a v1 document (no `bErrorReset`, `version: 1`) still loads.

**Acceptance:**
- [x] `bErrorReset` appears in the settings signal-map table and can be bound to a PLC bit. *(code: `kSignalRows`; visual confirmation belongs to Checkpoint A)*
- [x] Rising edge clears `bTaskFault` and `nFaultCode` in every state. *(`recoverFromFault()` clears before any state check)*
- [x] After a cycle fault with all devices healthy, a rising edge returns the task to Ready and `bTaskReady` goes true. *(`test_localization_runtime_error_reset_clears_fault_and_rearms`)*
- [x] With `bErrorReset` unbound entirely, the same cycle fault returns the task to Ready ~2 s later on its own. *(`test_localization_runtime_fault_auto_clears_without_error_reset`)*
- [x] An acknowledge arriving inside the 2 s window cancels the timer — the recovery runs once, not twice. *(asserted with a 500 ms budget, well under `kFaultAutoRecoverMs`; the log shows the cancel)*
- [x] A `PatternInvalid` fault does **not** loop: the runtime stays not-ready instead of re-arming every 2 s. *(by construction — `markRuntimeReady()` validates the pattern group; **not** covered by a dedicated test)*
- [x] A v1 project file loads without error. *(`test_error_reset_signal_round_trips_and_v1_documents_still_load`, which also checks a v3 document is still refused)*
- [x] Contract test passes. *(44 passed, 0 failed)*

---

## Phase B — Unlimited reconnect + quiet recovery log

### B1. Delete the retry budget

**Files:** `src/model/localization_recovery_policy.h`

- Remove `maxRetries`, `canRetry()`, and `LocalizationRecoveryAction::EscalateFault`.
- `decideRecoveryAction()` returns only `Ignore` or `RetryScheduled`.
- Remove `buildRecoveryFaultMessage()` — its only caller disappears in B2.
- Update the file-level and enum doc comments: the policy is now "how often to retry",
  never "how many times".

### B2. Remove the escalation path

**Files:** `src/model/localization_runtime_controller.h/.cpp`

- Delete `raiseRoleFault()` and its declaration.
- Delete the `EscalateFault` branch in `handleRoleStatusChanged()`.
- Delete `RoleRecoveryContext::faultRaised` (only escalation read it).
- `CycleState::Faulted` remains reachable from `abortCycle()` (cycle-level faults) and
  from setup failure — only the *recovery* path stops producing it.

> Keep `LocalizationFaultCode::CameraConnectFailed`, `CameraLost`, `PlcLost` and
> `VisionOutputLost`. They are still published by `abortCycle()` when a role drops
> mid-cycle; only the "retries exhausted" producer goes away. Do not renumber.

### B2b. Withdraw readiness when an outage starts (added during implementation)

**Files:** `src/model/localization_runtime_controller.cpp`

Not in the original plan; found by the B2 test failing. On the first retry of an outage,
if the runtime is `ReadyForTrigger`, move to `Recovering` and publish
`bTaskReady = false`. Before this, losing a role outside a cycle left both untouched, and
only the (now deleted) escalation eventually told the PLC anything was wrong.

Without it the PLC would keep triggering a runtime whose camera is gone: `startCycle()`
would run against a disconnected camera and fault on the grab timeout, and with A5 in
place that loop would repeat every couple of seconds indefinitely.

### B7. Checkpoint B bench findings (2026-08-20)

Checkpoint B confirmed the camera no longer faults — and surfaced five defects that the
old escalation path had been masking. Four of them share one root: **a device swallowed a
failure without publishing a status or a result**, so nothing downstream could react.

| # | Report | Root cause | Fix |
|---|---|---|---|
| 1 | Camera runner wedged on `CameraSingleShot` after a cable pull; could not process reconnect | `BaslerGigECamera::grabSingleShot()` had **four** exit paths that never emitted `grabFinished` (null instance, already-grabbing, and the `catch` block that swallowed the Pylon exception). `CameraRunner` resolves its in-flight command from that signal only. `GrabOne()` also throws with acquisition still started, so every later grab hit the already-grabbing branch and returned silently too. | Emit `grabFinished` on every path; `StopGrabbing()` in the catch and recover from a stuck acquisition instead of refusing forever. |
| 2 | Camera loss produced no fault **and** no recovery | The camera has no removal callback and no heartbeat, and `grabSingleShot()` never touched connection status — so a pulled cable changed nothing the runtime could observe. | `publishRemovalIfDetected()`: on a grab exception, check `IsCameraDeviceRemoved()`, close the instance and publish `LostConnected`. |
| 3 | PLC lost, re-plugged, never reconnected; task stayed Ready | After 5 failed sends the MC device called `deviceDisconnect()`, publishing **`Disconnected`**. That status is deliberately *not* recoverable (it means "an operator asked for this"), so `decideRecoveryAction()` returned `Ignore`: no retry was ever scheduled and `bTaskReady` was never withdrawn. | Publish `LostConnected` via `setDeviceLostConnect()`, and reset the per-session retry budget with the connection. Also publish `ConnectFailed` when `initialize_mc_device()` fails — that path returned false silently and permanently wedged `PlcRunner::m_busy`. |
| 4 | Entering recovery never appeared in the dashboard task log | Recovery only emitted `runtimeRecovering` and wrote the app log; nothing called `appendTaskLog()`. | Log connection loss once per outage and the reconnect once per recovery, both with role, device id and status. |
| 5 | Device errors never appeared in the dashboard task log | `onCameraError` / `onPrimaryPlcError` / `onVisionOutputError` only called `LOG_USER_WARN`. | New `reportRoleError()` writes both logs, suppressing an immediate repeat of the same message per role — a dead PLC fails every queued write, so forwarding each one would bury the log. |

**On the requested runner watchdog.** The user asked whether adding one is reasonable. A
watchdog already existed (`m_activeCommandTimer`) and already fired; adding another would
have masked the real defect, and no runner-side timer can unblock a device thread sitting
inside a Pylon call. What the watchdog *did* have were two genuine bugs, now fixed:

- its default was **3000 ms while the camera's own blocking grab waits 5000 ms**, so it
  always gave up before the camera could answer — a slow-but-successful grab was reported
  `TimedOut` and the real `grabFinished` then arrived with no command to resolve. Now
  `CameraRunner::kSingleShotTimeoutMs = 8000`, with
  `test_camera_runner_watchdog_outlasts_device_grab_timeout` locking the ordering;
- it was started once per command and never restarted per retry, so the whole retry chain
  shared one timeout (this is Phase C defect C2.1, brought forward).

Also fixed while here: `setGrabTimeout()` wrote `m_grab_timeout` and **nothing ever read
it** — the grab hardcoded 5000. The member is now the value actually used.

**Still hardware-owned:** a camera removed while the runtime is idle is not noticed until
the next grab, because there is no heartbeat. The next trigger surfaces it. Adding a Pylon
heartbeat/removal callback is the follow-up if idle detection turns out to matter.

### B3. De-duplicate recovery logging

**Files:** `src/model/localization_runtime_controller.h/.cpp`

- `RoleRecoveryContext` gains `vc::device::ConnectStatus lastLoggedStatus` and
  `QDateTime outageStartedAt`.
- `emit runtimeRecovering(...)` **only on the first attempt of an outage**
  (`retryCount == 1`) or when the status changes to a different unhealthy status.
  Consecutive identical failures produce no further signal, so the dashboard event log
  gets one "recovering" line per outage instead of one every 5 seconds.
- `scheduleRoleReconnect()` / `requestRoleConnectNow()`: keep `LOG_USER_WARN` for
  attempt 1 and thereafter every `kQuietRetryLogStride` attempts (proposed: 12 ≈ once a
  minute at the default 5000 ms interval); every other attempt drops to `LOG_DEV_INFO`.
  Nothing is lost from the developer log.
- On recovery, `handleRoleStatusChanged()` already logs a "recovered" message; extend it
  to carry attempt count **and elapsed outage duration**, which is the number an operator
  actually wants after the fact.

### B4. Verify retries stop at `endRuntime()`

**Files:** `src/model/localization_runtime_controller.cpp` (verification, likely no change)

An unbounded retry loop that outlives its runtime session is the main new hazard in this
phase. `scheduleRoleReconnect()` uses `QTimer::singleShot(interval, this, lambda)`, so
the pending callback dies with the controller, and `TaskLocalization::endRuntime()`
destroys and recreates the controller. **Confirm this by inspection and by the manual
check in Checkpoint B**, and add a comment at the `singleShot` call recording *why* the
`this` context argument is required rather than incidental — a future edit to a
context-free `QTimer::singleShot` would reintroduce exactly this bug.

### B5. Update tests

**Files:** `tests/architecture_contract_test/main.cpp`

- Rewrite the policy test at ~1104-1148: drop the `maxRetries == 10` assertions and the
  `EscalateFault` case; assert instead that a high retry count still yields
  `RetryScheduled`, and that `retryAlreadyScheduled` still yields `Ignore`.
- Remove the `buildRecoveryFaultMessage()` test at ~1153.

### B6. Documentation

**Files:** `docs/domains/task_localization/plc_signal_contract.md`,
`docs/domains/task_localization/task_localization.md`,
`docs/domains/task_localization/maintenance_and_extension.md`,
`docs/domains/task_localization/runtime_controller_api.md`

- Rewrite "Recovery Behavior": retry is unlimited until the runtime ends; a lost role
  never escalates to a task fault; recovery returns to Ready automatically.
- State the operator-visible consequence plainly: **a device that never comes back leaves
  the task in Recovering indefinitely, and the PLC sees `bTaskReady = false` with
  `bTaskFault = false`.** That is the intended trade — but a PLC program that waits for
  `bTaskFault` to detect a dead link will now wait forever. This is the single most
  important line in the Phase B docs.
- Document the log de-duplication rule so nobody "fixes" the missing lines later.

**Acceptance:**
- [x] Unplugging a camera for longer than 10 × 5 s no longer faults the task. *(covered by `test_localization_role_outage_retries_forever_without_faulting`, which runs ~50 attempts at a compressed interval and asserts `runtimeFault` count 0 and `bTaskFault` false)*
- [x] Re-plugging returns the task to Ready with no operator action. *(same test, final leg)*
- [x] The event log shows one recovering line per outage, not one per attempt. *(same test asserts ≤ 2 `runtimeRecovering` notices — one per distinct unhealthy status — across ~50 attempts)*
- [x] An outage withdraws `bTaskReady`, so the PLC stops triggering into a dead runtime. *(B2b)*
- [ ] Ending runtime during an outage stops the retries. **Verified by inspection only** — the pending retry is a `QTimer::singleShot` bound to the controller as context, and `TaskLocalization::endRuntime()` destroys the controller. A comment at the call site records that the context argument is load-bearing. Hardware check belongs to Checkpoint B.
- [x] Contract test passes. *(44 passed, 0 failed)*

---

## Phase C — Camera grab-failure retry

The request asked only for documentation here, on the basis that the behaviour was
already shipped. Auditing it in order to write that documentation showed the retry chain
mostly does not execute (C2.1), so documenting it as working would have been inaccurate.
The user brought the fix into this phase on 2026-08-19; **C2 is now in scope, and C1's
documentation describes the fixed behaviour.**

### C2 lands before C1

Ordering matters within this phase only: writing the docs first would mean writing them
twice. Fix, then describe what was fixed.

> **C2 landed early, inside B7 (2026-08-20).** All three defects were on the direct path
> of the Checkpoint B camera bug — the wedged runner could not be fixed without them — so
> they were taken there rather than left for a later phase. C1 below documents the fixed
> behaviour and is the only work that remained when Phase C was opened.

### C1. Document the behaviour

**Files:** `docs/domains/task_localization/runtime_controller_api.md`,
`docs/domains/task_localization/task_localization.md`,
`docs/domains/task_localization/plc_signal_contract.md`

`CameraRunner::onGrabFinished()` re-emits `sig_singleShot()` on a failed grab instead of
failing the command, so a transient grab failure does not interrupt the automatic cycle.
Document:

- where the retry lives (the **runner**, not the controller — the controller never sees
  the intermediate failures, by design);
- that `grabFinished` is deliberately not re-emitted on a retry, so the cycle observes
  one outcome;
- that fault code 102 `CameraGrabTimeout` now means "the retry budget was also
  exhausted", which changes what that code tells the operator;
- the counter's reset rule.

### C2. Fix the three defects found while auditing

**Files:** `src/runtime/camera_runner.h`

1. **The retry chain shares one timeout.** `m_activeCommandTimer` is started once in
   `runCommand()` and is **not** restarted per retry. With the default 3000 ms and a grab
   that fails slowly, `onActiveCommandTimedOut()` fires mid-chain and the later retries
   never happen. The documented "up to 5 retries" would be fiction under exactly the
   conditions retries exist for. Fix: restart the timer when re-emitting
   `sig_singleShot()`.
2. **The counter is never reset on command start.** `m_grabFailedCount` resets only on
   success, so failures during commissioning carry into the next runtime cycle and can
   consume the budget before it starts. Fix: reset it in `runCommand()` for
   `CameraSingleShot`.
3. **`> 5` is a bare literal meaning six attempts.** Replace with a named
   `kMaxGrabRetries` constant and make the off-by-one explicit in its doc comment.

**Interaction with A5.** These two changes compound in the right direction and it is
worth knowing why. A grab failure that survives the retry budget aborts the cycle with
`CameraGrabTimeout`; A5 then re-arms the runtime 2 s later and the PLC re-triggers. So a
camera with an intermittent fault now recovers at two levels — silently inside the runner
first, visibly through a faulted cycle second — without an operator ever touching it.
That is the intent, and it is also why A5's fault-storm guard matters: with C2 fixed,
reaching fault code 102 at all means six consecutive grab failures, and a repeating 102
is a real hardware problem the log must not hide.

**Acceptance:**
- [x] A slow-failing grab retries the full budget instead of timing out mid-chain. *(B7: watchdog restarted per attempt, and `kSingleShotTimeoutMs` raised above the device's grab timeout — the ordering is now locked by `test_camera_runner_watchdog_outlasts_device_grab_timeout`)*
- [x] A grab failure during commissioning does not consume the next runtime cycle's budget. *(B7: the counter resets in `runCommand()` when a `CameraSingleShot` starts)*
- [x] Docs describe the retry, its owner, and its effect on fault code 102. *(C1: `CameraRunner` class/slot doc comments, a new "Camera Grab Retry" section in `runtime_controller_api.md` and `task_localization.md`, the fault-code rows in both contract tables, and the `CameraRunner` note in `uml/03_runtime_threading.puml`)*
- [x] Docs state what the runner cannot do — the watchdog ends the command, never a blocked device call — so the next reader does not reach for another watchdog. *(added after the bench asked exactly that question)*
- [x] `maintenance_and_extension.md` testing guidance covers the new behaviours, plus the two invariant classes that unit tests missed on the bench.

---

## Phase D — `ncr_runtime.exe`

### D1. Build split

**Files:** `qmake/app_common.pri` (new), `ncr_picking.pro`,
`runtime_app/ncr_runtime.pro` (new), `runtime_app/runtime_app.pri` (new),
`runtime_app/AGENTS.md` (new), `AGENT.md`, `app/AGENTS.md`, `docs/README.md`,
`docs/rules/build_and_verification.md`, `docs/product/phase4_product_release_plan.md`

- Extract from `ncr_picking.pro` into `qmake/app_common.pri`: QT modules, `CONFIG`,
  `INCLUDEPATH`, the seven `src/*.pri` includes, RobotKinematics, ADS libs/includes,
  the property-browser vendor include, `RESOURCES`, `local_dependencies.pri`,
  `deploy_dependencies.pri`.
- `ncr_picking.pro` keeps `include(qmake/app_common.pri)`, `include(app/app.pri)`,
  `RC_ICONS`, and its install rules.
- `runtime_app/ncr_runtime.pro` mirrors that with `include(runtime_app/runtime_app.pri)`
  and `TARGET = ncr_runtime`. Paths inside `app_common.pri` must be `$$PWD`-relative to
  survive being included from a subdirectory.
- Build output: `runtime_app/build/<build-name>`, per the existing rule that root
  `build\` is reserved for the root application.
- **Amend the invariants:** `AGENT.md` "Current Release Decision" (the "no separate
  runtime executable" bullet), `app/AGENTS.md` invariant #1, and the Phase 4 release-shape
  section — each gains a dated note that Phase 6 superseded it by user decision, with a
  pointer here. Do not delete the original text; the reasoning behind it is still useful.

### D2. Architecture contract test

**Files:** `tests/architecture_contract_test/main.cpp`

The layering test hard-codes `modules` and maps `"app"` to `repoRoot/app`. A new
top-level folder is **not scanned at all** — it would pass by being invisible. Add
`runtime_app` as a scanned module with `allowed = all`, and assert that `app/` contains
no include of `runtime_app/…` and vice versa.

### D3. Persist the last project path

**Files:** `src/core/app_settings/app_settings.h/.cpp`

Follow the "Extending" recipe documented in the header: add
`AppKey::lastRuntimeProjectPath`, a constructor default, and a typed
`lastRuntimeProjectPath()` / `setLastRuntimeProjectPath()` pair. A dedicated key, not a
reuse of `MainWindow`'s recent list — that list is in-memory only and is not persisted.

### D4. Runtime shell window

**Files:** `runtime_app/main.cpp`, `runtime_app/runtime_shell_window.h/.cpp/.ui` (new)

- `main.cpp` mirrors `app/main.cpp`: Pylon init, translator, `AppSettings`,
  `ThemeManager`, then `RuntimeShellWindow`.
- `RuntimeShellWindow` hosts a `QStackedWidget`:
  - page 0 **Project select** — last-used path, browse button, and the load error when
    one occurred;
  - page 1 **Runtime view** — the ADS dock manager with one dashboard per task.
- Startup: read `lastRuntimeProjectPath()`; if non-empty **and** `QFile::exists()` **and**
  `ProjectRepository::load()` succeeds → page 1. Otherwise page 0 with the reason shown.
  Three separate failure modes, three distinguishable messages — "no project" and
  "project is corrupt" must not look the same to an operator at 6 a.m.
- On a successful manual open, persist the path.

### D5. Auto-enter runtime

**Files:** `runtime_app/runtime_shell_window.cpp`

- For each `TaskLocalization` (project order, first 8): create a `CDockWidget`, put a
  `LocalizationDashboardWidget` in it, then call `task->beginRuntime()`.
- No commission entry anywhere. Explicitly **not** `LocalizationTaskWidget` — see the
  architecture decision above.
- If `beginRuntime()`/`setupTask()` fails the task goes Faulted; the dashboard shows it.
  Do not swallow the failure or hide the tile.
- If the project has more than 8 tasks, log which were dropped and show it in the shell,
  per assumption A-i.

### D6. Dock / float / tile layout

**Files:** `runtime_app/runtime_layout_controller.h/.cpp` (new)

- Tile counts 1, 2, 4, 6, 8 → grids 1×1, 1×2, 2×2, 2×3, 2×4. Chosen from a toolbar
  combo; default is the smallest grid that fits the task count.
- Implemented as a sequence of `CDockManager::addDockWidget(area, dock, targetArea)`
  placements; ADS provides floating natively, so float/dock needs no extra code.
- A floated task keeps running — floating is a view operation, never a lifecycle one.

### D7. Documentation and UML

**Files:** `docs/domains/runtime_app/runtime_shell.md` (new), `docs/README.md`,
`uml/11_runtime_shell.puml` (new), `uml/06_ui_widgets.puml`

- New domain doc: startup decision tree, task lifecycle in the shell, layout rules, the
  8-task cap, and the explicit statement that the runtime shell never writes to the
  project file.
- New UML for the shell; `06_ui_widgets.puml` gains the reuse edge from
  `RuntimeShellWindow` to `LocalizationDashboardWidget`.
- `docs/README.md` gains rows for the new domain doc and the new module scope card.

### D8. Contract-test blind spot found during D2 (2026-08-20)

Adding `runtime_app` to the layering scan made it fail immediately — on
`runtime_app/build/.../moc_*.cpp`, whose generated `"../../../../src/..."` includes are
correct for the generator and meaningless to this contract.

The scan had never met a build directory before: `src/` modules have none, and `app/`
builds to the repo root. Shells build next to their own `.pro`, so their output lands
inside the scanned tree. Fixed by skipping any path containing `/build/`.

Also added, because the original gap was that a missing directory scanned as zero files:
the loop now asserts each listed module directory **exists**. A typo in the module list
used to mean "silently scan nothing", which passes.

**Acceptance:**
- [x] `ncr_runtime.exe` builds from `runtime_app/ncr_runtime.pro` and `ncr_picking.exe` still builds unchanged. *(both link; the qmake split was verified against the existing app before any new code was written)*
- [ ] First launch with no saved path shows the project-select page. **Manual.**
- [ ] Second launch loads the previous project and goes straight to the runtime view. **Manual.**
- [ ] A moved/deleted project file shows the select page with a distinguishable message. **Manual** — three separate messages, worth checking all three read differently.
- [ ] Every task enters runtime without a button press. **Manual.**
- [ ] Layouts 1/2/4/6/8 arrange correctly; a floated task keeps running. **Manual.**
- [x] Contract test passes with `runtime_app` scanned. *(46 passed, 0 failed — including `test_runtime_shell_is_a_peer_of_the_app_shell`, which asserts both shells carry a scope card and both `.pro` files include the shared qmake config)*

---

## Phase E — Field deployment and startup

Added 2026-08-20 from the two-executable packaging review. Phase D produced a second
executable; this phase makes shipping and running it on a field machine safe.

**The organising distinction:** *build layout* and *deploy layout* are different questions.
Two qmake projects must build into separate directories — they collide on `Makefile`,
`.qmake.stash`, `ui_*.h` and the `moc_*.cpp` generated from the same shared `src/` headers
— but they must **deploy into one folder**, because the runtime set beside
`ncr_picking.exe` is 38 DLLs plus `robot_assets/`, and two copies of that can drift to
different Qt or Pylon versions. A drift that only manifests on the customer's machine.

`dist/` is the boundary between the two.

### Dependency graph

```text
E1 single-instance guard ──────────────► E5 auto-start guidance
                                          (a guard must exist before boot-start ships)
E2 build-path alignment ───► E3 dist/ staging ◄─── E4 version stamp
                                          (staging needs settled paths and something
                                           to stamp the manifest with)
```

E1 is sequenced first despite being independent: it is the highest-risk gap, and a
fail-fast order puts the risky item where there is still room to react.

---

### Task E1: Single-instance guard in both shells

**Description.** Auto-start at boot **plus** an operator double-clicking the desktop icon
gives two `ncr_runtime.exe` processes competing for the same Basler camera, the same MC
PLC socket, and the same vision-output TCP port. The symptoms — grab failures, "device
removed", a port that will not bind — look exactly like the hardware faults fixed in B7,
which is what makes this expensive: it costs a bench session before anyone suspects a
second process. A named-mutex / `QLockFile` guard in both shells; a second launch raises
the running window instead of starting.

**Acceptance criteria:**
- [x] A second launch of `ncr_runtime.exe` does not start a second runtime. *(`SingleInstanceGuard` acquired before any device is touched; a losing launch returns 0 — not an error, the application the user asked for is already up)*
- [x] The already-running window is raised and focused instead. *(`QLocalServer` raise channel; on Windows the losing process first calls `AllowSetForegroundWindow()` with the PID from the lock file — without it Windows only flashes the taskbar button, which reads as "clicking the icon did nothing")*
- [x] The same guard covers `ncr_picking.exe` (its device ownership is identical). *(separate key, so the two shells do not block each other)*
- [x] A stale lock left by a crash does not block the next launch. *(inherited from `QLockFile`: a lock whose recorded PID is no longer running is reclaimed)*
- [x] Losing the raise channel does not prevent startup. *(added during implementation — refusing to launch the runtime because a named pipe could not be created would trade a small convenience for the whole application)*

**Verification:**
- [x] Contract test passes. *(49 passed, 0 failed — three new cases: second acquire blocked, independent keys, key reusable after the owner is destroyed)*
- [ ] Manual: launch each exe twice; confirm one process in Task Manager and the existing
      window comes forward. **Pending user check** — the foreground handover is the part
      only a real desktop session exercises.
- [ ] Manual: kill the process, relaunch, confirm it starts (stale-lock recovery).
      **Pending user check.**

**Outcome notes.**
- The guard lives in `src/core/utils/` because both shells need it; `core` is level 0 and
  a platform API is not a module dependency, so no layering rule is bent.
- `core.pri` now links `user32`. The contract test `.pro` lists src sources individually
  instead of including the module `.pri` files, so it declares the same dependency
  separately — recorded in `src/core/AGENTS.md` because that duplication is exactly the
  kind that drifts silently.
- **Deliberately not done:** mutual exclusion *between* the two shells. Each is
  single-instance of itself. Stopping the editor and the runtime from co-running needs an
  ordered device hand-off, not a refusal to start — see the deferred mode-switch section.
  Until it exists, the docs say plainly: do not run both against the same hardware.

**Dependencies:** None.

**Files likely touched:** `src/core/` (a small shared helper — both shells need it, so it
belongs in `core`, not in either shell), `app/main.cpp`, `runtime_app/main.cpp`,
`tests/architecture_contract_test/main.cpp`.

**Estimated scope:** Small (3–4 files).

---

### Task E2: Align build-output paths with the configured layout

**Description.** Phase D documented the runtime building to `runtime_app/build/`, but the
project is actually configured to build to `build/runtime_build/Release`. Both are
defensible; what is not defensible is the docs describing one and the toolchain doing the
other. Adopt the configured layout — gathering all build output under `build/` is easier
to gitignore and to clean — and correct every place that states the rule.

**Acceptance criteria:**
- [x] `AGENT.md`, `docs/rules/build_and_verification.md`, `runtime_app/AGENTS.md` and `runtime_app/ncr_runtime.pro` all state `build/runtime_build/<build-name>`.
- [x] The reservation rule is amended rather than contradicted: "one subfolder per application shell under `build/`", with the *reason* kept — two shells sharing a build directory collide on `Makefile`, `.qmake.stash` and the generated moc/ui files.
- [x] Both shells build from a clean directory. *(the runtime was rebuilt from scratch at the new location)*

**Verification:**
- [x] Both shells build.
- [x] Contract test passes.

**Outcome note.** The stale `runtime_app/build/` tree from Phase D is now unused. It is
left in place — removing it is a filesystem deletion and needs the owner's say-so.

**Dependencies:** None.

**Files likely touched:** `AGENT.md`, `docs/rules/build_and_verification.md`,
`runtime_app/AGENTS.md`, `docs/history/plan/phase_6_implementation_plan.md` (D1's stated
path).

**Estimated scope:** XS (docs only).

---

### Task E3: `dist/` staging step — one install image, one runtime set

**Description.** `CONFIG+=deploy_deps` copies the full DLL set next to **each** built
binary. That is fine as a developer convenience and wrong as a shipping strategy: it is
the "two deployment manifests" cost recorded in R4. Add a staging step that assembles both
executables plus **one** shared runtime set into `dist/`, matching the field install
layout exactly.

The file list this step produces **is** the installer manifest that
`docs/backlog/later_todo_list.md` #27 already calls for — so this is the first half of
planned work, not new work.

**Acceptance criteria:**
- [x] `dist/` contains both exes, one copy of the Qt/OpenCV/Pylon/ADS/RobotKinematics runtime, `robot_assets/`, and the Qt plugin folders. *(measured: 2 shells, 38 DLLs, 10 folders, ~218 MB)*
- [x] No DLL appears twice in `dist/`. *(0 duplicates)*
- [x] No build intermediates leak into the image. *(0 `.obj`/`.cpp`/`.moc`/`Makefile*`)*
- [x] The staging list is written down as the manifest, not left implicit in a script. *(`docs/product/install_image.md`)*
- [x] `ncr_runtime.exe` runs from `dist/` without the dependency directories on `PATH`. *(2026-08-23 — see below)*

**Verification:**
- [x] `ncr_picking.exe` starts and stays up from `dist/` with Qt, OpenCV and Basler stripped from `PATH`.
- [x] `ncr_runtime.exe` the same way. *(2026-08-23, during E7b. **The reason this was deferred turned out to be conditional, not absolute:** it reaches for hardware only once a project has been remembered. `%APPDATA%\NCRN Pick Runtime\settings.dat` did not exist, so `lastRuntimeProjectPath` was empty, the shell stopped on the project-select page and touched no device. On a machine that has already run a project the original caution still applies — check for that file before treating this as a harmless smoke test.)*
- [ ] Manual: confirm the layout matches what the field machine will have.

**Outcome notes.**
- The script excludes intermediates **by extension** rather than listing what to keep, so a
  newly added DLL family lands in the image automatically instead of being silently absent
  — the failure mode of a keep-list is a missing file nobody notices until the customer's
  machine.
- `OPENCV_BIN` and `PYLON_RUNTIME_DIR` must be set when building, or `deploy_deps` skips
  those families with only a warning and the image comes out incomplete. Recorded in
  `build_and_verification.md`.
- `vc_redist.x64.exe` rides along when the build directory has it (Basler ships one).
  Useful — an installer can chain it — but it arrived by the copy rule rather than by
  design, so the manifest says so rather than implying it was planned.
- `dist/` is not in `.gitignore`; the repository has none at all. Worth one when the
  packaging track resumes.

**Dependencies:** E2 (needs the settled build paths), E4 (the manifest records a version).

**Files likely touched:** `qmake/` (a staging include or script),
`docs/product/customer_installer_packaging.md`, `docs/backlog/later_todo_list.md` (#27
status).

**Estimated scope:** Medium.

> **Deliberately not doing:** a full installer (MSI/NSIS), GenTL producer deployment, or
> `GENICAM_GENTL64_PATH` setup. Those stay in #27. `dist/` is the input an installer
> consumes, and having it lets the installer be written later without re-deriving what
> ships.

---

### Task E4: Shared version stamp across both executables

**Description.** Two executables in one folder must come from the same build. The risk is
concrete now that `kSchemaVersion` is 2: a v2 editor writing a project that a v1 runtime
refuses to load is exactly the drift-in-one-folder scenario, and the field symptom is
"runtime will not open the project the editor just saved". The schema gate already refuses
loudly, so nothing corrupts — but the diagnosis should be one glance, not a log hunt.

One version constant, compiled into both shells, logged at startup and shown in the
runtime's project-select page.

**Acceptance criteria:**
- [x] Both exes report the same version string at startup.
- [x] The version is defined in exactly one place. *(`qmake/version.pri`)*
- [x] `ncr_runtime.exe` shows its version on the project-select page — the page an operator already reaches when something is wrong.

**Verification:**
- [x] Contract test: both shells include `core/app_version.h` and call `setApplicationVersion`. *(50 passed, 0 failed)*
- [x] Both executables report `0.6.0.0` in their Windows file resource.
- [ ] Manual: launch both, confirm identical version strings in the log. **Pending user check.**

**Outcome note — the first attempt was a check that could not fail.** `app_version.h`
initially hardcoded the version, which satisfied the acceptance criteria but left the
executables' file resource at `0.0.0.0`. The staging script read that resource to compare
the pair, so it printed a comparison that would have matched forever. Reworked so
`qmake/version.pri` is the single source feeding **both** the file resource and the
compiled-in string, and the script now *fails* on `0.0.0.0` rather than reporting it.

Builds outside `app_common.pri` (only the contract test) fall back to `0.0.0-dev` — marked
unreleased on purpose, because a stale but believable number is worse than an obviously
fake one.

**Dependencies:** None (but E3 records it in the manifest, so land this first).

**Files likely touched:** `src/core/` (version header), `app/main.cpp`,
`runtime_app/main.cpp`, `runtime_app/runtime_shell_window.cpp`.

**Estimated scope:** Small.

---

### Task E5: Field startup guidance — Task Scheduler, not the Run key

**Description.** Document how the runtime is set to start with Windows, and why the
obvious choice is the wrong one. `Run` registry key and the Startup folder both fire the
instant the desktop appears; a GigE camera needs the NIC up and Pylon device enumeration
ready. Task Scheduler can delay the start, restart on failure, and declare its privilege
level explicitly.

Two things worth recording because they are non-obvious:

- **Phase B made auto-start materially safer.** With unlimited reconnect, a runtime that
  starts before the network is up now recovers by itself. Before Phase B it would have
  exhausted 10 retries and faulted during boot, every boot.
- **The operator must never be stuck.** Auto-start means the runtime owns the screen. This
  is already handled — a missing or corrupt project falls to the project-select page
  rather than dying — but it is a property to protect, not an accident.

**Acceptance criteria:**
- [x] The recommended mechanism, delay rationale, and restart policy are documented. *(a settings table in `runtime_shell.md` → "Starting With Windows", with the reason beside each value)*
- [x] The operator's route out of a bad project is documented as a supported path. *(and promoted to an invariant in `runtime_app/AGENTS.md`, so the next change to startup has to preserve it)*
- [x] Single-instance interaction is called out: auto-start plus a manual launch is the normal case, not an edge case.
- [x] The two silent-failure traps are named where an installer author will meet them. *(added during writing — see below)*

**Verification:**
- [ ] Manual: configure the scheduled task on a test machine, reboot, confirm the runtime
      starts once and reaches the runtime view. **Pending user check** — this is the whole
      task's verification; nothing here can be proven from the repository.

**Outcome notes — two traps that fail silently, which is why they are stated loudly.**

1. **"Run whether user is logged on or not" makes the application invisible.** It places
   the process in a session with no desktop: the runtime starts, takes the camera and the
   PLC socket, and displays nothing. The station looks dead while it is working. This is
   the most likely way to get boot-start wrong.
2. **The scheduled task must run as the interactive account.** `SingleInstanceGuard` is
   per-user, so a task under a service account cannot see the operator's instance and
   **both** start — directly into the conflict E1 exists to prevent. E1's per-user scope
   note became an operational requirement here.

**A Phase B consequence worth recording.** The delay in the trigger is no longer a
correctness requirement. With unlimited reconnect, a runtime that starts before the network
is up recovers by itself; before Phase B it would have exhausted its retry budget and
faulted on *every* boot. The delay now only buys a clean event log — stated as such rather
than left looking load-bearing, so nobody treats a missing delay as a fault later.

**Dependencies:** E1 (auto-start without the guard is the trap this phase exists to close).

**Files likely touched:** `docs/domains/runtime_app/runtime_shell.md`,
`docs/product/customer_installer_packaging.md`.

**Estimated scope:** Small (docs only).

---

---

### Task E6: Build both shells in one pass (added 2026-08-21)

**Description.** With two shells over one `src/`, a change to shared code can leave one
rebuilt and the other stale — and the stale one still launches, so the mismatch surfaces
as behaviour rather than as a build error. Add an umbrella `TEMPLATE = subdirs` project
that drives both, usable from Qt Creator and the CLI, with a single canonical build
directory so the IDE and the command line share one incremental state instead of redoing
each other's work.

**Acceptance criteria:**
- [x] `ncr_picking_all.pro` builds both shells in one invocation.
- [x] Both existing `.pro` files still build standalone, unchanged.
- [x] Canonical build directory is under the repo-root `build/` — `build/all/<config>` — and documented for both CLI and Qt Creator.
- [x] The contract test asserts the umbrella lists every shell.

**Verification:**
- [x] Probed first: confirmed qmake gives each subproject a uniquely named makefile (`Makefile.ncr_picking`, `runtime_app/Makefile.ncr_runtime`) rather than colliding with the umbrella's own `Makefile` — the root-level `.pro` sharing a directory with the umbrella was the specific risk.
- [x] Full umbrella build produces both executables from one `nmake`, both stamped 0.6.0.0.
- [x] `dist/` stages from the umbrella output: 2 shells, 38 DLLs, 0 leaked intermediates.
- [x] Contract test passes. *(50 passed, 0 failed)*
- [ ] Manual: point Qt Creator at `build/all/Release` and confirm one Build produces both.
      **Pending user check** — this is the half of the task only the IDE exercises.

> **The probe was right and I then broke it.** After proving relative `.file` paths
> produce distinct makefile names, I "tidied" them to `$$PWD/ncr_picking.pro`. With an
> absolute path qmake resolves the editor subproject's makefile to plain `Makefile` — the
> umbrella's own name — so the umbrella invoked itself: infinite recursion, no build,
> ~1.3 MB of identical `NMAKE : fatal error U1077` in the log.
>
> Same class of mistake as the Phase 5 gizmo knob: validate a thing, then change the thing
> without re-validating. The `.pro` now carries a comment saying the paths must stay
> relative and what breaks otherwise, because "why is this not `$$PWD`" is exactly the
> question the next reader will ask.

**Dependencies:** E2 (build-path rule).

**Files touched:** `ncr_picking_all.pro` (new), `AGENT.md`,
`docs/rules/build_and_verification.md`, `docs/product/install_image.md`,
`tests/architecture_contract_test/main.cpp`.

**Estimated scope:** Small.

**Rejected alternatives, and why.**

| Option | Why not |
|---|---|
| Static library for `src/`, compiled once | The right long-term answer and roughly halves a full rebuild — but Qt resources inside a static library are not initialised without `Q_INIT_RESOURCE()`, so `.qrc` content vanishes **silently**: icons, QSS and `:/i18n` all missing with no build error. Worth doing when full-rebuild time is the actual bottleneck, with the `.qrc` files kept in the shells. **→ Adopted as E7 on 2026-08-21**, on exactly those terms: full-rebuild time became the bottleneck, and the resources stay in the shells. |
| Qt Creator "Build All Projects" over two open projects | Zero repo change and works today, but it is per-developer session state (`.qws`), so it does not travel with the repository and CI cannot use it. |

**Known cost, stated rather than discovered later.** `src/` compiles twice, once per
shell. The umbrella makes them one *command*, not one *compilation*: cheap for incremental
work (one changed file → two recompiles, two relinks, seconds) and roughly double for a
full rebuild.

> **The cost landed. 2026-08-21: a full umbrella rebuild took ~25 minutes**, and the user
> raised it immediately. The static-library option rejected above is now **Task E7** — the
> trade flipped once the number was real rather than predicted.

---

---

## Task E7: Compile `src/` once — static library + unified deploy (added 2026-08-21)

### Why this task exists

E6 delivered a single build command and the user confirmed it works — **and reported the
build is very slow.** That is E6's stated cost arriving in practice: the umbrella makes the
two shells one *command*, not one *compilation*, so every full rebuild compiles all of
`src/` twice. Measured on 2026-08-21: **~25 minutes**.

E6's own notes named the fix and deferred it. The user has now asked for it, with two
requirements attached:

1. **Resources stay at shell level.** This is the right call and it removes the trap that
   made the static library worth deferring: Qt resources inside a static library are not
   initialised unless `Q_INIT_RESOURCE()` is called, so `.qrc` content vanishes silently —
   icons, QSS and `:/i18n` gone with no build error. Compiling two small `qrc_*.cpp` twice
   costs nothing and sidesteps it entirely.
2. **An opt-in deploy that produces one complete folder** containing both executables.

Deferring was correct while the cost was hypothetical. It is now measured, so the trade
has flipped.

### Two facts checked before planning

Both change how hard this is, and both were verified against the source rather than
assumed:

- **No header under `src/ui` includes a generated `ui_*.h`** — the forward-declared
  `namespace Ui` pattern holds throughout. Shells therefore do not need the library's
  `UI_DIR` on their include path, which removes an entire class of breakage.
- **`robotkinematics.pri` compiles sources *and* post-link-copies DLLs/assets**, but the
  copy is guarded by `CONFIG += robotkinematics_copy_dlls robotkinematics_copy_assets`
  which a host `.pro` may clear *before* including it (the file says so at line 55). So the
  library can take the sources without the copying, and the copying can move to the deploy
  step. Without that guard this task would have needed the component modified.
  > **Wrong — see E7a's "Correction" below.** The opt-out was documented but did not work:
  > the line after the comment re-added the flags. The component did need modifying, and
  > was. Left here as written because the mistake is the point: this was read from a
  > comment, not from the behaviour.

### Target structure

```text
ncr_picking_all.pro                 TEMPLATE = subdirs
  ├── src/src.pro                   TEMPLATE = lib, CONFIG += staticlib   → ncr_shared.lib
  ├── ncr_picking.pro               links ncr_shared, owns its .qrc
  └── runtime_app/ncr_runtime.pro   links ncr_shared, owns its .qrc

build/all/Release/
  shared/    ncr_shared.lib + every src/ object  ← compiled ONCE
  editor/    shell objects
  runtime/   shell objects
  bin/       ncr_picking.exe + ncr_runtime.exe + the full runtime   ← DESTDIR for both
```

`bin/` is the requested complete folder, produced by the build itself rather than by a
separate assembly step. `deploy_dependencies.pri` already honours `DESTDIR`
(`!isEmpty(DESTDIR): DEPLOY_DEST = $$DESTDIR`) and copies only what is missing, so the
second shell to link skips what the first already placed.

### Dependency graph

```text
E7a extract the library ──► E7b unified deploy into bin/ ──► E7c docs + contract test
     (the risky part)          (needs a place to deploy to)
```

E7a is sequenced first because it is where this can fail. Everything after it is
mechanical.

---

### Task E7a: Extract `src/` into a static library ✅ CODE COMPLETE (2026-08-21)

**Description.** Add `src/src.pro` (`TEMPLATE = lib`, `CONFIG += staticlib`) that includes
the seven module `.pri` files and RobotKinematics **with its copy flags cleared**. Split
`qmake/app_common.pri` into `qmake/common_deps.pri` (everything both the library and the
shells need to compile and link: Qt modules, `INCLUDEPATH src/`, OpenCV, Pylon, ADS,
property browser, Eigen/Coal include paths) and a shell-only remainder (resources,
`version.pri`, deploy, and the link against `ncr_shared`). Both shells link the library
instead of compiling the modules.

**Acceptance criteria:**
- [x] `src/` compiles exactly once per full build; both shells link `ncr_shared`. *(re-verified 2026-08-23 from a clean tree: 262 objects under `src/release`, 9 editor and 8 runtime objects, `ncr_shared.lib` 41.9 MB)*
- [x] Each shell owns `ads.qrc` and `resrc.qrc`. *(4 `qrc_*.cpp` per shell, **0** in the library — the invariant the whole design rests on)*
- [ ] Icons, QSS and `:/i18n` all still resolve at runtime. **Manual** — no build or link can show this; see Checkpoint E7.
- [x] No duplicate-symbol link errors from RobotKinematics sources being compiled on both sides.

**Verification:**
- [x] Umbrella full rebuild produces both executables. *(clean rebuild 2026-08-23: **10 min 50 s**, both stamped 0.6.0.0. Reported as a data point, **not** as an improvement on the 18.6 min figure: it was measured on a different day with a second build running concurrently, and the 2026-08-21 conditions cannot be reproduced now.)*
- [x] Contract test passes. *(50 passed / 0 failed on 2026-08-23 before E7c added the 51st)*
- [ ] Manual: launch both shells and confirm window icon, theme/QSS and the Japanese translation still load. A missing `.qrc` produces no build error, so only running proves it. **Pending user check** — both shells were confirmed to *start* and reach their main window, which proves the DLL set, not the resources.

**Dependencies:** E6.

**Files likely touched:** `src/src.pro` (new), `qmake/common_deps.pri` (new),
`qmake/app_common.pri`, `ncr_picking.pro`, `runtime_app/ncr_runtime.pro`,
`ncr_picking_all.pro`.

**Estimated scope:** Medium–Large. If it starts sprawling, land the library with a single
shell first and convert the second separately.

#### Outcome (2026-08-21)

Built clean into `build\all\Release_e7a` with `nmake`, the same way the baseline was taken.

| | before E7a | after E7a |
|---|---|---|
| Full rebuild | ~25 min | **18.57 min** (−26%) |
| `src/` objects | ~262 compiled **twice** | 262 compiled **once** (39.6 MB `ncr_shared.lib`) |
| Shell objects | — | editor 9, runtime 8 (sources + 4 `qrc_*`) |
| `qrc_*.cpp` in the library | — | **0** — the invariant this design rests on |
| Contract test | 50 passed | 50 passed, 0 failed |

Less than the ~50% a naive reading would predict, and the plan's risk row said to report
that rather than bury it. What did not halve: linking two executables, moc, and the
RobotKinematics/Coal/Eigen template-heavy compile — all real time that was never duplicated
per shell in the first place, or is duplicated by design.

**`ncr_runtime.exe` fell 3.33 MB → 1.77 MB.** This was not a goal and deserved checking
before being called a win: a static library lets the linker drop objects nothing references,
which silently loses code that only registers itself at static-init time. Checked, and there
is none — `qRegisterMetaType` calls all sit inside constructors, and `DeviceRegistry` is one
static table in a TU that `device_factory.cpp` references directly, so every concrete device
comes in with it. The drop is editor-only UI (wizards, dialogs, the property-browser
editors) that the runtime shell genuinely never uses. Worth re-checking if a future module
ever adopts self-registration.

#### Correction to "two facts checked before planning"

The second fact was wrong, and it was wrong because it was read from a comment instead of
from the code. `robotkinematics.pri` did document `CONFIG -= robotkinematics_copy_dlls`
before the include as the opt-out — but the next line added the flags back unconditionally,
so the documented escape hatch had never worked. Fixed in the component: the flags are now
added only when the host has not asked for `robotkinematics_no_copy_dlls` /
`_no_copy_assets`, which `src/src.pro` does. Default behaviour for every existing consumer
is unchanged.

---

### Task E7b: One complete output folder, and an opt-in deploy

**Description.** Point both shells' `DESTDIR` at a shared `bin/` inside the umbrella build
directory so the build itself yields one folder holding both executables and the whole
runtime. Move the RobotKinematics DLL/asset copying — orphaned by E7a, since a static
library never links and so never runs a post-link step — into that same deployment, ending
the current split where `deploy_dependencies.pri` handles Qt/OpenCV/Pylon/ADS and
`robotkinematics.pri` separately handles Coal/Assimp/Boost/`robot_assets`. Add an opt-in
target so producing the shippable folder is one command.

**Acceptance criteria:**
- [x] Both executables land in one `bin/` with a single copy of the runtime, plus `robot_assets/`. *(`build\bin\release`: 2 exes, 40 DLLs, 10 plugin/asset folders, `robot_assets\Nachi\MZ04`, 0 `.obj`)*
- [x] Deployment is driven from one place. **Not** by merging the two mechanisms — see the correction below.
- [x] `scripts/make_dist.ps1` simplified to a verified copy of `bin/`. *(one `-BinDir` parameter; the per-shell `-EditorBuildDir`/`-RuntimeBuildDir` pair is gone)*

**Verification:**
- [x] Both shells run from `bin/` with Qt, OpenCV and Basler stripped from `PATH`. *(2026-08-23, `PATH` cut to `system32;Windows;Wbem`; both reached their main window)*
- [x] `dist/` audit: 0 leaked intermediates, 0 duplicate DLLs. *(now asserted by the script itself, which throws on a duplicate and warns by name on an intermediate)*

**Dependencies:** E7a.

**Files touched:** `qmake/app_common.pri`, `qmake/deploy_dependencies.pri`,
`scripts/make_dist.ps1`, `docs/product/install_image.md`,
`docs/rules/build_and_verification.md`, `AGENT.md`, `ncr_picking_all.pro`,
`app/AGENTS.md`, `runtime_app/AGENTS.md`, `.gitignore`.

**Estimated scope:** Medium. **Actual: Small** — see below.

#### Outcome (2026-08-23)

**The task description's premise was wrong, and checking it first is what made this
small.** It says the RobotKinematics copying was "orphaned by E7a, since a static library
never links and so never runs a post-link step". It was not orphaned. Only `src/src.pro`
sets `robotkinematics_no_copy_dlls` / `_no_copy_assets`; the shells include
`robotkinematics.pri` through `common_deps.pri` **without** those flags, so each shell was
still copying `coal.dll`, `assimp-vc143-mt.dll`, two Boost DLLs and `robot_assets/`. Both
were verified present beside both executables before anything was changed.

So the real defect was never a missing copy — it was **duplication**: two folders, each
with a complete ~40-DLL runtime set. And both mechanisms already honoured `DESTDIR`
(`deploy_dependencies.pri` line 35, `robotkinematics.pri` line 70). Pointing one shared
`DESTDIR` at `build/bin/<config>` therefore collapsed them onto one folder with **no
change to either copy mechanism**.

**Deliberately NOT done: merging the two copy mechanisms into one file.** The acceptance
criterion asks for "one place, not two mechanisms in two files", and this delivers the
first half only. Moving RobotKinematics' DLL list into `deploy_dependencies.pri` would
mean this repository hard-codes the names of files the component already knows, duplicating
that knowledge in a second place and breaking the component's own examples — it is
consumed outside this repo. The boundary that does hold, and is now written into
`deploy_dependencies.pri`, is: **the component owns its own runtime; the repo owns the
third-party runtime it pulls in; both resolve `DESTDIR`.** One folder at the output, which
is what the criterion was protecting.

Three things worth recording:

- **`DESTDIR` must be set *before* `common_deps.pri` is included**, because that file
  includes `robotkinematics.pri`, which resolves its copy destination at include time. Set
  it after and `coal.dll` and `robot_assets/` go to each shell's own `release/` while
  everything else goes to `bin/` — an install image missing the mesh-collision runtime,
  with nothing to say so. There is a comment at the assignment saying exactly this.
- **`bin/` is a fixed location** (`build/bin/<config>`), not derived from `OUT_PWD` — the
  same reasoning as `NCR_SHARED_LIB_DIR`. The umbrella and the two standalone build
  directories sit at three different depths, so an `OUT_PWD`-relative path would quietly go
  back to one folder per shell.
- **A new hazard this creates, stated rather than discovered later:** building one shell
  alone now leaves the *other* shell's executable sitting in `bin/`, stale against the same
  `ncr_shared`. Nothing in the build can notice. `make_dist.ps1`'s version check is the
  backstop, and both scope cards now say to prefer the umbrella.

`dist/` was added to `.gitignore` (which now exists — the plan's E3 note that the
repository had none is out of date).

---

### Task E7c: Documentation and contract test

**Description.** Record the new layout and, more importantly, the new rules a contributor
can break silently: where a new source file registers now, and why the `.qrc` files must
stay in the shells.

**Acceptance criteria:**
- [x] `AGENT.md`, `build_and_verification.md`, and all seven module `AGENTS.md` state that files register in the module `.pri`, which `src/src.pro` consumes into `ncr_shared`.
- [x] The `Q_INIT_RESOURCE` hazard is written down where someone tempted to "tidy" the `.qrc` files into the library will read it. *(`AGENT.md` module-map section, `build_and_verification.md` → "Why The `.qrc` Files Stay In The Shells", `src/ui/AGENTS.md`, plus the existing comments in `src/src.pro` and `qmake/app_common.pri`)*
- [x] Contract test asserts each shell owns the resources and the library does not. *(`test_resources_belong_to_the_shells_not_the_shared_library`)*

**Verification:**
- [x] Contract test passes. *(**51 passed, 0 failed**, 4650 ms)*
- [x] **The new test was proved able to fail.** A `.qrc` was temporarily added to `src/runtime/runtime.pri`; the run went to 50 passed / **1 failed**, naming the offending file, and the injection was reverted. E4's "a check that could not fail" is a mistake this phase has already made once.
- [x] Doc links resolve.

**Dependencies:** E7b.

**Files touched:** `AGENT.md`, `docs/rules/build_and_verification.md`,
`src/{core,device,calibration,matching,model,runtime,ui}/AGENTS.md`,
`docs/domains/runtime_app/runtime_shell.md`,
`runtime_app/runtime_layout_controller.h`,
`tests/architecture_contract_test/main.cpp`.

**Estimated scope:** Small–Medium.

#### Outcome (2026-08-23)

The seven module scope cards already said "register in `<module>.pri` only", which was
correct but no longer sufficient: none of them said **who consumes it**. Each now states
that `src/src.pro` compiles it once into `ncr_shared` and that no shell `.pro` lists module
sources, so a file registered anywhere else is simply not built.

The contract test checks the resource split from **four** directions, because a comment
saying "do not move these" is exactly what a tidying edit removes:

1. `qmake/app_common.pri` still lists all three `.qrc` files;
2. `src/src.pro` is still a `staticlib`, references no `.qrc`, and still carries the bare
   `RESOURCES =` reset that takes back what `qtpropertybrowser_vendor.pri` adds;
3. no module `.pri` adds a `.qrc`;
4. neither shell `.pro` includes a module `.pri` directly — that would compile all of
   `src/` a second time, and it would *link*, so nothing else would notice.

**Comments are stripped before matching.** The qmake files explain this rule at length and
those comments name the very filenames being searched for, so a naive `contains(".qrc")`
would have passed on prose in `src/src.pro` — the same class of always-true check as E4's.

Also corrected here: the tile-grid table in `runtime_shell.md` was written as
columns × rows while `RuntimeLayoutController`'s class comment uses rows × columns, so `6`
appeared as both `3×2` and `2x3`. Same layouts; neither stated its axis order. Both now do.

Two deviations found while verifying `runtime_app/` were **recorded, not fixed**, per the
project owner's decision on 2026-08-23 — `docs/backlog/later_todo_list.md` #35
(`RuntimeShellWindow` has no `.ui`, contrary to UI Rule 1.1 *and* to this plan's Task D4)
and #36 (the two shells keep separate `settings.dat`, so the editor's language and theme do
not reach the runtime, which has no UI to change either).

---

### Checkpoint E7 — after E7a, E7b, E7c

- [x] Full rebuild time recorded. *(10 min 50 s on 2026-08-23, clean `build/all/Release`. Against a ~25 min pre-E7a baseline and an 18.6 min post-E7a one — but measured under different conditions, so treat it as a fresh reading rather than a trend.)*
- [x] Both shells run from `bin/` with dependency directories stripped from `PATH`. *(and from `dist/`, which also closes E3's long-pending `ncr_runtime.exe` item)*
- [ ] Icons, QSS and the Japanese translation load in **both** shells — the failure mode this design avoids is silent, so it must be checked by running, not by building. **Pending user check, and it is the one item here nothing in the repository can substitute for.** The contract test now asserts the resources stay at shell level, which stops the split from being undone; it cannot prove the resources actually resolved.
- [x] Contract test passes. *(51 passed, 0 failed — and the new case was proved able to fail)*
- [ ] Qt Creator still builds everything from `build/all/Release` in one action. **Pending user check** — the half only the IDE exercises. Note the executables now appear in `build/bin/Release`, not next to the objects, so Qt Creator's Run configuration may need its working directory pointed there.

### Risks specific to E7

| Risk | Impact | Mitigation |
|---|---|---|
| **Resources silently dropped.** The whole reason this was deferred. If a `.qrc` migrates into the library later, content disappears with no build error. | High — looks like a UI bug, not a build bug | Resources stay in the shells (user's requirement, and correct). E7c writes the hazard down; the contract test asserts the split. |
| **RobotKinematics compiled on both sides** → duplicate symbols, or its DLLs stop being copied because a static library has no post-link step. | Medium | Include it only in the library, with `robotkinematics_copy_dlls`/`_copy_assets` cleared; the copying moves to E7b's deploy. |
| **Running from the build directory breaks** during the transition, because deployment moves. | Medium — costs developer time, not correctness | E7b lands `bin/` in the same pass that removes the old per-shell copying. |
| **The saving is smaller than hoped.** Linking two ~3.5 MB executables and compiling both `.qrc` sets still happens twice. | Low | E7a's verification records the actual time. If it does not move meaningfully, that is a finding to report, not to bury. |

---

### Checkpoint E1 — after E1 and E2

- [ ] Both shells build from clean directories.
- [ ] Contract test passes.
- [ ] Double-launch is a no-op with the existing window raised, on both exes.
- [ ] Docs agree with the toolchain on where build output goes.

### Checkpoint E2 — after E3, E4, E5

- [ ] Both exes run from `dist/` with dependency directories off `PATH`.
- [ ] Both report the same version.
- [ ] A scheduled-task boot brings the runtime up once, into the runtime view.

---

### Deferred: editor ↔ runtime mode switch

The user expects this in a later phase. It is **not** scheduled here — it has real design
surface and Phase 7 is already committed to pattern masking. What is recorded now, while
the reasoning is fresh, are the constraints any implementation must obey:

1. **It is a hand-off, never co-running.** Two processes cannot both own the Basler camera
   (Pylon exclusive access), bind the vision-output TCP port, or hold the MC PLC socket.
   The sequence is: `endRuntime()` on every task → confirm devices released → launch the
   other shell → exit.
2. **That preserves "one writer, one reader."** The project file is SQLite. Never having
   both shells live is what keeps the invariant in
   `docs/domains/runtime_app/runtime_shell.md` true, and is why no project-file
   compatibility layer between the shells is needed.
3. **Locate the sibling via `QCoreApplication::applicationDirPath()`** plus a fixed
   filename. This is the reason the two exes must ship in one folder (E3), stated as a
   requirement rather than a convenience.
4. **Gate entry to the editor by privilege.** A line worker must not reach commissioning by
   mis-tapping. `MainWindow` already models Admin/Standard access levels; reuse that rather
   than inventing a second concept.
5. **E1 is a hard prerequisite.** A hand-off that starts the other shell before this one
   has exited is exactly the two-process collision E1 guards against.

## Checkpoints (user-owned verification)

**Checkpoint A — fault acknowledge and auto-recovery.** With the robot connected: force a
cycle fault, confirm `bTaskFault` goes true, pulse `bErrorReset`, confirm the flag clears
**and** the task returns to Ready. Then repeat with a camera unplugged and confirm the
flags clear while the state stays in recovery (D4). Finally, with `bErrorReset` left
unbound, force the same cycle fault and confirm the task returns to Ready by itself after
~2 s — **and check on the PLC side that the fault was observable while it lasted** (R7).
That last check is the one worth doing on real hardware rather than by reasoning: it is
the only way to know whether a 2-second pulse is long enough for the scan rate in use.

**Checkpoint B — unlimited reconnect.** The highest-value item in the phase: unplug the
camera for **over a minute** (well past the old 10 × 5 s budget) and confirm the task
never faults, the event log stays readable, and re-plugging returns it to Ready with no
operator action. Then end runtime while unplugged and confirm retries stop.

**Checkpoint C — grab retry.** Confirm a slow-failing grab retries rather than timing out
mid-chain, and that reaching fault code 102 now genuinely means six consecutive failures.

**Checkpoint D — runtime shell.** All four startup paths (no saved path / valid path /
missing file / corrupt file), then the 8-task cap and each tile layout.

**Checkpoint E — field deployment.** In order, because each step depends on the one above:

1. **Double-launch, both exes.** One process in Task Manager, existing window comes
   forward. The foreground handover is the part only a real desktop session exercises.
2. **Kill and relaunch.** Confirms the lock is reclaimed rather than wedging the machine.
3. **`ncr_runtime.exe` from `dist/`** with Qt/OpenCV/Basler stripped from `PATH`. Not done
   in the repo because launching it reaches for real hardware.
4. **Scheduled-task reboot.** Runtime starts **once** and reaches the runtime view. Then
   click the icon while it is running and confirm the window is raised rather than a second
   process started — that is auto-start and E1 interacting, which is the combination the
   field will produce daily.

**Regression.** A project saved by `ncr_picking.exe` at schema v1 must load in both
executables after the v2 bump.

## Risks

| # | Risk | Handling |
|---|---|---|
| **R1** | **A PLC program that detects dead links via `bTaskFault` breaks.** After B, a permanently dead device leaves `bTaskReady = false`, `bTaskFault = false` forever. | Called out explicitly in the Phase B docs. The robot side is user-owned (Phase 5 Q1); flag it before Checkpoint B, not after. |
| **R2** | **The `kSchemaVersion` bump makes new projects unreadable by older builds.** | Intended — that is what the gate is for. Worth stating so a v2 project opened on a stale build reads as a clear refusal rather than a mystery. |
| **R3** | **Acknowledge is PLC-driven only.** If the *PLC* is the lost device, `handlePlcValues()` never runs and the operator cannot acknowledge. | Largely closed by A5: the automatic path needs no PLC input, so an unreachable PLC no longer traps the runtime in a fault. Combined with B (a lost PLC no longer produces a fault at all), the remaining gap is small enough not to justify a shell-side acknowledge button yet. |
| **R4** | **Two executables, two deploy manifests.** `deploy_dependencies.pri` runs per-target; `ncr_runtime.exe` needs its own Qt/OpenCV/Pylon/ADS/RobotKinematics copy. | Included in `app_common.pri` so both targets get the hooks by construction. That solves the *developer* case; the *shipping* case is E3, which collapses the two per-binary copies into one `dist/` image. |
| **R5** | **The contract test silently ignores new top-level folders.** | D2 fixes it for `runtime_app`, but the hard-coded list will hide the next one too. Consider deriving the module list from the `.pri` files — logged for the backlog, not this phase. |
| **R6** | **Unbounded retry outliving its session.** | B4 verifies it by inspection and Checkpoint B by hand. Same reasoning applies to A5's timer, which is why it is a cancellable member rather than a fire-and-forget `singleShot`. |
| **R7** | **`bTaskFault` becomes a 2-second pulse, not a latched state.** A PLC or HMI that samples slowly can miss a fault entirely, and any logic that waits for a *persistent* `bTaskFault` will never see one. | Documented in A6 with the recommended sampling rule (`bMatchingFinished` is the stable edge; latch on the PLC side). Verified on hardware in Checkpoint A. The robot side is user-owned (Phase 5 Q1) — raise this **before** Checkpoint A, not after. |
| **R8** | **`bErrorReset` is demoted by its own safety net.** With auto-recovery at 2 s, the acknowledge input saves the operator two seconds and little else, and a site that *wants* a fault to stay visible until confirmed can no longer get that. | Accepted, and stated rather than hidden: the user's explicit priority is that the line never parks. If a site later needs the latching behaviour, the natural shape is making `kFaultAutoRecoverMs` configurable with 0 meaning "wait for acknowledge" — deliberately **not** built now, since no second case exists yet (`AGENT.md`: no abstraction before a second concrete implementation). |
| **R9** | **Two runtime processes fighting for one camera.** Auto-start at boot plus an operator double-click is the *normal* case, not an edge case. The symptoms are indistinguishable from the hardware faults fixed in B7 — grab failures, "device removed", a port that will not bind — so it costs a bench session before anyone suspects a second process. | E1, sequenced first in Phase E and a hard prerequisite for E5. Do not ship auto-start before it. |
| **R10** | **Version drift between two exes in one folder.** A v2 editor writing a project a v1 runtime refuses to load; the field symptom is "the runtime will not open what the editor just saved". | Nothing corrupts — the `kSchemaVersion` gate refuses loudly (R2). E4 makes the mismatch a one-glance diagnosis instead of a log hunt, and E3 records the version in the manifest so an install image is self-identifying. |
| **R11** | **Build and deploy layouts get conflated.** The natural request — "put both exes in one folder" — reads as "build into one folder", which collides on `Makefile`, `.qmake.stash`, `ui_*.h` and the `moc_*.cpp` generated from shared `src/` headers. | Phase E's opening states the distinction, and `dist/` is the named boundary. E2 removes the current docs/toolchain disagreement that would otherwise invite the shortcut. |

## Sequencing

A → B → C → D. A and B both touch the runtime controller's state handling, and doing B
first would mean writing the acknowledge against an escalation path that is about to be
deleted. Within A, the auto-recovery timer (A5) comes after the acknowledge (A3/A4) so
both entry points share one already-written recovery body. Within C, the fix (C2)
precedes the documentation (C1). C as a whole is independent of A/B/D and can move
anywhere in the order. D is the largest and depends on A/B only for what the dashboard
displays, so it lands last and once.

**E follows D** because it exists only to make D's second executable shippable. Inside E:
E1 first — it is the highest-risk item and a fail-fast order puts it where there is still
room to react; E2 is trivial and unblocks E3; E4 lands before E3 so the manifest has a
version to record; E5 last, because documenting boot-start before the single-instance
guard exists would document a trap.

E1, E2 and E4 are independent of each other and safe to parallelise. E3 and E5 are not —
each waits on the tasks above.
