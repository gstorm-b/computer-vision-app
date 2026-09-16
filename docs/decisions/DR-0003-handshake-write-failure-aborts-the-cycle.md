# DR-0003: A failed handshake write is retried, then aborts the cycle with fault 301

**Status:** Implemented — landed in Phase 9 Tasks E1–E4 (E4 is the policy; E1–E3 met the hard precondition). The E4 hardware observation is still pending; see Verification.
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D3
**Cluster:** cycle-fault
**Supersedes / replaces text in:** Not recorded in the source.

## Question
What does the runtime do when a PLC write of a handshake signal fails?

## Decision
*Item 8, contract write failure:* **bounded retry then `abortCycle`** with a new fault code, for the **handshake signals only** (`bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode`, `nDetectedNumber`). Log-only for the rest. **Hard precondition:** on MC, `writeDigitalIoByName`/`writeWordIoByName` return true for **queued**, not written (`mc_protocol_device.cpp:240`, `:263`). A real completion signal must exist there first or this policy is unimplementable.

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
A retry policy over a primitive that cannot report failure would log "retrying" and "giving up" about writes that were never observed — worse than today's honest silence.

## Rejected options and why
- A retry policy built before a real write-completion signal exists — the rationale above; hence the hard precondition, met by E1 and E2 before E4 was built.
- Retrying every signal, not only the five handshake signals — the plan's Task E4: "Retrying all fourteen would turn a degraded link into a write storm."

## Contract changes
`docs/domains/task_localization/plc_signal_contract.md` → "Handshake Writes Are Acknowledged" — the section backlog item 69 names as the contract for E4. Rows in the runtime state contract tables: Not recorded in the source.

## Verification
- Ring 1 (table tests):
  - E1, `mc_frame_test`: `test_a_write_abandoned_by_disconnect_resolves_once_as_failed`, `test_a_tracked_write_that_is_acked_resolves_once_as_ok`, `test_a_tracked_write_the_plc_refuses_resolves_once_as_failed_with_the_end_code`, `test_a_lost_link_fails_every_queued_write_not_just_the_one_in_flight`, `test_polling_and_the_comm_active_heartbeat_produce_no_completions`, `test_resolutions_match_submissions_exactly` (named by the plan's E1 record and Checkpoint E-1).
  - E2, Modbus: `test_a_deferred_client_write_resolves_with_the_outcome_of_the_replay`, `test_a_server_tracked_write_resolves_immediately_with_the_real_outcome` (named by Checkpoint E-1).
  - E4, `architecture_contract_test`: `test_an_advisory_write_failure_does_not_retry_and_does_not_abort`, `test_localization_fault_code_values_are_stable`, `test_every_localization_fault_code_has_a_name` (named by the plan's E4 landing record). The remaining E4 cases (retry-then-succeed, retry-then-abort, dead-PLC-no-recursion) are described but not named in the source: to be filled from the WP-02 test map.
- Ring 2 (owner-run script): no script id in the source. Owner-confirmed on hardware 2026-09-09 (backlog item 69): E5 — no startup fault on the MC binding; E1 — a cable pulled mid-write on the C24 produces a failure completion. **Pending:** E4 — a real PLC refusing a write on a healthy link. Backlog item 69 records two untried recipes that need no PLC cooperation (a Modbus handshake signal mapped to a discrete-input tag; an MC handshake signal mapped to a device address the PLC does not have).
- Ring 3 (log replay): backlog item 69 records that the 2026-09-14 Modbus run logged `PLC write failed: Modbus client is not connected` during a cable pull, and that this is **not** the E4 observation — a disconnected role is `300 PlcLost`'s path by design and deliberately untracked. No replay of a 301 abort is recorded in the source.

## Implementation
- Symbols touched (per the plan's E1–E4 landing records and backlog item 69): `PlcWriteFailed = 301` in `localization_fault_code.h` with its `localizationFaultCodeName()` arm; in `LocalizationRuntimeController`: `isHandshakeSignal()`, `kPlcWriteRetryBudget = 3`, `kPlcWriteRetryDelayMs = 40` (beside `kFaultAutoRecoverMs`), `trackHandshakeWrite()` (a disconnected role is not counted), `m_plcWriteEscalating` (the abort's own publishes are not tracked), `clearPendingWrites()` at the top of `abortCycle()` and in `clearRoleContext()`; `IPlcIoWriter` tracked writes carry an id and `PlcRunner::writeFinished(id, ok, message)` reports every outcome (E3); `McProtocolDevice` resolves every write exactly once on every abandon path, with a `createMsgInterface()` seam (E1); the Modbus client and server and `VirtualPlcDevice` resolve their writes (E2); `VirtualPlcDevice::failWritesForTag` as the hardware-free failure injection.
- Backlog items closed: none. Backlog item 69 was **filed** by Task Z3 to record what shipped; it stays open only for the E4 hardware observation.
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here.

**Numbering correction, recorded here as the plan states it and not resolved in this record.** The plan says: "D3 calls the write-failure work 'item 8'. In `later_todo_list.md`, **item 8 is `RobotRunner` — no runtime wiring**, unrelated; there is no backlog entry for PLC write acknowledgement at all." and "**Z3 files the write-failure item as a new entry** rather than closing an unrelated one." Task Z3 filed that entry as backlog item 69. The Decision text above keeps "Item 8" as written.

What this replaces: before E4 a failed handshake write was log-only, and on MC the write primitives reported "queued", not "written", so a lost `bMatchingFinished` or `bTaskFault` was never observed by the runtime — "today's honest silence" in the rationale.
