# Phase 10 — Runtime core redesign: charter and work breakdown

**Date:** 2026-09-16
**Status:** approved by the owner 2026-09-16 (all five points of §6); Stage 0 issued 2026-09-16 —
WP-00..03 handed to agents, handoffs in `wp/`, reports back into `reports/`.
**Roles:** I am PM + system design engineer; implementation is handed to other agents through
handoff md files.
**Environment constraint:** this machine does not run git. Every WP is designed to be worked with a
touch-list + report; the owner syncs/tags from the permitted machine at each checkpoint.
**Working copy:** the Vietnamese original in `temp_docs/` until Checkpoint 0.

---

## 1. Assessment of the "close 9 → reorganise → redesign" plan

**Conclusion: worth doing, on three conditions.** Without any one of these three the plan turns into
a laborious "cleanup phase" after which behaviour is no better, or into a rewrite that loses the
knowledge already paid for on the cell.

### 1.1 Close Phase 9 — close it *honestly*, not *cleanly*

Checkpoint Z currently reads **NOT CLOSED** because of owner-run and carried items. Closing Phase 9
is a bookkeeping action, not a technical milestone: write the closeout with a carried table, each
item naming only a destination (a Phase 10 WP or a backlog number), then stop.

| Carried out of Phase 9 | Proposed destination |
|---|---|
| Phase G / item 54 (held trigger), not started | **The Stage 5 pilot** — the G2 design is reused, implemented on the new core |
| E4 on hardware (the real PLC refuses the write) | backlog 69, not blocking |
| D1 vision-output reconnect failing on the cell → item 70 | Stage 3 (role health machine) must have a row for scenario S-06; fix the device bug separately |
| Owner review of [`plc_signal_contract.md`](../../domains/task_localization/plc_signal_contract.md) (Z2) | Replaced by a review of the **contract table** at Checkpoint 2 — more effective than reviewing 440 lines of prose |
| Doc build (tools missing — backlog 44) | not blocking |
| Backlog 51 (device half), 56, 58B, 63, 64, 66, 67, 68 | Triage in WP-03: what the redesign absorbs, what stays |

Do not do Phase G inside Phase 9. Do not wait for E4 hardware or item 70. Both have a better home in
Phase 10.

### 1.2 Reorganise — *bounded* and *behaviour-preserving*

Reorganise only what the redesign needs. The list is closed; it is not extended mid-flight:

| Do | Do not |
|---|---|
| `docs/plan/` for the current plan; `docs/decisions/` with globally numbered DRs; import the Phase 9 D1–D8 rulings as DR-0001..DR-0008 | Do not move modules, do not rename source files |
| [`runtime_state_contract.md`](../../domains/task_localization/runtime_state_contract.md) extracted *as-is* from the code (the current state table, including the ugly parts) | Do not rename states, fault codes or signal names (the PLC program depends on them) |
| One `transitionTo()` funnel + a guard table in *observation* mode + a fixed trace line | Do not modify the existing tests (the 159 cases are the safety net) |
| Triage the backlog's 65 items, numbered up to 72: close/merge/keep, one line each | Do not rewrite prose docs — only mark them "explanatory, see table" |
| (Optional) split the contract test's `main.cpp` by area | Do not split it if that slows Stage 3 down; it may move to the end |

Acceptance for every WP in this part: the contract suite has **the same test count and 0 failures**
before and after.

### 1.3 Redesign — *design it whole first*, *replace the core behind the adapter*, no rewrite from scratch

I agree with the assessment "the logic is not tight yet, it hides many bugs", but I want to say
precisely *where* it is, because the fix depends on that. The current structure lets the following
**six classes of defect** exist without any test seeing them:

1. State is assigned in 15 places; one of them (`onVisionOutputResultFinished()`) already goes around
   the gate. Nothing prevents a second one.
2. An event arriving in an unexpected state is handled by scattered `if` chains with no complete
   table → it is impossible to answer "which (state, event) pair is unhandled?".
3. `Recovering` carries two meanings (role outage; cycle fault waiting for auto-clear) with two
   different output sets.
