# Maintenance And Extension Guide

This guide describes how to safely change or extend localization runtime code.

## Required Project Rules

- Source comments, identifiers, logs, and committed docs are English.
- Keep UI structure in `.ui`, behavior in `.cpp`, styling in `.qss`.
- Update `uml/` when changing architecture or code organization.
- Do not add backward-compatibility shims unless explicitly requested.
- Do not call device methods directly across runtime threads; use runners.

See:

- `docs/rules/design_rules.md`
- `docs/rules/ui_design_rules.md`
- `AGENT.md`

## Adding A PLC Signal

Touchpoints:

1. Add a field to `TaskLocalizeConfigPrivate` in
   `src/model/task_localization_config.h`.
2. Add the property macro to `TaskLocalizeConfig`.
3. Add JSON save/load keys in `toJson()` and `fromJson()`.
4. Add the logical signal name to `localization_signal_mapper.cpp`.
5. Add the row to `localization_setting_widget.cpp`.
6. Add the monitor row to `localization_dashboard_widget.cpp` if it is
   operator-visible.
7. Publish the signal from `LocalizationRuntimeController` if it is an output.
8. Add or update tests in `tests/architecture_contract_test/main.cpp`.
9. Update docs in this folder and relevant UML if architecture changes.

Do not parse visible UI text. Signal names are stable logical API names.

## Adding A Fault Code

Touchpoints:

1. Add the enum value in `src/model/localization_fault_code.h`.
2. Add the name mapping in `localizationFaultCodeName()`.
3. Reserve and document a stable numeric value in
   [plc_signal_contract.md](plc_signal_contract.md).
4. Add or update controller logic that publishes the new code.
5. Add or update tests that assert the numeric value is stable.
6. Update dashboard display if the fault needs special operator handling.

Never renumber existing fault codes. PLC integrations depend on numeric
stability.

## Adding A Runtime Device Role

Use the existing role pattern:

1. Add the binding to `TaskDeviceBindings`.
2. Add config UI support in the setting widget.
3. Extend `LocalizationRuntimeController::RuntimeContext`.
4. Snapshot the role in `TaskLocalization::buildRuntimeContext()`.
5. Add a role recovery context and policy in `LocalizationRuntimeController`.
6. Bind runner signals with queued connections.
7. Request device actions through the runner, not the device.
8. Forward the device `connectStatusChanged` through the base
   `IDeviceRunner::connectStatusChanged` signal. `TaskRunner::enterIdle()` relies
   on it to know when `IDeviceRunner::disconnectAndWait()` has finished closing
   the connection during teardown; a runner that does not forward it will fall
   back to the disconnect timeout on every phase exit.
9. Make the device `deviceConnect()` idempotent and have `deviceDisconnect()`
   release its transport on the device's own thread (no leaks, no cross-thread
   socket use).
10. Add fake-device tests.
11. Update UML.

The controller must remain task-independent. If it needs data from the task,
snapshot that data in `RuntimeContext`.

## Changing Runtime Matching

Current contract:

- The controller emits `runtimeMatchingRequested(cycleId, group, image)`.
- The matching worker runs `LocalizationPipeline::runMatch(...)`.
- The result returns through `onRuntimeMatchingFinished(cycleId, result)`.
- The controller ignores stale cycle ids.

When changing this flow:

- Preserve cycle-id stale-result protection.
- Keep matching off the controller thread.
- Keep `cv::Mat` arguments cloned when crossing asynchronous boundaries.
- Update contract tests for late result, abort, and fault cases.
- Update UML and this documentation.

## Changing Camera Switching

Current contract:

- `TaskLocalization::setCameraNumber()` validates logical number and assigned
  camera type only.
- The controller owns runtime camera switching.
- Switching is rejected while `Running`.
- Switching uses `CameraRunner::requestDisconnect()` and
  `CameraRunner::requestConnect()`.
- Calibration is validated from the runtime snapshot.

Do not reintroduce direct calls to `CameraDevice::deviceConnect()` or
`CameraDevice::deviceDisconnect()` in `TaskLocalization`.

## Changing Output Coordinates

VisionOutput payloads use world coordinates:

- image point: `MatchedObject::point_Center`
- image angle: `MatchedObject::point_angle`
- world point: `calib::Calibrator::imageToRobot(...)`
- world angle: `calib::Calibrator::rotateImageToRobot(...)`

If the coordinate contract changes:

1. Update `buildVisionOutputPositions()`.
2. Update dashboard result table semantics.
3. Update PLC/robot integration docs.
4. Add tests for conversion and skipped-result reasons.

## Testing Guidance

Existing focused coverage lives in `tests/architecture_contract_test/main.cpp`.

Keep fake-device tests for runtime behavior whenever possible:

- fake PLC captures `IPlcIoWriter` writes
- fake camera emits success/failure grabs, and `connectSucceeds` holds it unreachable
  across many reconnect attempts (needed to exercise an unbounded retry policy)
- fake VisionOutput captures sent positions and can fail sends

Required behavior coverage:

- Rising trigger starts one cycle.
- Held trigger does not start another cycle.
- Trigger reset clears `bMatchingFinished` and restores ready.
- Grab timeout publishes `CameraGrabTimeout`.
- VisionOutput send failure publishes `VisionOutputSendFailed`.
- Invalid pattern publishes `PatternInvalid`.
- Invalid calibration publishes `CalibrationInvalid`.
- Camera change while running is rejected.
- Lost device during running aborts with the correct role fault.
- A rising `bErrorReset` clears a latched fault and re-arms the runtime.
- A latched fault clears itself after `kFaultAutoRecoverMs` when no acknowledge arrives.
- A role outage retries indefinitely without faulting, withdraws `bTaskReady`, and
  re-arms on reconnect.
- `CameraRunner::kSingleShotTimeoutMs` stays above the camera's own grab timeout.

### Invariants worth a test rather than a comment

Two classes of defect found on the bench were invisible to unit tests because they lived
in the gap between two components. Prefer an assertion over a comment when you meet them:

- **Ordered constants across a boundary.** A watchdog in one component must outlast the
  operation it guards in another. Assert the ordering; a comment does not survive someone
  tuning either side.
- **A signal that a peer depends on for liveness.** If a component resolves state only
  from a signal, every exit path of the emitter must emit it. A silent `return` in an
  error branch is the failure mode, and it only shows up when that branch is taken —
  i.e. on hardware, in the field.

## Documentation And UML Checklist

For every localization runtime change, check:

- `docs/domains/task_localization/task_localization.md`
- `docs/domains/task_localization/task_localization_implementation_plan.md`
- files in `docs/domains/task_localization/`
- `uml/03_runtime_threading.puml`
- `uml/04_localization_task.puml`

If a change does not affect diagrams, say so in the task summary.
