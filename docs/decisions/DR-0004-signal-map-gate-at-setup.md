# DR-0004: Signal-map gate at setup: unmapped required signals and orphan tags are hard errors

**Status:** Implemented — landed in Phase 9 Task C4
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D4
**Cluster:** startup-arming
**Supersedes / replaces text in:** Not recorded in the source.

## Question
Which signal-map problems refuse the runtime at setup, and which only warn?

## Decision
*Item 1, signal map gate:* required subset `bExecuteTrigger`, `bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode` → **hard setup error** if unmapped. All others → **visible warning**. **Orphan tags** (non-empty but absent from the bound device's tag list) → **hard error in every case**.

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
An unmapped `bTaskFault` means the PLC never learns a cycle failed and keeps consuming results. An orphan tag is worse than a blank one: `reportRoleError()` deduplicates by message (`:1752-1755`), so a permanently failing write logs once and is silent forever.

## Rejected options and why
Not recorded in the source.

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): `test_each_required_signal_unmapped_fails_setup_naming_it`, `test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike`, `test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal`, `test_orphan_row_names_reports_orphans_without_clearing_them` (named by backlog item 1); `test_an_unmapped_optional_signal_is_valid_and_warns` (named by the plan's Task C4 verification record). The duplicate-tag and `m_lastErrorReset` cases are described but not named in the source: to be filled from the WP-02 test map.
- Ring 2 (owner-run script): no script id in the source. Owner-run, runtime half — done 2026-09-08 on the cell: a signal bound outside the device's span refused the runtime naming the tag. Owner-run, save dialog — done 2026-09-08: the dialog appears on a project with orphan rows and all three buttons behave (*Clear and save* / *Save as-is* / *Go back*). **Pending:** the save-time dialog read in both themes and in Japanese.
- Ring 3 (log replay): `app_log_2026-09-08.txt` — the gate refused a real orphan on the owner's cell: *"Signal "Camera selection" (nActiveCamera) is mapped to tag IR01000, which the device 05 does not provide as a register."* → Faulted (quoted in backlog item 1 and the plan's Task C4).

## Implementation
- Symbols touched (per the plan's Task C4 and backlog item 1): `LocalizationRuntimeController::validateSignalMap()`, called in `setup()` **after** `bindFixedRoleRunners()` (placed earlier, `primaryPlcRunner()` finds no tag provider and the gate silently passes — recorded as the ordering trap); `LocalizationRuntimeController::requiredSignalNames()` promoted to public so the editor and the runtime share one list; orphan check per kind through `IPlcTagProvider` (`availableDigitalIoNames()` / `availableWordIoNames()`, never the union); duplicate tags raised into `SetupResult::errors`; `m_lastErrorReset` reset by `setup()` alongside `m_lastExecuteTrigger`. Commissioning half: `SignalsMapWidget::checkEmpty()` **deleted, not wired** (it purged before it reported), replaced by `SignalsMapWidget::orphanRowNames()` and `SignalsMapWidget::clearRowTags()`; `LocalizationSettingWidget::confirmOrphanedSignalsBeforeSave()` → `LocalizationTaskWidget` → `MainWindow::saveToFile()`, which aborts the save if any task cancels; a required orphan asks a second time before being cleared.
- Backlog items closed: 1.
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here.

What this replaces: `setup()` applied no signal-map gate. An unmapped or orphan tag reached the runtime and failed at write time, where `reportRoleError()` deduplicated the failure into silence; `SignalsMapWidget::checkEmpty()` existed with no caller (backlog item 1, "wire it or delete it"). The plan's Task C4 records that 12 of the 14 audited `.vproj` files already mapped all five required signals, and that the two which did not had no device bindings either, so the gate "adds messages but breaks nothing that works"; the orphan half of that audit could not be closed offline because it depends on each device's configured ranges.
