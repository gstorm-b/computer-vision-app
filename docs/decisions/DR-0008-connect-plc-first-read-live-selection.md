# DR-0008: `beginRuntime()` connects the PLC and output roles first; setup reads the live selection from the PLC

**Status:** Implemented — landed in Phase 9 Task C6 (parts a and b)
**Date accepted:** 2026-09-08
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Two decisions taken after the draft, 2026-09-08", D8
**Cluster:** startup-arming
**Supersedes / replaces text in:** Not recorded in the source.

## Question
Where does the runtime take the active camera and pattern-group indices from at startup, and in what order are the roles connected?

## Decision
`beginRuntime()` connects the PLC and output roles first, and setup reads the live selection from the PLC.

## Owner's ruling (verbatim)
> "Có — đây chính là gốc của bug index 0."

— Owner, 2026-09-08, on whether to do the work their notes had left commented out.

## Rationale
This is the **root fix for item 58**, and it changes the shape of Phase C. Today `setup()` takes both indices from `buildRuntimeContext()`, which fills them from `cameraDeviceIds.firstKey()` / `patternGroups.firstKey()` (`task_localization.cpp:724-737`) — the project's bindings, never the PLC. The two index signals are **inputs owned by the PLC role**, so resolving them before that role is connected is resolving them from the wrong source by construction.

It lands as **C6**, and it is the reason **C2 is worth building**: with the live read in place, C2's setup-side validators stop being defence-in-depth against a context the app cannot produce and start gating real master-written values. C6 also builds the `PlcValueMap` accessor that **G2 needs**, so the two owner-gated items share one piece of infrastructure.

## Rejected options and why
- Keeping the startup resolution from the project bindings (`buildRuntimeContext()` → `firstKey()`) — the rationale above: "resolving them before that role is connected is resolving them from the wrong source by construction."
- Resolving the selection *after* `setup()` to avoid the bounded wait — the plan's Task C6: "the task would reach Ready on the project default first and only then fault, and 'does not reach Ready' is the acceptance criterion this task exists for."

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`, `test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it`, `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal`, `test_build_runtime_context_leaves_the_selection_for_setup_to_resolve`, `test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped` (named by backlog item 58); `test_a_plc_with_no_snapshot_is_not_read_as_holding_zero` (named by the plan's Task C6 verification record).
- Ring 2 (owner-run script): no script id in the source. OWNER-RUN at Checkpoint C — the exact sequence from the 2026-09-07 report — confirmed by the owner on the cell 2026-09-09 (backlog item 58): a runtime started with the master holding camera 0 and pattern 0 faults instead of going Ready, and correcting both registers returns it to Ready with no restart and no operator action.
- Ring 3 (log replay): the plan's Task C6 records the field session of 2026-09-08 (`app_log_2026-09-08.txt`: camera reconnected, pattern went valid, `bErrorReset` pulsed, no `Faulted -> Ready` ever appeared) that exposed the first cut's terminal fault; backlog item 58 records the follow-on MC defect (item 59.5, the first `pollingUpdate` snapshot carrying only the zero-fill), fixed by Task E5 and owner-confirmed 2026-09-09. No replay of the final behaviour is recorded in the source.

## Implementation
- Symbols touched (per the plan's Task C6 landing record and backlog item 58): `PlcValueMap::valueForTag()` — the name→value accessor — on `McDeviceMap`, `ModbusRegisterMap` and the new `VirtualPlcValueMap` (`VirtualPlcDevice` had no value map at all; it now publishes a snapshot on `deviceConnect()` and on every injection); `PlcRunner::pollingUpdate` gains its first real consumer; the snapshot reaches `setup()` through `RuntimeContext`, so `setup()` stays a pure function of its context; `TaskLocalization::beginRuntime()` connects the primary PLC and vision-output roles first and waits on a bounded nested `QEventLoop`, `kPlcSnapshotWaitMs = 2000`, whose expiry falls back to the project default at USER level; `LocalizationRuntimeController::setup()` resolves the indices via `commandedIndexFromPlc()` through Task C2's validators, falls back to the project's `firstKey()` only when the signal is unmapped, treats "no snapshot" as unmapped and never as 0, and reports a refused startup index recoverably (per-signal latch, fault signals, `Faulted`, `runtimeFault`) without entering `SetupResult::errors`.
- Backlog items closed: 58 part A (together with Tasks C2 and C3). Backlog item 58 part B stays open: a mapped-but-never-written register as its own fault, and the MC / Modbus-client first-poll adoption for every non-index signal.
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here.

What this replaces: "`setup()` takes both indices from `buildRuntimeContext()`, which fills them from `cameraDeviceIds.firstKey()` / `patternGroups.firstKey()` — the project's bindings, never the PLC." `setup()` fired `requestRoleConnectNow()` at all three roles together and then evaluated readiness, so a task reached Ready with the master's registers at 0 and ran a cycle on whichever camera sorted first (the owner's report of 2026-09-07: *"khi runtime khoi dong mac du camera number va pattern number deu la 0 nhung task van ready."*, quoted in backlog item 58).
