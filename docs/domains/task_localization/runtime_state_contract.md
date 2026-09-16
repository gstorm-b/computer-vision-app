# Runtime State Contract — `LocalizationRuntimeController::CycleState` (AS-IS)

**Status:** AS-IS — extracted 2026-09-16 from the code; describes implemented behaviour, not the
target design. Reviewed at Phase 10 Checkpoint 0. Plan context: `docs/plan/phase_10/`.

**What this file is.** A descriptive extraction of the runtime cycle state machine exactly as
`src/model/localization_runtime_controller.h` / `.cpp` implement it on the extraction date. Every
row was read off the code; nothing here is a proposal. The only judgements made are the four cell
markers in §3 (IGNORED / NO-OP / UNREACHABLE / UNSPECIFIED) and the DRIFT lines in §8.

**Rules for this file**

1. State names are the enum names, verbatim (`CycleState`). No diagram aliases.
2. Every `(state, event)` pair has exactly one cell in §3b, holding a row id from §3 or one of the
   four markers. Marker counts sum to states × events.
3. Symbols (`Class::method`, constants, test names), never line numbers.
4. Shared action blocks are defined once (§3.0) and referenced by rows.
5. Where the code does something the surrounding comments or documents say it should not, the
   code is recorded and the disagreement goes to §8.

**Row-id abbreviations.** `NR` = `NotReady`, `RFT` = `ReadyForTrigger`, `RUN` = `Running`,
`WTR` = `WaitingTriggerReset`, `REC` = `Recovering`, `FLT` = `Faulted`, `Any` = handler does not
read `m_cycleState` (or reads it only to pick a branch that is stated in the row).

---

## 1. States

Six values of the private `enum class LocalizationRuntimeController::CycleState`. The member is
`m_cycleState`, initialised to `NotReady`. The "invariant" column is what the code actually
enforces on entry; the "published on entry" column names the `publish*` function or the ad-hoc
sequence (§4) that runs at the assignment site.

| State | Invariant as enforced in code | Published on entry (§4 snapshot) | Entered via (assignment sites, §3c) |
|---|---|---|---|
| `NotReady` | None enforced. `setup()` assigns it before validating anything; the trigger-fall path assigns it as a stepping stone so that `markRuntimeReady()` decides whether the runtime is armed. Nothing checks `NotReady` specifically anywhere. | none by the assignment itself (`setup()` goes on to publish per-failure outputs, `SETUP_STATUS`, and possibly `READY`) | AS-05 `setup()` start; AS-09 `handlePlcValues()` falling edge with `!m_pendingCycleResult.faulted` |
| `ReadyForTrigger` | Everything `markRuntimeReady()` gates on at the moment of assignment: `m_cycleState` not `Running`/`WaitingTriggerReset`; `m_valid`; `allRequiredRolesHealthy()` (PrimaryPlc, VisionOutput, Camera contexts bound, runner and device non-null, device `connectStatus() == Connected`); `validateActivePatternGroup()`; `validateActiveCameraCalibration()`; `!m_activeCameraSelectionRejected && !m_activePatternGroupSelectionRejected`. **Exception:** `onVisionOutputResultFinished()` assigns `ReadyForTrigger` directly with none of those checks, then calls `markRuntimeReady()` (AS-15; PQ-11). The `execute()` and trigger paths re-check only `m_cycleState == ReadyForTrigger`, not the gate. | `READY` via `publishInitialReadyOutputs()` (AS-10 only); `CYCLE_OK` was published just before at AS-15 and `READY` follows only if the gate passes | AS-10 `markRuntimeReady()`; AS-15 `onVisionOutputResultFinished()` (direct, when `!m_lastExecuteTrigger`) |
| `Running` | `startCycle()` assumes and re-checks: `m_cycleState == ReadyForTrigger` on entry, `validateActivePatternGroup()`, `validateActiveCameraCalibration()`, `activeCameraRunner() != nullptr` (each failure aborts instead). It does **not** check `m_valid`, `allRequiredRolesHealthy()` or the two latches. While `Running`: one `m_activeCycleId` is live; `m_cycleClock` restarted; `m_faultRecoverTimer` cancelled; grab (`Qt::SingleShotConnection`) and command (`Qt::UniqueConnection`) connections exist. | `CYCLE_START` via `publishCycleStartOutputs()` | AS-12 `startCycle()` |
| `WaitingTriggerReset` | Entered only while `m_lastExecuteTrigger == true`, by construction of the two ternaries. `m_pendingCycleResult.faulted` says which kind of cycle end it was. `markRuntimeReady()` refuses while in this state. The state is left only by the trigger falling edge (AS-08/AS-09), by `setup()` (AS-05), by an index refusal / type mismatch (AS-01..04, AS-11 → `Faulted`), or by a handshake-write escalation re-entering `abortCycle()` (AS-13). | `CYCLE_OK` (`publishCycleSuccessOutputs()`) or `CYCLE_FAULT(code)` (`publishCycleFaultOutputs()`), published immediately before the assignment | AS-13 `abortCycle()` (trigger high); AS-15 `onVisionOutputResultFinished()` (trigger high) |
| `Recovering` | Two distinct meanings, not distinguished by any member: (a) a required role reported an unhealthy, recoverable status while `ReadyForTrigger` (AS-14); (b) a cycle fault came to rest with the trigger low (AS-13 with `!m_lastExecuteTrigger`, AS-08). In (b) `m_faultRecoverTimer` is armed by `armFaultAutoRecovery()`; in (a) it is not. Nothing checks `Recovering` except `recoverFromFault()` (which re-arms from it) and `handleRoleStatusChanged()`'s `ReadyForTrigger`-only publish guard (which therefore does **not** publish `bTaskReady=false` a second time). | (a) `RECOVERING` (`bTaskReady=false`); (b) `CYCLE_FAULT(code)` just before | AS-08 `handlePlcValues()`; AS-13 `abortCycle()` (trigger low); AS-14 `handleRoleStatusChanged()` |
| `Faulted` | Nothing enforced on entry. Reached by: `setup()` invalid (`m_valid == false`); `setup()` valid but a PLC-commanded startup index refused (latch set, `m_valid == true`); either index setter refusing a number (latch set) or the camera setter finding the accepted camera uncalibrated (no latch); `reportSignalTypeMismatch()` (latch set). `Faulted` is left by `markRuntimeReady()` (from an accepted index write, a role reconnect, `bErrorReset`, or the auto-recover timer), by `setup()`, or by a handshake-write escalation (`abortCycle()` → `WaitingTriggerReset`/`Recovering`). `escalatePlcWriteFailure()` emits `runtimeFault()` **without** assigning `Faulted`. | `INDEX_REFUSED_CAM`, `INDEX_REFUSED_GRP`, `CAM_UNCALIBRATED`, `TYPE_MISMATCH`, `SETUP_*` partial sets, `STARTUP_REFUSED` (§4) | AS-01, AS-02 `setActiveCameraNumber()`; AS-03, AS-04 `setActivePatternGroupNumber()`; AS-06, AS-07 `setup()`; AS-11 `reportSignalTypeMismatch()` |

Members that carry state alongside `m_cycleState` and are read by the rows below:
`m_valid`, `m_lastExecuteTrigger`, `m_lastErrorReset`, `m_activeCameraSelectionRejected`,
`m_activePatternGroupSelectionRejected`, `m_startupSelectionFault`, `m_activeCycleId`,
`m_pendingCycleResult.faulted` / `.faultCode`, `m_faultRecoverTimer.isActive()`,
`m_consecutiveAutoRecoveries`, `m_recoveryContexts[role].{retryCount, retryScheduled,
reportedStatus}`, `m_pendingWrites`, `m_plcWriteRetryQueue`, `m_plcWriteEscalating`, and the three
`QMetaObject::Connection` handles `m_cameraGrabConnection`, `m_cameraCommandConnection`,
`m_visionOutputResultConnection`.

---

## 2. Events

Source-independent names. "Handler" is the controller symbol that reads the stimulus. "Producing
thread" is where the emitting object lives; every runner signal reaches the controller through a
`Qt::AutoConnection` made in `setup()`/`bindRoleContext()`/`startCycle()`/
`onRuntimeMatchingFinished()`, so it is queued onto the controller's thread whenever the two differ
(in production the controller is moved to `TaskRunner::runtimeThread()` by
`TaskLocalization::beginRuntime()`; in `LocalizationRuntimeFixture` it lives on the test thread).

