# DR-0001: Active selection is announced on separate optional status signals, never echoed onto the command registers

**Status:** Implemented — landed in Phase 9 Task C3
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D1
**Cluster:** selection
**Supersedes / replaces text in:** Not recorded in the source.

## Question
How does the runtime announce the camera and pattern-group selection it actually adopted, without writing onto the master's command registers?

## Decision
*Item 58, announcing the active selection:* add **separate status signals** (`nActiveCameraStatus` / `nActivePatternGroupStatus`), and make them **optional tags exactly like every other output signal**. If the operator leaves the tag empty in the signal-map UI, the runtime performs **no PLC write** — it only keeps the value internally and emits `signalChanged()` for the UI/dashboard. **Never** echo onto the master's command registers (`nActiveCamera` / `nActivePatternGroup`).

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
`publishNumberSignal()` (`:820-832`) already implements the rule — emit always, write only when a tag is mapped — so no special-casing is needed. A task that writes back onto the master's own command register can fight a master about to write its own value; the comment at `:299-303` records that.

## Rejected options and why
- Echoing the adopted selection onto the master's command registers (`nActiveCamera` / `nActivePatternGroup`) — the rationale above. The plan's Task C3 adds the field confirmation (backlog 60): on a Modbus **client** binding "a master may not write input registers, so every accepted selection change is rejected by the device and raised as a `runtimeError`", and on a server binding "the same echo succeeds silently into a register the master owns, which is a write race against the master's own command."

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): `test_setup_announces_the_active_selection_on_status_signals`, `test_status_signals_with_no_tag_emit_but_do_not_write` (named by backlog item 58); `test_the_runtime_never_writes_the_command_registers`, `test_a_refused_selection_is_not_announced` (named by the plan's Task C3 verification record).
- Ring 2 (owner-run script): no script id in the source. Owner-run confirmed 2026-09-09 at Checkpoint C-1 — the plan records: "The status registers report the live selection; the command registers are never written; the app log's startup summary names the camera, the group, the workspace and where each came from." The two OWNER-RUN boxes listed under Task C3 itself are unticked in the plan; the Checkpoint C-1 record is the confirmation.
- Ring 3 (log replay): Not recorded in the source.

## Implementation
- Symbols touched (per the plan's Task C3 and its landing record): `nActiveCameraStatus` / `nActivePatternGroupStatus` on `TaskLocalizeConfig` (display names "Active camera (status)" / "Active pattern group (status)", `kDisplayNameSources[]`, `toJson`/`fromJson`, `kSchemaVersion` 2 → 3); `kSignalFields[]` in `localization_signal_mapper.cpp`; publish calls in `LocalizationRuntimeController::setup()` (valid path) and `publishInitialReadyOutputs()`, and on every accepted change in both setters; the two command-register echoes deleted; `kSignalRows[]` in `localization_dashboard_widget.cpp` and `localization_setting_widget.cpp` (both status rows plus `bErrorReset`); `applySignalToDashboard()` matching both the command name and the status name for each index.
- Backlog items closed: 58 part A (together with Tasks C2 and C6).
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here.

What this replaces: the controller echoed the adopted selection onto the master's command registers through `publishNumberSignal("nActiveCamera", …)` and `publishNumberSignal("nActivePatternGroup", …)`. Task C3 deleted both echoes; backlog item 58 records that "the echo onto the master's **command** registers is deleted, not moved". The plan's Task C3 also records the migration for a commissioned master that reads those registers back: map the two status tags, move any master readback logic onto them, then upgrade — keeping the echo "for one release" would be a backwards-compatibility shim, which `AGENT.md` forbids.
