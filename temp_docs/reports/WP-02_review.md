# WP-02 review — `docs/domains/task_localization/runtime_state_contract.md` (AS-IS)

**Verdict: ACCEPT WITH CHANGES.** The table describes the code: 15 `m_cycleState =` sites confirmed
exactly, grid arithmetic sound, fault codes and every §6 constant correct, DRIFT quotes verbatim, no
proposals, no line numbers, and all four report findings I re-derived are real. The changes are in
§7 (three test attributions are wrong in a way that flatters coverage) and in two marker/scope
omissions that a redesign built on this file would inherit.

## What checked out

- **Grid.** 6×23=138. A 81 · I 24 · N 18 · U 15 sums to 138; every per-event line sums to 6; the
  matrix rows agree with the per-event line. `PlcSnapshot` exclusion is right — `PlcRunner::pollingUpdate`
  is connected only in `TaskLocalization::awaitPrimaryPlcSnapshot()` (one-shot, disconnected after
  `loop.exec()`) and in two UI widgets; no controller connect site exists.
- **Assignment sites.** Grep gives exactly 15, in the 10 functions named, matching AS-01..AS-15.
- **Rows spot-checked against code (14):** T-RFT-TriggerRise-1..4, T-WTR-TriggerFall-1/-2,
  T-FLT-ErrorResetRise-1, T-WTR-ErrorResetRise-1, T-RFT-SelectCamera-1/-3, T-RFT-SelectGroup-2,
  T-Any-TypeMismatch-1, T-Any-Setup-1/-2, T-RUN-RoleUnhealthy-1, T-RFT-RoleUnhealthy-1/-2,
  T-RUN-SendFinished-1/-2, T-Any-WriteFinished-1..3, T-Any-Reconnect-1. Guards, action order, next
  state and snapshot all match. §4.1/§4.2 values match the five `publish*` bodies.
- **Constants.** 2000/5/12/3/40, camera 1..16 (`TaskDeviceBinding`), group 1..32 (`MatchGroup`
  statics), `kPlcSnapshotWaitMs`=2000, `kMaxGrabAttempts`=6, `kSingleShotTimeoutMs`=8000, fault codes
  100/102/103/200/201/300/301/400/401.
