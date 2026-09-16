# Runtime State Contract — `LocalizationRuntimeController::CycleState`

**Status:** TEMPLATE (proposal). When adopted, this file becomes the single normative
description of the runtime cycle state machine. Prose documents
(`task_localization.md`, `runtime_controller_api.md`, `plc_signal_contract.md`) explain
and link here; they do not restate the tables.

**Rules for this file**

1. State names are the enum names, verbatim. No diagram aliases.
2. Every `(state, event)` pair has exactly one row in §3. "Ignore and log" is a valid row.
   An empty cell is a defect. A pair the owner has not decided is written as
   `UNSPECIFIED (DR-xxxx)` and listed in §7.
3. Every row has a test id. A row without one is listed in §7, never silently left.
4. History does not live here. When a row changes, the old behaviour goes into the DR
   that changed it, and the row's `DR` column points there.
5. Symbols, not line numbers.
6. `uml/08_runtime_state_machines.puml` is generated from §3 (`scripts/gen_state_uml.py`,
   to be written). Do not edit the generated block by hand.

---

## 1. States

| State | Invariant (must hold while in state) | Required PLC output snapshot (§4) | Entered only via |
|---|---|---|---|
| `NotReady` | Setup not yet valid, or a re-arm is pending and not yet granted | none published by this state itself | `setup()` start; trigger fall after a clean cycle |
| `ReadyForTrigger` | `m_valid`, all required roles healthy, active group + calibration valid, no index-rejection latch held | `READY` | `markRuntimeReady()` only |
| `Running` | One active `cycleId`; grab/match/send in flight; no index change accepted | `CYCLE_START` | `startCycle()` only |
| `WaitingTriggerReset` | Cycle finished (ok or faulted); `bExecuteTrigger` still high; results latched for the PLC | `CYCLE_OK` or `CYCLE_FAULT` (whichever ended the cycle) | end of cycle with trigger high |
| `Recovering` | At least one required role unhealthy, **or** a cycle fault has come to rest with trigger low | `RECOVERING` or `CYCLE_FAULT` | role loss outside a cycle; cycle abort with trigger low |
| `Faulted` | Setup invalid, or an index refusal latched | `INDEX_REFUSED` / setup fault outputs | `setup()`; index setters; `reportSignalTypeMismatch()` |

> Seed note (2026-09-16): `Recovering` currently carries two meanings (role outage; latched cycle
> fault awaiting auto-clear). Whether to split it is a policy question — see
> `01_open_policy_questions.md`.

## 2. Events

Events are **source-independent**. Mapping from a PLC tag, a TCP command, a runner signal or a
timer to an event happens in the adapter (today: `handlePlcValues()`, the `on*` slots), never
in the core.

| Event | Payload | Today's source(s) | Kind | Producing thread | If it arrives while `Running` |
|---|---|---|---|---|---|
| `TriggerRise` | — | `bExecuteTrigger` 0→1 | edge | PLC runner (queued) | ignore + log |
| `TriggerFall` | — | `bExecuteTrigger` 1→0 | edge | PLC runner (queued) | record level only |
| `ErrorResetRise` | — | `bErrorReset` 0→1 | edge | PLC runner (queued) | ignore |
| `SelectCamera` | `n` | `nActiveCamera` value | level/command | PLC runner (queued); UI (dead today) | reject + log |
| `SelectPatternGroup` | `n` | `nActivePatternGroup` value | level/command | same | reject + log |
| `IndexTypeMismatch` | signal name | non-numeric index tag | level | PLC runner | fault |
| `RoleStatus` | role, `ConnectStatus` | `connectStatusChanged` | status | runner (queued) | abort if unhealthy |
| `GrabFinished` | ok, frame | `CameraRunner::grabFinished` | completion | camera runner | consume if `cycleId` matches |
| `GrabCommandFailed` | result code | `commandFinished(TimedOut/...)` | completion | camera runner | abort `102` |
| `MatchFinished` | `cycleId`, result | matching worker | completion | matching thread | consume if `cycleId` matches |
| `SendFinished` | ok, message | `resultRequestFinished` | completion | output runner | success / abort `201` |
| `HandshakeWriteFinished` | id, ok | `PlcRunner::writeFinished` | completion | PLC runner | retry / abort `301` |
| `FaultAutoRecoverTimeout` | — | `m_faultRecoverTimer` | timer | runtime thread | n/a (never armed while Running) |
| `PlcSnapshot` | value map | `PlcRunner::pollingUpdate` | snapshot | PLC runner | ignore |
| `SetupRequested` | `RuntimeContext` | `TaskLocalization::beginRuntime()` | lifecycle | blocking queued | n/a |
| `TeardownRequested` | — | `endRuntime()` / `stopAll()` | lifecycle | queued | abort silently |

