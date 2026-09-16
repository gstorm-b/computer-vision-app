# WP-10 — One transition funnel for `CycleState`, observe-mode guard, fixed trace line

**Stage:** 1 · **Size:** M · **Kind:** code, **behaviour-neutral** · **Depends on:** WP-02 (as-is table) · **Sequential:** yes (touches the controller)

## 1. Goal

Make every `CycleState` change go through one function that logs a fixed trace line and checks a
transition table in observe mode, without changing any runtime behaviour, so that the field log can
show which transitions actually happen before the redesign decides which ones should.

## 2. Read first

1. `AGENT.md`; `src/runtime/AGENTS.md`; `src/model/AGENTS.md` (if present)
2. `src/model/task_state_machine.h` — `canTransitionTaskState()`: the pattern to mirror
3. `src/model/itask.cpp` — `ITask::transitionTaskState()`: the funnel pattern to mirror
4. `temp_docs/templates/runtime_state_contract_template.md` §3 and the as-is table from WP-02
5. `docs/rules/build_and_verification.md` and `docs/rules/doc_comment_style.md`

## 3. Facts (verified 2026-09-16)

- `m_cycleState` is assigned at 15 sites in 10 functions of `localization_runtime_controller.cpp`:
  `setActiveCameraNumber` (2), `setActivePatternGroupNumber` (2), `setup` (3), `handlePlcValues`
  (2), `markRuntimeReady` (1), `reportSignalTypeMismatch` (1), `startCycle` (1), `abortCycle` (1),
  `handleRoleStatusChanged` (1), `onVisionOutputResultFinished` (1). Confirm with a grep for
  `m_cycleState =` before you start; the count is the baseline for the new test.
- `onVisionOutputResultFinished()` assigns `ReadyForTrigger` directly and then calls
  `markRuntimeReady()`. **Keep that behaviour** in this WP; it is PQ-11 and belongs to Stage 2.
- The contract suite is at **159 passed / 0 failed** (Checkpoint Z, 2026-09-14). Re-measure before
  editing; if it differs, stop.
- `architecture_contract_test` already contains source-grep style tests (the include-layering
  contract), so a test that greps the controller source is in keeping with the suite.

## 4. Steps

1. Add to the controller (private): `void transitionTo(CycleState next, const char *reason);`
   It must: read `m_cycleState` as `from`; if `from == next`, log at DEV level and return without
   a trace line; otherwise call `canTransitionCycleState(from, next)` and, when it returns false,
   log **one** `LOG_DEV_WARN` naming from/to/reason (observe mode: **never** refuse); then assign;
   then log the trace line at DEV level exactly as:
   `RT state=<from>-><next> event=<reason> cycle=<m_activeCycleId>`
   where state names are the enum identifiers verbatim (`ReadyForTrigger`, not a display name).
2. Add `src/model/cycle_state_machine.h` with `inline bool canTransitionCycleState(CycleState from,
   CycleState to)` whose allowed pairs are **exactly the rows of the as-is table from WP-02** — no
   more, no fewer. Document above the function that the table describes the machine as
   implemented on the WP-02 date and is enforced in observe mode only. It is header-only and
   included from the controller `.cpp`; register it in `src/model/model.pri` under HEADERS and
   nowhere else (never in a shell `.pro`, per `AGENT.md`).
3. Replace all 15 assignments with `transitionTo(..., "<reason>")`. Reasons are short stable
   tokens, one per site, e.g. `"setup.reset"`, `"trigger.fall.faulted"`, `"cycle.abort"`,
   `"role.lost"`, `"send.ok"`. Do not reorder any surrounding statements.
4. Add to the contract test: `test_cycle_state_has_exactly_one_assignment_site` — reads
   `localization_runtime_controller.cpp` from source (the way the layering test locates files) and
   asserts that `m_cycleState =` occurs exactly once (inside `transitionTo`).
5. Add `test_cycle_state_guard_table_covers_every_as_is_row` — for each row of the as-is table
   (hard-code the pairs in the test, with a comment pointing at WP-02), `QVERIFY(canTransitionCycleState(from, to))`;
   and for three pairs the table does not contain, `QVERIFY(!...)`.
6. Doxygen `///` comments on the new function and header per `doc_comment_style.md`.

## 5. Acceptance criteria

- [ ] Grep `m_cycleState =` in the controller `.cpp` returns exactly one hit, inside `transitionTo()`.
- [ ] Every transition at runtime produces one `RT state=…` DEV log line; a same-state call
      produces none.
- [ ] The guard never blocks: with every pair rejected (temporarily return `false`), the whole
      contract suite still passes — this is the observe-mode proof.
- [ ] Contract suite: **161 passed / 0 failed** (159 + the two new cases); no existing test edited.
- [ ] Umbrella build clean; both shells relink.

## 6. Verification

Commands verbatim from `docs/rules/build_and_verification.md` ("umbrella build", "contract test",
`-functions` shape check before trusting the total).

**Negative checks (both halves, restore in the same session, state the restore in the report):**
- (a) Change one call site back to a raw assignment → `test_cycle_state_has_exactly_one_assignment_site`
      red; every other case green. Restore.
- (b) Remove one pair from `canTransitionCycleState()` → `..._guard_table_covers_every_as_is_row`
      red; all cycle tests **stay green** (observe mode). Restore.
- (c) Make the guard refuse (`if (!ok) return;` before the assignment) → at least one existing cycle
      test must still be green **and** the suite must not hang. This is a smoke check that the
      table is complete for the paths the suite exercises; if any existing test goes red, record
      which pair is missing from the as-is table — that is a finding for WP-02, not something to
      fix here. Restore.

## 7. Touch-list / Do-not

**May edit:** `src/model/localization_runtime_controller.h`, `src/model/localization_runtime_controller.cpp`,
`src/model/cycle_state_machine.h` (new), `src/model/model.pri`, `tests/architecture_contract_test/main.cpp`.
**Do not:** change any state name, fault code, published signal or the order of any existing
statement; touch `task_localization.*`; touch `uml/` (no structural change — say so in the report);
fix PQ-11 or any other behaviour you notice.

## 8. Stop rules

Stop and ask if: the pre-edit suite is not 159/0; the grep baseline is not 15; negative check (c)
turns an existing test red (report the pair, do not patch the table silently); or a transition you
meet is not in the as-is table.

## 9. Report

`temp_docs/reports/WP-10_report.md`: changed files; suite counts before/after with the `-functions`
shape check; the three negative checks with which cases went red/stayed green and the restore
confirmation; the list of 15 reason tokens as landed; findings out of scope; open questions.