| ID | Event | Payload | Today's source (concrete) | Kind | Producing thread | Handler |
|---|---|---|---|---|---|---|
| E01 | `TriggerRise` | — | `bExecuteTrigger` sample `true` while `m_lastExecuteTrigger == false`, from `PlcRunner::valueChanged` → `m_plcValueConnection` (made in `setup()`). `TaskLocalization::queueHandlePlcValues()` is a second entry point reachable only from `TaskLocalization::onCommDeviceValueChanged()`, which has no connect site in the repository. | edge | PLC runner thread (device signal forwarded by `Qt::QueuedConnection`) | `handlePlcValues()` → `startCycle()` |
| E02 | `TriggerFall` | — | `bExecuteTrigger` sample `false` while `m_lastExecuteTrigger == true`, same path | edge | PLC runner thread | `handlePlcValues()` |
| E03 | `ErrorResetRise` | — | `bErrorReset` sample `true` while `m_lastErrorReset == false` | edge | PLC runner thread | `handlePlcValues()` → `acknowledgeFault()` → `recoverFromFault()` |
| E04 | `SelectCamera(n)` | `int n` | `nActiveCamera` value that converts with `QVariant::toInt()`; forwarded **unfiltered**. Also `TaskLocalization::queueSetActiveCameraNumber()` (queued onto the controller thread) — no caller in `src/`, `components/` or `runtime_app/` was found. | level / command | PLC runner thread (or the queuing caller's thread) | `setActiveCameraNumber()` |
| E05 | `SelectPatternGroup(n)` | `int n` | `nActivePatternGroup` value; also `TaskLocalization::queueSetActivePatternGroupNumber()` (no caller found) | level / command | as E04 | `setActivePatternGroupNumber()` |
| E06 | `IndexTypeMismatch(signal)` | signal name, `LocalizationSignalEvent` (tag) | `nActiveCamera` / `nActivePatternGroup` value whose `toInt()` fails | level | PLC runner thread | `handlePlcValues()` → `reportSignalTypeMismatch()` |
| E07 | `ManualExecute` | — | `execute()`, queued from `TaskLocalization::executeLocalization()`; the only invoker of that slot is the moc table (no caller in source) | command | caller's thread (queued) | `execute()` → `startCycle()` |
| E08 | `SetupRequested(context)` | `RuntimeContext` | `TaskLocalization::setupRuntimeController()` via `Qt::BlockingQueuedConnection` (or direct when on the same thread); tests call `setup()` directly | lifecycle | GUI thread, blocking | `setup()` |
| E09 | `RoleHealthy(role)` | `RunnerRole`, `ConnectStatus::Connected` | `IDeviceRunner::connectStatusChanged` with `Connected`, per role, through `onPrimaryPlcStatusChanged` / `onVisionOutputStatusChanged` / `onCameraStatusChanged` (connections made in `bindRoleContext()`) | status | that runner's thread | `handleRoleStatusChanged()` |
| E10 | `RoleUnhealthy(role, status)` | `RunnerRole`, any `ConnectStatus` other than `Connected` (`NoConnection`, `Disconnected`, `LostConnected`, `ConnectFailed`, `Connecting` — `vc::device::ConnectStatus`, named by `connectStatusName()`) | same signals | status | that runner's thread | `handleRoleStatusChanged()` |
| E11 | `GrabFinished(result)` | `vc::device::GrabResult` (`isGrabSuccess`, `frame`) | `CameraRunner::grabFinished`, connected `Qt::SingleShotConnection` in `startCycle()`. The runner re-emits it once per command after its own `kMaxGrabAttempts` retry loop. | completion | camera runner thread | `onCameraGrabFinished()` |
| E12 | `GrabCommandFinished(result)` | `DeviceCommandResult` (`kind`, `status`, `code`, `message`) | `CameraRunner::commandFinished`, connected `Qt::UniqueConnection` in `startCycle()`; disconnected by `onCameraGrabFinished()`, `abortCycle()`, `resetRuntimeBindings()` | completion | camera runner thread | `onCameraCommandFinished()` |
| E13 | `MatchFinished(cycleId, result)` | `int`, `mtc::MatchResult` | `TaskLocalization::wireRuntimeControllerSignals()` lambda on the matching worker → `QMetaObject::invokeMethod(controller, …, Qt::QueuedConnection)`; tests call the public method directly | completion | matching worker thread → queued | `onRuntimeMatchingFinished()` |
| E14 | `SendFinished(ok, message)` | `bool`, `QString` | `IDeviceRunner::resultRequestFinished`, connected `Qt::SingleShotConnection` in `onRuntimeMatchingFinished()`; `IDeviceRunner::requestSendResult()`'s default refuses synchronously | completion | vision-output runner thread (or synchronous) | `onVisionOutputResultFinished()` |
| E15 | `HandshakeWriteFinished(id, ok, message)` | `quint64`, `bool`, `QString` | `PlcRunner::writeFinished`, connected in `bindRoleContext(PrimaryPlc)`; fires for **every** write, tracked or not | completion | PLC runner thread | `onPlcWriteFinished()` |
| E16 | `PlcWriteRetryTimeout` | — | `m_plcWriteRetryTimer` (single-shot, `kPlcWriteRetryDelayMs` = 40 ms), started by `onPlcWriteFinished()` | timer | controller thread | constructor lambda → `reissueTrackedWrite()` |
| E17 | `FaultAutoRecoverTimeout` | — | `m_faultRecoverTimer` (single-shot, `kFaultAutoRecoverMs` = 2000 ms), started by `armFaultAutoRecovery()` | timer | controller thread | constructor lambda → `recoverFromFault()` |
| E18 | `RoleReconnectTimeout(role)` | `RunnerRole` | `QTimer::singleShot(policy.retryIntervalMs, this, …)` in `scheduleRoleReconnect()` | timer | controller thread | lambda → `requestRoleConnectNow()` |
| E19 | `RoleError(role, message)` | `QString` | `IDeviceRunner::errorOccurred` per role, through `onPrimaryPlcError` / `onVisionOutputError` / `onCameraError` | status (log only) | that runner's thread | `reportRoleError()` |
| E20 | `ConfigureRequested(config)` | `TaskLocalizeConfig` | `configure()`, from `TaskLocalization::createRuntimeController()` and `queueConfigureRuntimeController()` (called from the task's config setter) | lifecycle | caller's thread (queued when cross-thread) | `configure()` |
| E21 | `SetRecoveryPolicies` | three `LocalizationRecoveryPolicy` | `setRecoveryPolicies()`; only caller is `test_localization_role_outage_retries_forever_without_faulting` | lifecycle | caller's thread | `setRecoveryPolicies()` |
| E22 | `TeardownRequested` | — | `TaskLocalization::destroyRuntimeController()` (from `endRuntime()` / `stopAll()`): disconnects task↔controller signals, then `deleteLater()` queued on the controller's thread, or `delete` when same thread / thread not running | lifecycle | GUI thread | none — QObject destruction |
| E23 | `LevelSampleNoEdge` | `bool` | a `bExecuteTrigger` or `bErrorReset` sample equal to the last one (or a `bErrorReset` falling edge) | level | PLC runner thread | `handlePlcValues()` (records `m_lastExecuteTrigger` / `m_lastErrorReset` only) |

**Adapter-level facts that are not events.**

- `handlePlcValues()` with an empty map logs `LOG_USER_ERR << "Communication device passed an empty
  values map."` and returns. Tags not in the signal map are dropped silently by
  `LocalizationSignalMapper::mapValues()`. Every mapped value, input or not, is re-emitted as
  `signalChanged(name, value)` before dispatch.
- Values in one `valueChanged` batch are dispatched in `QMap` key order (tag string order), one
  event after another, in the same call.
- `PlcRunner::pollingUpdate` (the whole-register snapshot) is **not** connected to the controller.
  It is consumed once by `TaskLocalization::awaitPrimaryPlcSnapshot()` before `setup()` and reaches
  the controller only as `RuntimeContext::plcSnapshot` inside E08. The template's `PlcSnapshot`
  event therefore has no handler and is not in the grid.
- The concrete PLC devices emit `valueChanged` with changed tags only (`McProtocolDevice`: the
  diff's `changedValues`; Modbus client/server: `takeChangedValues()`; `VirtualPlcDevice`: the
  injected tag). Whether a trigger already high at the first poll is delivered as a change is a
  device-level question this document does not settle (see S-04, §5).

---

## 3. Transition table

### 3.0 Shared action blocks

Referenced by the rows so each is stated once.

- **G-READY** — `markRuntimeReady(message)`: `return` if `m_cycleState` is `Running` or
  `WaitingTriggerReset`; `return` if `!m_valid || !allRequiredRolesHealthy() ||
  !validateActivePatternGroup() || !validateActiveCameraCalibration()`; `return` if either
  selection latch is set. Otherwise `m_cycleState = ReadyForTrigger` (AS-10),
  `publishInitialReadyOutputs()` (`READY`), `appendTaskLog("INFO", message)`,
  `emit runtimeReady(message)`. A refused gate is silent: no log, no signal.
- **A-ABORT(code, message)** — `abortCycle()`: disconnect and clear the three cycle connections;
  `++m_activeCycleId`; `clearPendingWrites()`; `m_pendingCycleResult.faulted = true`,
  `.faultCode = code`, `.detectedNumber = 0`, `.sentNumber = 0`; `publishCycleFaultOutputs(code)`
  (`CYCLE_FAULT(code)`); `emit cycleResultUpdated(m_pendingCycleResult)`;
  `appendTaskLog("ERROR", "<message> fault=<name>")`; `m_cycleState = m_lastExecuteTrigger ?
  WaitingTriggerReset : Recovering` (AS-13); if now `Recovering` → `armFaultAutoRecovery()`;
  `emit runtimeRecovering(message)`. **No state check**: callable from any state.
- **A-RECOVER(reason)** — `recoverFromFault()`: `appendTaskLog("INFO", reason)`; publish
  `bTaskFault=false`, `nFaultCode=0` (`FAULT_CLEAR`); `m_pendingCycleResult.faulted = false`,
  `.faultCode = None`; if `m_cycleState` is `Faulted` or `Recovering` → G-READY(reason).
- **A-RETRY(role, status)** — the tail of `handleRoleStatusChanged()` for an unhealthy status:
  `decideRecoveryAction(policy, status, retryScheduled)`; `Ignore` (status not
  `ConnectFailed`/`LostConnected` under the role policy, or a retry already scheduled) → return.
  `RetryScheduled` → if `retryCount == 0`: stamp `outageStartedAt`, `appendTaskLog("WARN",
  "Connection lost: role=… Reconnecting every … ms until it returns.")`, and **only if
  `m_cycleState == ReadyForTrigger`**: `m_cycleState = Recovering` (AS-14) + publish
  `bTaskReady=false` (`RECOVERING`). Then `retryScheduled = true`, `retryCount += 1`,
  `reportedStatus = status`; if the status differs from the previously reported one →
  `emit runtimeRecovering(buildRecoveryProgressMessage(...))`; `scheduleRoleReconnect(role)`
  (log at USER level on attempt 1 and every `kQuietRetryLogStride` = 12 attempts, DEV otherwise;
  `QTimer::singleShot(retryIntervalMs, this, …)` → E18).
- **A-START** — `startCycle()` body after its three validations pass: `cancelFaultAutoRecovery()`;
  `m_cycleState = Running` (AS-12); `++m_activeCycleId`; `m_pendingCycleResult = CycleResult()`;
  `m_cycleClock.start()`; `publishCycleStartOutputs()` (`CYCLE_START`);
  `emit runtimeCycleStarted("Localization cycle started.")`; `appendTaskLog("INFO", "Trigger
  accepted. Localization cycle started.")`; connect `grabFinished` (`Qt::SingleShotConnection`) and
  `commandFinished` (`Qt::UniqueConnection`); `cameraRunner->requestSingleShot()`.
- **A-CAM-ACCEPT(n)** — `setActiveCameraNumber()` after `validateCameraNumber(n).accepted()`:
  `m_activeCameraSelectionRejected = false`; `m_activeCameraNumber = n`;
  `m_activeCameraFromProjectDefault = false`; publish `bTaskReady=false`;
  `bindActiveCameraRole(n)` (clears and re-binds the Camera context, reconnecting its status/error
  signals); publish `nActiveCameraStatus=n`; if the previous runner differs →
  `previousRunner->requestDisconnect()`; `calibrated = validateActiveCameraCalibration()`;
  `rebuildPickingChecker()`; publish `bCameraValid=calibrated`. If `!calibrated`: publish
  `bTaskFault=true`, `nFaultCode=401`, `m_cycleState = Faulted` (AS-02),
  `emit runtimeFault("Active camera calibration is invalid.")`, return. Else
  `newRunner->requestConnect()`; `applyActiveCameraWorkspace()`; if `allRequiredRolesHealthy()`
  → G-READY("Active camera changed. Runtime ready.").
- **A-CAM-REFUSE(n)** — `setActiveCameraNumber()` when `validateCameraNumber(n)` returns
  `OutOfRange` (outside `TaskDeviceBinding::kMinCameraNumber..kMaxCameraNumber` = 1..16) or
  `NotRegistered` (no runner in `m_context.cameraRunners`): `appendTaskLog("WARN", "Camera change
  refused: …")`; `m_activeCameraSelectionRejected = true`; publish `bTaskReady=false`,
  `bCameraValid=false`, `bTaskFault=true`, `nFaultCode=103` (`INDEX_REFUSED_CAM`);
  `m_cycleState = Faulted` (AS-01); `emit runtimeFault(check.message)`. `m_activeCameraNumber`
  unchanged.
- **A-GRP-ACCEPT(n)** — `setActivePatternGroupNumber()` after `validatePatternGroupNumber(n)`
  (range `mtc::MatchGroup::validateIndexRange()`, 1..32): `m_activePatternGroupNumber = n`
  (committed before validation); `valid = validateActivePatternGroup()`;
  `m_activePatternGroupSelectionRejected = !valid`; publish `bPatternValid=valid`. If `valid`:
  `m_activePatternGroupFromProjectDefault = false`; publish `nActivePatternGroupStatus=n`; if
  `allRequiredRolesHealthy()` → G-READY("Active pattern group changed. Runtime ready."). If
  `!valid`: `appendTaskLog("WARN", "Pattern group change refused: no pattern group is registered
  for number n.")`; publish `bTaskReady=false`, `bTaskFault=true`, `nFaultCode=400`;
  `m_cycleState = Faulted` (AS-04); `emit runtimeFault("No pattern group is registered for
  number n.")`.
- **A-GRP-REFUSE(n)** — `setActivePatternGroupNumber()` when out of range: `appendTaskLog("WARN",
  "Pattern group change refused: …")`; `m_activePatternGroupSelectionRejected = true`; publish
  `bTaskReady=false`, `bPatternValid=false`, `bTaskFault=true`, `nFaultCode=400`
  (`INDEX_REFUSED_GRP`); `m_cycleState = Faulted` (AS-03); `emit runtimeFault(check.message)`.
  `m_activePatternGroupNumber` unchanged.
- **A-MISMATCH(signal)** — `reportSignalTypeMismatch()`: `appendTaskLog("WARN", "<signal> is
  mapped to <tag>, which did not read as a number…")`; `LOG_USER_ERR`; set the matching latch;
  publish `bTaskReady=false`, `bCameraValid=false` (camera) or `bPatternValid=false` (group),
  `bTaskFault=true`, `nFaultCode=103` (camera) or `400` (group) (`TYPE_MISMATCH`);
  `m_cycleState = Faulted` (AS-11); `emit runtimeFault(message)`. **No state check.**
- **A-SETUP(context)** — `setup()`: `m_valid = false`; adopt context/config; reconfigure
  `m_signalMapper`; `m_cycleState = NotReady` (AS-05); `m_lastExecuteTrigger = false`;
  `m_lastErrorReset = false`; both latches `false`; `m_startupSelectionFault.clear()`;
  `m_activeCycleId = 0`; `m_pendingCycleResult = CycleResult()`; `resetRuntimeBindings()`
  (`cancelFaultAutoRecovery()`, `m_lastRoleError.clear()`, disconnect the PLC value connection and
  the three cycle connections, `clearRoleContext()` ×3 — the PrimaryPlc one also
  `clearPendingWrites()`). Then, in order: role-presence errors; `m_plcValueConnection` remade
  (`Qt::UniqueConnection`); `bindFixedRoleRunners()`; `validateSignalMap(&result)`; camera index
  resolved (PLC snapshot via `commandedIndexFromPlc()` → `validateCameraNumber()`; refused →
  latch + `m_startupSelectionFault`; type mismatch → hard error; `< 0` → `cameraMap.firstKey()`
  with `m_activeCameraFromProjectDefault = true`); `validateCameraNumber()` on the resolved number
  (refused → error + publish `bTaskReady=false`, `bCameraValid=false`, `bTaskFault=true`,
  `nFaultCode=103`); `bindActiveCameraRole()`; `applyActiveCameraWorkspace()`; pattern-group index
  resolved the same way (refused → error + publish `bTaskReady=false`, `bPatternValid=false`,
  `bTaskFault=true`, `nFaultCode=400`); if group index accepted and
  `!validateActivePatternGroup()` → error + publish `bPatternValid=false`, `bTaskFault=true`,
  `nFaultCode=400` (no `bTaskReady`); if camera index accepted and
  `!validateActiveCameraCalibration()` → error + publish `bCameraValid=false`, `bTaskFault=true`,
  `nFaultCode=401` (no `bTaskReady`); `rebuildPickingChecker()` (error appended only when the
  camera index was accepted); `result.valid = errors.isEmpty()`; `m_valid = result.valid`; every
  error → `LOG_USER_ERR` + `appendTaskLog("ERROR", …)`. If `m_valid`:
  `logStartupSelectionSummary()`; if `m_startupSelectionFault` non-empty → publish
  `bTaskReady=false`, `bTaskFault=true`, `bCameraValid=false` (if camera latch),
  `bPatternValid=false` (if group latch), `nFaultCode = 103 if camera latch else 400`
  (`STARTUP_REFUSED`), `m_cycleState = Faulted` (AS-06), log ERROR, `emit runtimeFault(...)`;
  then publish `nActiveCameraStatus`, `nActivePatternGroupStatus` (`SETUP_STATUS`);
  `requestRoleConnectNow()` for PrimaryPlc, VisionOutput, Camera; if `allRequiredRolesHealthy()`
  → G-READY("Runtime ready.") else `appendTaskLog("INFO", "Runtime setup valid. Waiting for
  device connections.")`. If `!m_valid`: `m_cycleState = Faulted` (AS-07), no connect requests,
  no `runtimeFault` emitted.

### 3.1 Rows

Columns: **ID** · **From** · **Event** · **Guard** (quoted by symbol) · **Actions** (ordered) ·
**To** · **Outputs** (§4 snapshot) · **Test** (§7; "—" = none found).

#### E01 `TriggerRise`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RFT-TriggerRise-1 | `ReadyForTrigger` | `risingEdge` (`trigger && !m_lastExecuteTrigger`); `m_cycleState == ReadyForTrigger`; in `startCycle()`: `validateActivePatternGroup()`, `validateActiveCameraCalibration()`, `activeCameraRunner() != nullptr` all pass | `m_lastExecuteTrigger = true`; A-START | `Running` | `CYCLE_START` | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`, `test_task_localization_runs_a_full_cycle_through_the_real_matching_worker` |
| T-RFT-TriggerRise-2 | `ReadyForTrigger` | as above but `!validateActivePatternGroup()` | `m_lastExecuteTrigger = true`; A-ABORT(`PatternNotRegistered` 400, "Active pattern group is invalid.") — trigger is high, so `WaitingTriggerReset`, no auto-recover armed | `WaitingTriggerReset` | `CYCLE_FAULT(400)` | — |
| T-RFT-TriggerRise-3 | `ReadyForTrigger` | as above but `!validateActiveCameraCalibration()` | `m_lastExecuteTrigger = true`; A-ABORT(`CalibrationInvalid` 401, "Active camera calibration is invalid.") | `WaitingTriggerReset` | `CYCLE_FAULT(401)` | — |
| T-RFT-TriggerRise-4 | `ReadyForTrigger` | as above but `activeCameraRunner() == nullptr` | `m_lastExecuteTrigger = true`; A-ABORT(`CameraLost` 100, "Active camera runner is not available.") | `WaitingTriggerReset` | `CYCLE_FAULT(100)` | — |
| T-NR-TriggerRise-1 | `NotReady` | `risingEdge`; `m_cycleState != ReadyForTrigger` | `m_lastExecuteTrigger = true`; `appendTaskLog("WARN", "Trigger ignored: task is not ready.")` | `NotReady` | none | — |
| T-RUN-TriggerRise-1 | `Running` | same | same log; `m_lastExecuteTrigger = true` (already true unless E02 intervened) | `Running` | none | — (`test_localization_runtime_trigger_cycle_uses_matching_worker_contract` re-sends `true` while `WaitingTriggerReset`, which is E23, not this row) |
| T-WTR-TriggerRise-1 | `WaitingTriggerReset` | — | **UNREACHABLE**: `WaitingTriggerReset` is entered only with `m_lastExecuteTrigger == true` and every falling edge leaves it (AS-08/AS-09), so `risingEdge` cannot be computed there. If it were, the code path is the same WARN log. | — | — | — |
| T-REC-TriggerRise-1 | `Recovering` | `risingEdge`; state not `ReadyForTrigger` | same log; `m_lastExecuteTrigger = true` — from now on an A-ABORT run in this state (E15 exhaustion) selects `WaitingTriggerReset` | `Recovering` | none | — |
| T-FLT-TriggerRise-1 | `Faulted` | same | same log; `m_lastExecuteTrigger = true` | `Faulted` | none | `test_localization_unregistered_camera_number_faults_and_stays_faulted`, `test_localization_unregistered_pattern_group_faults_and_stays_faulted`, `test_localization_a_refused_index_is_not_forgiven_by_the_other_index` (assert no cycle) |

#### E02 `TriggerFall`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-WTR-TriggerFall-1 | `WaitingTriggerReset` | `fallingEdge`; `m_cycleState == WaitingTriggerReset`; `m_pendingCycleResult.faulted == true` | `m_lastExecuteTrigger = false`; publish `bMatchingFinished=false`; `m_cycleState = Recovering` (AS-08); `armFaultAutoRecovery()` (no-op if already active) | `Recovering` | `TRIGGER_FALL` | `test_localization_runtime_error_reset_clears_fault_and_rearms`, `test_localization_runtime_fault_auto_clears_without_error_reset` |
| T-WTR-TriggerFall-2 | `WaitingTriggerReset` | `fallingEdge`; `m_pendingCycleResult.faulted == false` (either a successful cycle, or a faulted one whose flag E03/E17 cleared while waiting) | `m_lastExecuteTrigger = false`; publish `bMatchingFinished=false`; `m_cycleState = NotReady` (AS-09); G-READY("Trigger reset. Runtime ready.") | `ReadyForTrigger` if G-READY passes, else `NotReady` | `TRIGGER_FALL` then `READY` (gate) | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs` (pass branch); `NotReady` outcome — |
| T-NR-TriggerFall-1 | `NotReady` | `fallingEdge && m_cycleState == WaitingTriggerReset` fails | `m_lastExecuteTrigger = false` only | `NotReady` | none | — (IGNORED, no log) |
| T-RFT-TriggerFall-1 | `ReadyForTrigger` | same | `m_lastExecuteTrigger = false` only | `ReadyForTrigger` | none | — (IGNORED, no log) |
| T-RUN-TriggerFall-1 | `Running` | same | `m_lastExecuteTrigger = false` only; consulted later by A-ABORT and by `onVisionOutputResultFinished()` (selects `Recovering` / direct `ReadyForTrigger` instead of `WaitingTriggerReset`) | `Running` | none | — (IGNORED, no log) |
| T-REC-TriggerFall-1 | `Recovering` | same | level only | `Recovering` | none | — (IGNORED) |
| T-FLT-TriggerFall-1 | `Faulted` | same | level only. Note: a `Faulted` entered from `WaitingTriggerReset` (E04/E05/E06 there) never gets its `bMatchingFinished` cleared by the falling edge. | `Faulted` | none | `test_localization_a_refused_index_is_not_forgiven_by_the_other_index` (sends `M10=false` while `Faulted`; asserts nothing on it) |

#### E03 `ErrorResetRise`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-FLT-ErrorResetRise-1 | `Faulted` | `risingEdge` (`reset && !m_lastErrorReset`) | `m_lastErrorReset = true`; `cancelFaultAutoRecovery()`; A-RECOVER("Fault acknowledged via bErrorReset.") → G-READY | `ReadyForTrigger` if G-READY passes; else `Faulted` with `bTaskFault=false`, `nFaultCode=0` published and the latch / invalid selection / `m_valid == false` still in force | `FAULT_CLEAR`, then `READY` (gate) | `test_localization_error_reset_does_not_lift_a_refused_index` (stays `Faulted`), `test_setup_resets_the_error_reset_edge_detector` (stays `Faulted`); pass branch — |
| T-REC-ErrorResetRise-1 | `Recovering` | `risingEdge` | same; timer cancelled | `ReadyForTrigger` if gate passes; else `Recovering` (role outage: `allRequiredRolesHealthy()` false) | `FAULT_CLEAR`, `READY` (gate) | `test_localization_runtime_error_reset_clears_fault_and_rearms` (pass branch); refuse branch — |
| T-NR-ErrorResetRise-1 | `NotReady` | `risingEdge` | `cancelFaultAutoRecovery()` (no-op); A-RECOVER publishes `FAULT_CLEAR`, clears `m_pendingCycleResult.faulted`; state is neither `Faulted` nor `Recovering` → no G-READY | `NotReady` | `FAULT_CLEAR` | — |
| T-RFT-ErrorResetRise-1 | `ReadyForTrigger` | `risingEdge` | same as T-NR; publishes `bTaskFault=false`, `nFaultCode=0` (already those values) | `ReadyForTrigger` | `FAULT_CLEAR` | — |
| T-RUN-ErrorResetRise-1 | `Running` | `risingEdge` | same; publishes `FAULT_CLEAR` mid-cycle (both values already 0 from `CYCLE_START`); no transition | `Running` | `FAULT_CLEAR` | — |
| T-WTR-ErrorResetRise-1 | `WaitingTriggerReset` | `risingEdge` | same; `FAULT_CLEAR` published while the PLC still holds the trigger and `bMatchingFinished=1`; `m_pendingCycleResult.faulted = false`, so the next E02 takes T-WTR-TriggerFall-2 (clean branch) instead of T-WTR-TriggerFall-1 | `WaitingTriggerReset` | `FAULT_CLEAR` | — |

#### E04 `SelectCamera(n)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-SelectCamera-1 | `Running` | `m_cycleState == Running` | `appendTaskLog("WARN", "Camera change ignored while cycle is running.")` | `Running` | none | `test_localization_runtime_rejects_camera_change_while_running` (partial: asserts only that no `nActiveCamera` signal was emitted) |
| T-RFT-SelectCamera-1 | `ReadyForTrigger` | `validateCameraNumber(n)` not accepted | A-CAM-REFUSE(n) | `Faulted` | `INDEX_REFUSED_CAM` | `test_localization_unregistered_camera_number_faults_and_stays_faulted`, `test_localization_zero_active_index_is_refused_by_the_range_check`, `test_localization_out_of_range_active_index_is_refused`, `test_a_refused_selection_is_not_announced`, `test_an_index_that_names_nothing_reports_not_registered_not_lost` (1) |
| T-RFT-SelectCamera-2 | `ReadyForTrigger` | accepted; `validateActiveCameraCalibration()` false for `n` | A-CAM-ACCEPT(n), uncalibrated branch | `Faulted` | `CAM_ACCEPT` then `CAM_UNCALIBRATED` | — |
| T-RFT-SelectCamera-3 | `ReadyForTrigger` | accepted and calibrated; `allRequiredRolesHealthy()` after rebind | A-CAM-ACCEPT(n) → G-READY (publishes `bTaskReady=false` then `READY` again, even for the same number) | `ReadyForTrigger` | `CAM_ACCEPT`, `READY` | `test_a_camera_rebind_does_not_drop_the_plc_input_stream`, `test_the_runtime_never_writes_the_command_registers`, `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs` |
| T-RFT-SelectCamera-4 | `ReadyForTrigger` | accepted and calibrated; `allRequiredRolesHealthy()` false (new camera not yet `Connected`) | A-CAM-ACCEPT(n) without the G-READY call; `bTaskReady=false` stays published; re-arm happens later on E09 for the camera role | `ReadyForTrigger` (state unchanged although `bTaskReady=0`) | `CAM_ACCEPT` | — |
| T-NR-SelectCamera-1 | `NotReady` | refused | A-CAM-REFUSE(n) | `Faulted` | `INDEX_REFUSED_CAM` | `test_setup_resets_the_error_reset_edge_detector` (setter called right after `setup()` returns, before roles report) |
| T-NR-SelectCamera-2 | `NotReady` | accepted, uncalibrated | A-CAM-ACCEPT uncalibrated branch | `Faulted` | `CAM_ACCEPT`, `CAM_UNCALIBRATED` | — |
| T-NR-SelectCamera-3 | `NotReady` | accepted, calibrated | A-CAM-ACCEPT → G-READY if all roles healthy | `ReadyForTrigger` if gate passes, else `NotReady` | `CAM_ACCEPT`, `READY` (gate) | — |
| T-WTR-SelectCamera-1 | `WaitingTriggerReset` | refused | A-CAM-REFUSE(n); leaves the handshake state while the trigger is high | `Faulted` | `INDEX_REFUSED_CAM` | — |
| T-WTR-SelectCamera-2 | `WaitingTriggerReset` | accepted, uncalibrated | A-CAM-ACCEPT uncalibrated branch | `Faulted` | `CAM_ACCEPT`, `CAM_UNCALIBRATED` | — |
| T-WTR-SelectCamera-3 | `WaitingTriggerReset` | accepted, calibrated | A-CAM-ACCEPT; G-READY refuses (state guard) | `WaitingTriggerReset` | `CAM_ACCEPT` | — |
| T-REC-SelectCamera-1 | `Recovering` | refused | A-CAM-REFUSE(n); `m_faultRecoverTimer` not cancelled (E17 can still fire in `Faulted`) | `Faulted` | `INDEX_REFUSED_CAM` | — |
| T-REC-SelectCamera-2 | `Recovering` | accepted, uncalibrated | A-CAM-ACCEPT uncalibrated branch | `Faulted` | `CAM_ACCEPT`, `CAM_UNCALIBRATED` | — |
| T-REC-SelectCamera-3 | `Recovering` | accepted, calibrated | A-CAM-ACCEPT → G-READY if all roles healthy (a role outage keeps it refused; a resting cycle fault passes) | `ReadyForTrigger` or `Recovering` | `CAM_ACCEPT`, `READY` (gate) | — |
| T-FLT-SelectCamera-1 | `Faulted` | refused | A-CAM-REFUSE(n) (re-latches, republishes) | `Faulted` | `INDEX_REFUSED_CAM` | `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` is the accepted case; refused-in-Faulted — |
| T-FLT-SelectCamera-2 | `Faulted` | accepted, uncalibrated | A-CAM-ACCEPT uncalibrated branch | `Faulted` | `CAM_ACCEPT`, `CAM_UNCALIBRATED` | — |
| T-FLT-SelectCamera-3 | `Faulted` | accepted, calibrated | A-CAM-ACCEPT → G-READY: passes when the camera latch was the only blocker (or none); refused while the group latch, an invalid committed group, or `m_valid == false` holds | `ReadyForTrigger` or `Faulted` | `CAM_ACCEPT`, `READY` (gate) | pass: `test_localization_zero_active_index_is_refused_by_the_range_check`, `test_localization_out_of_range_active_index_is_refused`, `test_localization_recovers_when_a_valid_index_follows_a_rejected_one`, `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal`, `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`; refuse: `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` |

#### E05 `SelectPatternGroup(n)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-SelectGroup-1 | `Running` | `m_cycleState == Running` | `appendTaskLog("WARN", "Pattern group change ignored while cycle is running.")` | `Running` | none | — |
| T-RFT-SelectGroup-1 | `ReadyForTrigger` | out of range | A-GRP-REFUSE(n) | `Faulted` | `INDEX_REFUSED_GRP` | `test_localization_zero_active_index_is_refused_by_the_range_check`, `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera`, `test_an_index_that_names_nothing_reports_not_registered_not_lost` (4) |
| T-RFT-SelectGroup-2 | `ReadyForTrigger` | in range; `validateActivePatternGroup()` false after commit | A-GRP-ACCEPT(n), invalid branch (number **committed**) | `Faulted` | `GRP_ACCEPT` invalid form | `test_localization_unregistered_pattern_group_faults_and_stays_faulted`, `test_localization_recovers_when_a_valid_index_follows_a_rejected_one` |
| T-RFT-SelectGroup-3 | `ReadyForTrigger` | in range, valid; roles healthy | A-GRP-ACCEPT(n) valid branch → G-READY (no `bTaskReady=false` is published first, unlike the camera setter) | `ReadyForTrigger` | `GRP_ACCEPT`, `READY` | `test_the_runtime_never_writes_the_command_registers` |
| T-RFT-SelectGroup-4 | `ReadyForTrigger` | in range, valid; a role unhealthy | A-GRP-ACCEPT(n) valid branch, no G-READY call | `ReadyForTrigger` | `GRP_ACCEPT` | — |
| T-NR-SelectGroup-1 | `NotReady` | out of range | A-GRP-REFUSE(n) | `Faulted` | `INDEX_REFUSED_GRP` | — |
| T-NR-SelectGroup-2 | `NotReady` | in range, invalid | A-GRP-ACCEPT invalid branch | `Faulted` | `GRP_ACCEPT` invalid | — |
| T-NR-SelectGroup-3 | `NotReady` | in range, valid | A-GRP-ACCEPT valid → G-READY if roles healthy | `ReadyForTrigger` or `NotReady` | `GRP_ACCEPT`, `READY` (gate) | — |
| T-WTR-SelectGroup-1 | `WaitingTriggerReset` | out of range | A-GRP-REFUSE(n) | `Faulted` | `INDEX_REFUSED_GRP` | — |
| T-WTR-SelectGroup-2 | `WaitingTriggerReset` | in range, invalid | A-GRP-ACCEPT invalid branch | `Faulted` | `GRP_ACCEPT` invalid | — |
| T-WTR-SelectGroup-3 | `WaitingTriggerReset` | in range, valid | A-GRP-ACCEPT valid; G-READY refuses (state guard) | `WaitingTriggerReset` | `GRP_ACCEPT` | — |
| T-REC-SelectGroup-1 | `Recovering` | out of range | A-GRP-REFUSE(n) | `Faulted` | `INDEX_REFUSED_GRP` | — |
| T-REC-SelectGroup-2 | `Recovering` | in range, invalid | A-GRP-ACCEPT invalid branch | `Faulted` | `GRP_ACCEPT` invalid | — |
| T-REC-SelectGroup-3 | `Recovering` | in range, valid | A-GRP-ACCEPT valid → G-READY if roles healthy | `ReadyForTrigger` or `Recovering` | `GRP_ACCEPT`, `READY` (gate) | — |
| T-FLT-SelectGroup-1 | `Faulted` | out of range | A-GRP-REFUSE(n) | `Faulted` | `INDEX_REFUSED_GRP` | — |
| T-FLT-SelectGroup-2 | `Faulted` | in range, invalid | A-GRP-ACCEPT invalid branch | `Faulted` | `GRP_ACCEPT` invalid | — |
| T-FLT-SelectGroup-3 | `Faulted` | in range, valid | A-GRP-ACCEPT valid → G-READY: passes when the group latch / committed-invalid group was the only blocker; refused while the camera latch or `m_valid == false` holds | `ReadyForTrigger` or `Faulted` | `GRP_ACCEPT`, `READY` (gate) | pass: `test_localization_zero_active_index_is_refused_by_the_range_check`, `test_localization_recovers_when_a_valid_index_follows_a_rejected_one`, `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera`; refuse: `test_localization_a_refused_index_is_not_forgiven_by_the_other_index`, `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value` |

#### E06 `IndexTypeMismatch(signal)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-TypeMismatch-1 | any of the six | `event.value.toInt(&ok)` failed; **no state check** | A-MISMATCH(signal). From `Running`: the in-flight grab/match/send connections are left connected; the next E11/E12/E13/E14 is dropped by its own `!= Running` guard, so the cycle never publishes `CYCLE_OK`/`CYCLE_FAULT` and `bMatchingBusy` stays 1. From `WaitingTriggerReset`: the trigger's falling edge is then ignored (T-FLT-TriggerFall-1). From `Recovering`: `m_faultRecoverTimer` keeps running. | `Faulted` | `TYPE_MISMATCH` | from `ReadyForTrigger`: `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value`; from the other five states — |

#### E07 `ManualExecute`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RFT-ManualExecute-1 | `ReadyForTrigger` | `m_valid`; `m_cycleState == ReadyForTrigger`; `startCycle()` validations pass | A-START. `m_lastExecuteTrigger` is **not** touched, so the cycle's end (A-ABORT / `onVisionOutputResultFinished()`) sees whatever level the PLC last sent — typically `false` → `Recovering` / direct `ReadyForTrigger` rather than `WaitingTriggerReset` | `Running` | `CYCLE_START` | — |
| T-RFT-ManualExecute-2 | `ReadyForTrigger` | `m_valid`; a `startCycle()` validation fails | A-ABORT(400 / 401 / 100 as T-RFT-TriggerRise-2..4) with `m_lastExecuteTrigger` as last sampled | `WaitingTriggerReset` if the trigger is held, else `Recovering` (+ auto-recover armed) | `CYCLE_FAULT(code)` | — |
| T-NR-ManualExecute-1 | `NotReady` | `m_valid` true; state not `ReadyForTrigger` | `appendTaskLog("WARN", "Manual localization request ignored: task is not ready.")` | `NotReady` | none | — |
| T-RUN-ManualExecute-1 | `Running` | same | same log | `Running` | none | — |
| T-WTR-ManualExecute-1 | `WaitingTriggerReset` | same | same log | `WaitingTriggerReset` | none | — |
| T-REC-ManualExecute-1 | `Recovering` | same | same log | `Recovering` | none | — |
| T-FLT-ManualExecute-1 | `Faulted` | `!m_valid` → `LOG_DEV_ERR "… runtime is not set up"` (no task log); else the WARN log above | as guard | `Faulted` | none | — |

#### E08 `SetupRequested(context)`

`setup()` reads `m_cycleState` nowhere; the prior state only determines what
`resetRuntimeBindings()` has to tear down. The same four rows apply from every state.

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-Setup-1 | any | `result.errors` non-empty | A-SETUP: `m_valid = false`, per-failure partial publishes (`SETUP_CAM_REFUSED`, `SETUP_GRP_REFUSED`, `SETUP_GRP_INVALID`, `SETUP_CAL_INVALID` — whichever applied), errors logged, `m_cycleState = Faulted` (AS-07); no `runtimeFault()`; no connect requests | `Faulted` | those partial sets, no `SETUP_STATUS` | `test_localization_runtime_setup_faults_on_invalid_pattern_group`, `test_localization_runtime_setup_faults_on_invalid_calibration`, `test_setup_refuses_an_out_of_range_active_camera_with_a_range_message`, `test_setup_refuses_an_unregistered_active_camera_distinctly_from_calibration`, `test_setup_refuses_an_out_of_range_active_pattern_group_with_a_range_message`, `test_an_index_that_names_nothing_reports_not_registered_not_lost` (2), `test_each_required_signal_unmapped_fails_setup_naming_it`, `test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike`, `test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal`, `test_two_signals_mapped_to_one_tag_fail_setup`, `test_setup_errors_reach_the_operator_log_naming_the_signal`, `test_result_output_capability_carries_the_robot_pick_check_settings`, `test_enabled_pick_check_with_an_unregistered_preset_refuses_setup_and_names_it`, `test_enabled_pick_check_without_calibration_names_the_reason`, `test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role`, `test_task_localization_incomplete_bindings_end_runtime_faulted_and_invalid` |
| T-Any-Setup-2 | any | valid; `m_startupSelectionFault` non-empty (PLC snapshot commanded a refused index) | A-SETUP: `STARTUP_REFUSED`, `m_cycleState = Faulted` (AS-06), `emit runtimeFault(...)`; then `SETUP_STATUS` (the status outputs carry the **project default** that stayed bound), connect requests; `allRequiredRolesHealthy()` may be true but G-READY refuses on the latch | `Faulted` | `STARTUP_REFUSED`, `SETUP_STATUS` | `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal`, `test_an_index_that_names_nothing_reports_not_registered_not_lost` (3), `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready` |
| T-Any-Setup-3 | any | valid; no startup refusal; `allRequiredRolesHealthy()` already true after the three `requestRoleConnectNow()` calls (devices already `Connected`) | A-SETUP → `SETUP_STATUS` → G-READY("Runtime ready.") | `ReadyForTrigger` | `SETUP_STATUS`, `READY` | `test_the_plc_value_stream_survives_binding_the_fixed_roles` (second `setup()` from a ready runtime); with virtual devices connection is queued, so most fixtures land on T-Any-Setup-4 and reach `ReadyForTrigger` through E09 |
| T-Any-Setup-4 | any | valid; no startup refusal; some role not yet `Connected` | A-SETUP → `SETUP_STATUS`; `appendTaskLog("INFO", "Runtime setup valid. Waiting for device connections.")` | `NotReady` | `SETUP_STATUS` | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` and every other fixture that waits for `bTaskReady` after `setup()`; `test_setup_resolves_the_first_camera_and_group_when_the_context_asks_for_the_default`, `test_setup_adopts_the_camera_the_plc_commands_over_the_project_default`, `test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped`, `test_a_plc_with_no_snapshot_is_not_read_as_holding_zero`, `test_setup_announces_the_active_selection_on_status_signals`, `test_status_signals_with_no_tag_emit_but_do_not_write`, `test_an_unmapped_optional_signal_is_valid_and_warns`, `test_an_unmapped_error_reset_is_supported_and_does_not_fail_setup`, `test_disabled_pick_check_leaves_setup_valid`, `test_task_localization_begin_runtime_emits_runtime_started_after_runners_exist`, `test_build_runtime_context_leaves_the_selection_for_setup_to_resolve`, `test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it`, `test_vision_output_family_cell_still_starts_with_the_pick_check_commissioned` |

#### E09 `RoleHealthy(role)`

Common prefix in `handleRoleStatusChanged()`: no context for the role → return (NO-OP); context
with a null `runner` (`QPointer` expired) → context removed, return. Then bookkeeping:
`wasRecovering = retryCount > 0 || retryScheduled`; `retryCount = 0`, `retryScheduled = false`,
`reportedStatus = Connected`, `outageStartedAt` cleared; if `wasRecovering` →
`appendTaskLog("INFO", buildRecoveryReadyMessage(...))`. Then the state-dependent part below.

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-NR-RoleHealthy-1 | `NotReady` | `allRequiredRolesHealthy()` after this role's device reports `Connected` | prefix; G-READY("Runtime ready.") | `ReadyForTrigger` if gate passes, else `NotReady` (e.g. `m_valid == false` cannot occur here since invalid setups never request connects, but a pending latch from T-Any-Setup-2 leaves it in `Faulted`, not here) | `READY` (gate) | every fixture that waits for `bTaskReady` after `setup()` (see T-Any-Setup-4); `test_vision_output_family_cell_still_starts_with_the_pick_check_commissioned` |
| T-NR-RoleHealthy-2 | `NotReady` | some other role still unhealthy | prefix only | `NotReady` | none | same fixtures, for the first two of three connections |
| T-RFT-RoleHealthy-1 | `ReadyForTrigger` | all healthy (a redundant `Connected`) | prefix; G-READY → `READY` republished, `runtimeReady` re-emitted | `ReadyForTrigger` | `READY` | — |
| T-RUN-RoleHealthy-1 | `Running` | `m_cycleState == Running` | prefix; return before the re-arm | `Running` | none | — |
| T-WTR-RoleHealthy-1 | `WaitingTriggerReset` | `m_cycleState == WaitingTriggerReset` | prefix; return | `WaitingTriggerReset` | none | — |
| T-REC-RoleHealthy-1 | `Recovering` | all healthy | prefix; G-READY → passes unless a latch / invalid group / calibration blocks. `m_faultRecoverTimer` is **not** cancelled: if `Recovering` was a resting cycle fault, E17 still fires later in `ReadyForTrigger` (T-RFT-AutoRecover-1) | `ReadyForTrigger` (or `Recovering`) | `READY` (gate) | `test_localization_role_outage_retries_forever_without_faulting`, `test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault`, `test_localization_runtime_vision_output_loss_outside_a_cycle_withdraws_ready_without_fault` |
| T-REC-RoleHealthy-2 | `Recovering` | another role still unhealthy | prefix only | `Recovering` | none | — |
| T-FLT-RoleHealthy-1 | `Faulted` | all healthy | prefix; G-READY — refused for every `Faulted` cause that exists today (latch, committed invalid group, uncalibrated camera, `m_valid == false`), so the state holds; the code path is not structurally blocked | `Faulted` (or `ReadyForTrigger` if some future `Faulted` cause left the gate clean) | `READY` (gate) | — |
| T-FLT-RoleHealthy-2 | `Faulted` | another role unhealthy | prefix only | `Faulted` | none | — |

#### E10 `RoleUnhealthy(role, status)`

Same context prefix as E09 (missing context → NO-OP; expired runner → context removed).

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-RoleUnhealthy-1 | `Running` | `m_cycleState == Running` | A-ABORT with the role code: Camera → `CameraLost` 100 "Active camera lost during localization cycle."; VisionOutput → `VisionOutputLost` 200; PrimaryPlc → `PlcLost` 300. Then A-RETRY(role, status): the `ReadyForTrigger`-only branch does not fire (state is now `WaitingTriggerReset`/`Recovering`), so no second `bTaskReady=false`; retry scheduled if recoverable | `WaitingTriggerReset` (trigger high) or `Recovering` (+ auto-recover armed) | `CYCLE_FAULT(100/200/300)` | `test_localization_runtime_camera_loss_faults_running_cycle`, `test_localization_runtime_plc_loss_faults_running_cycle`, `test_localization_runtime_vision_output_loss_faults_running_cycle` (all with trigger high → `WaitingTriggerReset`); trigger-low branch — |
| T-RUN-RoleUnhealthy-2 | `Running` | as above but `decideRecoveryAction()` returns `Ignore` (status `Disconnected` / `NoConnection` / `Connecting`, or a retry already pending) | A-ABORT as above; no retry scheduled, no `runtimeRecovering` progress message beyond the abort's own | `WaitingTriggerReset` or `Recovering` | `CYCLE_FAULT` | — |
| T-RFT-RoleUnhealthy-1 | `ReadyForTrigger` | not `Running`; A-RETRY → `RetryScheduled`; `retryCount == 0` | A-RETRY: outage stamped, WARN task log, `m_cycleState = Recovering` (AS-14), publish `bTaskReady=false`, `runtimeRecovering(progress)` (status is new), retry scheduled | `Recovering` | `RECOVERING` | `test_localization_role_outage_retries_forever_without_faulting`, `test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault`, `test_localization_runtime_vision_output_loss_outside_a_cycle_withdraws_ready_without_fault` |
| T-RFT-RoleUnhealthy-2 | `ReadyForTrigger` | A-RETRY → `Ignore` (non-recoverable status such as `Disconnected` after a `requestDisconnect()`, or `Connecting`) | nothing: no log, no publish, no transition. `bTaskReady` stays 1 and a subsequent E01 runs `startCycle()`, whose checks do not include role health | `ReadyForTrigger` | none | — |
| T-RFT-RoleUnhealthy-3 | `ReadyForTrigger` | A-RETRY → `RetryScheduled` with `retryCount > 0` (a second recoverable status for the same outage after the previous retry fired and failed) | retry re-scheduled; `runtimeRecovering` only if the status changed; the `ReadyForTrigger` branch is skipped because `retryCount != 0` → **no** `Recovering` transition, `bTaskReady` untouched | `ReadyForTrigger` | none | — (UNSPECIFIED whether reachable: needs a reconnect attempt to have failed while the state was re-armed to `ReadyForTrigger` through another path, e.g. T-REC-RoleHealthy-1 for a different role) |
| T-NR-RoleUnhealthy-1 | `NotReady` | A-RETRY → `RetryScheduled` | outage bookkeeping, WARN log on the first attempt, retry scheduled; no transition (state is not `ReadyForTrigger`); this is the ordinary "device refuses to connect at start" path | `NotReady` | none | — |
| T-NR-RoleUnhealthy-2 | `NotReady` | A-RETRY → `Ignore` | nothing | `NotReady` | none | — |
| T-WTR-RoleUnhealthy-1 | `WaitingTriggerReset` | `RetryScheduled` | bookkeeping + retry; no transition, no publish | `WaitingTriggerReset` | none | — |
| T-WTR-RoleUnhealthy-2 | `WaitingTriggerReset` | `Ignore` | nothing | `WaitingTriggerReset` | none | — |
| T-REC-RoleUnhealthy-1 | `Recovering` | `RetryScheduled` (first loss of a second role, or the same role after a failed attempt) | bookkeeping + retry; `runtimeRecovering` if the status is new for that role | `Recovering` | none | `test_localization_role_outage_retries_forever_without_faulting` (`LostConnected` then `ConnectFailed` on each failed reconnect) |
| T-REC-RoleUnhealthy-2 | `Recovering` | `Ignore` (typically: retry already scheduled) | nothing | `Recovering` | none | same test (statuses arriving while a retry is pending) |
| T-FLT-RoleUnhealthy-1 | `Faulted` | `RetryScheduled` | bookkeeping + retry; no transition, no publish | `Faulted` | none | — |
| T-FLT-RoleUnhealthy-2 | `Faulted` | `Ignore` | nothing | `Faulted` | none | — |

#### E11 `GrabFinished(result)`

`onCameraGrabFinished()` first disconnects `m_cameraGrabConnection` and `m_cameraCommandConnection`
unconditionally, then checks the state.

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-GrabFinished-1 | `Running` | `result.isGrabSuccess && !result.frame.empty()`; `snapshotActivePatternGroup()` non-null | `timings.grabFinishedMs = cycleElapsedMs()`; `rawImage = frame.clone()`; `emit runtimeMatchingRequested(m_activeCycleId, group, m_activeCameraWorkspace, frame.clone(), m_pickingChecker)` | `Running` | none | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_setup_applies_the_active_camera_workspace_before_the_first_cycle`, `test_a_camera_with_no_workspace_keeps_the_default` |
| T-RUN-GrabFinished-2 | `Running` | `!isGrabSuccess || frame.empty()` | stamp `grabFinishedMs`; A-ABORT(`CameraGrabTimeout` 102, "Camera grab failed or returned an empty frame.") | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(102)` | `test_localization_runtime_grab_timeout_faults_without_vision_output`, `test_faulted_cycle_carries_only_the_stages_it_reached` |
| T-RUN-GrabFinished-3 | `Running` | grab ok; `snapshotActivePatternGroup()` null | stamp; A-ABORT(`PatternNotRegistered` 400, "Active pattern group snapshot failed.") | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(400)` | — |
| T-FLT-GrabFinished-1 | `Faulted` | `m_cycleState != Running` | connections dropped; return. Reachable: E06 while `Running` leaves the single-shot connection alive (T-Any-TypeMismatch-1) | `Faulted` | none | — (IGNORED, silent) |
| T-NR/RFT/WTR/REC-GrabFinished | those four | — | **UNREACHABLE**: the only connection to this slot is the `Qt::SingleShotConnection` made in `startCycle()`, and every exit from `Running` other than E06 (A-ABORT, `onVisionOutputResultFinished()` after the shot already fired, `resetRuntimeBindings()`) disconnects it | — | — | — |

#### E12 `GrabCommandFinished(result)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-GrabCommand-1 | `Running` | `result.kind == CameraSingleShot && status == Failed && code == TimedOut` (the runner's `kSingleShotTimeoutMs` watchdog) | A-ABORT(`CameraGrabTimeout` 102, `result.message`); `timings.grabFinishedMs` stays unset | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(102)` | — |
| T-RUN-GrabCommand-2 | `Running` | any other result (`Failed` with `DeviceError` — the runner's final grab failure, which is followed by E11 — or a non-`SingleShot` kind, or `Succeeded`) | return | `Running` | none | `test_localization_runtime_grab_timeout_faults_without_vision_output` exercises the `DeviceError` ignore followed by T-RUN-GrabFinished-2 (partial) |
| T-FLT-GrabCommand-1 | `Faulted` | `m_cycleState != Running` | return (the `Qt::UniqueConnection` survives E06) | `Faulted` | none | — (IGNORED) |
| T-NR/RFT/WTR/REC-GrabCommand | those four | — | **UNREACHABLE**: connection exists only between `startCycle()` and the earlier of `onCameraGrabFinished()` / A-ABORT / `resetRuntimeBindings()`, none of which leave the machine in these states with the connection alive except through E06 | — | — | — |

#### E13 `MatchFinished(cycleId, result)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-MatchFinished-1 | `Running` | `cycleId == m_activeCycleId`; `buildVisionOutputPositions()` reports no fault (calibrator calibrated); `visionOutputRunner()` non-null and `supportsResultOutput()` | `timings.matchingFinishedMs`; fill `m_pendingCycleResult` (`detectedNumber = totalPossiblePicking`, `sentNumber = positions.size()` ≤ 2, `matchingTimeMs`, `lowArea`, `displayImage`, `matchResult`, `rows`); connect `resultRequestFinished` (`Qt::SingleShotConnection`); `requestSendResult(positions)` | `Running` | none | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_successful_cycle_stamps_every_stage_monotonically`, `test_localization_runtime_ready_and_cycle_outputs_write_plc_tags` |
| T-RUN-MatchFinished-2 | `Running` | id matches; `buildVisionOutputPositions()` sets `CalibrationInvalid` (calibrator for `m_activeCameraNumber` not calibrated) | stamp; A-ABORT(401, "Failed to convert match result to world coordinates.") | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(401)` | — |
| T-RUN-MatchFinished-3 | `Running` | id matches; no vision-output runner or `!supportsResultOutput()` | stamp; fill result; A-ABORT(`VisionOutputLost` 200, "Vision output runner is not available.") | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(200)` | — |
| T-RUN-MatchFinished-4 | `Running` | `cycleId != m_activeCycleId` (stale result after an abort-and-restart) | return, silently | `Running` | none | — |
| T-NR-MatchFinished-1 | `NotReady` | `m_cycleState != Running` (and, after any A-ABORT, the id also mismatches) | return silently. Reachable: a late worker result after `setup()` | `NotReady` | none | — (IGNORED, silent) |
| T-RFT-MatchFinished-1 | `ReadyForTrigger` | same | return | `ReadyForTrigger` | none | — (IGNORED) |
| T-WTR-MatchFinished-1 | `WaitingTriggerReset` | same | return. Reachable: role lost mid-match (T-RUN-RoleUnhealthy-1) then the worker answers | `WaitingTriggerReset` | none | `test_localization_runtime_plc_loss_faults_running_cycle` sets it up but never delivers the result (partial) |
| T-REC-MatchFinished-1 | `Recovering` | same | return | `Recovering` | none | — (IGNORED) |
| T-FLT-MatchFinished-1 | `Faulted` | same | return | `Faulted` | none | — (IGNORED) |

#### E14 `SendFinished(ok, message)`

`onVisionOutputResultFinished()` first disconnects `m_visionOutputResultConnection`
unconditionally.

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-RUN-SendFinished-1 | `Running` | `ok`; `m_lastExecuteTrigger == true` | `timings.sendFinishedMs`; `m_consecutiveAutoRecoveries = 0`; `publishCycleSuccessOutputs()` (`CYCLE_OK`); `timings.outputsPublishedMs`; `emit cycleResultUpdated`; `appendTaskLog("INFO", "Vision output sent. detected=… sent=… time=… ms. cycle=…")`; `LOG_DEV_INFO`; `m_cycleState = WaitingTriggerReset` (AS-15); `appendTaskLog("WARN", "Waiting for execute trigger reset.")` | `WaitingTriggerReset` | `CYCLE_OK` | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_successful_cycle_stamps_every_stage_monotonically`, `test_localization_runtime_ready_and_cycle_outputs_write_plc_tags`, `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`, `test_task_localization_runs_a_full_cycle_through_the_real_matching_worker` |
| T-RUN-SendFinished-2 | `Running` | `ok`; `m_lastExecuteTrigger == false` | as above up to the log; `m_cycleState = ReadyForTrigger` **assigned directly** (AS-15, PQ-11); then G-READY("Localization cycle completed."): if the gate passes, `READY` is published and `runtimeReady` emitted; if it refuses (latch, unhealthy role, invalid group/calibration — none of which A-START checked), the state **stays `ReadyForTrigger` with `bTaskReady=0`** and the next E01 starts a cycle | `ReadyForTrigger` | `CYCLE_OK`, then `READY` (gate) | — |
| T-RUN-SendFinished-3 | `Running` | `!ok` | stamp `sendFinishedMs`; A-ABORT(`VisionOutputSendFailed` 201, `message`) | `WaitingTriggerReset` / `Recovering` | `CYCLE_FAULT(201)` | `test_localization_runtime_vision_output_failure_faults_cycle`, `faultCycleAndReleaseTrigger()` (used by the two recovery tests) |
| T-FLT-SendFinished-1 | `Faulted` | `m_cycleState != Running` | connection dropped; return. Reachable via E06 during the send | `Faulted` | none | — (IGNORED) |
| T-NR/RFT/WTR/REC-SendFinished | those four | — | **UNREACHABLE**: the single-shot connection is made in `onRuntimeMatchingFinished()` (only while `Running`) and removed by A-ABORT / `resetRuntimeBindings()` / its own firing | — | — | — |

#### E15 `HandshakeWriteFinished(id, ok, message)`

`onPlcWriteFinished()` reads no state. Which cell a completion lands in is decided by the tracked
set, not by `m_cycleState`; the three rows below apply from every state, with the state-specific
result of the escalation given in the last row.

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-WriteFinished-1 | any | `id` not in `m_pendingWrites` (advisory signal; write issued while the PLC role was not `Connected`; write issued under `m_plcWriteEscalating`; or `ok` for a tracked id) | return, or forget the entry | unchanged | none | `test_an_advisory_write_failure_does_not_retry_and_does_not_abort`, `test_a_handshake_write_that_succeeds_on_retry_raises_no_fault` |
| T-Any-WriteFinished-2 | any | tracked, `!ok`, `write.attempts < kPlcWriteRetryBudget` (3) | `LOG_DEV_INFO "Handshake write failed; retrying."`; id appended to `m_plcWriteRetryQueue`; entry kept; `m_plcWriteRetryTimer.start(kPlcWriteRetryDelayMs)` if not active | unchanged | none | `test_a_handshake_write_that_succeeds_on_retry_raises_no_fault` |
| T-Any-WriteFinished-3 | any | tracked, `!ok`, `attempts == 3` | `escalatePlcWriteFailure()`: `LOG_USER_ERR` detail; `m_plcWriteEscalating = true`; A-ABORT(`PlcWriteFailed` 301, detail) — **regardless of state**; `m_plcWriteEscalating = false`; `emit runtimeFault(detail)` while `m_cycleState` is `WaitingTriggerReset` or `Recovering`, never `Faulted`. Per state: from `Running` the in-flight cycle is aborted; from `ReadyForTrigger` (a `READY` write failing) or `NotReady` there is no cycle, yet `CYCLE_FAULT(301)` with `bMatchingFinished=1` is published and `cycleResultUpdated` emitted; from `WaitingTriggerReset` the state is re-entered over `CYCLE_OK`; from `Recovering` (a `FAULT_CLEAR` or `READY` write failing) the auto-recover timer is re-armed → after 2000 ms `READY` is written again and, if the link still refuses, the sequence repeats every ~2 s; from `Faulted` (an `INDEX_REFUSED_*` write failing) the state leaves `Faulted` while the latch stays set | `WaitingTriggerReset` if `m_lastExecuteTrigger`, else `Recovering` | `CYCLE_FAULT(301)` | `test_a_handshake_write_retried_to_exhaustion_aborts_with_301` (from the ready path, i.e. `NotReady`/`ReadyForTrigger`), `test_a_link_that_refuses_everything_escalates_once_and_stops`; from `Running` / `WaitingTriggerReset` / `Recovering` / `Faulted` — |

#### E16 `PlcWriteRetryTimeout`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-WriteRetry-1 | any | timer fired; for each id in `m_plcWriteRetryQueue` still present in `m_pendingWrites` (A-ABORT's `clearPendingWrites()` empties both) and `primaryPlcRunner()` non-null | `reissueTrackedWrite(id)`: erase the entry, re-issue the same tag/value through `requestWriteDigitalIo()` / `requestWriteWordIo()` under a new id, `trackHandshakeWrite(newId, …, attempts + 1)` (which again requires the PLC device `Connected` and `!m_plcWriteEscalating`) | unchanged | the re-issued tag only | `test_a_handshake_write_that_succeeds_on_retry_raises_no_fault`, `test_a_handshake_write_retried_to_exhaustion_aborts_with_301` |

#### E17 `FaultAutoRecoverTimeout`

Common body (constructor lambda): `code = m_pendingCycleResult.faultCode`;
`++m_consecutiveAutoRecoveries`; `LOG_USER_WARN "Repeating auto-recovered localization fault."`
when the count is a multiple of `kAutoRecoverWarnStride` (5), else `LOG_DEV_INFO`; then
A-RECOVER("Fault auto-cleared after 2000 ms (no bErrorReset received).").

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-REC-AutoRecover-1 | `Recovering` | timer armed by A-ABORT (trigger low) or T-WTR-TriggerFall-1; not cancelled by E03, A-START or `setup()` | body; G-READY | `ReadyForTrigger` if gate passes, else `Recovering` (role outage in progress, or a latch) with `bTaskFault=0` published | `FAULT_CLEAR`, `READY` (gate) | `test_localization_runtime_fault_auto_clears_without_error_reset` (pass branch); refuse branch — |
| T-RFT-AutoRecover-1 | `ReadyForTrigger` | timer still armed after the machine was re-armed by another path (T-REC-RoleHealthy-1, T-REC-SelectCamera-3, T-REC-SelectGroup-3) | body; `FAULT_CLEAR` republished; state neither `Faulted` nor `Recovering` → no G-READY; `m_consecutiveAutoRecoveries` incremented although the runtime was already ready | `ReadyForTrigger` | `FAULT_CLEAR` | — |
| T-FLT-AutoRecover-1 | `Faulted` | timer armed in `Recovering`, then E04/E05/E06 moved to `Faulted` without cancelling it | body; `FAULT_CLEAR` published (`bTaskFault=0`, `nFaultCode=0` while the refusal latch holds); G-READY refuses on the latch | `Faulted` | `FAULT_CLEAR` | — |
| T-WTR-AutoRecover-1 | `WaitingTriggerReset` | timer armed in `Recovering`; E01 ignored there but sets `m_lastExecuteTrigger = true`; then T-Any-WriteFinished-3 runs A-ABORT → `WaitingTriggerReset` without cancelling the timer | body; `FAULT_CLEAR`; `m_pendingCycleResult.faulted = false` so the next E02 takes the clean branch; no G-READY (state guard) | `WaitingTriggerReset` | `FAULT_CLEAR` | — |
| T-NR-AutoRecover / T-RUN-AutoRecover | `NotReady`, `Running` | — | **UNREACHABLE**: `setup()` (the only path into `NotReady` other than T-WTR-TriggerFall-2, which requires the timer to have fired or E03 to have cancelled it) calls `cancelFaultAutoRecovery()` through `resetRuntimeBindings()`; `startCycle()` (the only path into `Running`) calls it directly | — | — | — |

#### E18 `RoleReconnectTimeout(role)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-Reconnect-1 | any | lambda bound to `this`; context for the role still present (else return); in `requestRoleConnectNow()`: runner non-null (else context removed) | `retryScheduled = false`; `context.runner->requestConnect()`; `LOG_USER_WARN` / `LOG_DEV_INFO "Recovery reconnect requested."` per `shouldReportRetryToUser()` | unchanged (the outcome arrives later as E09 / E10) | none | `test_localization_role_outage_retries_forever_without_faulting` |

#### E19 `RoleError(role, message)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-RoleError-1 | any | `m_lastRoleError[role] != message` | `LOG_USER_WARN "<role> runtime error: …"`; `appendTaskLog("ERROR", "<role> error: …")`; message remembered. Repeat of the same message → `LOG_DEV_INFO` only. No state or output effect. | unchanged | none | — (`test_every_runner_family_forwards_device_errors_to_the_controller` stops at the runner signal and does not observe the controller) |

#### E20 `ConfigureRequested(config)`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-Configure-1 | any | — | `m_config = config`; `m_signalMapper.configure(config)`. No state read, nothing published. **NO-OP** for the state machine (the mapping used by subsequent `handlePlcValues()` / `publish*Signal()` calls changes). | unchanged | none | `test_setup_applies_the_active_camera_workspace_before_the_first_cycle` (calls `configure()` before `setup()`) |

#### E21 `SetRecoveryPolicies`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-Policies-1 | any | — | three members replaced; bound contexts keep their copies until the next `bindRoleContext()`. **NO-OP** for the state machine. | unchanged | none | `test_localization_role_outage_retries_forever_without_faulting` |

#### E22 `TeardownRequested`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-Teardown-1 | any | — | `TaskLocalization::destroyRuntimeController()` disconnects every task↔controller connection and deletes the object (`deleteLater()` on its thread, or synchronous `delete`). The controller has no stop method and publishes nothing: member timers die with it; the `QTimer::singleShot(…, this, …)` reconnect closures die with it; runner→controller connections are dropped by QObject destruction. Pending PLC writes already queued in `PlcRunner` are not withdrawn. | object destroyed | none (no `STOP` snapshot exists) | every `TaskLocalizationRuntimeFixture` destructor (`endRuntime()` + `stopAll()`), not asserted |

#### E23 `LevelSampleNoEdge`

| ID | From | Guard | Actions | To | Outputs | Test |
|---|---|---|---|---|---|---|
| T-Any-LevelSample-1 | any | `bExecuteTrigger` sample equal to `m_lastExecuteTrigger`, or `bErrorReset` sample not a rising edge | `m_lastExecuteTrigger` / `m_lastErrorReset` assigned (same value, or `false` on a reset falling edge). **NO-OP.** `signalChanged` was already emitted by the adapter. | unchanged | none | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` (second `M10=true` while `WaitingTriggerReset`: asserts no second cycle) |

### 3b. Completeness grid

6 states × 23 events = **138** cells. A cell holds the row id(s) that apply, followed by the
cell class: **A** = at least one row with an action or transition; **I** = IGNORED (state checked,
event dropped; log quoted in the row if any); **N** = NO-OP (state not read, nothing observable);
**U** = UNREACHABLE (reason in the row); **S** = UNSPECIFIED.

| Event | `NotReady` | `ReadyForTrigger` | `Running` | `WaitingTriggerReset` | `Recovering` | `Faulted` |
|---|---|---|---|---|---|---|
| E01 TriggerRise | T-NR-TriggerRise-1 · I | T-RFT-TriggerRise-1..4 · A | T-RUN-TriggerRise-1 · I | T-WTR-TriggerRise-1 · U | T-REC-TriggerRise-1 · I | T-FLT-TriggerRise-1 · I |
| E02 TriggerFall | T-NR-TriggerFall-1 · I | T-RFT-TriggerFall-1 · I | T-RUN-TriggerFall-1 · I | T-WTR-TriggerFall-1,2 · A | T-REC-TriggerFall-1 · I | T-FLT-TriggerFall-1 · I |
| E03 ErrorResetRise | T-NR-ErrorResetRise-1 · A | T-RFT-ErrorResetRise-1 · A | T-RUN-ErrorResetRise-1 · A | T-WTR-ErrorResetRise-1 · A | T-REC-ErrorResetRise-1 · A | T-FLT-ErrorResetRise-1 · A |
| E04 SelectCamera | T-NR-SelectCamera-1..3 · A | T-RFT-SelectCamera-1..4 · A | T-RUN-SelectCamera-1 · I | T-WTR-SelectCamera-1..3 · A | T-REC-SelectCamera-1..3 · A | T-FLT-SelectCamera-1..3 · A |
| E05 SelectPatternGroup | T-NR-SelectGroup-1..3 · A | T-RFT-SelectGroup-1..4 · A | T-RUN-SelectGroup-1 · I | T-WTR-SelectGroup-1..3 · A | T-REC-SelectGroup-1..3 · A | T-FLT-SelectGroup-1..3 · A |
| E06 IndexTypeMismatch | T-Any-TypeMismatch-1 · A | T-Any-TypeMismatch-1 · A | T-Any-TypeMismatch-1 · A | T-Any-TypeMismatch-1 · A | T-Any-TypeMismatch-1 · A | T-Any-TypeMismatch-1 · A |
| E07 ManualExecute | T-NR-ManualExecute-1 · I | T-RFT-ManualExecute-1,2 · A | T-RUN-ManualExecute-1 · I | T-WTR-ManualExecute-1 · I | T-REC-ManualExecute-1 · I | T-FLT-ManualExecute-1 · I |
| E08 SetupRequested | T-Any-Setup-1..4 · A | T-Any-Setup-1..4 · A | T-Any-Setup-1..4 · A | T-Any-Setup-1..4 · A | T-Any-Setup-1..4 · A | T-Any-Setup-1..4 · A |
| E09 RoleHealthy | T-NR-RoleHealthy-1,2 · A | T-RFT-RoleHealthy-1 · A | T-RUN-RoleHealthy-1 · A (bookkeeping only) | T-WTR-RoleHealthy-1 · A (bookkeeping only) | T-REC-RoleHealthy-1,2 · A | T-FLT-RoleHealthy-1,2 · A |
| E10 RoleUnhealthy | T-NR-RoleUnhealthy-1,2 · A | T-RFT-RoleUnhealthy-1..3 · A | T-RUN-RoleUnhealthy-1,2 · A | T-WTR-RoleUnhealthy-1,2 · A | T-REC-RoleUnhealthy-1,2 · A | T-FLT-RoleUnhealthy-1,2 · A |
| E11 GrabFinished | · U | · U | T-RUN-GrabFinished-1..3 · A | · U | · U | T-FLT-GrabFinished-1 · I |
| E12 GrabCommandFinished | · U | · U | T-RUN-GrabCommand-1,2 · A | · U | · U | T-FLT-GrabCommand-1 · I |
| E13 MatchFinished | T-NR-MatchFinished-1 · I | T-RFT-MatchFinished-1 · I | T-RUN-MatchFinished-1..4 · A | T-WTR-MatchFinished-1 · I | T-REC-MatchFinished-1 · I | T-FLT-MatchFinished-1 · I |
| E14 SendFinished | · U | · U | T-RUN-SendFinished-1..3 · A | · U | · U | T-FLT-SendFinished-1 · I |
| E15 HandshakeWriteFinished | T-Any-WriteFinished-1..3 · A | T-Any-WriteFinished-1..3 · A | T-Any-WriteFinished-1..3 · A | T-Any-WriteFinished-1..3 · A | T-Any-WriteFinished-1..3 · A | T-Any-WriteFinished-1..3 · A |
| E16 PlcWriteRetryTimeout | T-Any-WriteRetry-1 · A | T-Any-WriteRetry-1 · A | T-Any-WriteRetry-1 · A | T-Any-WriteRetry-1 · A | T-Any-WriteRetry-1 · A | T-Any-WriteRetry-1 · A |
| E17 FaultAutoRecoverTimeout | · U | T-RFT-AutoRecover-1 · A | · U | T-WTR-AutoRecover-1 · A | T-REC-AutoRecover-1 · A | T-FLT-AutoRecover-1 · A |
| E18 RoleReconnectTimeout | T-Any-Reconnect-1 · A | T-Any-Reconnect-1 · A | T-Any-Reconnect-1 · A | T-Any-Reconnect-1 · A | T-Any-Reconnect-1 · A | T-Any-Reconnect-1 · A |
| E19 RoleError | T-Any-RoleError-1 · A (log only) | same · A | same · A | same · A | same · A | same · A |
| E20 ConfigureRequested | T-Any-Configure-1 · N | · N | · N | · N | · N | · N |
| E21 SetRecoveryPolicies | T-Any-Policies-1 · N | · N | · N | · N | · N | · N |
| E22 TeardownRequested | T-Any-Teardown-1 · A | same · A | same · A | same · A | same · A | same · A |
| E23 LevelSampleNoEdge | T-Any-LevelSample-1 · N | · N | · N | · N | · N | · N |

**Counts.** A (action/transition rows): 81 · IGNORED: 24 · NO-OP: 18 · UNREACHABLE: 15 ·
UNSPECIFIED: 0. Sum: 81 + 24 + 18 + 15 + 0 = **138** = 6 × 23.

Per-event check: E01 1A/4I/1U · E02 1A/5I · E03 6A · E04 5A/1I · E05 5A/1I · E06 6A ·
E07 1A/5I · E08 6A · E09 6A · E10 6A · E11 1A/1I/4U · E12 1A/1I/4U · E13 1A/5I · E14 1A/1I/4U ·
E15 6A · E16 6A · E17 4A/2U · E18 6A · E19 6A · E20 6N · E21 6N · E22 6A · E23 6N.

No cell needed UNSPECIFIED. The two open questions found are recorded where they sit: whether
E01 can be **delivered** with the trigger already high at the first poll is a device/adapter
question (S-04, §5), and the reachability of T-RFT-RoleUnhealthy-3 is noted inside an otherwise
determined cell.

### 3c. Assignment-site index

Every `m_cycleState =` site in `localization_runtime_controller.cpp` (15 found, matching the
handoff), the rows that exercise it, and the reason as the code itself names it (fault-code name
or message). The last column is the descriptive name only; the reason token is assigned in
WP-10, not here.

| Site | Function | Assigns | Rows | Reason as named in code |
|---|---|---|---|---|
| AS-01 | `setActiveCameraNumber()` (refusal) | `Faulted` | T-{NR,RFT,WTR,REC,FLT}-SelectCamera-1 | `IndexVerdict::OutOfRange` / `NotRegistered`; `CameraNotRegistered` (103) |
| AS-02 | `setActiveCameraNumber()` (uncalibrated) | `Faulted` | T-{NR,RFT,WTR,REC,FLT}-SelectCamera-2 | "Active camera calibration is invalid."; `CalibrationInvalid` (401) |
| AS-03 | `setActivePatternGroupNumber()` (out of range) | `Faulted` | T-{NR,RFT,WTR,REC,FLT}-SelectGroup-1 | `IndexVerdict::OutOfRange`; `PatternNotRegistered` (400) |
| AS-04 | `setActivePatternGroupNumber()` (committed, invalid) | `Faulted` | T-{NR,RFT,WTR,REC,FLT}-SelectGroup-2 | "No pattern group is registered for number n."; `PatternNotRegistered` (400) |
| AS-05 | `setup()` (start) | `NotReady` | T-Any-Setup-1..4 | unconditional reset |
| AS-06 | `setup()` (valid, startup selection refused) | `Faulted` | T-Any-Setup-2 | `m_startupSelectionFault`; 103 if camera latch else 400 |
| AS-07 | `setup()` (invalid) | `Faulted` | T-Any-Setup-1 | `SetupResult::errors` |
| AS-08 | `handlePlcValues()` (falling edge, faulted) | `Recovering` | T-WTR-TriggerFall-1 | `m_pendingCycleResult.faulted` |
| AS-09 | `handlePlcValues()` (falling edge, clean) | `NotReady` | T-WTR-TriggerFall-2 | "Trigger reset. Runtime ready." |
| AS-10 | `markRuntimeReady()` | `ReadyForTrigger` | every row that ends in G-READY passing | the `message` argument |
| AS-11 | `reportSignalTypeMismatch()` | `Faulted` | T-Any-TypeMismatch-1 | "did not read as a number"; 103 / 400 |
| AS-12 | `startCycle()` | `Running` | T-RFT-TriggerRise-1, T-RFT-ManualExecute-1 | "Trigger accepted. Localization cycle started." |
| AS-13 | `abortCycle()` | `WaitingTriggerReset` / `Recovering` | every row that calls A-ABORT: T-RFT-TriggerRise-2..4, T-RFT-ManualExecute-2, T-RUN-RoleUnhealthy-1,2, T-RUN-GrabFinished-2,3, T-RUN-GrabCommand-1, T-RUN-MatchFinished-2,3, T-RUN-SendFinished-3, T-Any-WriteFinished-3 | the `LocalizationFaultCode` passed in |
| AS-14 | `handleRoleStatusChanged()` | `Recovering` | T-RFT-RoleUnhealthy-1 | "Connection lost: role=…" |
| AS-15 | `onVisionOutputResultFinished()` | `WaitingTriggerReset` / `ReadyForTrigger` (direct) | T-RUN-SendFinished-1, T-RUN-SendFinished-2 | "Vision output sent." / "Localization cycle completed." |

---

## 4. Output snapshots

Every value goes through `publishBoolSignal()` / `publishNumberSignal()`: `signalChanged(name,
value)` is always emitted; a PLC write is issued only when `m_signalMapper.tagForSignalName(name)`
is non-empty and `primaryPlcRunner()` is bound; the write's id is tracked
(`trackHandshakeWrite()`) only when the name is one of the five returned by `isHandshakeSignal()`
(**H** below), the PLC device is `Connected`, and `m_plcWriteEscalating` is false. Number values
are narrowed to `qint16` on the wire. Order within a snapshot is the call order in the function.
"—" = not written by that snapshot (keeps its previous value on the PLC).

### 4.1 Named `publish*Outputs()` functions

| Snapshot | Function | `nActiveCameraStatus` | `nActivePatternGroupStatus` | `bTaskReady` **H** | `bCameraValid` | `bPatternValid` | `bMatchingBusy` | `bMatchingFinished` **H** | `bMatchingDetected` | `bMatchingLowArea` | `bTaskFault` **H** | `nDetectedNumber` **H** | `nFaultCode` **H** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `READY` | `publishInitialReadyOutputs()` (order: the two status words first, then `bTaskReady`, then the rest) | `m_activeCameraNumber` | `m_activePatternGroupNumber` | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `CYCLE_START` | `publishCycleStartOutputs()` | — | — | 0 | — | — | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `CYCLE_OK` | `publishCycleSuccessOutputs(result)` | — | — | **—** (stays 0 from `CYCLE_START`) | — | — | 0 | 1 | `result.detectedNumber > 0` | `result.lowArea` | 0 | `result.detectedNumber` | 0 |
| `CYCLE_FAULT(code)` | `publishCycleFaultOutputs(code)` | — | — | 0 | — | — | 0 | 1 | 0 | 0 | 1 | 0 | `code` |

### 4.2 Ad-hoc sequences

| Snapshot | Where | Values, in order |
|---|---|---|
| `FAULT_CLEAR` | `recoverFromFault()` | `bTaskFault=0`, `nFaultCode=0` |
| `RECOVERING` | `handleRoleStatusChanged()`, first loss while `ReadyForTrigger` | `bTaskReady=0` |
| `TRIGGER_FALL` | `handlePlcValues()`, falling edge in `WaitingTriggerReset` | `bMatchingFinished=0` |
| `INDEX_REFUSED_CAM` | `setActiveCameraNumber()` refusal | `bTaskReady=0`, `bCameraValid=0`, `bTaskFault=1`, `nFaultCode=103` |
| `CAM_ACCEPT` | `setActiveCameraNumber()` accepted | `bTaskReady=0`, `nActiveCameraStatus=n`, `bCameraValid=calibrated` |
| `CAM_UNCALIBRATED` | `setActiveCameraNumber()` accepted, uncalibrated (after `CAM_ACCEPT`) | `bTaskFault=1`, `nFaultCode=401` |
| `INDEX_REFUSED_GRP` | `setActivePatternGroupNumber()` out of range | `bTaskReady=0`, `bPatternValid=0`, `bTaskFault=1`, `nFaultCode=400` |
| `GRP_ACCEPT` (valid) | `setActivePatternGroupNumber()` in range, valid | `bPatternValid=1`, `nActivePatternGroupStatus=n` |
| `GRP_ACCEPT` (invalid) | in range, `validateActivePatternGroup()` false | `bPatternValid=0`, `bTaskReady=0`, `bTaskFault=1`, `nFaultCode=400` |
| `TYPE_MISMATCH` | `reportSignalTypeMismatch()` | `bTaskReady=0`, `bCameraValid=0` or `bPatternValid=0`, `bTaskFault=1`, `nFaultCode=103` or `400` |
| `SETUP_CAM_REFUSED` | `setup()`, resolved camera number refused | `bTaskReady=0`, `bCameraValid=0`, `bTaskFault=1`, `nFaultCode=103` |
| `SETUP_GRP_REFUSED` | `setup()`, resolved group number out of range | `bTaskReady=0`, `bPatternValid=0`, `bTaskFault=1`, `nFaultCode=400` |
| `SETUP_GRP_INVALID` | `setup()`, group in range but `validateActivePatternGroup()` false | `bPatternValid=0`, `bTaskFault=1`, `nFaultCode=400` (no `bTaskReady`) |
| `SETUP_CAL_INVALID` | `setup()`, camera accepted but calibration invalid | `bCameraValid=0`, `bTaskFault=1`, `nFaultCode=401` (no `bTaskReady`) |
| `STARTUP_REFUSED` | `setup()` valid, PLC-commanded index refused | `bTaskReady=0`, `bTaskFault=1`, [`bCameraValid=0`], [`bPatternValid=0`], `nFaultCode=103` if the camera latch is set else `400` |
| `SETUP_STATUS` | `setup()` valid, after the block above | `nActiveCameraStatus=m_activeCameraNumber`, `nActivePatternGroupStatus=m_activePatternGroupNumber` |
| `STOP` | — | **does not exist**: no function publishes anything on teardown (E22); `ITask::endRuntime()` / `stopAll()` write no tags |

Facts read off the tables: `bTaskReady` returns to 1 only through `READY`; `bCameraValid` /
`bPatternValid` return to 1 only through `READY` or their own accepted-setter path; the two status
words are written in `SETUP_STATUS`, `CAM_ACCEPT`, `GRP_ACCEPT` and `READY`, and never on a
refusal; `nActiveCamera` / `nActivePatternGroup` are never written.

---

## 5. Scenario catalogue

"Then" is what the code does today, expressed in §3 rows. Field items cite
`docs/backlog/technical_debt_and_next_steps.md` / `later_todo_list.md` numbering.

| ID | Title | Given | When | Then (as implemented) | Source | Test |
|---|---|---|---|---|---|---|
| S-01 | Normal cycle, trigger released after the result | `ReadyForTrigger` | E01 → E11(ok) → E13 → E14(ok) → E02 | T-RFT-TriggerRise-1 → T-RUN-GrabFinished-1 → T-RUN-MatchFinished-1 → T-RUN-SendFinished-1 (`WaitingTriggerReset`, `CYCLE_OK`) → T-WTR-TriggerFall-2 (`NotReady` → `ReadyForTrigger`, `TRIGGER_FALL` + `READY`) | contract | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`, `test_task_localization_runs_a_full_cycle_through_the_real_matching_worker`, `test_localization_runtime_ready_and_cycle_outputs_write_plc_tags`, `test_successful_cycle_stamps_every_stage_monotonically` |
| S-02 | Held trigger after the cycle | `WaitingTriggerReset` | further `bExecuteTrigger=true` samples | E23 (T-Any-LevelSample-1): no second cycle; `bMatchingFinished` stays 1 | contract | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` |
| S-03 | Master holds camera 0 at start | PLC snapshot `nActiveCamera=0` | E08 → E09 ×3 → E04(1) | T-Any-Setup-2 (`Faulted`, `STARTUP_REFUSED`, `m_valid` true, project-default camera bound, `SETUP_STATUS` reports it) → T-FLT-RoleHealthy-1 (gate refused by the latch) → T-FLT-SelectCamera-3 (`ReadyForTrigger`, `READY`) | field 2026-09-09 (item 58) | `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`, `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal` |
| S-04 | Trigger already high at runtime start | `bExecuteTrigger` true before `setup()` | E08 → first `valueChanged` batch | Controller side is determined: `setup()` sets `m_lastExecuteTrigger = false`, so a delivered `true` is a rising edge → T-NR-TriggerRise-1 (ignored, WARN) if it arrives before the roles are healthy, or T-RFT-TriggerRise-1 (a cycle starts) if after. **UNSPECIFIED at the adapter**: whether the first poll after connect delivers an unchanged-since-connect `true` through `valueChanged` is decided per PLC family (`McProtocolDevice` diff, Modbus `takeChangedValues()`), not by the controller; not measured on hardware | audit (item 54, Phase G, WP-50) | — |
| S-05 | Result publish collides with a poll on a dual-role Modbus PLC | `Running`, send in flight | E14(`ok=false`, "transaction in flight") | T-RUN-SendFinished-3: A-ABORT(201) → `WaitingTriggerReset` (trigger high) → T-WTR-TriggerFall-1 → `Recovering` + auto-recover → T-REC-AutoRecover-1 → `ReadyForTrigger` after 2000 ms | field 2026-09-14 (item 64) | — (no fixture can bind one device to both roles; see the note above `test_a_camera_rebind_does_not_drop_the_plc_input_stream`) |
| S-06 | Vision-output heartbeat link back, main link not | `Recovering` for the VisionOutput role | E10(`Connecting`) repeatedly, no `Connected` | T-REC-RoleUnhealthy-2 / -1: `Connecting` is not recoverable under `LocalizationRecoveryPolicy::isRecoverableStatus()`, so no new retry is scheduled from it; the retry loop continues only from `LostConnected` / `ConnectFailed`; `bTaskReady=0`, `bTaskFault=0` for the whole outage | field 2026-09-09 (item 70) | — |
| S-07 | PLC connects late, after `setup()` fell back to the project default | `awaitPrimaryPlcSnapshot()` timed out (`kPlcSnapshotWaitMs`); `setup()` used `firstKey()` | E09(PLC) → later `valueChanged` carrying `nActiveCamera=2` | T-NR-RoleHealthy-1 (→ `ReadyForTrigger` on camera 1, `READY` says status 1) → T-RFT-SelectCamera-3 (camera 2 adopted, `bTaskReady` 1→0→1, status 2). Whether the late PLC's first `valueChanged` actually carries the register is the same adapter question as S-04 | owner note ("bug found") | — |
| S-08 | Role lost outside a cycle, comes back | `ReadyForTrigger` | E10(`LostConnected`) → E18 ×n → E09 | T-RFT-RoleUnhealthy-1 (`Recovering`, `RECOVERING`, `runtimeRecovering`) → T-Any-Reconnect-1 repeated at `retryIntervalMs`, `runtimeRecovering` again only on a status change → T-REC-RoleHealthy-1 (`READY`); `bTaskFault` never raised | contract | `test_localization_role_outage_retries_forever_without_faulting`, `test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault`, `test_localization_runtime_vision_output_loss_outside_a_cycle_withdraws_ready_without_fault` |
| S-09 | Role lost during a cycle | `Running`, trigger high | E10(`LostConnected`, role) → E02 → E17 | T-RUN-RoleUnhealthy-1 (`CYCLE_FAULT(100/200/300)`, `WaitingTriggerReset`, retry scheduled) → T-WTR-TriggerFall-1 (`Recovering`, timer armed) → T-REC-AutoRecover-1: `FAULT_CLEAR`, then G-READY refused while the role is still down; re-arm happens on E09 (T-REC-RoleHealthy-1) | contract | `test_localization_runtime_camera_loss_faults_running_cycle`, `test_localization_runtime_plc_loss_faults_running_cycle`, `test_localization_runtime_vision_output_loss_faults_running_cycle` (first step only) |
| S-10 | Cycle fault acknowledged by `bErrorReset` | `Recovering` after T-WTR-TriggerFall-1 | E03 | T-REC-ErrorResetRise-1: timer cancelled, `FAULT_CLEAR`, `READY` | contract | `test_localization_runtime_error_reset_clears_fault_and_rearms` |
| S-11 | Cycle fault auto-clears | same | 2000 ms elapse | T-REC-AutoRecover-1 | contract | `test_localization_runtime_fault_auto_clears_without_error_reset` |
| S-12 | Refused index, then the other index written valid | `Faulted` (camera latch) | E05(valid) → E01 → E04(valid) | T-FLT-SelectGroup-3 (gate refused; `bPatternValid=1` published, `bTaskReady` stays 0) → T-FLT-TriggerRise-1 → T-FLT-SelectCamera-3 (`READY`) | field 2026-09-07 | `test_localization_a_refused_index_is_not_forgiven_by_the_other_index`, `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` |
| S-13 | Refused index, then `bErrorReset` | `Faulted` (latch) | E03 | T-FLT-ErrorResetRise-1: `FAULT_CLEAR` published, gate refused → `Faulted` with `bTaskFault=0`, `nFaultCode=0`, `bTaskReady=0` | contract | `test_localization_error_reset_does_not_lift_a_refused_index` |
| S-14 | Non-numeric index register | `ReadyForTrigger` | E06 → E05(valid) | T-Any-TypeMismatch-1 (`Faulted`, `TYPE_MISMATCH`) → T-FLT-SelectGroup-3 (gate refused by the camera latch) | contract | `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value` |
| S-15 | Handshake write refused by a connected PLC | `NotReady`/`ReadyForTrigger`, `READY` being written | E15(`!ok`) ×3 for `bTaskReady` | T-Any-WriteFinished-2 ×2 (retries at 40 ms) → T-Any-WriteFinished-3: A-ABORT(301) with no cycle in flight → `Recovering` + timer → T-REC-AutoRecover-1 → `READY` written again → repeats while the tag stays refused; `runtimeFault` each time | contract (Phase 9 D3) | `test_a_handshake_write_retried_to_exhaustion_aborts_with_301`, `test_a_link_that_refuses_everything_escalates_once_and_stops` (bounds one escalation per exhaustion) |
| S-16 | Camera change commanded mid-cycle | `Running` | E04 | T-RUN-SelectCamera-1: WARN, dropped; the number is **not** queued for after the cycle | contract | `test_localization_runtime_rejects_camera_change_while_running` (partial) |
| S-17 | Second `setup()` on a running controller | any | E08 | T-Any-Setup-*: every binding torn down and remade; `m_activeCycleId = 0`; latches cleared; a cycle in flight is silently abandoned (no `CYCLE_FAULT`) | contract | `test_the_plc_value_stream_survives_binding_the_fixed_roles`, `test_setup_resets_the_error_reset_edge_detector` |

---

## 6. Policy registry

Decision records: "none" unless WP-01's index names one (not consulted here).

| Policy | Implemented at (symbol) | Parameters (as in code) | Test (§7) | Decision record |
|---|---|---|---|---|
| Role reconnect: unbounded, never a task fault | `LocalizationRecoveryPolicy`, `decideRecoveryAction()`, `handleRoleStatusChanged()`, `scheduleRoleReconnect()`, `requestRoleConnectNow()` | `retryIntervalMs = 5000`; `retryOnConnectFailed = true`; `retryOnLostConnected = true`; recoverable statuses exactly `ConnectFailed`, `LostConnected`; defaults `defaultCameraRecoveryPolicy()` ("camera"), `defaultPlcRecoveryPolicy()` ("primary_plc"), `defaultVisionOutputRecoveryPolicy()` ("vision_output"); `setRecoveryPolicies()` has no production caller; policy copied into the context at bind time | `test_localization_recovery_policy_defaults_match_runtime_spec`, `test_localization_recovery_policy_decision_rules`, `test_localization_role_outage_retries_forever_without_faulting` | none |
| Recovery log rate limiting | `shouldReportRetryToUser()`, `scheduleRoleReconnect()`, `requestRoleConnectNow()`, `handleRoleStatusChanged()` (`statusIsNew`) | `kQuietRetryLogStride = 12`; `runtimeRecovering` only on outage start and status change | `test_localization_role_outage_retries_forever_without_faulting` (`recoveringSpy.count() <= 2`) | none |
| Cycle fault auto-clear | `armFaultAutoRecovery()`, `cancelFaultAutoRecovery()`, `recoverFromFault()`, constructor lambda on `m_faultRecoverTimer` | `kFaultAutoRecoverMs = 2000`; `kAutoRecoverWarnStride = 5`; `m_consecutiveAutoRecoveries` reset by `onVisionOutputResultFinished()` success | `test_localization_runtime_fault_auto_clears_without_error_reset` | none |
| Fault acknowledge is an edge | `handlePlcValues()` (`bErrorReset`), `acknowledgeFault()` | rising edge only; `m_lastErrorReset` reset by `setup()` | `test_localization_runtime_error_reset_clears_fault_and_rearms`, `test_setup_resets_the_error_reset_edge_detector` | none |
| Handshake write retry and escalation | `isHandshakeSignal()`, `trackHandshakeWrite()`, `onPlcWriteFinished()`, `reissueTrackedWrite()`, `escalatePlcWriteFailure()`, `clearPendingWrites()` | `kPlcWriteRetryBudget = 3` attempts total; `kPlcWriteRetryDelayMs = 40`; tracked set = {`bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode`, `nDetectedNumber`}; untracked when the PLC device is not `Connected` or during escalation; escalation = A-ABORT(301) from any state | `test_a_handshake_write_that_succeeds_on_retry_raises_no_fault`, `test_a_handshake_write_retried_to_exhaustion_aborts_with_301`, `test_an_advisory_write_failure_does_not_retry_and_does_not_abort`, `test_a_link_that_refuses_everything_escalates_once_and_stops` | none |
| Required signals | `requiredSignalNames()`, `validateSignalMap()` | `bExecuteTrigger`, `bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode`; optional signals warn; duplicate tag = error; orphan tag = error per kind (`b…` against digital list, `n…` against word list) when the device is an `IPlcTagProvider` | `test_each_required_signal_unmapped_fails_setup_naming_it`, `test_an_unmapped_optional_signal_is_valid_and_warns`, `test_an_unmapped_error_reset_is_supported_and_does_not_fail_setup`, `test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike`, `test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal`, `test_two_signals_mapped_to_one_tag_fail_setup` | none |
| Index ranges | `validateCameraNumber()` → `TaskDeviceBinding::kMinCameraNumber` / `kMaxCameraNumber`; `validatePatternGroupNumber()` → `mtc::MatchGroup::validateIndexRange()` (`m_min_index_range` / `m_max_index_range`, static, adjustable by `setIndexRange()`) | camera 1..16; pattern group 1..32 at their definitions | `test_localization_zero_active_index_is_refused_by_the_range_check`, `test_localization_out_of_range_active_index_is_refused`, `test_setup_refuses_an_out_of_range_active_camera_with_a_range_message`, `test_setup_refuses_an_out_of_range_active_pattern_group_with_a_range_message` | none |
| Index refusal latched per signal | `m_activeCameraSelectionRejected`, `m_activePatternGroupSelectionRejected`, `markRuntimeReady()`; set in both setters, `reportSignalTypeMismatch()`, `setup()`; cleared only by an accepted value for the same signal or by `setup()` | — | `test_localization_a_refused_index_is_not_forgiven_by_the_other_index`, `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera`, `test_localization_error_reset_does_not_lift_a_refused_index`, `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value` | none |
| Startup selection from the PLC | `commandedIndexFromPlc()`, `setup()`, `TaskLocalization::awaitPrimaryPlcSnapshot()` | `TaskLocalization::kPlcSnapshotWaitMs = 2000`; null snapshot / unmapped / absent tag → project `firstKey()`; refused → latched, runtime stays valid | `test_setup_adopts_the_camera_the_plc_commands_over_the_project_default`, `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal`, `test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped`, `test_a_plc_with_no_snapshot_is_not_read_as_holding_zero`, `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`, `test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it` | none |
| Grab retry in the runner | `CameraRunner::onGrabFinished()` | `CameraRunner::kMaxGrabAttempts = 6`; `kSingleShotTimeoutMs = 8000` per attempt; `grabFinished` re-emitted once per command | `test_camera_runner_watchdog_outlasts_device_grab_timeout` | none |
| Positions per cycle | `buildVisionOutputPositions()` | literal `2` (`positions.size() < 2`); objects beyond it get `status = "Skipped"` with no reason | — (item 68) | none |
| "No match" is not a fault | `publishCycleSuccessOutputs()`, `onRuntimeMatchingFinished()` | `detectedNumber = 0` → `bMatchingDetected=0`, `bTaskFault=0` | — | none |
| Pick check refusal at setup | `rebuildPickingChecker()`, `setup()` | enabled check with no calibration or unresolved preset = hard error unless the camera index itself was refused | `test_enabled_pick_check_with_an_unregistered_preset_refuses_setup_and_names_it`, `test_enabled_pick_check_without_calibration_names_the_reason`, `test_disabled_pick_check_leaves_setup_valid` | none |
| `WaitingTriggerReset` is busy | `markRuntimeReady()` state guard | — | `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` (indirect) | none |

---

## 7. Test map

Source: `tests/architecture_contract_test/main.cpp` (157 `test_` functions). A test is
"controller-level" when it uses `LocalizationRuntimeFixture`, `TaskLocalizationRuntimeFixture`, or
asserts a §6 constant. Rows are best effort from reading each body; "partial" marks a test whose
assertions cover less than the row states, or whose state at the call depends on connect timing
(virtual devices connect through a queued `requestConnect()`, so a setter called right after
`setup()` returns runs in `NotReady` or `ReadyForTrigger` depending on scheduling).

### 7.1 Controller-level tests

| Test | Rows / snapshots / policies exercised |
|---|---|
| `test_a_handshake_write_that_succeeds_on_retry_raises_no_fault` | T-Any-Setup-4, T-NR-RoleHealthy-1, T-Any-WriteFinished-2, T-Any-WriteRetry-1, T-Any-WriteFinished-1 |
| `test_a_handshake_write_retried_to_exhaustion_aborts_with_301` | T-Any-Setup-4, T-NR-RoleHealthy-1, T-Any-WriteFinished-2 ×2, T-Any-WriteRetry-1, T-Any-WriteFinished-3 (from `ReadyForTrigger`; asserts `CYCLE_FAULT(301)` values and the ERROR log) |
| `test_an_advisory_write_failure_does_not_retry_and_does_not_abort` | T-Any-WriteFinished-1 |
| `test_a_link_that_refuses_everything_escalates_once_and_stops` | T-Any-WriteFinished-3 (bounded `runtimeFault` count) |
| `test_result_output_capability_carries_the_robot_pick_check_settings` | T-Any-Setup-1 ("cannot output results"); partial — mostly device-level |
| `test_enabled_pick_check_with_an_unregistered_preset_refuses_setup_and_names_it` | T-Any-Setup-1 |
| `test_enabled_pick_check_without_calibration_names_the_reason` | T-Any-Setup-1 |
| `test_disabled_pick_check_leaves_setup_valid` | T-Any-Setup-4 |
| `test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role` | T-Any-Setup-1 through `TaskLocalization::beginRuntime()` |
| `test_vision_output_family_cell_still_starts_with_the_pick_check_commissioned` | T-Any-Setup-4, T-NR-RoleHealthy-1 |
| `test_camera_runner_watchdog_outlasts_device_grab_timeout` | §6 "Grab retry in the runner" (constants only) |
| `test_localization_recovery_policy_defaults_match_runtime_spec` | §6 "Role reconnect" (defaults only) |
| `test_localization_recovery_policy_decision_rules` | §6 "Role reconnect" (`decideRecoveryAction()` only) |
| `test_localization_role_outage_retries_forever_without_faulting` | T-Any-Policies-1, T-Any-Setup-4, T-NR-RoleHealthy-1, T-RFT-RoleUnhealthy-1, T-Any-Reconnect-1 ×~50, T-REC-RoleUnhealthy-1/-2, T-REC-RoleHealthy-1; S-08 |
| `test_localization_runtime_trigger_cycle_uses_matching_worker_contract` | S-01, S-02: T-Any-Setup-4, T-NR-RoleHealthy-1, T-RFT-TriggerRise-1, T-RUN-GrabFinished-1, T-RUN-MatchFinished-1, T-RUN-SendFinished-1, T-Any-LevelSample-1, T-WTR-TriggerFall-2 (pass branch) |
| `test_successful_cycle_stamps_every_stage_monotonically` | T-RFT-TriggerRise-1, T-RUN-GrabFinished-1, T-RUN-MatchFinished-1, T-RUN-SendFinished-1 (`CycleTimings`) |
| `test_faulted_cycle_carries_only_the_stages_it_reached` | T-RFT-TriggerRise-1, T-RUN-GrabCommand-2 (partial), T-RUN-GrabFinished-2 |
| `test_localization_unregistered_camera_number_faults_and_stays_faulted` | T-RFT-SelectCamera-1 (`NotRegistered`), T-FLT-TriggerRise-1 |
| `test_localization_unregistered_pattern_group_faults_and_stays_faulted` | T-RFT-SelectGroup-2, T-FLT-TriggerRise-1 |
| `test_localization_zero_active_index_is_refused_by_the_range_check` | T-RFT-SelectCamera-1 (`OutOfRange`), T-FLT-SelectCamera-3, T-RFT-SelectGroup-1, T-FLT-SelectGroup-3 |
| `test_localization_out_of_range_active_index_is_refused` | T-RFT-SelectCamera-1 (99, −5), T-FLT-SelectCamera-3 |
| `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value` | T-Any-TypeMismatch-1 (from `ReadyForTrigger`), T-FLT-SelectGroup-3 (gate refused); S-14 |
| `test_localization_a_refused_index_is_not_forgiven_by_the_other_index` | T-RFT-SelectCamera-1, T-FLT-SelectGroup-3 (refused), T-FLT-TriggerRise-1, T-FLT-TriggerFall-1, T-FLT-SelectCamera-3 (pass); S-12 |
| `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` | T-RFT-SelectGroup-1, T-FLT-SelectCamera-3 (refused), T-FLT-SelectGroup-3 (pass); S-12 |
| `test_localization_error_reset_does_not_lift_a_refused_index` | T-RFT-SelectCamera-1, T-FLT-ErrorResetRise-1 (refused), T-FLT-SelectCamera-3; S-13 |
| `test_localization_recovers_when_a_valid_index_follows_a_rejected_one` | T-RFT-SelectGroup-2, T-FLT-SelectGroup-3, T-RFT-SelectCamera-1, T-FLT-SelectCamera-3 |
| `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs` | S-01 through `PlcRunner::requestInjectInputValue()` → `valueChanged` → `m_plcValueConnection`; T-RFT-SelectCamera-3 (partial: asserts `nActiveCamera` echo only) |
| `test_setup_applies_the_active_camera_workspace_before_the_first_cycle` | T-Any-Configure-1, T-Any-Setup-4, T-RFT-TriggerRise-1, T-RUN-GrabFinished-1 (workspace payload) |
| `test_a_camera_with_no_workspace_keeps_the_default` | T-Any-Setup-4, T-RFT-TriggerRise-1, T-RUN-GrabFinished-1 |
| `test_a_camera_rebind_does_not_drop_the_plc_input_stream` | T-RFT-SelectCamera-3, T-RFT-TriggerRise-1 (delivery path) |
| `test_the_plc_value_stream_survives_binding_the_fixed_roles` | T-Any-Setup-3 or -4 (second `setup()` from a set-up controller), T-RFT-TriggerRise-1; S-17 |
| `test_localization_runtime_grab_timeout_faults_without_vision_output` | T-RUN-GrabCommand-2 (partial), T-RUN-GrabFinished-2 |
| `test_localization_runtime_vision_output_failure_faults_cycle` | T-RUN-SendFinished-3 |
| `test_localization_runtime_error_reset_clears_fault_and_rearms` | T-RUN-SendFinished-3, T-WTR-TriggerFall-1, T-REC-ErrorResetRise-1; S-10 |
| `test_localization_runtime_fault_auto_clears_without_error_reset` | T-RUN-SendFinished-3, T-WTR-TriggerFall-1, T-REC-AutoRecover-1; S-11 |
| `test_localization_runtime_rejects_camera_change_while_running` | T-RUN-SelectCamera-1 (partial: asserts no `nActiveCamera` emission, which nothing emits on this path anyway); S-16 |
| `test_localization_runtime_setup_faults_on_invalid_pattern_group` | T-Any-Setup-1 (`SETUP_GRP_REFUSED`) |
| `test_localization_runtime_setup_faults_on_invalid_calibration` | T-Any-Setup-1 (`SETUP_CAL_INVALID`) |
| `test_localization_runtime_ready_and_cycle_outputs_write_plc_tags` | `READY`, `CYCLE_START`, `CYCLE_OK` tag values; T-RFT-TriggerRise-1, T-RUN-MatchFinished-1, T-RUN-SendFinished-1 |
| `test_localization_runtime_camera_loss_faults_running_cycle` | T-RUN-RoleUnhealthy-1 (Camera, trigger high); S-09 first step |
| `test_localization_runtime_plc_loss_faults_running_cycle` | T-RUN-RoleUnhealthy-1 (PrimaryPlc); T-WTR-MatchFinished-1 set up but not delivered |
| `test_localization_runtime_vision_output_loss_faults_running_cycle` | T-RUN-RoleUnhealthy-1 (VisionOutput) |
| `test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault` | T-RFT-RoleUnhealthy-1, T-REC-RoleHealthy-1; S-08 |
| `test_localization_runtime_vision_output_loss_outside_a_cycle_withdraws_ready_without_fault` | T-RFT-RoleUnhealthy-1, T-REC-RoleHealthy-1; S-08 |
| `test_task_localization_begin_runtime_emits_runtime_started_after_runners_exist` | T-Any-Setup-4 via `beginRuntime()` (lifecycle ordering) |
| `test_build_runtime_context_leaves_the_selection_for_setup_to_resolve` | T-Any-Setup-4 (`firstKey()` fallback), T-NR-RoleHealthy-1; `logStartupSelectionSummary()` |
| `test_task_localization_incomplete_bindings_end_runtime_faulted_and_invalid` | T-Any-Setup-1 (missing PLC role) |
| `test_task_localization_runs_a_full_cycle_through_the_real_matching_worker` | S-01 through the task and the real matching worker |
| `test_task_localization_forwards_controller_signals_to_its_own_listeners` | S-01; task-side signal forwards |
| `test_setup_refuses_an_out_of_range_active_camera_with_a_range_message` | T-Any-Setup-1 (`SETUP_CAM_REFUSED`) |
| `test_setup_refuses_an_unregistered_active_camera_distinctly_from_calibration` | T-Any-Setup-1 (`SETUP_CAM_REFUSED`) |
| `test_setup_refuses_an_out_of_range_active_pattern_group_with_a_range_message` | T-Any-Setup-1 (`SETUP_GRP_REFUSED`) |
| `test_setup_resolves_the_first_camera_and_group_when_the_context_asks_for_the_default` | T-Any-Setup-4, T-NR-RoleHealthy-1 |
| `test_setup_announces_the_active_selection_on_status_signals` | `SETUP_STATUS` |
| `test_the_runtime_never_writes_the_command_registers` | T-RFT-SelectCamera-3, T-RFT-SelectGroup-3; `nActiveCamera`/`nActivePatternGroup` never written |
| `test_status_signals_with_no_tag_emit_but_do_not_write` | `publishNumberSignal()` no-tag path (`SETUP_STATUS`, `READY`) |
| `test_a_refused_selection_is_not_announced` | T-RFT-SelectCamera-1 (`NotRegistered`; status word untouched) |
| `test_an_index_that_names_nothing_reports_not_registered_not_lost` | (1) T-NR/RFT-SelectCamera-1 (partial: timing), (2) T-Any-Setup-1, (3) T-Any-Setup-2, (4) T-NR/RFT-SelectGroup-1 (partial: timing) |
| `test_each_required_signal_unmapped_fails_setup_naming_it` | T-Any-Setup-1 ×5 |
| `test_an_unmapped_optional_signal_is_valid_and_warns` | T-Any-Setup-4 |
| `test_an_unmapped_error_reset_is_supported_and_does_not_fail_setup` | T-Any-Setup-4 |
| `test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike` | T-Any-Setup-1 ×2 |
| `test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal` | T-Any-Setup-1 |
| `test_two_signals_mapped_to_one_tag_fail_setup` | T-Any-Setup-1 |
| `test_setup_resets_the_error_reset_edge_detector` | T-NR/RFT-SelectCamera-1 (partial: timing), T-FLT-ErrorResetRise-1 (refused), T-Any-Setup-* from `Faulted` (`m_lastErrorReset` reset), repeated |
| `test_setup_adopts_the_camera_the_plc_commands_over_the_project_default` | T-Any-Setup-4 (commanded, `SETUP_STATUS` = 2) |
| `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal` | T-Any-Setup-2, T-FLT-SelectCamera-3; S-03 |
| `test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped` | T-Any-Setup-4 |
| `test_a_plc_with_no_snapshot_is_not_read_as_holding_zero` | T-Any-Setup-4 |
| `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready` | T-Any-Setup-2, T-FLT-RoleHealthy-1 (refused), T-FLT-SelectCamera-3 via delivery; S-03 |
| `test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it` | T-Any-Setup-4, T-NR-RoleHealthy-1 |
| `test_setup_errors_reach_the_operator_log_naming_the_signal` | T-Any-Setup-1 (task-log ERROR) |

Fixture users that do **not** exercise controller behaviour (listed so they are not mistaken for
coverage): `test_plc_runner_rejects_invalid_tag_writes` (runner only),
`test_every_runner_family_forwards_device_errors_to_the_controller` (stops at the runner signal;
E19 is not observed), `test_runtime_matching_payload_metatypes_support_queued_connection`
(constructs a controller to register meta-types only),
`test_task_localization_runtime_signals_drive_recovering_and_faulted_states` (task slots invoked
directly, no controller).

**N/A — not controller behaviour:** the remaining 81 tests (device factories and configs, JSON
round trips, signal mapper and fault-code tables, task factory / task state machine, task runner
phases, MC / Modbus / virtual device behaviour, camera runner commands, vision result adapter,
signals-map widget, single-instance guard, access control, settings, shells, translations, display
names, include layering).

### 7.2 Rows with no test

Grouped by event. "(branch)" = the row exists in a test but the named branch does not.

- E01: T-RFT-TriggerRise-2, -3, -4; T-NR-TriggerRise-1; T-RUN-TriggerRise-1; T-REC-TriggerRise-1.
- E02: T-WTR-TriggerFall-2 (`NotReady` outcome branch); T-NR-, T-RFT-, T-RUN-, T-REC-TriggerFall-1.
- E03: T-FLT-ErrorResetRise-1 (pass branch); T-REC-ErrorResetRise-1 (refuse branch); T-NR-, T-RFT-,
  T-RUN-, T-WTR-ErrorResetRise-1.
- E04: T-RFT-SelectCamera-2, -4; T-NR-SelectCamera-2, -3; T-WTR-SelectCamera-1, -2, -3;
  T-REC-SelectCamera-1, -2, -3; T-FLT-SelectCamera-1, -2.
- E05: T-RUN-SelectGroup-1; T-RFT-SelectGroup-4; T-NR-SelectGroup-1, -2, -3; T-WTR-SelectGroup-1,
  -2, -3; T-REC-SelectGroup-1, -2, -3; T-FLT-SelectGroup-1, -2.
- E06: T-Any-TypeMismatch-1 from `NotReady`, `Running`, `WaitingTriggerReset`, `Recovering`,
  `Faulted`.
- E07: every row (T-RFT-ManualExecute-1, -2; T-NR-, T-RUN-, T-WTR-, T-REC-, T-FLT-ManualExecute-1).
- E09: T-RFT-RoleHealthy-1; T-RUN-RoleHealthy-1; T-WTR-RoleHealthy-1; T-REC-RoleHealthy-2;
  T-FLT-RoleHealthy-2.
- E10: T-RUN-RoleUnhealthy-1 (trigger-low branch); T-RUN-RoleUnhealthy-2; T-RFT-RoleUnhealthy-2,
  -3; T-NR-RoleUnhealthy-1, -2; T-WTR-RoleUnhealthy-1, -2; T-FLT-RoleUnhealthy-1, -2.
- E11: T-RUN-GrabFinished-3; T-FLT-GrabFinished-1.
- E12: T-RUN-GrabCommand-1 (`TimedOut`); T-FLT-GrabCommand-1.
- E13: T-RUN-MatchFinished-2, -3, -4; T-NR-, T-RFT-, T-REC-, T-FLT-MatchFinished-1;
  T-WTR-MatchFinished-1 (set up, not delivered).
- E14: **T-RUN-SendFinished-2** (the direct `ReadyForTrigger` assignment, AS-15 / PQ-11, both
  gate outcomes); T-FLT-SendFinished-1.
- E15: T-Any-WriteFinished-3 from `Running`, `WaitingTriggerReset`, `Recovering`, `Faulted`.
- E17: T-RFT-AutoRecover-1; T-WTR-AutoRecover-1; T-FLT-AutoRecover-1; T-REC-AutoRecover-1 (refuse
  branch).
- E19: T-Any-RoleError-1.
- E22: T-Any-Teardown-1 (run by every `TaskLocalizationRuntimeFixture` destructor, asserted by
  nothing).
- Snapshots with no direct tag-level assertion: `CAM_UNCALIBRATED`, `SETUP_GRP_INVALID`,
  `FAULT_CLEAR` values are asserted only through `signalChanged` (`test_localization_runtime_error_reset_clears_fault_and_rearms`), never as PLC writes.

---

## 8. Drift list

One line per place where a "Read first" prose/UML source states something different from the
code. No fixes.

- **DRIFT-1** `uml/08_runtime_state_machines.puml` note "Every arrow INTO ReadyForTrigger runs
  through markRuntimeReady()": `onVisionOutputResultFinished()` assigns `ReadyForTrigger` directly
  before calling it (AS-15, T-RUN-SendFinished-2).
- **DRIFT-2** `uml/08` edge `ReadyForTrigger --> CycleFaulted : invalid active pattern or
  calibration`: a group/calibration failure found at trigger time runs `abortCycle()` and lands in
  `WaitingTriggerReset` (T-RFT-TriggerRise-2/-3), never `Faulted`.
- **DRIFT-3** `uml/08` has no edges for: `WaitingTriggerReset`/`Running`/`Recovering`/`NotReady`
  `--> CycleFaulted` on an index refusal or type mismatch (E04/E05/E06 read no state except
  `Running` for the setters); `CycleFaulted`/`ReadyForTrigger`/`NotReady` `--> RecoveringCycle |
  WaitingTriggerReset` on a handshake-write escalation (T-Any-WriteFinished-3);
  `RecoveringCycle --> ReadyForTrigger` by the auto-recover timer or an accepted index write;
  `WaitingTriggerReset --> CycleFaulted`.
- **DRIFT-4** `uml/08` edge `CycleFaulted --> NotReady : runtime teardown or setup reset`: teardown
  destroys the object (no state); only `setup()` assigns `NotReady`.
- **DRIFT-5** `task_localization.md` "Runtime Stop" (clear `bTaskReady`, `bMatchingBusy`,
  `bMatchingFinished`, `bTaskFault`, `nFaultCode` on stop): no code path publishes anything on
  teardown (`STOP` snapshot does not exist; `ITask::endRuntime()`/`stopAll()` write no tags).
- **DRIFT-6** `task_localization.md` "Recovery Behavior" ("grab timeout … enter Recovering and
  retry/reconnect the active camera"; "vision-output send failure … enter Recovering"; "PLC lost
  while ready or running: enter Recovering"): a cycle fault enters `WaitingTriggerReset` whenever
  the trigger is still high and reaches `Recovering` only on the falling edge; a grab timeout
  schedules no reconnect (only a `connectStatusChanged` does).
- **DRIFT-7** `task_localization.md` "Cycle-fault handshake" step 10 "Move to recovery or
  `Faulted` depending on the recovery policy": no cycle fault reaches `Faulted`; the policy is not
  consulted by `abortCycle()`.
- **DRIFT-8** `task_localization.md` "If recovery succeeds … 3. If the fault happened during an
  accepted trigger cycle, keep `bTaskFault = true` and `nFaultCode` until `bExecuteTrigger` returns
  to false": a `bErrorReset` edge in `WaitingTriggerReset` publishes `FAULT_CLEAR` with the trigger
  still high (T-WTR-ErrorResetRise-1).
- **DRIFT-9** `task_localization.md` "Runtime Updates" ("defer or reject the change until the
  cycle reaches …"; "`bCameraValid` must reflect whether the selected active camera is connected
  and ready"): a change during `Running` is dropped, never deferred; `bCameraValid` is written from
  registration/calibration checks only and is published `true` by `READY` regardless of connection
  (roles are checked separately by the gate).
- **DRIFT-10** `task_localization.md` "Runtime Trigger Cycle" step 6 ("runs matching synchronously
  after the frame arrives") and "Threading contract" ("the runtime controller object remains owned
  by the task object … moving it into the task runtime thread is a follow-up"): matching is handed
  to the matching worker via `runtimeMatchingRequested`, and `beginRuntime()` moves the controller
  to `TaskRunner::runtimeThread()`.
- **DRIFT-11** `task_localization.md` "Vision Output Payload" ("emit x, y, z, and r") and
  `runtime_controller_api.md` "Output Coordinate Contract" ("`world.r` comes from
  `rotateImageToRobot`"): `buildVisionOutputPositions()` writes `world.rx`, `world.ry` from the
  pattern's rotation offset and `world.rz = -rotateImageToRobot(angle) + offset.z`; there is no
  `r`.
- **DRIFT-12** `task_localization.md` fault table, 100 `CameraLost` "The active camera lost
  connection": `startCycle()` also publishes 100 when no camera runner is bound
  (T-RFT-TriggerRise-4); `plc_signal_contract.md` states both meanings.
- **DRIFT-13** `runtime_controller_api.md` "`runtimeFault` is raised by an invalid setup": the
  `!m_valid` branch of `setup()` assigns `Faulted` without emitting `runtimeFault()` (AS-07); only
  the valid-with-startup-refusal branch emits it (AS-06). The header doc comment on
  `runtimeFault()` says the same as the document.
- **DRIFT-14** `runtime_controller_api.md` "Race Guards" ("Every re-arm goes through
  `markRuntimeReady()`"): same fact as DRIFT-1.
- **DRIFT-15** `plc_signal_contract.md` "Successful Cycle" ("When `bExecuteTrigger` falls to false:
  `bMatchingFinished = false`, `bTaskReady = true`"): the falling edge publishes
  `bMatchingFinished=false`, then the whole `READY` set (twelve values) and only if the gate passes;
  a refused gate leaves `bTaskReady=0` and the state `NotReady`.
- **DRIFT-16** `plc_signal_contract.md` "Recovery Behavior" ("On recovery, current runtime state is
  republished instead of silently clearing a fault") and `task_localization.md` ("After PLC
  recovery, the task should publish the current fault state instead of silently clearing it"): a
  reconnect in `Recovering` runs G-READY, which publishes `READY` (`bTaskFault=0`, `nFaultCode=0`)
  when it passes; nothing republishes a fault.
- **DRIFT-17** `plc_signal_contract.md` "Fault Acknowledge And Auto-Recovery" ("[the countdown] is
  cancelled by an acknowledge, by a new cycle starting, and by runtime teardown"): it is also
  cancelled by `setup()` (`resetRuntimeBindings()`), and it is **not** cancelled when the runtime
  re-arms through a reconnect or an accepted index write, so it can fire in `ReadyForTrigger` or
  `Faulted` (T-RFT-AutoRecover-1, T-FLT-AutoRecover-1).
- **DRIFT-18** `plc_signal_contract.md` "Handshake Writes Are Acknowledged" describes the 301
  escalation as aborting "the cycle": `escalatePlcWriteFailure()` runs `abortCycle()` from any
  state, including when no cycle is in flight (`READY` writes from `NotReady`/`ReadyForTrigger`,
  `FAULT_CLEAR` writes from `Recovering`, refusal writes from `Faulted`), publishing
  `CYCLE_FAULT(301)` with `bMatchingFinished=1`.
- **DRIFT-19** `runtime_controller_api.md` and `plc_signal_contract.md` both describe
  `runtimeFault` for the 301 case as the runtime "faulting": the controller state after escalation
  is `WaitingTriggerReset` or `Recovering`, never `Faulted`, while `TaskLocalization::onRuntimeFault()`
  moves the task state to `Faulted`.

The template's seed rows (`temp_docs/templates/runtime_state_contract_template.md`) were checked
the same way; the ones that differed from the code were corrected in §3 rather than listed here
(T-Running-GrabCommandFailed-1's "if trigger high else Recovering", T-Ready-RoleStatus-lost's
snapshot, and the `PlcSnapshot` event, which has no handler).





