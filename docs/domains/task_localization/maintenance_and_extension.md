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
2. Add the `P_PROPERTY_*` macro to `TaskLocalizeConfig` **and, in the same edit, its display name to
   `kDisplayNameSources[]`**. `lupdate` cannot read `Q_CLASSINFO`, so a name without a marker can
   never be translated; `test_every_display_name_has_a_translation_marker` fails on it, and the
   display-name total the contract suite asserts moves with it.
3. Add JSON save/load keys in `toJson()` and `fromJson()`. **Bump `kSchemaVersion`** when an older
   build would load the new document wrongly rather than merely without the value, and record why in
   its history comment.
4. Add the logical signal name to `kSignalFields` in `localization_signal_mapper.cpp`. The
   signal-map gate iterates exactly that table (`LocalizationSignalMapper::signalFieldNames()`).
5. Decide whether it is **required**. A required signal is one the runtime cannot work without: add
   it to `LocalizationRuntimeController::requiredSignalNames()` and an unmapped tag refuses the start.
   Everything else only warns. Adding to the required list refuses every existing project that leaves
   the signal unmapped, so treat it as a migration.
6. Add the row to `kSignalRows` in `localization_setting_widget.cpp`.
7. Add the monitor row to `kSignalRows` in `localization_dashboard_widget.cpp` if it is
   operator-visible.
8. Publish the signal from `LocalizationRuntimeController` if it is an output. **Never publish onto a
   register the master owns** — a command input gets its own status output instead, the way
   `nActiveCamera` has `nActiveCameraStatus` (backlog item 60). Then decide whether a PLC program
   blocks on it: if so, add it to `isHandshakeSignal()` so a failed write is retried and escalated to
   `301`; if not, its write failures stay log-only.
9. Add or update tests in `tests/architecture_contract_test/main.cpp`.
10. Update [plc_signal_contract.md](plc_signal_contract.md), the other docs in this folder, and UML.

Do not parse visible UI text. Signal names are stable logical API names.

## Adding A Fault Code

Touchpoints:

1. Add the enum value in `src/model/localization_fault_code.h`.
2. Add the name mapping in `localizationFaultCodeName()`. **Easy to forget and
   silent when forgotten:** the switch has no `default`, so a missing case still
   compiles and the dashboard's fault panel — which calls this function — shows
   the literal text `Unknown` to the operator. `test_every_localization_fault_code_has_a_name`
   in the contract suite catches it; add the new value to that test's list.
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
5. Add a role recovery context and policy in `LocalizationRuntimeController`. Remember the default
   policy is what ships: `setRecoveryPolicies()` has no production caller (backlog item 67).
6. Bind runner signals with queued connections.
7. Request device actions through the runner, not the device.
8. **If the role is a capability rather than a family, ask the device, not the runner class.**
   `vision_output` is filled by any device implementing `IResultOutputDevice`; the runner reports it
   through `IDeviceRunner::supportsResultOutput()`, and `PlcRunner` answers per device with a
   `dynamic_cast` because the PLC family is mixed (the Modbus devices output results, the MC device
   does not). `setup()` refuses a runner that cannot serve the role, and the settings widget lists
   only devices that can. See `src/runtime/AGENTS.md`.
9. Forward the device `connectStatusChanged` through the base
   `IDeviceRunner::connectStatusChanged` signal. `TaskRunner::enterIdle()` relies
   on it to know when `IDeviceRunner::disconnectAndWait()` has finished closing
   the connection during teardown; a runner that does not forward it will fall
   back to the disconnect timeout on every phase exit.
10. Make the device `deviceConnect()` idempotent and have `deviceDisconnect()`
    release its transport on the device's own thread (no leaks, no cross-thread
    socket use).
11. Add fake-device tests.
12. Update UML.

The controller must remain task-independent. If it needs data from the task,
snapshot that data in `RuntimeContext`.

## Changing Runtime Matching

Current contract:

- The controller emits `runtimeMatchingRequested(cycleId, group, workspace, image, pickingChecker)`.
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

- The controller owns runtime camera switching, through
  `LocalizationRuntimeController::setActiveCameraNumber()`. The PLC reaches it through
  `handlePlcValues()`, a manual change through `TaskLocalization::queueSetActiveCameraNumber()`.
  There is no task-level setter any more (deleted in Phase 9 / C5).
- Switching is rejected while `Running`.
- **One validator for every entry point.** `validateCameraNumber()` — range first, registration
  second — is shared by the setter and by `setup()`. A second, looser check anywhere is how a
  startup and a runtime write once disagreed about what 0 means.
