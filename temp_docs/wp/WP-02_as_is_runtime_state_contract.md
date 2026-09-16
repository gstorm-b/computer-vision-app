# WP-02 — Extract the AS-IS runtime state contract from the code

**Stage:** 0 · **Size:** M, split as stated (honestly L) · **Kind:** docs only, read-heavy · **Owner approval:** granted 2026-09-16 (charter §6) · **Issued:** 2026-09-16 · **Runs in parallel with:** WP-00, WP-01, WP-03

## 1. Goal

Write `docs/domains/task_localization/runtime_state_contract.md` describing the
`LocalizationRuntimeController::CycleState` machine **exactly as implemented today** — every
(state, event) pair, its guard, actions, outputs, and which existing test covers which row — so the
redesign starts from a complete, reviewable picture instead of from five prose documents.

This document is **descriptive, not normative**. It records what the code does, including the parts
the redesign will change. Opinions, proposals and fixes are out of scope.

## 2. Read first

1. `AGENT.md`; `src/model/AGENTS.md`; `src/runtime/AGENTS.md`
2. `temp_docs/templates/runtime_state_contract_template.md` — the **structure** to follow (§1–§7).
   Its seed rows are illustrative and may be wrong; verify every one against the code.
3. `src/model/localization_runtime_controller.h` and `.cpp` — the primary source
4. `tests/architecture_contract_test/main.cpp` — for the test map (§7 below)
5. For the drift list only: `docs/domains/task_localization/runtime_controller_api.md`,
   `plc_signal_contract.md`, `task_localization.md`, `uml/08_runtime_state_machines.puml`

Invoke `using-agent-skills` first; `spec-driven-development` applies (the spec here is the table).

## 3. Facts (verified by the PM, 2026-09-16)

- `CycleState` has six values: `NotReady`, `ReadyForTrigger`, `Running`, `WaitingTriggerReset`,
  `Recovering`, `Faulted` (private `enum class` in the header).
- `m_cycleState` is assigned at **15 sites in 10 functions**: `setActiveCameraNumber` (2),
  `setActivePatternGroupNumber` (2), `setup` (3), `handlePlcValues` (2), `markRuntimeReady` (1),
  `reportSignalTypeMismatch` (1), `startCycle` (1), `abortCycle` (1), `handleRoleStatusChanged` (1),
  `onVisionOutputResultFinished` (1). Confirm with a grep before starting; if the count differs,
  record the difference and continue.
- Every path into `ReadyForTrigger` goes through `markRuntimeReady()` except
  `onVisionOutputResultFinished()`, which assigns it directly and then calls `markRuntimeReady()`.
  Record this as-is; it is PQ-11 for Stage 2, not something to judge here.
- External stimuli that reach the controller (starting list; you must complete it from the code):
  `handlePlcValues()` (edges for `bExecuteTrigger` and `bErrorReset`; unfiltered values for
  `nActiveCamera` / `nActivePatternGroup`; type mismatch); `execute()` (no production caller);
  the two index setters; `setup()`; slots `onCameraGrabFinished`, `onCameraCommandFinished`,
  `onVisionOutputResultFinished`, `onPrimaryPlcStatusChanged`, `onVisionOutputStatusChanged`,
  `onCameraStatusChanged`, the three `on*Error` slots (log only), `onPlcWriteFinished`,
  `onRuntimeMatchingFinished`; timers `m_faultRecoverTimer` (2000 ms), `m_plcWriteRetryTimer`
  (40 ms), the reconnect `QTimer::singleShot` in `scheduleRoleReconnect()`; teardown paths.
- Three error regimes are kept apart on purpose: role loss → unbounded retry, never `bTaskFault`;
  cycle fault → latched then auto-clear via `recoverFromFault()`; handshake write failure → bounded
  retry then `301` via `escalatePlcWriteFailure()`.
- The contract suite has 157 `test_` functions in source (159 passed including init/cleanup at
  Checkpoint Z). Fixtures: `LocalizationRuntimeFixture` (drives the controller),
  `TaskLocalizationRuntimeFixture` (drives `TaskLocalization::beginRuntime()`).
- Prose docs are known to have drifted from the code; where they disagree, the code is the truth
  and the disagreement is listed, not resolved.

## 4. Steps

1. **§1 States.** The six names verbatim. For each: the invariant *as enforced in code* (what
   `markRuntimeReady()` checks for `ReadyForTrigger`; what `startCycle()` assumes for `Running`; and
   so on), the output snapshot the code publishes on entry (name the `publish*` function), and the
   functions that can enter it.
2. **§2 Events.** Every stimulus in §3 plus any you find. Columns: event name (choose a
   source-independent name, e.g. `TriggerRise`, and give the concrete source in the next column),
   payload, today's source, kind (edge / level / completion / status / timer / lifecycle),
   producing thread, handler symbol.
