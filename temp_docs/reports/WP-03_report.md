# WP-03 report — Backlog triage against Phase 10

**Date:** 2026-09-16 · **Handoff:** `temp_docs/wp/WP-03_backlog_triage.md` · **Kind:** docs only

## 1. Changed files

- `docs/backlog/later_todo_list.md` — one new section `## Triage 2026-09-16 (Phase 10 start)` inserted
  between the intro paragraph and the existing `---` that precedes `## 1.`. 81 lines added (heading,
  four-sentence preamble, a 65-row table, a counts line). Nothing else in the file was touched.
- `temp_docs/reports/WP-03_report.md` — this report.

No other file was edited. `technical_debt_and_next_steps.md`, the charter and the PQ list were read
only. One read-only code lookup outside the "Read first" list: `src/model/localization_runtime_controller.cpp`,
to make the note on item 60 factual (see §5).

## 2. Heading count

**65** headings of the form `## <n>.`, numbered 1–70 with 12, 16, 17, 18 and 19 absent. The order is
not numeric (53 precedes 52; 50 follows 52). The table has 65 rows, in file order. The handoff and
charter say "70 items"; that is the numbering range, not the heading count.

## 3. Evidence

- `Select-String -Path docs\backlog\later_todo_list.md -Pattern '^## \d+\.' | Measure-Object` → **65**,
  before and after the edit (the new `## Triage` heading does not match the pattern).
- Line count 3376 → 3457 (+81). Every original heading shifted by exactly 81 (`## 1.` 9 → 90, `## 2.`
  62 → 143, `## 70.` 3323 → 3404), so everything below the new section is byte-for-byte the old content.
- Every ABSORB target exists: WP-21 (charter §3, Stage 2); PQ-1, PQ-2, PQ-3, PQ-7, PQ-8, PQ-10, PQ-14,
  PQ-15 (PQ list). WP ids named only in notes (WP-32, WP-33, WP-34, WP-50) also exist in §3.
- The one MERGE row (10 → 9) targets an item classified KEEP, not CLOSED-ALREADY.

## 4. Counts per disposition

| Disposition | Count | Items |
|---|---|---|
| CLOSED-ALREADY | 21 | 1, 2, 5, 13, 21, 22, 24, 25, 26, 28, 32, 33, 35, 36, 43, 53, 55, 57, 59, 61, 65 |
| KEEP | 35 | 3, 4, 6, 7, 8, 9, 11, 14, 15, 20, 23, 27, 29, 30, 31, 34, 37, 38, 39, 40, 41, 42, 44, 45, 46, 47, 48, 49, 52, 50, 56, 60, 62, 66, 69 |
| ABSORB | 8 | 51 → WP-21; 54 → PQ-1; 58 → PQ-2, PQ-3; 63 → PQ-15; 64 → PQ-10; 67 → PQ-14; 68 → PQ-8; 70 → PQ-7 |
| MERGE | 1 | 10 → item 9 |
| NEEDS-OWNER | 0 | — |

Total 65. Stop rule (more than ten NEEDS-OWNER) not triggered; file structure matched handoff §3.

The handoff's expected mapping was followed exactly: 54 → PQ-1, 58B → PQ-2/PQ-3, 64 → PQ-10,
68 → PQ-8, 70 → PQ-7, 63 → PQ-15, 67 → PQ-14; 53 (fixed by reasoning) is CLOSED-ALREADY per its own
status; 66 (no PQ) is KEEP; 51 (device half) and 56 were the two the charter left to triage — 51 is
ABSORB → WP-21, 56 is KEEP (owner-run hardware verification).

## 5. NEEDS-OWNER rows

None.

## 6. Items where I was unsure between two dispositions

