# WP-00 report — Close Phase 9 with an explicit carry list

**Date:** 2026-09-16 · **Kind:** docs only · **Build:** none required, none run · **Git:** none run (machine cannot).

## 1. Changed files

- `C:\DGB\Project\ncr_picking\docs\history\plan\phase_9_implementation_plan.md`
- `C:\DGB\Project\ncr_picking\docs\backlog\technical_debt_and_next_steps.md`

Nothing else was opened for editing. No source, test, `.pro`, `.pri`, `uml/`, `AGENT.md`, `docs/README.md`
or `later_todo_list.md` change.

### What changed, per file

**`phase_9_implementation_plan.md`** (append-only inside Checkpoint Z, the header, the location note):

- Header `**Date:**` / `**Status:**` — now "2026-09-08 (issued) · 2026-09-16 (closed)" and
  "CLOSED WITH CARRIED ITEMS — 2026-09-16 …", with the original "DRAFT — not started" wording kept in
  the sentence so the issue state is still readable.
- "Location note" blockquote — one sentence appended: the file's place under `docs/history/plan/`
  is correct now that the phase is closed; active plans live in `docs/plan/` (created by WP-01).
  The existing sentences were not edited.
- "Checkpoint Z — Phase 9 complete" — directly below the existing
  `**Status 2026-09-14: NOT CLOSED.**` paragraph, added
  `**Status 2026-09-16: CLOSED WITH CARRIED ITEMS — see the carry table below and Phase 10 charter.**`,
  a short lead-in paragraph, and the "Carried out of Phase 9" table (below). The original NOT CLOSED
  paragraph, the checklist, the OWNER-RUN table and the "Everything carried" bullet are untouched.

**`technical_debt_and_next_steps.md`**: one new section "Carried Out Of Phase 9 (closed 2026-09-16)"
inserted above "Carried Out Of Phase 8 (closed 2026-09-07)", containing a lead-in paragraph in the
Phase 8 section's shape and the same table, copied (not linked only). No other change.

## 2. Carry table as landed (identical in both files)

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

Thirteen rows: the five named items in handoff §3 plus one row per backlog item in the "51 (device
half), 56, 58 part B, 63, 64, 66, 67, 68" group. The charter groups those eight on one line; I gave
each its own row so WP-03 can tick them individually and none is lost in a list.

## 3. Reconciliation against Checkpoint Z (nothing missing, nothing invented)

Checkpoint Z's "Everything carried, with its reason" bullet lists: Phase G and item 54; E4 on
hardware (backlog 69); D1's field check (backlog 70); the owner's review of the PLC signal contract;
the documentation build (backlog 44); and Z3's open items 51 (device half), 56, 58 part B, 63, 64,
66, 67, 68. That is exactly handoff §3 and exactly the thirteen rows. The "what is unfinished" text
for each row was taken from Checkpoint Z's OWNER-RUN table, Task Z3's "Not closed, and why" table,
and the opening lines of backlog items 63, 64, 66, 67, 68, 69, 70 — not paraphrased from memory.

## 4. Items whose destination had to be guessed

**None.** Every destination is the one charter §1.1 names. Two rows resolve a charter *stage* or
*checkpoint* reference to the WP id the handoff requires ("every row must name either a Phase 10 WP
id or a backlog item number"); these are resolutions, not guesses, but the PM should confirm them:

- **D1 / backlog 70** — charter §1.1 says "Stage 3 (role health machine) must have a row for scenario
  S-06". The only Stage 3 WP that owns role health is **WP-32** ("Role health rows + recovery policy
  struct"), so the row names WP-32. Backlog 70 is the primary destination either way.
- **Owner review of `plc_signal_contract.md`** — charter §1.1 says "replace with a review of the
  contract **table** at Checkpoint 2". The table reviewed at Checkpoint 2 is the to-be table, which is
  **WP-21**; the row names WP-21. (The as-is table, WP-02, is read at Checkpoint 0 — I did not name
  it, because the charter says Checkpoint 2.)
- **Phase G / item 54** — charter §1.1 says "Stage 5 pilot"; Stage 5 has exactly one held-trigger WP,
  **WP-50**, so that is not ambiguous.

## 5. Verification done

- Docs only; nothing built, nothing run.
- `docs/README.md` links: the only touched file it links is `backlog/technical_debt_and_next_steps.md`
  (row "Active technical-debt backlog"); the Phase 9 plan is not linked from `docs/README.md`. Neither
  file moved, so both resolve.
- Both new sections use the same relative-link conventions as their neighbours
  (`../history/plan/…`, `later_todo_list.md`).

## 6. Open questions

1. **A1's M-only hardware confirmation is in Checkpoint Z's OWNER-RUN table but not in its carried
   list, and therefore not in handoff §3 — I did not add a row.** The row reads "untested on hardware
   — no M-only station identified (open question O-3); A1 ships on unit evidence", i.e. the plan
   resolved it by decision (O-3's recommendation) rather than carrying it. If the PM wants it carried
   anyway, it is a one-row addition to both tables; a destination would need naming (there is no
   backlog item for it today).
2. **The status line names "Phase 10 charter" without a path**, exactly as the handoff worded it,
   because the charter sits in `temp_docs/` until Checkpoint 0. The lead-in paragraph says it moves to
   `docs/plan/phase_10/` at Checkpoint 0. Once WP-01 lands and the move happens, a link could be
   added in one edit.
3. **Column names differ from the Phase 8 carry table** (`#` · Item · Needs). I used the handoff's
   columns (Item · What is unfinished · Destination) in both places, so the two Phase 9 copies match
   each other; the Phase 8 table was not touched.
4. **The Location note's first sentence still says "it is the CURRENT plan for Phase 9"** — left as
   written per "do not rewrite or shorten any existing Phase 9 text"; the appended sentence states
   the phase is closed and where active plans live.

## 7. Findings out of scope (not fixed, for the record)

- None in files outside the touch-list that would block WP-01/02/03. Nothing in `AGENT.md`,
  `docs/README.md` or `later_todo_list.md` was edited.

---

## PM review — 2026-09-16 — ACCEPTED with one addition

Checked: both changed files opened at the changed sections; the thirteen rows reconciled against
Checkpoint Z's "Everything carried" bullet; touch-list respected (no edit outside the two files).

Decisions on the open questions:

1. **A1 M-only hardware check — carried after all.** "Ships on unit evidence" is an unverified claim,
   and a closeout that omits an unverified claim is not truthful. The PM added a fourteenth row to
   both tables (destination: a new backlog item filed at Checkpoint 0, after WP-03's triage lands;
   non-blocking). Both copies remain identical.
2. Charter path in the status line — acceptable; a link is added when the charter is published to
   `docs/plan/phase_10/` at Checkpoint 0.
3. Column names differing from the Phase 8 table — acceptable.
4. Location note's first sentence — acceptable; the appended sentence carries the correction.

Resolutions confirmed: D1/70 → WP-32; Z2 review → WP-21 at Checkpoint 2; Phase G → WP-50.
