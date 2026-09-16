# WP-02 report — AS-IS runtime state contract

**Date:** 2026-09-16 · **Agent:** wp-implementer (WP-02) · **Session note:** the first run was
cut by a session rate limit after §3 E08; the second run resumed from the file on disk and landed
the rest. No git on this machine; nothing was built or run.

## Created file

- `docs/domains/task_localization/runtime_state_contract.md` (761 lines). Header carries
  `Status: AS-IS — extracted 2026-09-16 from the code; describes implemented behaviour, not the
  target design. Reviewed at Phase 10 Checkpoint 0.` and links `docs/plan/phase_10/` (the folder
  did not exist at extraction time — WP-01 owns it).
- No other file was created or edited. Source, tests, `uml/`, the template, and the four drift
  sources are untouched.

## Which part landed

**(a) + (b), complete.** §1 states, §2 events, §3 transition rows (with §3.0 shared action
blocks and §3c assignment-site index), §3b completeness grid, §4 output snapshots, §5 scenario
catalogue (S-01..S-17), §6 policy registry, §7 test map with the rows-with-no-test list, §8 drift
list. No section is marked incomplete.

## Grid counts

- Events: 23 (E01..E23; the template's `PlcSnapshot` is excluded because no controller handler
  exists — it reaches the controller only as `RuntimeContext::plcSnapshot` inside `setup()`; an
  explicit `LevelSampleNoEdge` event was added for non-edge samples of the two edge inputs).
- States: 6. Cells: 138.
- A (action/transition rows): 81 · IGNORED: 24 · NO-OP: 18 · UNREACHABLE: 15 · UNSPECIFIED: 0.
  Sum 138. Per-event breakdown is printed under the grid.
- No cell needed UNSPECIFIED. The two genuinely open points are recorded where they sit: S-04
  (held trigger at start — an adapter/device delivery question, not a controller one) and the
  reachability of T-RFT-RoleUnhealthy-3 (noted inside an otherwise determined cell).

## Assignment sites as found

`m_cycleState =` grep on `localization_runtime_controller.cpp`: **15 sites in 10 functions**,
identical to the handoff — `setActiveCameraNumber` (2), `setActivePatternGroupNumber` (2),
`setup` (3), `handlePlcValues` (2), `markRuntimeReady` (1), `reportSignalTypeMismatch` (1),
`startCycle` (1), `abortCycle` (1), `handleRoleStatusChanged` (1),
`onVisionOutputResultFinished` (1). Indexed AS-01..AS-15 in §3c, each with its rows and the
reason as the code names it (the WP-10 token column is left to WP-10, as instructed).

## Rows with no test (summary; full list in §7.2)

- Every E07 `ManualExecute` row (no test calls `execute()`).
- **T-RUN-SendFinished-2** — the direct `ReadyForTrigger` assignment (AS-15 / PQ-11): every
  cycle test releases the trigger after the send, so the trigger-low completion path and its
  gate-refused sub-branch are unexercised.
- E06 type mismatch from any state but `ReadyForTrigger`; E04/E05 setters from
  `WaitingTriggerReset`, `Recovering`, and most `NotReady`/`Faulted` branches; the uncalibrated
  accepted-camera branch (AS-02) in every state.
- E15 exhaustion (301) from `Running`, `WaitingTriggerReset`, `Recovering`, `Faulted`.
- E17 auto-recover firing in `ReadyForTrigger`, `WaitingTriggerReset`, `Faulted`, and the
  refused-gate branch in `Recovering`.
- E10 non-recoverable statuses (`Disconnected`/`Connecting`/`NoConnection`) in every state;
  role loss in `NotReady`, `WaitingTriggerReset`, `Faulted`; the trigger-low branch of a mid-cycle
  role loss.
- E12 `TimedOut` command failure; E11/E13 abort branches other than grab failure.
- E19 role error, E22 teardown, T-WTR-ErrorResetRise-1 and the other non-`Recovering`/
  `Faulted` `bErrorReset` cells.

