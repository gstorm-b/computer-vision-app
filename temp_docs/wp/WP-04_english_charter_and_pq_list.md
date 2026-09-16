# WP-04 — English charter and policy-question list for `docs/plan/phase_10/`

**Stage:** 0 (Checkpoint 0 preparation) · **Size:** S · **Kind:** docs only · **Issued:** 2026-09-16 · **Runs in parallel with:** WP-02 (disjoint files)

## 1. Goal

Produce the English, committable versions of the Phase 10 charter and the open policy-question
list, so that at Checkpoint 0 the PM can publish them to `docs/plan/phase_10/` unchanged. The
Vietnamese originals in `temp_docs/` remain the owner-facing working copies.

## 2. Read first

1. `AGENT.md` (English-only rule for committed text; no line-number citations)
2. `temp_docs/02_phase10_charter_and_work_breakdown.md` — the charter (Vietnamese, with the owner's
   inline answers in §6 and the Stage 0 progress notes in §3)
3. `temp_docs/01_open_policy_questions.md` — the PQ list (Vietnamese; PQ-1..PQ-16)
4. `docs/plan/README.md` and `docs/decisions/README.md` (created by WP-01: folder conventions, DR
   numbering, template)
5. `temp_docs/00_assessment_and_runtime_design_process.md` only for terms the charter refers to
   (the "contract-first loop", the three verification rings)

## 3. Facts

- The owner approved all five §6 points on 2026-09-16; §6 now carries their answers inline. Point 4
  ("theo đề xuất của bạn") means the full Stage 0–5 programme with a stop option at every checkpoint.
- Stage 0 progress as of issue: WP-00, WP-01, WP-03 accepted; WP-02 running; the PM's housekeeping
  bullets in §3 are partly ticked. Reproduce that state; do not advance it.
- Decision records exist: `DR-0001`..`DR-0008` in `docs/decisions/`. Where the charter or PQ list
  refers to "D1–D8", the English version cites the DR numbers.
- PQ-16 (item 66, two editors for the robot pick check) was added after the list was first written.
- The backlog now has 65 items numbered up to 72 (71 and 72 filed 2026-09-16); the charter's
  "65 item (đánh số tới 70)" wording should become "65 items, numbered up to 72".

## 4. Steps

1. Write `temp_docs/en/charter.md`: a faithful English rendering of the charter, same section
   structure (§1 assessment, §2 design hypothesis, §3 work breakdown with all WP tables and the
   Stage 0 progress/housekeeping notes, §4 risks, §5 PM↔agent protocol, §6 owner decisions —
   rendered as a "Decisions taken" list with the owner's answers, not as open questions). Keep
   tables as tables. Keep WP ids, PQ ids, test names, symbols and file paths exactly. Quote the
   owner's Vietnamese answers verbatim inside the §6 list, each followed by a one-line English gloss.
   Title: `# Phase 10 — Runtime core redesign: charter and work breakdown`. Add a header block:
   Date, Status ("approved by the owner 2026-09-16; Stage 0 issued"), Roles, Environment constraint
   (no git on the working machine), and "Working copy: the Vietnamese original in `temp_docs/`
   until Checkpoint 0".
2. Write `temp_docs/en/open_policy_questions.md`: English rendering of the PQ list, same clusters,
   same PQ numbering (1..16), same four columns, same "suggested order" section. Keep the
   distinction between what is verified today and why a ruling is needed.
3. Both files: English only in prose; symbols and test names unchanged; no line-number citations
   (if the Vietnamese text carries one, drop it and keep the symbol); relative links rewritten for
   their future home `docs/plan/phase_10/` (so `wp/…`, `reports/…`, `../../decisions/…`,
   `../../domains/…`, `../../backlog/…`).
4. Do not condense or "improve" the content. Where a sentence is genuinely untranslatable
   without a choice, keep the more literal reading and list the choice in the report.

## 5. Acceptance criteria

- [ ] `temp_docs/en/charter.md` and `temp_docs/en/open_policy_questions.md` exist; every section
      and every table row of the originals is present in the same order.
- [ ] Every WP id, PQ id, DR id, symbol, test name and path appears exactly as in the originals
      (the reviewer greps for a sample of ten).
- [ ] §6 reads as decisions taken, with the owner's Vietnamese answers quoted verbatim.
- [ ] No line-number citations; all relative links target the future `docs/plan/phase_10/` layout.
- [ ] Nothing outside `temp_docs/en/` is created or edited.

## 6. Verification

- Reviewer (PM) reads both files side by side with the originals; greps ten identifiers; opens
  every relative link against the intended target layout.

## 7. Touch-list / Do-not

**May create:** `temp_docs/en/charter.md`, `temp_docs/en/open_policy_questions.md`.
**Do not:** edit the Vietnamese originals, anything under `docs/`, or any source or test; do not
publish to `docs/plan/phase_10/` — that is the PM's Checkpoint 0 action after the owner confirms.

## 8. Stop rules

Stop and ask if a section of the charter contradicts the state of the repository you can verify
(for example a WP id in the charter that the tables do not define) — report it rather than
repairing the charter.

## 9. Report

`temp_docs/reports/WP-04_report.md`: created files; translation choices made; any inconsistency
found between the charter and the repository; open questions.