4. `CycleState` merges **three orthogonal concerns**: the progress of the cycle, the health of the
   roles, and the validity of the selection/config. Each concern already has its own mechanism in the
   code (cycleId, role context, per-signal latch) but is forced into one 6-value enum → the states
   "NotReady / Recovering / Faulted" are combinations that have been flattened, and every new case
   (late PLC connect, held trigger, TCP) has to be flattened further.
5. `device()->connectStatus()` is read across threads without synchronisation; a nested `QEventLoop`
   sits inside the lifecycle.
6. Policy is constants and scattered `if`s, with no registry.

From that, three options:

| Option | Pro | Con | Assessment |
|---|---|---|---|
| **A. Patch each defect on the current code** | Cheap per step | Does not address classes 2 and 4; the TCP command source will pile on top | Only suitable if the next six months of roadmap do not touch the runtime |
| **B. Rewrite the controller from a new spec** | Clean | 159 tests + the PLC contract + the field lessons (latch, first-poll suppression, cycleId, WaitingTriggerReset-is-busy) are easy to lose silently; every Phase F defect was found by the owner on the cell — a rewrite multiplies that count | Not recommended |
| **C. Design the new core whole (table + API), build the core *beside* the old one, run both the new table tests and the 159 old tests, then move the adapter onto the new core slice by slice** | Keeps all existing knowledge as the net; the design is still a real redesign; every step can stop | Longer than B on paper; needs touch-list discipline | **Recommended** |

Option C is a "redesign", not a "light refactor": the new core has a completely different structure
(§2); only the adapter *shell* and the PLC contract are kept.

### 1.4 Is it worth it, compared with building features?

Phase 4 is on hold because "the product needs more features". Phase 10 costs roughly 20 S/M-sized
WPs. It is worth it **if** the following two things are true — and per the owner's notes they are:

- the TCP command source (`Trigger,` / `ChangePart,` / `ChangeCamera,`) is about to arrive → it
  changes the shape of the state machine;
- the owner still finds runtime defects on the cell every phase.

If the roadmap for the next few months is only mask + manual, doing **Stage 0 + 1 + 2** (about 8 WPs,
no behaviour change, producing the contract table and the DRs) and then stopping is also a good
outcome: the Phase 10 manual will have a canonical source, and Stages 3–5 reopen when needed.

---

## 2. Proposed design direction (a hypothesis for Stage 2 to verify, not yet a decision)

Split `CycleState` into **three orthogonal state machines + one fault register + two derived
functions**, all inside a plain C++ class (no QObject, no thread, no real timer):

```
RuntimeCore
├─ CyclePhase        Idle | Grabbing | Matching | Sending | Publishing | AwaitingTriggerReset
├─ RoleHealth[role]  Connected | Connecting | Lost           (camera, primaryPlc, visionOutput)
├─ Selection[index]  Adopted(n) | Refused(n, latched)         (camera, patternGroup)
├─ ConfigValidity    setupValid, calibrationValid, groupUsable
├─ FaultRegister     none | latched(code, armedAutoClear)
├─ derived: isReady()        = setupValid && all Connected && no Refused && phase==Idle && fault==none
├─ derived: outputSnapshot() = f(phase, fault, isReady, selection)   → the set of PLC tag values
└─ step(event, ctx) -> Actions   (one function, one table, every transition goes through it)
```

- **Event** is source-independent: `TriggerRise`, `TriggerFall`, `ErrorResetRise`, `SelectCamera(n)`,
  `RoleStatus(role, s)`, `GrabDone(ok)`, `MatchDone(id, r)`, `SendDone(ok)`, `WriteDone(id, ok)`,
  `Timer(kind)`, `Snapshot(map)`, `Setup(ctx)`, `Teardown`.
- **Action** is data: `Publish(snapshot)`, `RequestGrab`, `RequestMatch`, `RequestSend(positions)`,
  `RequestReconnect(role)`, `StartTimer(kind, ms)`, `CancelTimer(kind)`, `Log(level, msg)`,
  `Trace(from, to, event, reason)`.
- **Policy** is a parameter struct passed into the core: recovery interval, handshake retry
  budget/delay, auto-clear ms, positions cap, required signals.
- **Adapter** (the current controller, shrinking step by step): maps runner signal / PLC tag / TCP
  message / timer → event; executes Actions through the runners; keeps cycleId and stale-drop; keeps
  everything Qt.

