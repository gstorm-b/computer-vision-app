# Task Localization Developer Documentation

This folder documents the localization task API and maintenance contract for
developers. It is a technical reference for implementing, testing, and
extending the localization runtime, not an operator manual.

## Reading Order

1. [task_localization_api.md](task_localization_api.md)
   - Public task surface.
   - Runtime lifecycle.
   - Device binding and configuration ownership.
2. [runtime_controller_api.md](runtime_controller_api.md)
   - Runtime controller API.
   - `RuntimeContext`.
   - Trigger cycle, async matching, fault, and recovery behavior.
3. [plc_signal_contract.md](plc_signal_contract.md)
   - PLC input/output signal names.
   - Handshake behavior.
   - Stable fault-code values.
4. [pick_geometry_and_output_contract.md](pick_geometry_and_output_contract.md)
   - Pattern pick geometry, gripper presets, and the pattern JSON schema versions.
   - The 6-axis TOOL-frame pick offset and how a match becomes a robot pose.
   - The 6-axis wire format and the per-axis absolute pick-path semantics.
5. [maintenance_and_extension.md](maintenance_and_extension.md)
   - How to safely add signals, fault codes, device roles, matching behavior,
     tests, and UML changes.

## Core Source Files

| Area | File |
| --- | --- |
| Task facade and lifecycle | `src/model/task_localization.h`, `src/model/task_localization.cpp` |
| Runtime state machine | `src/model/localization_runtime_controller.h`, `src/model/localization_runtime_controller.cpp` |
| Config and PLC tag schema | `src/model/task_localization_config.h` |
| PLC tag mapper | `src/model/localization_signal_mapper.h`, `src/model/localization_signal_mapper.cpp` |
| Fault codes | `src/model/localization_fault_code.h` |
| Recovery policy | `src/model/localization_recovery_policy.h` |
| Matching facade | `src/model/localization_pipeline.h`, `src/model/localization_pipeline.cpp` |
| Pattern pick geometry | `src/matching/match_pattern_config.h`, `src/matching/gripper_boxes.h` |
| Pattern library persistence | `src/matching/pattern_group_manager.cpp` (schema v0/v1/v2) |
| Gripper presets | `src/model/gripper_preset_store.h`, `src/model/gripper_preset_store.cpp` |
| Vision output wire format | `src/device/output_device/vision_output_request.h` |
| Robot pick check settings (incl. pick-path waypoints) | `src/device/robot_kinematic_check_config.h` (types); `TaskLocalizeConfig::robotCheckConfig()` (the task's setting) |
| Advisory pickability check | `src/model/robot_kinematic_picking_checker.h`, `src/matching/robot_picking_checker.h` |
| Pattern authoring wizards | `src/ui/forms/pattern/add_pattern_wizard.*`, `src/ui/forms/pattern/edit_pattern_wizard.*` |
| Runtime runners | `src/runtime/camera_runner.h`, `src/runtime/plc_runner.h`, `src/runtime/vision_output_runner.h` |
| Dashboard | `src/ui/forms/task/localization_dashboard_widget.*` |
| Settings UI | `src/ui/forms/task/localization_setting_widget.*` |
| Architecture diagrams | `uml/03_runtime_threading.puml`, `uml/04_localization_task.puml`, `uml/05_matching_calibration.puml` |
| Contract tests | `tests/architecture_contract_test/main.cpp` |

## Architecture Summary

`TaskLocalization` is the task facade. It owns persistent configuration,
pattern groups, the matching worker thread, and lifecycle entry points. Runtime
control is delegated to `LocalizationRuntimeController`.

At runtime:

- Device I/O stays on per-device runner threads.
- `LocalizationRuntimeController` is moved to `TaskRunner::runtimeThread()`.
- `TaskLocalization` builds a `RuntimeContext` snapshot and queues controller
  operations.
- The controller coordinates PLC input, camera single-shot requests, vision
  output requests, fault publishing, and recovery.
- Matching runs asynchronously on the task matching worker thread. The
  controller emits `runtimeMatchingRequested(cycleId, group, workspace, image, pickingChecker)`, then
  receives `onRuntimeMatchingFinished(cycleId, result)`.
- Stale matching results are ignored by cycle id.

## Important Contracts

- `nActivePatternGroup` is the active pattern group number. The old
  `nActivePattern` signal is intentionally not supported.
- Runtime output coordinates are world coordinates converted from image
  coordinates through the active camera calibrator, then composed with the
  pattern's 6-axis TOOL-frame pick offset. Each position is emitted as six axes
  (`x, y, z, rx, ry, rz`); it was four before Phase 5, and a robot program reading
  four **misaligns** rather than ignoring the extras. See
  [pick_geometry_and_output_contract.md](pick_geometry_and_output_contract.md).
- Pattern JSON is versioned (`kSchemaVersion` = 2) and its back-compatible read of
  older gripper-geometry shapes is a deliberate, agreed exception to the
  no-compat-shims rule. Do not remove it.
- PLC fault reporting uses `bTaskFault` and `nFaultCode`.
- The two active-index signals are **command registers the PLC owns**. The task reads them — from
  the PLC's first snapshot at startup, and on every change — and reports what it adopted on
  `nActiveCameraStatus` / `nActivePatternGroupStatus`, never on the command register. See
  [plc_signal_contract.md](plc_signal_contract.md) → "Active Index Signals".
- The signal map is validated before any device connects: an unmapped required signal, a shared tag,
  or a tag the PLC does not provide refuses the start with a message naming the signal.
- The five handshake outputs are write-acknowledged; three failed attempts abort the cycle with fault
  `301 PlcWriteFailed`.
- The robot pick check is a **task** setting (`TaskLocalizeConfig::robotCheckConfig()`), never read
  from the output device; an enabled check that cannot run refuses the start.
- `CycleResult::timings` breaks every cycle down by stage from one monotonic clock.
- `bExecuteTrigger` is rising-edge triggered. A held trigger must not enqueue a
  second cycle.
- `bMatchingFinished` means the accepted trigger was handled. It does not imply
  success. Check `bTaskFault` and `nFaultCode`.
- Runtime pattern and calibration edits are not supported while runtime is
  active. Stop runtime, edit, then start runtime again to refresh snapshots.
- Phase teardown disconnects devices. `TaskRunner::enterIdle()` closes each
  device connection on its own worker thread (through
  `IDeviceRunner::disconnectAndWait()`) before detaching/stopping, so the
  stop-edit-start cycle reconnects from a clean state. Device `deviceConnect()`
  is idempotent; the runtime controller may request connect on an already-open
  device.

## Related Documents

- `docs/domains/task_localization/task_localization.md` is the behavior contract.
- `docs/domains/task_localization/task_localization_implementation_plan.md` records the implementation
  plan and completed architecture debt pass.
- `docs/domains/task_localization/task_localize_setting_widget.md` documents the settings widget.
- `docs/rules/design_rules.md` and `docs/rules/ui_design_rules.md` are mandatory project
  rules.

