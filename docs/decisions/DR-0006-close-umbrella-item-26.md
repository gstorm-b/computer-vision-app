# DR-0006: Close umbrella backlog item 26, folding its bullets into their real homes

**Status:** Implemented — landed in Phase 9 Task Z3 (backlog item 26 closed 2026-09-14)
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D6
**Cluster:** none of the template's runtime clusters — this is backlog hygiene, not runtime behaviour
**Supersedes / replaces text in:** Not recorded in the source.

## Question
What happens to umbrella backlog item 26 now that its remaining bullets are individual Phase 9 tasks?

## Decision
Close umbrella item 26, folding its bullets into item 25, the latency task, and item 8.

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
An umbrella that outlives its children is where work is forgotten twice.

## Rejected options and why
Not recorded in the source.

## Contract changes
Not applicable — the ruling changes backlog records, not runtime behaviour.

## Verification
- Ring 1 (table tests): none — the landing task is documentation-only. The plan's Z3 landing record: "Verification. Documentation only; nothing to build. Every item number cited above was opened." (Backlog item 26's closing record names `test_successful_cycle_stamps_every_stage_monotonically` and `test_faulted_cycle_carries_only_the_stages_it_reached` as the evidence for the folded latency bullet; they verify the fold target, Task F3, not this ruling.)
- Ring 2 (owner-run script): not applicable — documentation only.
- Ring 3 (log replay): not applicable — documentation only.

## Implementation
- Symbols touched: none. Files: `docs/backlog/later_todo_list.md` item 26, closed 2026-09-14 with each bullet's new home named — latency measurement → Task F3; remaining runtime edge-case tests → Tasks B1 / B2; the operator-UI pass → Tasks F2 and F3(b); the conditional PLC-write bullet met by Phase 8 plus E1–E4.
- Backlog items closed: 26.
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged.

**Numbering correction, recorded here as the plan states it and not resolved in this record.** The plan says: "D6's third fold-target is therefore not item 8 either: item 26's three remaining bullets are the operator-UI pass (→ **F2**), latency measurement (→ **F3**) and remaining runtime edge-case tests (→ **B1/B2**)." — because "item 8 is `RobotRunner` — no runtime wiring", unrelated. The Decision text above keeps "item 25, the latency task, and item 8" as written; Task Z3 folded the bullets into the tasks named in the correction.

What this replaces: backlog item 26, "Localization runtime production follow-ups after first implementation pass", an umbrella entry whose open bullets duplicated work that Phase 9 tasks were carrying individually.