## DRIFT count

**19** DRIFT lines (§8): 4 against `uml/08_runtime_state_machines.puml`, 8 against
`task_localization.md`, 3 against `runtime_controller_api.md`, 4 against
`plc_signal_contract.md` (two of them shared with another source). The largest: no `STOP`
publish exists (DRIFT-5); cycle faults never reach `Faulted` and go through
`WaitingTriggerReset` first (DRIFT-2/6/7); the 301 escalation runs `abortCycle()` from any state
with no cycle in flight (DRIFT-18/19); `setup()` invalid does not emit `runtimeFault()`
(DRIFT-13).

## Findings out of scope (suspected defects — recorded only, no fixes)

1. `onVisionOutputResultFinished()` assigns `ReadyForTrigger` directly; if `markRuntimeReady()`
   then refuses (latch, role down, invalid group/calibration), the state stays `ReadyForTrigger`
   with `bTaskReady=0` and the next trigger starts a cycle (T-RUN-SendFinished-2). Known: PQ-11.
2. `reportSignalTypeMismatch()` reads no state: a non-numeric index register arriving while
   `Running` moves to `Faulted` mid-cycle, leaves the grab/match/send connections alive, and the
   cycle never publishes `CYCLE_OK`/`CYCLE_FAULT` (`bMatchingBusy` stays 1). From
   `WaitingTriggerReset` it also swallows the subsequent falling edge (T-Any-TypeMismatch-1).
3. `escalatePlcWriteFailure()` aborts "the cycle" from any state. A `READY` write refused three
   times while idle publishes `CYCLE_FAULT(301)` with `bMatchingFinished=1`, emits
   `cycleResultUpdated`, arms the auto-recover timer, and after 2000 ms writes `READY` again —
   repeating every ~2 s while the tag is refused. `runtimeFault` fires each time while the
   controller state is `Recovering`, not `Faulted` (T-Any-WriteFinished-3).
4. A role reporting a non-recoverable status (`Disconnected`, `Connecting`, `NoConnection`)
   while `ReadyForTrigger` changes nothing: `bTaskReady` stays 1, and `startCycle()` does not check
   role health, so a trigger starts a cycle against a disconnected PLC/output
   (T-RFT-RoleUnhealthy-2).
5. `m_faultRecoverTimer` is not cancelled when the runtime re-arms through a reconnect or an
   accepted index write; it later fires in `ReadyForTrigger` (republishing `FAULT_CLEAR`,
   incrementing `m_consecutiveAutoRecoveries`) or in `Faulted` (publishing `bTaskFault=0` under a
   held latch) (T-RFT-AutoRecover-1, T-FLT-AutoRecover-1).
6. `bErrorReset` in `WaitingTriggerReset` clears `bTaskFault`/`nFaultCode` while the PLC still
   holds the trigger and `bMatchingFinished=1`, and flips the falling edge onto the clean branch
   (T-WTR-ErrorResetRise-1). Contradicts `task_localization.md` "keep bTaskFault until the trigger
   returns to false".
7. `setup()` with `!m_valid` assigns `Faulted` but never emits `runtimeFault()`; the task learns
   of it only through `SetupResult`. The header comment on `runtimeFault()` says otherwise.
8. `TaskLocalization::onCommDeviceValueChanged()` / `queueHandlePlcValues()` have no connect
   site; the only live delivery path is `m_plcValueConnection` made in `setup()`.
   `queueSetActiveCameraNumber()` / `queueSetActivePatternGroupNumber()` /
   `executeLocalization()` have no callers outside moc tables.
9. `handlePlcValues()` still carries the `/// temp debug` `qDebug()` line before
   `setActiveCameraNumber()` (out of scope per the handoff; recorded here as asked).
