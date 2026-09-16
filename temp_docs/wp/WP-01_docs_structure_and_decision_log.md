# WP-01 — Active-plan folder, global decision log, import Phase 9 decisions D1–D8

**Stage:** 0 · **Size:** S · **Kind:** docs only · **Owner approval:** granted 2026-09-16 (charter §6) · **Issued:** 2026-09-16 · **Runs in parallel with:** WP-00, WP-02, WP-03 (disjoint touch-lists)

## 1. Goal

Give the project one place for the active plan (`docs/plan/`) and one globally numbered decision
log (`docs/decisions/`), import the eight Phase 9 owner decisions as DR-0001..DR-0008, and point
`AGENT.md` and `docs/README.md` at both, so that Phase 10 starts with decisions that never reset
per phase and a plan that no longer lives under "history".

## 2. Read first

1. `AGENT.md` (operating rules; English-only for committed text)
2. `docs/README.md` (the documentation map you will extend)
3. `docs/history/plan/phase_9_implementation_plan.md` → sections "Decisions taken with the project
   owner (locked 2026-09-07)" (D1–D6) and "Two decisions taken after the draft, 2026-09-08" (D7, D8)
4. `temp_docs/templates/policy_decision_record_template.md` → §B "Decision Record" (the DR shape)
5. `.claude/skills/documentation-and-adrs/SKILL.md` (invoke it via the Skill tool before writing)

## 3. Facts (verified 2026-09-16)

- `AGENT.md` "Read First" lists `docs/README.md`, the rules docs and the two backlog files; it
  names no plan folder and no decision log. "Highest Priority Next Work" does not mention Phase 10.
- `docs/README.md` has a table "Active Planning And Backlog" listing the two backlog files, the
  architecture backlog, the vision-widget handoff and the closed restructure plan.
- The Phase 9 plan's "Location note" says it lives under `docs/history/` although it was the current
  plan, and that the recommendation to create `docs/plan/` is "five phases old". WP-00 is closing
  Phase 9 at the same time as this WP, so the plan **stays** in `docs/history/plan/` — do not move it.
- D1–D6 are recorded in the plan with columns *Decision (verbatim)*, *Rationale*, *Lands in*. D7 and
  D8 carry the owner's words in Vietnamese inside quotation marks. Landing tasks: D1 → C3, D2 → F1,
  D3 → E1–E4 (fault `301`), D4 → C4, D5 → Z1, D6 → Z3, D7 → no code change (standing behaviour),
  D8 → C6.
- The Phase 10 charter is published to `docs/plan/phase_10/charter.md` by the PM at Checkpoint 0;
  until then the working draft is `temp_docs/02_phase10_charter_and_work_breakdown.md` (Vietnamese).

## 4. Steps

1. Create `docs/plan/README.md`: what lives here (the active phase's charter, its work packages and
   their reports, and the owner's request notes for that phase); lifecycle (a phase's folder moves
   to `docs/history/plan/` when the phase closes); layout `phase_<n>/charter.md`, `phase_<n>/wp/`,
   `phase_<n>/reports/`, `phase_<n>/request.md`; language English; link to `../decisions/README.md`.
2. Create `docs/plan/phase_10/README.md` (stub, ≤ 10 lines): "Phase 10 — runtime core redesign.
   The charter is published here at Checkpoint 0. Until then the working draft is
   `temp_docs/02_phase10_charter_and_work_breakdown.md`."
3. Create `docs/decisions/README.md`: purpose (the record of owner rulings that constrain design);
   numbering `DR-0001`… global and never reset per phase; statuses `Proposed → Accepted →
   Implemented → Superseded by DR-nnnn`; file naming `DR-<nnnn>-<slug>.md`; the rule that contract
   docs describe current behaviour only and history goes into the DR that changed it; an index table
   (DR · Title · Status · Phase · Supersedes) listing DR-0001..0008; and the DR template copied
   **verbatim** from `temp_docs/templates/policy_decision_record_template.md` §B (the fenced block
   and the four rules beneath it).