Why this does not violate "no broad abstractions before a second implementation": these three
machines **already exist implicitly** in the code (cycleId + WaitingTriggerReset; role recovery
context; per-signal latch) — this is factoring what is already there, and the second implementation
of a "command source" (TCP) is already in the owner's notes.

Two design decisions need the owner's confirmation at Stage 2 (each will get a PQ card):

- `Recovering` and `Faulted` are no longer states; they are *combinations* (Lost role / Refused /
  latched fault) and the dashboard displays them by combination. The names on the PLC side do not
  change.
- PLC output is published as a **diff snapshot** (only the tags whose value changed) instead of
  through the chain of `publishXxxOutputs()`; handshake retry still applies to the 5 H tags.

---

## 3. Work breakdown

Size: XS/S/M per the planning skill (≤ 5 files per WP). Each WP is its own handoff md (template in
§5). Parallel only when the touch-lists are disjoint.

### Stage 0 — Close and set up the frame (docs only, fully parallelisable)

| WP | Content | Size | Touch |
|---|---|---|---|
| **WP-00** | Phase 9 closeout: the Checkpoint Z status line, a carried table with destinations, the header + location note of the Phase 9 plan | S | the Phase 9 plan, [`technical_debt_and_next_steps.md`](../../backlog/technical_debt_and_next_steps.md) |
| **WP-01** | `docs/plan/` + `docs/decisions/`; import D1–D8 as DR-0001..0008; `docs/README.md` and `AGENT.md` point to both folders and to the Phase 10 charter | S | `docs/plan/**`, `docs/decisions/**`, `docs/README.md`, `AGENT.md` |
| **WP-02** | Extract `runtime_state_contract.md` **as-is** from the code: 6 states × 16 events, every cell either a value or `UNSPECIFIED`; map the 159 existing tests → rows; list the cells with no test | M | 1 new doc |
| **WP-03** | Triage the backlog's 65 items, numbered up to 72: close / merge / keep / absorbed-by-Phase-10, one line per item | S | [`later_todo_list.md`](../../backlog/later_todo_list.md) (insert one table at the top) |
| **WP-04** | English versions of the charter and the PQ list, to be published to `docs/plan/phase_10/` at Checkpoint 0 (the Vietnamese originals remain the owner's working copies) | S | `temp_docs/en/` |

**Stage 0 progress (2026-09-16):** WP-00 ✅ accepted (+1 carried row for A1 M-only, added by the PM);
WP-01 ✅ accepted; WP-03 ✅ accepted (21 closed-already · 35 keep · 8 absorb · 1 merge · 0
needs-owner); WP-04 ✅ accepted; WP-02 running (resumed after a session rate limit).

**Housekeeping the PM does at Checkpoint 0, once all Stage 0 WPs are done (one touch per file):**

- ✅ 2026-09-16 `later_todo_list.md`: closing note for items 60 and 23; items 71 (A1 M-only) and 72
  (residual of 61) filed; the two carried tables point to backlog 71.
- ✅ 2026-09-16 review of WP-01: rule 5 for DRs imported from history; the DR-0003/0008 cluster
  corrected; `history/plan/` added to the doc map; the Phase 9 plan header points to
  `docs/decisions/`.
- Publish the English charter to `docs/plan/phase_10/charter.md`; add the link to the Checkpoint Z
  status line and to the "Carried Out Of Phase 9" section.
- Move `wp/`, `reports/` and the PQ list into `docs/plan/phase_10/` (the PQ list is translated into
  English as part of the move).

**Checkpoint 0:** the owner reads the as-is table (one page) and the `UNSPECIFIED` list; confirms
Phase 9 is closed. Sync/tag `phase9-closeout`.

### Stage 1 — Make the current machine observable (code, no behaviour change, sequential)

| WP | Content | Size | Touch |
|---|---|---|---|
| **WP-10** | `transitionTo(next, reason)` is the only write path (15 places); `canTransitionCycleState()` in a new header, in **observation mode** (log WARN, do not block); the trace line `RT state=<from>-><to> event=<e> reason=<r> cycle=<id>`; a source-grep test for "exactly one assignment point" | M | controller `.h/.cpp`, new header, contract test |
| **WP-11** | Delete the dead entry points (`execute()`, the queue setters, `onCommDeviceValueChanged()`) — **conditional on the PQ-13 ruling**; if the owner keeps them as the UI/TCP path, drop this WP | S | `task_localization.*`, controller |
| **WP-12** *(optional)* | Split `tests/architecture_contract_test/main.cpp` into 4 files by area, same binary | M | tests |

**Checkpoint 1:** contract suite 159/0 (159 = 157 `test_` functions + `initTestCase`/`cleanupTestCase`; or the same count after WP-11); one owner field run whose
app_log carries `RT state=` lines and **0 guard warnings**. If there is a warning → that is an
unexpected transition; record it in the as-is table before going any further.

### Stage 2 — Design (me + reviewer agent + owner)

| WP | Content | Size | Who |
|---|---|---|---|
| **WP-20** | PQ cards for cluster A (PQ-1..4), PQ-6, PQ-8/9/11 (cheap), PQ-12/13 (command source) | M | me |
| **WP-21** | The **to-be** table per §2: state × event, output snapshot, scenarios S-01..S-07+, policy registry; every cell has a DR or `UNSPECIFIED` | M | me |
| **WP-22** | Adversarial review of the to-be table by a new agent (fresh context): pair completeness, duplicate guards, invariants, every old test still mappable onto a new row | S | reviewer agent |
| **WP-23** | `RuntimeCore` API spec (event/action/policy structs, `step()`), following `api-and-interface-design` | S | me |

**Checkpoint 2 (the most important gate):** the owner's rulings for the PQs → DRs Accepted; the
to-be table has no empty cell left; every old test has a destination row; the core API is approved.
No core code is written before this gate.

### Stage 3 — Build the core beside the old one (new code, new test project; parallelisable per sub-machine)

| WP | Content | Size | Touch |
|---|---|---|---|
| **WP-30** | `RuntimeCore` skeleton + table-driven test project `tests/runtime_core_test` (`_data()`), no event loop; a completeness-check function generated from the table | M | new files |
| **WP-31** | Cycle machine rows (Idle→…→AwaitingTriggerReset, stale drop, abort) | M | core + test |
| **WP-32** | Role health rows + recovery policy struct | S | core + test |
| **WP-33** | Selection + config validity + fault register rows (latch, auto-clear, ErrorReset) | M | core + test |
| **WP-34** | `outputSnapshot()` + diff publish + handshake retry policy | M | core + test |

**Checkpoint 3:** every row has a test and is green; completeness check = 0 missing; not yet wired
into the app. The old contract suite is unchanged.

### Stage 4 — Move the adapter onto the new core (sequential, touches the controller)

| WP | Content | Size |
|---|---|---|
| **WP-40** | Adapter: runner signal / PLC tag / timer → event; Action → runner request; the controller delegates cycle + fault + readiness to the core; delete the old code paths slice by slice | M (split in 2 if it exceeds 5 files) |
| **WP-41** | Run **the 159 old tests unmodified** on the new core; every red one is classified: new-core bug / old test pinning old behaviour that a DR has changed (a test may only be changed when there is a DR) | S |
| **WP-42** | Owner-run S-01..S-07 per script + expected `RT` lines; the agent replays app_log → an expected vs observed table | S |
| **WP-43** | Prose docs shrink to explanation + a link to the table; UML generated from the table; DRs → Implemented | S |

**Checkpoint 4:** contract suite ≥ 159/0; the replay matches; DRs Implemented; sync/tag
`phase10-core`.

### Stage 5 — First capability on the new core (proving the design)

| WP | Content |
|---|---|
| **WP-50** | Phase G (held trigger) per the DR from PQ-1: add rows, add tests, owner-run |
| **WP-51** | TCP command source: a new adapter maps message → event; **the core is not modified** — this is the success criterion for the whole phase |

---

## 4. Risks and mitigations

| Risk | Mitigation |
|---|---|
| The reorganisation swells into unbounded cleanup | The closed list in §1.2; every WP must hold 159/0; WP-12 is optional |
| A design that is beautiful on paper and wrong on the cell | Stage 1 runs first to collect real traces; the to-be table must map every old test; Checkpoint 4 has the log replay |
| Old tests "fixed until green" | The WP-41 rule: a test is only changed when a DR says the behaviour changed; the report must list every changed test and its DR |
| No local git → an agent works outside its scope | Touch-list in the handoff; changed-file list in the report; the reviewer cross-checks; the owner syncs/tags from the permitted machine at each checkpoint |
| Two agents colliding on the controller | Stages 1 and 4 are sequential; Stages 0 and 3 run in parallel along disjoint files |
| Owner-runs piling up | Only three points: Checkpoint 1 (one field run to collect the trace), 4 (S-01..S-07), 5 |
| Bumping the schema / the PLC contract | Do not rename a tag or a fault code in Phase 10; if some DR demands a rename, write a separate migration as C3 did |

---

## 5. PM ↔ agent working protocol (over md, no local git)

```
temp_docs/            (while awaiting approval)  →  docs/plan/phase_10/   (after approval)
├─ charter.md                       ← this file
├─ wp/WP-xx_<slug>.md               ← handoff, I write it, the agent reads it
├─ reports/WP-xx_report.md          ← the agent writes it when done
└─ (docs/decisions/DR-xxxx.md)      ← I write it after the owner's ruling
```

**Handoff (me → agent), at most ~1 page:**

1. Goal — one sentence.
2. Read first — ≤ 5 links (`AGENT.md` is always number 1).
3. Facts — what has been verified, citing symbol/test id.
4. Steps — in order.
5. Acceptance — ≤ 5 bullets, checkable.
6. Verification — the build/test command verbatim from `build_and_verification.md`; test names.
7. Touch-list — the files that may be changed; **Do-not** — what must not be touched.
8. Stop rules — when to stop and ask (for example: an old test goes red; a file outside the
   touch-list needs changing; behaviour is found that is not in the table).
9. Report format — the section below.

**Report (agent → me):**

1. Changed files — complete, one by one.
2. Evidence — test ids green/red, counts, log lines.
3. Negative check — what was injected, which test went red as intended, and that it was restored
   (confirmed by naming the injection site again).
4. Deviations — where it differs from the handoff, and why.
5. Findings out of scope — one line each, not fixed.
6. Open questions.

**Review (me):** read the report, open every file in the changed-file list, re-run the verification
command, cross-check the touch-list. A WP without a complete report does not count as done. The
owner only looks at checkpoints.

**Language:** handoff, report, DR, contract — English (they will be committed). The charter and the
exchange with the owner — Vietnamese.

---

## 6. Decisions taken by the owner (2026-09-16)

The five points the owner had to decide before the phase could start, with their rulings quoted
verbatim. All five are decided; Stage 0 was issued on the strength of them.

1. Close Phase 9 per the carried table in §1.1 — agreed?

    > Đồng ý

    *Agreed.* — Phase 9 closes on the carried table exactly as it stands in §1.1.

2. The scope of the reorganisation per §1.2 — anything to drop or add? (in particular WP-12,
   splitting the test, and WP-11, deleting the dead entry points)

    > Tổ chức lại theo 1.2 đã đề xuất.

    *Reorganise as proposed in §1.2.* — the closed list stands as written, nothing dropped and
    nothing added.

3. The design direction in §2 — accept it as the hypothesis for Stage 2, or see an alternative
   first?

    > Chấp nhận.

    *Accepted.* — §2 is the hypothesis Stage 2 verifies; no alternative is required first.

4. Run the whole of Stage 0–5, or stop at Stage 2 and decide again (§1.4)?

    > Theo đề xuất của bạn.

    *As you propose.* — the full Stage 0–5 programme, with the option to stop at every checkpoint.

5. Where the plan lives: keep it in `temp_docs/` until Checkpoint 0, then move it to
   `docs/plan/phase_10/` — agreed?

    > Đồng ý.

    *Agreed.* — the plan stays in `temp_docs/` and moves to `docs/plan/phase_10/` at Checkpoint 0.

Sample handoffs were written in advance so the owner could see the shape:
[wp/WP-00_phase9_closeout.md](wp/WP-00_phase9_closeout.md) and
[wp/WP-10_transition_funnel.md](wp/WP-10_transition_funnel.md).
