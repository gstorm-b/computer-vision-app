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
4. [maintenance_and_extension.md](maintenance_and_extension.md)
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
| Runtime runners | `src/runtime/camera_runner.h`, `src/runtime/plc_runner.h`, `src/runtime/vision_output_runner.h` |
| Dashboard | `src/ui/forms/task/localization_dashboard_widget.*` |
| Settings UI | `src/ui/forms/task/localization_setting_widget.*` |
| Architecture diagrams | `uml/03_runtime_threading.puml`, `uml/04_localization_task.puml` |
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
  controller emits `runtimeMatchingRequested(cycleId, group, image)`, then
  receives `onRuntimeMatchingFinished(cycleId, result)`.
- Stale matching results are ignored by cycle id.

## Important Contracts

- `nActivePatternGroup` is the active pattern group number. The old
  `nActivePattern` signal is intentionally not supported.
- Runtime output coordinates are world coordinates converted from image
  coordinates through the active camera calibrator.
- PLC fault reporting uses `bTaskFault` and `nFaultCode`.
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

