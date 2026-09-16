# DR-0002: The robot pick check is a task setting, read family-independently

**Status:** Implemented — landed in Phase 9 Task F1 (parts a and b)
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D2
**Cluster:** output
**Supersedes / replaces text in:** Not recorded in the source.

## Question
Where does the localization runtime read the robot pick-check configuration from, and what happens when the check is enabled but no usable checker exists?

## Decision
*Item 57, robot pick-check config:* move the settings to `TaskLocalizeConfig` and read them **family-independently**, ignoring `IResultOutputDevice::robotKinematicCheckConfig()` for this purpose. A **null checker with `enabled=true` is a hard setup error**.

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
The Modbus devices hold `m_kinematicCheck` with a setter called only from tests, no `toJson`/`fromJson` and no widget — so the check is structurally inert on the dual-role binding. The comment at `device_capabilities.h:152-159` currently blesses the silent disable and must be revised.

## Rejected options and why
Not recorded in the source.

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): `test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role` (named in full by backlog item 57). The plan's F1 landing record refers to three further cases by suffix only, as recorded there: `..._vision_output_family_cell_still_starts...`, `..._without_calibration_names_the_reason`, `..._disabled_pick_check_leaves_setup_valid`. Full names of those three: to be filled from the WP-02 test map.
- Ring 2 (owner-run script): no script id in the source. Owner run 2026-09-11 (MC + `VisionOut_01`): unreachable poses were skipped with the check commissioned from the task settings. Owner run 2026-09-14 on the binding the item names — a Modbus TCP client carrying both roles — confirmed unreachable poses are skipped; backlog item 57 was closed on that observation.
- Ring 3 (log replay): backlog item 57 cites `app_log_2026-09-14.txt` — the runtime requesting `role= primary_plc deviceId= 05` and `role= vision_output deviceId= 05` on the same device (`Modbus_Client_1`), followed by 85 cycles against it.

## Implementation
- Symbols touched (per the plan's F1 landing record): `TaskLocalizeConfig::robotCheckConfig()` / `setRobotCheckConfig()` backed by `m_robotCheckConfig`, persisted under `"robotCheckConfig"`, `kSchemaVersion` 3 → 4 (deliberately not a `P_PROPERTY_*`; `totalNames` unchanged at 124); `buildRuntimeContext()` reads the task config instead of `IResultOutputDevice::robotKinematicCheckConfig()`; `RobotKinematicPickingChecker::isReady()` exposes `m_robotValid`; `rebuildPickingChecker()` returns a reason string and `setup()` turns a non-empty reason into a refusal; the checker is still installed when the preset does not resolve (fail-closed, stated as a deviation); `frame_robot_check` with a **Set…** button opening `RobotKinematicCheckWidget` in a `QDialog` on the localization settings page. Scope fence kept: `IResultOutputDevice::robotKinematicCheckConfig()` stays with its live implementors, and the device-side advisory check in `VisionTcpipDeviceBase::runKinematicCheck()` keeps reading its own device config.
- Backlog items closed: 57 (rewritten as a missing-feature item, then closed 2026-09-14).
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here.

What this replaces: `buildRuntimeContext()` read the pick-check settings from the bound output device (`IResultOutputDevice::robotKinematicCheckConfig()`). Backlog item 57 records the consequence: "a device with nothing commissioned answers 'disabled', and that is indistinguishable from 'this cell does not want the check'", so on a dual-role Modbus binding the check silently defaulted off and the setting lived only on the vision-output device panels, where such a cell had no way to reach it.