- A refused number is **not adopted** and **latches** (`m_activeCameraSelectionRejected`); only an
  accepted value for the same signal clears it.
- An accepted change publishes `nActiveCameraStatus`, never `nActiveCamera`.
- Switching uses `CameraRunner::requestDisconnect()` and
  `CameraRunner::requestConnect()`, re-resolves the camera's workspace
  (`applyActiveCameraWorkspace()`) and rebuilds the pick checker for the new calibration.
- Calibration is validated from the runtime snapshot.

Do not reintroduce direct calls to `CameraDevice::deviceConnect()` or
`CameraDevice::deviceDisconnect()` in `TaskLocalization`.

## Runtime Readiness Invariants

Every path that re-arms the runtime goes through `markRuntimeReady()`. Each of these has been the
whole fix for a field defect; keep them.

- **`WaitingTriggerReset` counts as busy.** The PLC still holds `bExecuteTrigger` and has not read
  the latched results. Re-arming there publishes the ready outputs over `bMatchingFinished`,
  `bMatchingDetected` and `nDetectedNumber` before the PLC consumed them. This guard is also what
  makes it safe for the index setters to re-arm on success.
- **A refusal is latched per signal.** A refused index is never adopted, so every other check reads
  the previous, good selection and passes. Without the latch, a valid write to the *other* index
  signal silently re-armed the task on a selection nobody made. Each latch clears only on an accepted
  value for its own signal — not on `bErrorReset`, not on a reconnect.
- **Range before registration, then content.** `validateCameraNumber()` /
  `validatePatternGroupNumber()` decide whether a number can name anything and whether it does; only
  an index that passes reaches the content checks (`validateActivePatternGroup()`,
  `validateActiveCameraCalibration()`). Reporting a missing camera as a calibration problem is the
  mis-signposting backlog item 55 was about.
- **A refused *startup* selection keeps the runtime valid.** A PLC-commanded index refused in
  `setup()` is published as a fault but kept out of `SetupResult::errors`
  (`m_startupSelectionFault`), so the next valid write re-arms it. Routed through the errors list,
  the refusal was correct and unrecoverable.

## Robot Pick Check Ownership (Phase 9 / F1)

The pickability gate's settings belong to the **task**: `TaskLocalizeConfig::robotCheckConfig()`,
persisted since task schema version 4 and edited from the task's Settings tab (**Robot pick check →
Set…**). `TaskLocalization::buildRuntimeContext()` reads that and nothing else.

- `IResultOutputDevice::robotKinematicCheckConfig()` still exists and every result-output device
  implements it, but the localization task no longer calls it. The vision-output device widgets also
  keep their own copy of the editor; that copy drives only the **device-side advisory** check,
  `VisionTcpipDeviceBase::runKinematicCheck()`. Two editors for one kind of setting is a commissioning
  trap; retiring one is **deferred** — backlog item 66.
- `setup()` refuses an **enabled** check that cannot run: a preset that does not resolve
  (`RobotKinematicPickingChecker::isReady()`) or a camera with no usable calibration. Both used to
  start and then report every pose unpickable.
- A document written before schema version 4 loads with the check **disabled** — the answer a v3
  build already gave whenever the output role was not a vision-output device.

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
- Invalid pattern publishes `PatternNotRegistered`.
- Invalid calibration publishes `CalibrationInvalid`.
- An unregistered/out-of-range camera number publishes `CameraNotRegistered`, not
  `CameraLost`.
- Camera change while running is rejected.
- Lost device during running aborts with the correct role fault.
- A rising `bErrorReset` clears a latched fault and re-arms the runtime.
- A latched fault clears itself after `kFaultAutoRecoverMs` when no acknowledge arrives.
- A role outage retries indefinitely without faulting, withdraws `bTaskReady`, and
  re-arms on reconnect.
- `CameraRunner::kSingleShotTimeoutMs` stays above the camera's own grab timeout.
- A PLC-commanded invalid index at startup faults, keeps the runtime valid, and re-arms on a valid
  write.
- An unmapped required signal, a shared tag, or a tag the PLC does not provide refuses setup.
- A handshake write that fails `kPlcWriteRetryBudget` times aborts the cycle with `PlcWriteFailed`
  and does not re-enter the retry machinery.
- An enabled pick check with an unresolvable preset, or without calibration, refuses setup.
- A successful cycle carries monotonic `CycleTimings`; a faulted one leaves unreached stages unset.

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
