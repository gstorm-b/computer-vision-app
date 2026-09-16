# WP-00 — Close Phase 9 with an explicit carry list

**Stage:** 0 · **Size:** S · **Kind:** docs only · **Owner approval:** granted 2026-09-16 (charter §6, all five points) · **Issued:** 2026-09-16 · **Runs in parallel with:** WP-01, WP-02, WP-03 (disjoint touch-lists)

## 1. Goal

Record Phase 9 as closed, with every unfinished item carried to a named destination, so that
Phase 10 starts from a truthful baseline and nothing in Checkpoint Z is silently dropped.

## 2. Read first

1. `AGENT.md` (operating rules; do not skip)
2. `docs/history/plan/phase_9_implementation_plan.md` → "Checkpoint Z — Phase 9 complete" (status **NOT CLOSED**, and its carried list)
3. `docs/backlog/technical_debt_and_next_steps.md` → "Carried Out Of Phase 8" (the shape to mirror for Phase 9)
4. `temp_docs/02_phase10_charter_and_work_breakdown.md` §1.1 (the destinations)

## 3. Facts (verified 2026-09-16)

- Checkpoint Z: build clean, all suites green at the recorded counts, Z1–Z3 landed. Held open only by
  owner-run items and carried work: Phase G / item 54; E4 on hardware (backlog 69); D1 field check
  failing (backlog 70); owner review of `plc_signal_contract.md`; doc build (backlog 44); backlog
  51 (device half), 56, 58 part B, 63, 64, 66, 67, 68.
- `AGENT.md` is **not** in this WP's scope: WP-01 owns every `AGENT.md` edit (including the pointer
  to the Phase 10 charter), so that the two WPs can run at the same time without touching one file.
- Charter §6 is answered: Phase 9 closes with the carry table; plans stay in `temp_docs/` until
  Checkpoint 0 and then move to `docs/plan/phase_10/`.

## 4. Steps

1. In the Phase 9 plan, under "Checkpoint Z", add one status line directly below the existing
   `**Status 2026-09-14: NOT CLOSED.**`: `**Status <date>: CLOSED WITH CARRIED ITEMS — see the carry
   table below and Phase 10 charter.**` Do not edit or delete the existing text.
2. Below that, add a table "Carried out of Phase 9" with columns *Item · What is unfinished ·
   Destination*. One row per item in §3, destination taken from the charter §1.1. Every row must
   name either a Phase 10 WP id or a backlog item number — "later" is not a destination.
3. In `technical_debt_and_next_steps.md`, add a section "Carried Out Of Phase 9 (closed <date>)"
   above "Carried Out Of Phase 8", containing the same table (copy, do not link only — that file
   is read on its own).
4. Update the `**Date:**` / `**Status:**` header of the Phase 9 plan to say closed-with-carry, and
   append one sentence to its "Location note" blockquote: now that the phase is closed, the file's
   place under `docs/history/plan/` is correct; active plans live in `docs/plan/` (created by WP-01).

## 5. Acceptance criteria

- [ ] Checkpoint Z shows both status lines (the original NOT CLOSED and the new CLOSED WITH CARRIED
      ITEMS) and the carry table; every row has a concrete destination.
- [ ] `technical_debt_and_next_steps.md` carries the same table and no other change.
- [ ] The Phase 9 plan header and location note say the phase is closed and where active plans live.
- [ ] No source, test, `.pro`, `.pri`, `uml/` or `AGENT.md` file is touched.

## 6. Verification

- No build required. Reviewer (PM) reads the three files and checks each carried item against
  Checkpoint Z's original list — nothing missing, nothing invented.
- `docs/README.md` links still resolve (open each link touched).

## 7. Touch-list / Do-not

**May edit:** `docs/history/plan/phase_9_implementation_plan.md` (append-only inside Checkpoint Z,
the header and the location note), `docs/backlog/technical_debt_and_next_steps.md`.
**Do not:** rewrite or shorten any existing Phase 9 text; close any backlog item; touch
`later_todo_list.md` (WP-03), `AGENT.md` or `docs/README.md` (WP-01); move any file.

## 8. Stop rules

Stop and ask if: a carried item has no destination in the charter, or Checkpoint Z lists something
not in §3 above.

## 9. Report

Write `temp_docs/reports/WP-00_report.md` with: changed files; the carry table as landed; any
item whose destination you had to guess (there should be none); open questions.