## 3. Transition table

Columns: **ID** `T-<from>-<event>-<n>` · **From** · **Event** · **Guard** · **Actions** (ordered) ·
**To** · **Outputs** (§4 snapshot name) · **Test** · **DR**.

| ID | From | Event | Guard | Actions | To | Outputs | Test | DR |
|---|---|---|---|---|---|---|---|---|
| T-Ready-TriggerRise-1 | `ReadyForTrigger` | `TriggerRise` | `m_valid` | `startCycle()`: `++cycleId`, clock start, request single-shot | `Running` | `CYCLE_START` | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` | — |
| T-NotReady-TriggerRise-1 | `NotReady` | `TriggerRise` | — | log "Trigger ignored: task is not ready." | `NotReady` | — | *(none)* | — |
| T-Running-GrabCommandFailed-1 | `Running` | `GrabCommandFailed` | `cycleId` matches | `abortCycle(CameraGrabTimeout)` | `WaitingTriggerReset` if trigger high else `Recovering` | `CYCLE_FAULT(102)` | `test_localization_runtime_grab_timeout_faults_without_vision_output` | — |
| T-Running-SendFinished-ok | `Running` | `SendFinished(ok)` | `cycleId` matches | publish success, stamp timings | `WaitingTriggerReset` if trigger high else `ReadyForTrigger` **(bypasses `markRuntimeReady()` gate today)** | `CYCLE_OK` | `test_localization_runtime_ready_and_cycle_outputs_write_plc_tags` | *(open: should go through the gate)* |
| T-Wait-TriggerFall-clean | `WaitingTriggerReset` | `TriggerFall` | no cycle fault | clear `bMatchingFinished`; `markRuntimeReady()` | `ReadyForTrigger` via `NotReady` | `READY` | same as above | — |
| T-Wait-TriggerFall-faulted | `WaitingTriggerReset` | `TriggerFall` | cycle faulted | clear `bMatchingFinished`; arm auto-recover | `Recovering` | `CYCLE_FAULT` | `test_localization_runtime_fault_auto_clears_without_error_reset` | — |
| T-Ready-RoleStatus-lost | `ReadyForTrigger` | `RoleStatus(unhealthy)` | role is required | `bTaskReady=false`; schedule reconnect | `Recovering` | `RECOVERING` | `test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault` | — |
| T-Any-TriggerRise-heldAtStart | `NotReady` | `TriggerRise` observed as first sample after setup | — | **UNSPECIFIED (DR-xxxx, backlog 54)** | ? | ? | — | pending |
| … | | | | *(remaining rows extracted in step D0)* | | | | |

**Completeness check (to be automated):** 6 states × 16 events = 96 pairs. Rows present: __.
Missing: __. `UNSPECIFIED`: __.

## 4. Output snapshots

Named sets of PLC output values. A transition publishes exactly one snapshot (or none).
Handshake signals (retried, escalate to `301`) are marked **H**.

| Snapshot | `bTaskReady` **H** | `bMatchingBusy` | `bMatchingFinished` **H** | `bMatchingDetected` | `bMatchingLowArea` | `bTaskFault` **H** | `nDetectedNumber` **H** | `nFaultCode` **H** | `bCameraValid` | `bPatternValid` |
|---|---|---|---|---|---|---|---|---|---|---|
| `READY` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 1 |
| `CYCLE_START` | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | — | — |
| `CYCLE_OK` | 0 | 0 | 1 | n>0 | result | 0 | n | 0 | — | — |
| `CYCLE_FAULT(code)` | 0 | 0 | 1 | 0 | 0 | 1 | 0 | code | — | — |
| `RECOVERING` | 0 | — | — | — | — | 0 | — | — | — | — |
| `INDEX_REFUSED(103/400)` | 0 | — | — | — | — | 1 | — | 103 / 400 | 0 (camera) | 0 (group) |
| `STOP` | 0 | 0 | 0 | — | — | 0 | — | 0 | — | — |

`nActiveCameraStatus` / `nActivePatternGroupStatus` are published on every accepted selection and
on every return to `READY`, before `bTaskReady`.

## 5. Scenario catalogue

Sequences the machine must survive. Field-observed ones cite the log they came from.

| ID | Title | Given | When (event sequence) | Then | Source | Test |
|---|---|---|---|---|---|---|
| S-01 | Normal cycle | Ready | TriggerRise → GrabFinished(ok) → MatchFinished → SendFinished(ok) → TriggerFall | Running → WaitingTriggerReset → ReadyForTrigger; `CYCLE_OK` then `READY` | contract | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` |
| S-02 | Held trigger after cycle | Ready | TriggerRise → … → SendFinished(ok); trigger stays high | stays `WaitingTriggerReset`; no second cycle | contract | *(name)* |
| S-03 | Master holds camera 0 at start | PLC snapshot: camera=0 | SetupRequested → PlcSnapshot | `Faulted`, `INDEX_REFUSED(103)`, runtime stays valid; `SelectCamera(1)` → Ready | field 2026-09-09 | `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready` |
| S-04 | Held trigger at start | trigger high before setup | SetupRequested → first PlcSnapshot(trigger=1) | **UNSPECIFIED (backlog 54)** | audit | — |
| S-05 | Result publish collides with poll (dual-role Modbus) | Running | SendFinished(fail: transaction in flight) | today: abort `201`; policy open | field 2026-09-14 (item 64) | — |
| S-06 | Vision-output heartbeat back, main link not | Recovering | RoleStatus(Connecting) repeatedly | stays Recovering | field 2026-09-09 (item 70) | — |
| S-07 | PLC connects late, after setup fell back to project default | setup w/o snapshot | RoleStatus(PLC Connected) → PlcSnapshot(camera=2) | **UNSPECIFIED** (scratch notes "bug found") | owner note | — |
| … | | | | | | |