4. Create `docs/decisions/DR-0001-…` to `DR-0008-…`, one per D1..D8, in that order, following the
   template exactly. Fill from the Phase 9 plan:
   - **Decision** = the *Decision (verbatim)* column, unchanged.
   - **Owner's ruling (verbatim)** = for D7/D8 the quoted Vietnamese sentence; for D1–D6 write
     "Recorded in English in the Phase 9 plan; the decision text above is that record."
   - **Rationale** = the *Rationale* column. **Rejected options** = only what the plan states;
     otherwise "Not recorded in the source."
   - **Status** = `Implemented` for D1–D6 and D8 with the landing task named; `Accepted` for D7
     (standing behaviour, no code change) — and say so.
   - **Verification** = test names only where the plan or `docs/backlog/later_todo_list.md`
     already names them for that landing task (e.g. item 58 for D1/D8, item 1 for D4, item 69 for
     D3); otherwise "To be filled from the WP-02 test map." Do not invent test names.
   - **History** = "Imported from the Phase 9 implementation plan on <date>; that plan remains the
     primary record of the discussion." Do not paraphrase the plan's rationale — copy it.
5. Edit `docs/README.md`: in "Active Planning And Backlog" add two rows at the top —
   `plan/README.md` (active phase plan) and `decisions/README.md` (owner decision log). Change
   nothing else.
6. Edit `AGENT.md`:
   - In "Read First", insert after item 3a a new item "3b. `docs/decisions/` for the owner's
     standing design rulings (`DR-xxxx`); read the index before proposing a behaviour change.
     `docs/plan/` holds the active phase plan."
   - In "Source-Of-Truth Hierarchy" item 3, add a bullet "owner design rulings: `docs/decisions/`"
     after the `uml/` bullet.
   - In "Highest Priority Next Work", add one bullet at the top: "Phase 10 (runtime core redesign):
     start from `docs/plan/phase_10/` — the charter is published there at Checkpoint 0; until then
     the working draft is `temp_docs/02_phase10_charter_and_work_breakdown.md`. Phase 9 is closed
     with carried items; see `docs/backlog/technical_debt_and_next_steps.md`."
   Change nothing else in `AGENT.md`.

## 5. Acceptance criteria

- [ ] `docs/plan/README.md`, `docs/plan/phase_10/README.md`, `docs/decisions/README.md` and eight
      DR files exist, in English, and every DR section is filled or reads "Not recorded in the
      source" / "To be filled from the WP-02 test map" — never left blank.
- [ ] Each DR's Decision and Rationale text is the plan's text unchanged (reviewer compares).
- [ ] `docs/README.md` and `AGENT.md` reference both new folders; a diff by eye shows only the
      insertions described in steps 5–6.
- [ ] Nothing under `docs/history/` is moved, renamed or edited.

## 6. Verification

- No build. Reviewer (PM) opens every link added; checks the DR index against the eight files;
  compares three DRs word-for-word against the plan.

## 7. Touch-list / Do-not

**May create/edit:** `docs/plan/**` (new), `docs/decisions/**` (new), `docs/README.md`, `AGENT.md`.
**Do not:** touch `docs/history/**` (WP-00 is editing the Phase 9 plan right now), the backlog
files (WP-00, WP-03), `docs/domains/**` (WP-02), any source or test; do not rewrite the DR
template's wording; do not translate the owner's Vietnamese quotes.

## 8. Stop rules

Stop and ask if: a decision's text in the plan is internally inconsistent (e.g. D3's "item 8"
numbering correction — record it in the DR's History, do not resolve it); a landing task cannot be
identified; or the plan's D-list has more or fewer than eight decisions.

## 9. Report

Write `temp_docs/reports/WP-01_report.md`: changed and created files (every path); the DR index as
landed; any place you had to choose wording not present in the source; open questions.