3. **§3 Transition table.** One or more rows per assignment site (`T-<From>-<Event>-<n>`), with
   the guard quoted by symbol (`cycleId == m_activeCycleId`, `m_lastExecuteTrigger`, `m_valid`,
   latch flags…), the actions in order (calls, publishes, timers, signals emitted), the resulting
   state, and the snapshot name. Then, for every (state, event) pair with no row, determine from the
   code what happens and add a row marked one of: **IGNORED** (state is checked and the event is
   dropped — quote the log line if any), **NO-OP** (state is not checked and nothing observable
   happens), **UNREACHABLE** (the event cannot arrive in that state — say why, e.g. a
   `SingleShotConnection` made only in `startCycle()`), **UNSPECIFIED** (behaviour depends on other
   flags or timing and cannot be read off the code — describe what it depends on).
4. **§3b Completeness grid.** A matrix states × events, each cell a row id or one of the four
   markers. Below it: counts per marker; they must sum to states × events.
5. **§4 Output snapshots.** One table per `publish*Outputs()`-style function (and any ad-hoc
   publish sequence, e.g. in the index setters and in `abortCycle()`), listing the exact tag values
   written, and marking the five handshake signals returned by `isHandshakeSignal()`.
6. **§6 Policy registry.** From code: `LocalizationRecoveryPolicy` defaults and
   `decideRecoveryAction()`; `kFaultAutoRecoverMs`, `kAutoRecoverWarnStride`,
   `kQuietRetryLogStride`, `kPlcWriteRetryBudget`, `kPlcWriteRetryDelayMs`; `isHandshakeSignal()`
   set; `requiredSignalNames()`; index ranges and their sources; `CameraRunner::kMaxGrabAttempts`
   and `kSingleShotTimeoutMs`; the positions cap literal in `buildVisionOutputPositions()`;
   `TaskLocalization::kPlcSnapshotWaitMs`. Columns: policy · implemented at (symbol) · parameters ·
   test (from §7) · decision record (leave "none" unless WP-01's DR index names one — do not wait
   for WP-01).
7. **§7 Test map.** For every `test_` function that uses either runtime fixture or otherwise drives
   the controller, list the row ids it exercises (read the body; best effort, say "partial" when
   unsure). Group every other test under one line "N/A — not controller behaviour". Then list every
   row id with no test.
8. **§5 Scenario catalogue.** Seed S-01..S-07 from the template, corrected against code and tests;
   add any scenario a test encodes that the seeds miss. Cite field items 54, 58, 64, 70 where they
   apply. Expected outcome column = what the code does today.
9. **§8 Drift list.** `DRIFT-n`: one line per place where the four prose/UML sources in "Read
   first" item 5 state something different from the code (state names, transitions, outputs,
   constants). No fixes.
10. **Header.** `Status: AS-IS — extracted <date> from the code; describes implemented behaviour,
    not the target design. Reviewed at Phase 10 Checkpoint 0.` Link to `docs/plan/phase_10/`.
    Cite symbols and test names, never line numbers.

**Split point.** If the work overruns one session, land part **(a)** = §1, §2, §3, §3b, §7's
"rows with no test" (states, events, transitions, grid, coverage) and stop with a report; part
**(b)** = §4, §5, §6, the full test map and §8 lands in a second session. Say in the report which
part landed.

## 5. Acceptance criteria

- [ ] The grid is complete: every cell has a row id or a marker, and the marker counts sum to
      states × events.
- [ ] Every one of the assignment sites appears in at least one row, cited by function name and
      with the reason token it will get in WP-10.
- [ ] Every controller-level test is mapped or explicitly "partial"; every other test is under
      the N/A line; rows without tests are listed.
- [ ] The document contains no proposal, recommendation or fix; the only judgement it makes is a
      marker or a DRIFT line.
- [ ] No source, test or other document is changed.

## 6. Verification

- Reviewer (PM) picks ten rows at random and checks each against the code; re-runs the
  `m_cycleState =` grep and compares with the sites cited; checks the grid arithmetic.

## 7. Touch-list / Do-not

**May create:** `docs/domains/task_localization/runtime_state_contract.md`. Nothing else.
**Do not:** edit any source, test, `.pri`, `uml/` or other doc (including the four drift sources);
do not edit the template in `temp_docs/`; do not design the target machine; do not fix the
`qDebug()` or any defect you notice — put it in the report.

## 8. Stop rules

Stop and ask if: a transition's outcome genuinely cannot be read off the code even with the
`UNSPECIFIED` marker and a description; or the document would need a section the template does not
have (add it, and say so in the report — do not stop for that alone).

## 9. Report

`temp_docs/reports/WP-02_report.md`: created file; which part landed; grid counts; the assignment
sites as found (with any difference from 15); rows with no test; DRIFT count; "Findings out of
scope" (suspected defects, one line each, no fixes); open questions.