10. `test_localization_runtime_rejects_camera_change_while_running` asserts
    `lastSignalValue(spy, "nActiveCamera") == 0`, which is true whether or not the change is
    rejected — nothing emits `nActiveCamera` on that path. Coverage of T-RUN-SelectCamera-1 is
    nominal.
11. `handleRoleStatusChanged()` publishes `bTaskReady=false` only when the loss happens in
    `ReadyForTrigger` and `retryCount == 0`; a second recoverable status after a failed retry
    while the runtime was re-armed by another role does not withdraw readiness
    (T-RFT-RoleUnhealthy-3; reachability not proven).
12. The `uml/08` cycle-state block is missing roughly a dozen edges that exist in code
    (DRIFT-3); the diagram cannot be regenerated from this file yet (the template's
    `scripts/gen_state_uml.py` does not exist).

## Open questions

1. S-04 / item 54: whether each PLC family delivers a trigger that is already high at the first
   poll through `valueChanged` (McProtocolDevice diff vs. Modbus `takeChangedValues()`). The
   controller side is determined (a delivered `true` after `setup()` is a rising edge); the
   adapter side is not readable off the controller and was not measured.
2. Whether WP-10's reason tokens should be attached to §3c's AS-nn rows or to the T-rows; §3c
   carries the code's own names as placeholders either way.
3. Whether `Recovering`'s two meanings (role outage vs. resting cycle fault) should be split is
   a policy question (template seed note); the document records both meanings under one state.
4. `docs/plan/phase_10/` is linked in the header but did not exist when this file was written.

---

## PM review — 2026-09-16 — ACCEPTED WITH CHANGES (changes deferred to WP-02b on the git machine)

Checked by the PM: grid arithmetic re-added per event; §3c index against the PM's own grep (15 sites);
§1, §2, §5, §6, §8 read in full. Independent adversarial review by `wp-reviewer`:
`temp_docs/reports/WP-02_review.md` — verdict ACCEPT WITH CHANGES; 14 rows, all constants, 9 DRIFT
lines verified against code; findings 2, 3, 4, 5, 6 of this report CONFIRMED (re-ranked by cell
consequence as R1..R5 in `temp_docs/03_checkpoint_0_package.md` §4).

PM rulings on the reviewer's three questions:

1. **Re-order "Findings out of scope" by cell consequence** — yes; the ranked list is R1..R5 in the
   checkpoint package, and WP-02b copies it into this report's §"Findings" as a preface.
2. **Re-derive §7 with the connect-timing rule applied uniformly** — yes, not a caveat line: §7 is
   the source for DR ring-1 fills and for Stage 3 test derivation, so a flattering attribution
   propagates. WP-02b re-derives every mapping that involves a setter or a re-arm right after
   `setup()`, states the rule once at the top of §7, and moves T-RFT-SelectCamera-4 and
   T-RFT-RoleHealthy-1 out of §7.2.
3. **E23 stays a modelled event, re-marked A** with row T-Any-LevelSample-1 ("records the level;
   the recorded trigger level selects `WaitingTriggerReset` vs `Recovering` at the next
   `abortCycle()`"). Grid becomes A 87 · I 24 · N 12 · U 15 = 138. The edge detector
   (`m_lastExecuteTrigger`, `m_lastErrorReset`) is real state the to-be design must model.

Also for WP-02b: E22 row and a new S-18 for the teardown window (runner→controller connections live
until the queued `deleteLater()` runs; `endRuntime()`/`stopAll()` create a new controller at once);
E09/`Running` and E09/`WaitingTriggerReset` re-marked I; a row for `startCycle()`'s own guard/log;
§3.1 aligned to §7.1 on the 301 test's starting state; a scenario for "an expected completion never
arrives" (only the grab has a watchdog; `Running` has no exit of its own); the line count in this
report corrected to 887.

No edit was made to the contract file in this session: the owner asked to stop after the review
and move to the git machine. WP-02b is the first docs task there.