- **DRIFT (9 of 19 verified).** DRIFT-1/-2/-4 (`uml/08` lines 91/59/70), -5 (`task_localization.md`
  "Runtime Stop" lists exactly those five), -9 ("defer or reject"), -11 ("Emit `x`, `y`, `z`, and `r`"
  + `runtime_controller_api.md` "`world.r` comes from…" vs `world.rx/ry/rz` in code), -13/-14, -17
  (`plc_signal_contract.md` "cancelled by an acknowledge, by a new cycle starting, and by runtime
  teardown"). All faithful.

## Findings, most severe first

**1 — CONFIRMED, silent wrong result. `escalatePlcWriteFailure()` → `abortCycle()` from idle; the
test that names it does not bound it.** (report finding 3; §3.1 T-Any-WriteFinished-3, S-15)
Only the refused tag fails — the other four handshake tags write normally — so a master latching on
`bMatchingFinished` sees "cycle finished, 0 detected" with no trigger. `recoverFromFault()` then
re-publishes `bTaskFault`/`nFaultCode` (tracked again: `m_plcWriteEscalating` is already false) and
`markRuntimeReady()` re-writes `bTaskReady` → next exhaustion → ~every 2 s. `test_a_link_that_refuses_everything_escalates_once_and_stops`
waits `QTest::qWait(1000)`, under `kFaultAutoRecoverMs`=2000, so it proves no *synchronous* recursion
only; its name overstates it. Confirm: in that test raise the wait to 6000 ms — `faultSpy.count()` grows.

**2 — CONFIRMED, hang. Type mismatch mid-cycle strands the cycle at `bMatchingBusy=1`.**
(report finding 2; T-Any-TypeMismatch-1) `reportSignalTypeMismatch()` reads no state and disconnects
nothing; `onCameraGrabFinished`/`onCameraCommandFinished`/`onRuntimeMatchingFinished`/`onVisionOutputResultFinished`
each return on `!= Running`. Only `publishInitialReadyOutputs`/`publishCycleSuccessOutputs`/`publishCycleFaultOutputs`
clear `bMatchingBusy`, and `TYPE_MISMATCH` publishes none of them. Exit only via a valid index write.

**3 — CONFIRMED, silent wrong result. A non-recoverable status leaves `bTaskReady=1` and triggers
still start cycles.** (report finding 4; T-RFT-RoleUnhealthy-2) `decideRecoveryAction()` returns
`Ignore` for anything but `ConnectFailed`/`LostConnected`, and `handleRoleStatusChanged()` returns
before the `ReadyForTrigger` branch. `startCycle()` checks group, calibration and camera runner only.
With the PLC role `Disconnected`, the cycle runs and `trackHandshakeWrite()` silently drops every
handshake write (device not `Connected`) — no retry, no 301, no fault.

**4 — CONFIRMED, fault with wrong code. `m_faultRecoverTimer` fires in `Faulted`.** (report finding 5;
T-FLT-AutoRecover-1) `markRuntimeReady()` never cancels it and E04/E05/E06 do not either, so
`FAULT_CLEAR` publishes `bTaskFault=0`, `nFaultCode=0` under a held latch with `bTaskReady=0`.

**5 — CONFIRMED, diagnostic loss. `bErrorReset` in `WaitingTriggerReset`.** (report finding 6;
T-WTR-ErrorResetRise-1) `acknowledgeFault()` has no state check; `recoverFromFault()` publishes
unconditionally and only gates `markRuntimeReady()`. Fault code wiped before the master reads it, and
the next falling edge takes the clean branch. DRIFT-8 records the doc conflict correctly.

**6 — DEFECT IN THE TABLE, medium. §7 mis-attributes the camera-rebind rows.** The fixture connects
only the *active* camera (`setup()` → `requestRoleConnectNow(Camera)`); camera 2 is never connected,
and `newRunner->requestConnect()` in `setActiveCameraNumber()` is queued, so `allRequiredRolesHealthy()`
on the next line is false. Therefore `test_a_camera_rebind_does_not_drop_the_plc_input_stream` and
`test_the_runtime_never_writes_the_command_registers` exercise **T-RFT-SelectCamera-4**, not -3, and
the re-arm they wait on is **T-RFT-RoleHealthy-1** — both listed in §7.2 as having no test. By the
same mechanism, `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` probably
never reaches G-READY from the setter, so its mapping to "T-FLT-SelectCamera-3 (refused)" is unproven.
Confirm: add a `runtimeReady` spy, or assert `camera2->connectStatus()` immediately before the setter.
Consequence: a redesign that preserves "covered" rows would preserve the wrong ones.

**7 — DEFECT IN THE TABLE, medium-low. E22 understates the teardown window.** `destroyRuntimeController()`
drops only task↔controller links and queues `deleteLater()`; `ITask::endRuntime()` stops the runners
*after* that. Runner→controller connections stay live until the queued delete runs, so E09/E10/E15
can still reach a released controller and `handleRoleStatusChanged()` can still `abortCycle()` and
issue PLC writes. Also `endRuntime()`/`stopAll()` immediately `createRuntimeController()`, so "object
destroyed" is half the outcome: a fresh `NotReady`, `m_valid==false` controller exists at once.

**8 — DEFECT IN THE TABLE, low. E23 is marked NO-OP in all six cells but mutates the edge detector.**
§3b defines N as "state not read, nothing observable". E23 as defined includes a `bErrorReset` falling
edge (`m_lastErrorReset` true→false, re-arming the next acknowledge), and the trigger level it records
decides `WaitingTriggerReset` vs `Recovering` at the next `abortCycle()` — T-REC-TriggerRise-1 says so
itself. Either re-mark those cells or narrow the legend to "no cycle-state effect". Up to 6 cells move
between A/I/N; 138 is unaffected.

**9 — cosmetic.** (a) E09/`Running` and E09/`WaitingTriggerReset` are marked A "(bookkeeping only)"
although `handleRoleStatusChanged()` checks the state and returns — structurally the same shape as
E01/`NotReady`, which is I. (b) No row quotes `startCycle()`'s own guard/log
(`"Cycle start ignored: runtime is not ready."`, currently dead — both callers pre-check); §3.0 A-START
begins "after its three validations". (c) §3.1 says the 301 test fires "from the ready path, i.e.
`NotReady`/`ReadyForTrigger`" while §7.1 says "from `ReadyForTrigger`" — the latter is right, since
`bTaskReady` is written only by `publishInitialReadyOutputs()`, after the state assignment.
(d) There is no row or S-scenario for "an expected completion never arrives": the controller owns only
`m_faultRecoverTimer` and `m_plcWriteRetryTimer`, the sole grab watchdog is `CameraRunner::kSingleShotTimeoutMs`
(E12), and E13/E14 have none — `Running` has no exit of its own. Worth one line in §5 for the redesign.
(e) The report says the file is 761 lines; it is 887.

## What I did not check

Rows for E16/E18/E19/E20/E21 beyond one read each; the remaining 10 DRIFT lines; §7's 81 "N/A" tests
and the claim of 157 `test_` functions; all `SETUP_*` partial publish sets (I checked the code paths,
not each cell of §4.2); `LocalizationSignalMapper::mapValues()` ordering claims; the device-level S-04
question (untestable here); anything about UML regeneration. Nothing was built or run — no compiler or
test evidence, only source reading. No git.

## Questions for the PM

1. Finding 1 outranks everything in the report's own list — should the report's "Findings out of
   scope" be re-ordered by cell consequence before Stage 1 picks what to fix first?
2. Do you want §7 re-derived with the connect-timing rule applied uniformly (every row reached through
   a camera rebind), or is a caveat line on the affected rows enough for Phase 10's purposes?
3. Should E23 stay a modelled event at all, or become an "adapter-level fact" like the empty-map case,
   so the grid stays 6×22 and the edge detector is recorded as hidden state instead?
