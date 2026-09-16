# WP-03 — Backlog triage against Phase 10

**Stage:** 0 · **Size:** S · **Kind:** docs only · **Owner approval:** granted 2026-09-16 (charter §6) · **Issued:** 2026-09-16 · **Runs in parallel with:** WP-00, WP-01, WP-02

## 1. Goal

Give every item in `docs/backlog/later_todo_list.md` a one-line disposition relative to Phase 10 —
absorbed by a work package or policy question, kept, already closed, merged, or needs the owner —
without editing any item's body, so the redesign knows what it owns and nothing is forgotten twice.

## 2. Read first

1. `AGENT.md`
2. `docs/backlog/later_todo_list.md` — every `## <n>.` heading and the status lines beneath it
   (bodies only where the status is unclear)
3. `docs/backlog/technical_debt_and_next_steps.md` (cross-cutting items already tracked there)
4. `temp_docs/02_phase10_charter_and_work_breakdown.md` §1.1 (carried list) and §3 (WP ids)
5. `temp_docs/01_open_policy_questions.md` (PQ ids, and which backlog items they cite)

## 3. Facts (verified 2026-09-16)

- The file has 70 numbered items (numbers 1–70; 12 and 16–19 do not exist; the order is not
  sequential — 53 precedes 52, and 50 follows 52). Count the `## <n>.` headings yourself and use
  your count.
- Many items already carry a `RESOLVED` / `CLOSED` status line at the top of their body.
- The charter names 51 (device half), 54, 56, 58 part B, 63, 64, 66, 67, 68, 69, 70 as carried out
  of Phase 9. The PQ list maps 54 → PQ-1, 58B → PQ-2/PQ-3, 64 → PQ-10, 68 → PQ-8, 70 → PQ-7,
  63 → PQ-15, 67 → PQ-14, 66 → (none yet), 53 → fixed-by-reasoning.
- WP-00 is editing `technical_debt_and_next_steps.md` at the same time — do not touch it.

## 4. Steps

1. Classify every item with exactly one disposition:
   - **CLOSED-ALREADY** — the item's own status says resolved/closed; nothing to do.
   - **KEEP** — open, not affected by Phase 10; stays as written.
   - **ABSORB → WP-xx / PQ-n** — Phase 10 will resolve or re-decide it; name the WP or PQ id from
     the charter or PQ list. If it needs a Stage 2 decision, name the PQ; if it is code that the
     new core replaces, name the Stage 3/4 WP.
   - **MERGE → item n** — duplicate or strict subset of another open item.
   - **NEEDS-OWNER** — cannot be classified without a ruling; say why in one line.
2. Insert **one new section at the top** of `later_todo_list.md`, immediately after the intro
   paragraph and before `## 1.`: heading `## Triage 2026-09-16 (Phase 10 start)`; three or four
   lines saying why (Phase 9 closed with carried items; Phase 10 redesigns the runtime core), that
   dispositions are relative to Phase 10, and that **item bodies below are untouched**; then a table
   with columns `# · Title (≤ 8 words) · Existing status · Disposition · Target · Note (≤ 1 line)`,
   one row per heading, in file order.
3. Below the table, one line of counts per disposition.
4. Change nothing below the new section.

## 5. Acceptance criteria

- [ ] The table has exactly one row per `## <n>.` heading in the file (your count, stated in the
      report), in file order.
- [ ] Every ABSORB row names a WP id that exists in the charter §3 or a PQ id that exists in the
      PQ list; every MERGE row names an item that is not itself CLOSED-ALREADY.
- [ ] Everything below the new section is byte-for-byte unchanged (reviewer checks the line count:
      old total + inserted lines).
- [ ] English; no item is closed, reworded or moved.

## 6. Verification

- `Select-String -Path docs\backlog\later_todo_list.md -Pattern '^## \d+\.' | Measure-Object`
  equals the row count.
- Reviewer (PM) spot-checks ten rows against the item bodies.

## 7. Touch-list / Do-not

**May edit:** `docs/backlog/later_todo_list.md` — insert-only, at the top.
**Do not:** touch `technical_debt_and_next_steps.md` (WP-00), any item body, any other file;
do not close, merge or renumber an item in place — the table is the only artefact.

## 8. Stop rules

Stop and ask if the file's structure differs materially from §3 (for example headings that are
not `## <n>.`), or if more than ten items end up NEEDS-OWNER — that would mean the charter is
missing a cluster, which is a PM problem, not a triage problem.

## 9. Report

`temp_docs/reports/WP-03_report.md`: changed file; heading count; counts per disposition; the
NEEDS-OWNER rows with their reasons; items where you were unsure between two dispositions; open
questions.
