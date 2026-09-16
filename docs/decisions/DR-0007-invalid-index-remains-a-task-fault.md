# DR-0007: An invalid camera/pattern index remains a task fault

**Status:** Accepted — standing behaviour confirmed by the owner. No code change was made or required (the draft task that would have reversed it was deleted), so there is no landing task and this record does not advance to Implemented.
**Date accepted:** 2026-09-08
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Two decisions taken after the draft, 2026-09-08", D7
**Cluster:** selection
**Supersedes / replaces text in:** Not recorded in the source.

## Question
Does an invalid (out-of-range, unregistered or non-numeric) camera or pattern-group index remain a task fault, or is it reported only through `bCameraValid` / `bPatternValid`?

## Decision
An invalid camera/pattern index REMAINS a task fault.

The draft carried a contingent task (C1) to reverse this: stop publishing `bTaskFault`/`nFaultCode` for an index rejection on the grounds that `bCameraValid`/`bPatternValid` already report it. **That task is deleted.** Phase 8's F1/F4/F5 behaviour stands unchanged — an out-of-range, unregistered or non-numeric index publishes `bTaskFault=true` + `nFaultCode` (100/400), enters `CycleState::Faulted`, sets the per-signal rejection latch, and recovers only on a valid write to that same signal. Nothing in Phase 9 touches that contract, and no task may weaken it as a side effect.

## Owner's ruling (verbatim)
> "vẫn giữ nguyên hành vi khi index invalid thì xem là task fault."

— Owner, 2026-09-08.

## Rationale
This also settles the sub-question the draft's critique raised: `reportSignalTypeMismatch()` (`:914`) — a number signal mapped onto a coil — keeps faulting too. The runtime is now consistent across all three failure modes rather than split between two rules.

## Rejected options and why
- The draft's contingent Task C1 — "stop publishing `bTaskFault`/`nFaultCode` for an index rejection on the grounds that `bCameraValid`/`bPatternValid` already report it" — deleted 2026-09-08; the owner ruled the opposite. The plan left the task numbering with the gap "so that references to C2–C6 written before this revision still resolve."

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): the plan names the Phase 8 cases as this ruling's guard — `test_localization_runtime_setup_faults_on_invalid_pattern_group` and "the camera/latch cases" (the Phase 8 index-rejection and rejection-latch cases in `architecture_contract_test`, which the plan's "Known test breakage" section says must stay green untouched: "An edit to any of them is a defect in this phase's work"); `test_localization_zero_active_index_is_refused_by_the_range_check` (named by Task C2's verification as a setter-level case that stayed green). Cases added by later tasks that assert the fault per D7: `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready` and `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal` (Task C6, backlog item 58).
- Ring 2 (owner-run script): no script id in the source. Owner-run confirmed 2026-09-09 (Checkpoint C-1: "An invalid index still faults per **D7**, so C2's extraction changed the message and the fault code, not the contract."; backlog item 58: a runtime started with the master holding camera 0 and pattern 0 faults instead of going Ready, and correcting both registers returns it to Ready with no restart).
- Ring 3 (log replay): the plan's Task C6 and backlog item 58 record the field session of 2026-09-08 in which the first cut of C6 faulted a read 0 terminally — no `Faulted -> Ready` transition after both registers were corrected and `bErrorReset` pulsed — which is what established that a startup rejection must fault *recoverably*, exactly as a written one does. No replay of the standing behaviour itself is recorded.

## Implementation
- Symbols touched: none — no code change. The standing behaviour is the Phase 8 F1/F4/F5 path (`bTaskFault=true` + `nFaultCode` 100/400, `CycleState::Faulted`, the per-signal rejection latch), and `reportSignalTypeMismatch()` keeps faulting too. Phase 9 tasks that touched the neighbourhood were required to preserve it: Task C2's validator extraction ("behaviour-neutral at the setter level") and Task C6 ("C6 changes *where the startup value comes from*, never *what an invalid value means*").
- Backlog items closed: none by this ruling.
- Git tag: not applicable.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged from the plan's D7 paragraphs, including the plan's own source line reference, which is as of 2026-09-08 and is not maintained here.

What this replaces: nothing — the ruling confirms behaviour that Phase 8 established. The plan's "Known test breakage" section records that two rows listing `test_localization_runtime_setup_faults_on_invalid_pattern_group` and the camera/latch cases as sanctioned breakage were deleted on 2026-09-08 under this ruling; "under **D7** the Phase 8 fault behaviour stands, so **none of those tests changes**." The plan also notes, for the owner-gated held-trigger item (Phase G), that a held `bExecuteTrigger` is a handshake state, not an invalid selection, "so **D7 does not apply to it**".