| # | Chosen | Alternative | Why chosen |
|---|---|---|---|
| 2 | CLOSED-ALREADY | KEEP | Status chain is DONE → PARTIALLY RESOLVED → "Done (2026-06-24)" closing the remaining surfaces; there is no literal CLOSED line, but nothing is left open in the body. |
| 10 | MERGE → 9 | CLOSED-ALREADY | Original scope completed in Phase 1; the residual (serial widget + factory branch) is inherently part of delivering item 9 (`AGENT.md`: a new device subtype must update UI dispatch). |
| 23 | KEEP | CLOSED-ALREADY | Own status says PARTIALLY RESOLVED, but both listed blockers are reported closed in item 2 and in `technical_debt_and_next_steps.md`. The status line is stale; kept because the rule for CLOSED-ALREADY is the item's own status. |
| 25 | CLOSED-ALREADY | KEEP | Top status RESOLVED, lamp half CLOSED 2026-09-14; the "remaining verification" paragraph is the Phase 4 on-hold operator UI pass tracked in `technical_debt_and_next_steps.md`. |
| 44 | KEEP | NEEDS-OWNER | The item itself needs an owner decision (which tool root is canonical), but the triage disposition does not: not a Phase 10 concern, not blocking (charter §1.1). |
| 51 | ABSORB → WP-21 | KEEP (or a new PQ) | The remaining decision ("are device errors worth a task-log entry, else delete the channel") determines whether the to-be event set has a device-error event; WP-21 defines that set. No PQ exists. |
| 60 | KEEP | CLOSED-ALREADY | Own status says "open ... fixed by Phase 9 Task C3", written before C3 landed. Code check: the controller now publishes `nActiveCameraStatus` / `nActivePatternGroupStatus` and the old echo site carries a comment explaining why echoing was removed. Kept because its own status says open; it needs a closing note, which this insert-only WP may not write. |
| 66 | KEEP | NEEDS-OWNER | The item lists options with none chosen, but it is a config/UI ownership question, not a runtime-core state; the handoff already notes there is no PQ. |

## 7. Findings out of scope (not changed)

- Item 60's status line is stale (fix landed 2026-09-09 with C3). Item 23's status line is stale for
  the same reason (blockers closed by the 2026-06-24 sweep). Both need a closing note by whoever next
  owns the file; the triage table records the staleness only.
- Item 61 (CLOSED) contains an unfiled residual defect: on the refusal paths `setActiveCameraNumber()`
  returns before publishing the index, so a refused selection updates no camera visual. The to-be
  design (Selection + `outputSnapshot()`, WP-33/WP-34) would cover it by construction; it has no item
  of its own.
- Item 60 also mentions, unfiled, that a device error on a dual-role binding is reported twice (once
  per role). Cosmetic.
- Handoff §3 and charter §1.2/§3 say "70 items"; the actual heading count is 65. Wording only.
- The preamble names `temp_docs/` as the current home of the charter and PQ list and
  `docs/plan/phase_10/` as the home after Checkpoint 0 (charter §5/§6.5), so the committed text stays
  correct across the move.

## 8. Deviations from the handoff

None in substance. Insertion point: after the intro paragraph's trailing blank line and before the
existing `---`, so the file's own separator closes the new section and the first untouched byte is
that `---`. This satisfies "immediately after the intro paragraph and before `## 1.`".

## 9. Open questions

1. Item 51 (device half): fold into WP-21 as a to-be row, or give it a PQ card in WP-20 so the owner
   rules on it explicitly? The table says WP-21; the PM may prefer a PQ.
2. Item 66: leave in the backlog (as triaged), or add a Cluster E PQ so it is decided before the
   Phase 10 manual?
3. Who writes the closing notes for the stale-status items 60 and 23 — a later docs WP, or WP-43?
4. Should item 61's residual refusal-path lamp gap be filed as its own backlog item, or is a row in
   the to-be table (WP-21) enough?

---

## PM review — 2026-09-16 — ACCEPTED

Checked: heading count re-run by the PM (65); the inserted section read in full; the first
untouched line is the original `---`; every ABSORB target exists in the charter or PQ list; the
"unsure" calls all follow the stated rule (an item's own status decides CLOSED-ALREADY).

Rulings on the open questions:

1. **Item 51** — stays ABSORB → WP-21. The to-be event table gets a `DeviceError` event whose
   as-is note is "no emitter today"; if the table shows it needs a ruling, a PQ is raised then.
2. **Item 66** — a Cluster E policy question is added by the PM (PQ-16) so it is decided before
   the Phase 10 manual. Triage row stays KEEP; the PQ list is the tracking place.
3. **Items 60 and 23** — the PM writes the closing notes at Checkpoint 0 housekeeping (one edit
   to the backlog file after every Stage 0 WP has landed).
4. **Item 61 residual** — filed as its own backlog item at Checkpoint 0 housekeeping. "Covered by
   the to-be design" is not a fix until Stage 4 lands; the shipped code still has the gap.

Wording: the charter's "70 items" is corrected to "65 items, numbered up to 70".