## 6. Policy registry

Everything that is a *decision*, with its parameters, in one place.

| Policy | Decided in | Implemented at (symbol) | Parameters | Test |
|---|---|---|---|---|
| Role reconnect: unbounded, never a fault | Phase B (pre-DR) | `LocalizationRecoveryPolicy`, `decideRecoveryAction()`, `scheduleRoleReconnect()` | `retryIntervalMs = 5000` | `test_localization_role_outage_retries_forever_without_faulting` |
| Cycle fault auto-clear | Phase 8 | `armFaultAutoRecovery()`, `recoverFromFault()` | `kFaultAutoRecoverMs = 2000`, `kAutoRecoverWarnStride = 5` | `test_localization_runtime_fault_auto_clears_without_error_reset` |
| Handshake write retry | DR (Phase 9 D3) | `isHandshakeSignal()`, `onPlcWriteFinished()`, `escalatePlcWriteFailure()` | `kPlcWriteRetryBudget = 3`, `kPlcWriteRetryDelayMs = 40` | `test_a_handshake_write_retried_to_exhaustion_aborts_with_301` |
| Required signals | DR (Phase 9 D4) | `requiredSignalNames()`, `validateSignalMap()` | 5 names | `test_each_required_signal_unmapped_fails_setup_naming_it` |
| Index refusal latched per signal | DR (Phase 9 D7) | `validateCameraNumber()`, `m_activeCameraSelectionRejected` … | ranges 1..16 / 1..32 | index tests |
| Grab retry in runner | Phase 8 | `CameraRunner::kMaxGrabAttempts` | 6, `kSingleShotTimeoutMs = 8000` | `test_camera_runner_watchdog_outlasts_device_grab_timeout` |
| Positions per cycle | **none** | `buildVisionOutputPositions()` | literal `2` | — (backlog 68) |
| "No match" is not a fault | **none** | `publishCycleSuccessOutputs()` | — | — |

## 7. Coverage and open cells

- Rows with no test: …
- `UNSPECIFIED` rows: T-Any-TriggerRise-heldAtStart (DR-xxxx), S-04, S-05, S-07 …
- Policies with no DR: positions-per-cycle cap, no-match-not-a-fault, `Recovering` dual meaning.
