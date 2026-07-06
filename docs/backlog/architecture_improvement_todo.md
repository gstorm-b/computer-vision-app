# Architecture Improvement TODO

> **Purpose.** Ordered implementation backlog for improving the current device,
> runtime, task, and UI architecture. This is not a bug list; it is a staged
> refactor plan. Keep the sequence unless a task explicitly says it can be
> reordered.

## Operating Rules

- Keep each task independently buildable and reviewable.
- Do not mix unrelated cleanups into an architecture step.
- Update the matching PlantUML files under `uml/` whenever a task changes code
  organization, ownership, abstract contracts, runtime flow, or persistence
  shape.
- Add or update focused tests for every task that changes behavior.
- Prefer direct refactors over legacy compatibility shims; this project has not
  been deployed to a customer yet.
- When a task exposes an adjacent issue, record it in `later_todo_list.md`
  unless the issue is required to complete the current step.

## Priority Legend

| Priority | Meaning |
|---|---|
| P0 | Guardrail or prerequisite. Do this before structural refactors. |
| P1 | High-impact architecture improvement with low-to-medium blast radius. |
| P2 | Larger refactor that should build on completed P1 tasks. |
| P3 | Long-term extensibility improvement; valuable after the core shape settles. |

---

## Phase 0 - Baseline and Guardrails

### A00. Add architecture test harness

**Priority.** P0

**Status.** Implemented as `tests/architecture_contract_test/`. Re-verified on
2026-06-24 with Qt 6.8.2 MSVC 2022 Debug build: suite exit code `0`, including
the runtime matching queued-payload metatype guard.

**Depends on.** None

**Goal.** Create a small set of contract tests that protect factory dispatch,
config JSON round trips, task device assignment, and runner creation before
larger refactors begin.

**Implementation notes.**
- Add a Qt Test subproject under `tests/architecture_contract_test/`.
- Keep the test target independent from `ncr_picking.pro`, following
  `docs/rules/design_rules.md` section 14.
- Cover at minimum:
  - `DeviceFactory::fromJson()` creates concrete devices for current supported
    sub-types.
  - Unsupported declared sub-types fail as `nullptr` without crashing.
  - Concrete `IDeviceCfg` classes round-trip through `toJson()/fromJson()`.
  - `TaskFactory::fromJson()` restores `TaskLocalization` via
    `TaskLocalization::fromJson()`.
  - `TaskRunner::registerDevice()` creates runners for Camera, PLC, and
    VisionOutput, and does not create one for Robot.

**Acceptance criteria.**
- Tests compile and pass locally.
- No production code behavior changes unless a failing test exposes a required
  fix.

**UML updates.** None expected. If the test harness introduces a new documented
testing package, update `uml/README.md`.

**Non-conflict note.** This task only adds tests. It should be completed before
any structural refactor so later changes have a stable safety net.

---

### A01. Normalize current UML and docs references

**Priority.** P0

**Status.** Implemented. Remaining `uml_diagram/` mentions are intentional
historical or task-description references, not active architecture links.

**Depends on.** None

**Goal.** Make `uml/` the only current architecture reference and remove stale
links to `uml_diagram/` from active docs once the old folder is deleted.

**Implementation notes.**
- Search active docs for `uml_diagram`.
- Replace references with `uml/` where appropriate.
- Do not modify historical docs under `docs/history/old_session/`.
- If `uml_diagram/` is deleted, do not recreate it.

**Acceptance criteria.**
- Active docs point to `uml/`.
- `AGENT.md` still gives a clear starting path for new sessions.

**UML updates.** None expected.

**Non-conflict note.** Documentation-only. Can be done before or after A00.

---

## Phase 1 - UI and Binding Boundaries

### A10. Introduce `DeviceWidgetFactory`

**Priority.** P1

**Status.** Implemented. `LocalizationTaskWidget` delegates device page
creation to `src/ui/forms/device_widget_factory.*`.

**Depends on.** A00 recommended

**Goal.** Move device-config widget dispatch out of
`LocalizationTaskWidget::getOrCreateDeviceConfigPage()` into a dedicated
factory.

**Implementation notes.**
- Create a small UI factory, for example `src/ui/forms/device_widget_factory.*`.
- Input should include:
  - `std::shared_ptr<vc::device::IDevice>`
  - `vc::runtime::IDeviceRunner*`
  - optional dock widget / parent widget
- Dispatch by concrete family and sub-type:
  - `CameraType::BaslerGigE` -> `BaslerCameraWidget`
  - `PlcType::MitsubishiMc` -> `MitsubishiMcDeviceWidget`
  - `VisionOutputType::VisionTCPIP` -> current output widget
- Return `nullptr` plus log for unsupported sub-types.
- Keep widget constructors unchanged in this task unless a tiny signature
  adjustment is required.

**Acceptance criteria.**
- `LocalizationTaskWidget` no longer owns concrete widget dispatch.
- Existing device pages still open and receive the correct runner.
- Unsupported sub-types fail gracefully.

**UML updates.**
- Update `uml/06_ui_widgets.puml`.
- Update `uml/01_project_overview.puml` if a new UI package/component is shown.

**Non-conflict note.** This isolates UI dispatch first. Do not rename widgets
or change device contracts in the same task.

---

### A11. Rename concrete-only device widgets

**Priority.** P1

**Status.** Implemented. `VisionOutputDeviceWidget` was renamed to
`VisionTcpipDeviceWidget`, and the Mitsubishi MC PLC page is now
`MitsubishiMcDeviceWidget`.

**Depends on.** A10

**Goal.** Align widget class names with actual sub-type scope.

**Implementation notes.**
- Rename `VisionOutputDeviceWidget` to `VisionTcpipDeviceWidget`.
- Rename the PLC widget to `MitsubishiMcDeviceWidget`.
- Update `.h`, `.cpp`, `.ui`, qmake entries, includes, and factory dispatch.
- Keep behavior unchanged.

**Acceptance criteria.**
- Class/file names no longer imply family-level support when implementation is
  sub-type-specific.
- Existing pages still work through `DeviceWidgetFactory`.

**UML updates.**
- Update `uml/06_ui_widgets.puml`.
- Update `uml/02_device_families.puml` only if class names are shown there.

**Non-conflict note.** This is a mechanical rename after dispatch is isolated.
Do not introduce new sub-types in this step.

---

### A12. Introduce task device role binding

**Priority.** P1

**Status.** Implemented. `TaskLocalizeConfig` now stores role bindings through
`TaskDeviceBindings` and persists them as `deviceBindings`.

**Depends on.** A00 recommended, A10 optional

**Goal.** Replace scattered task role fields with a clear role-to-device
binding model for `TaskLocalization`.

**Implementation notes.**
- Add a small model type, for example:
  - `TaskDeviceRole`
  - `TaskDeviceBinding`
  - `TaskDeviceBindings`
- Represent roles such as:
  - `primary_plc`
  - `vision_output`
  - `camera_number_<n>` or a typed camera-number role structure
- Keep `ITask::assignedDeviceIds()` as the ownership list.
- Use role bindings to define how owned devices are used by a task.
- For `TaskLocalization`, migrate:
  - `TaskLocalizeConfig::sCommDeviceId`
  - `TaskLocalizeConfig::sOutputDeviceId`
  - `TaskLocalizeConfig::sCameraNumberMap`
- Because there is no deployed customer data yet, update JSON schema directly
  without compatibility aliases unless explicitly requested.

**Acceptance criteria.**
- `LocalizationSettingWidget` writes role bindings instead of independent role
  fields.
- `TaskLocalization::setupTask()` resolves devices by role binding.
- Save/load round trips task role bindings.
- Tests cover missing required role, wrong device family, and valid mappings.

**UML updates.**
- Update `uml/04_localization_task.puml`.
- Update `uml/07_persistence_sequence.puml` if the JSON shape changes.

**Non-conflict note.** This changes task configuration shape. Do not change
runtime threading, command handling, or device factory dispatch in the same
task.

---

## Phase 2 - Localization Runtime Separation

### A20. Extract `LocalizationSignalMapper`

**Priority.** P1

**Status.** Implemented. `LocalizationSignalMapper` owns PLC tag to logical
signal-name translation and is covered by the architecture contract test.

**Depends on.** A12

**Goal.** Move PLC tag-to-logical-signal translation out of
`TaskLocalization`.

**Implementation notes.**
- Create a focused class that owns:
  - configured logical signal names
  - PLC tag mapping
  - translation from `QMap<QString, QVariant>` device updates to logical task
    signal events
- Keep it Qt-light if possible; it can return a value object rather than emit
  directly.
- `TaskLocalization::onCommDeviceValueChanged()` should delegate to the mapper.

**Acceptance criteria.**
- Signal translation is unit-testable without a running task or PLC runner.
- Existing `signalChanged(name, value)` behavior is preserved.

**UML updates.**
- Update `uml/04_localization_task.puml`.

**Non-conflict note.** This is an extraction only. Do not change runtime phase
logic in this task.

---

### A21. Extract `LocalizationPipeline`

**Priority.** P1

**Status.** Implemented. Commission matching now delegates image/model matching
to `LocalizationPipeline::runMatch()`.

**Depends on.** A20 recommended

**Goal.** Move image-to-match-result processing into a testable pipeline class.

**Implementation notes.**
- Create a class responsible for:
  - selecting the active `MatchGroup`
  - configuring `ImageMatcher`
  - running matching on a provided image
  - producing a result object ready for output conversion
- Keep device access outside the pipeline. The pipeline should not know about
  `CameraRunner`, `PlcRunner`, or `VisionOutputRunner`.
- Keep `PatternGroupManager` ownership in `TaskLocalization`; pass references
  or selected groups into the pipeline.

**Acceptance criteria.**
- Matching logic can be tested with a `cv::Mat` and pattern data without
  starting device threads.
- `TaskLocalization::executeLocalization()` becomes orchestration rather than
  algorithm implementation.

**UML updates.**
- Update `uml/04_localization_task.puml`.
- Update `uml/05_matching_calibration.puml` only if matching ownership changes.

**Non-conflict note.** Do not change device role schema or command transport in
this task.

---

### A22. Extract `LocalizationRuntimeController`

**Priority.** P2

**Status.** Implemented as the first runtime separation pass.
`LocalizationRuntimeController` resolves role bindings, wires the PLC runner to
the signal mapper, owns setup validity, and delegates future production-cycle
work to `LocalizationPipeline`.

**Depends on.** A20, A21

**Goal.** Separate runtime orchestration from the persistent task model.

**Implementation notes.**
- `TaskLocalization` should keep ownership of persistent config and pattern
  manager.
- `LocalizationRuntimeController` should own runtime orchestration:
  - resolving runners from `TaskRunner`
  - connecting runtime signals
  - handling trigger events
  - calling `LocalizationPipeline`
  - sending `VisionOutputRequest`
  - applying recovery policy hooks
- Start with composition: `TaskLocalization` creates and owns the controller.
- Avoid moving all logic at once. First move runtime-only methods, then shrink
  the task surface.

**Acceptance criteria.**
- `TaskLocalization` is primarily model/config plus lifecycle entry points.
- Runtime behavior remains unchanged.
- Controller can be tested with fake runner-like objects or a small adapter.

**UML updates.**
- Update `uml/04_localization_task.puml`.
- Update `uml/03_runtime_threading.puml` if controller owns runtime signal
  wiring.

**Non-conflict note.** This depends on signal mapping and pipeline extraction
to keep the controller small. Do not introduce the command queue in this step.

---

## Phase 3 - Runtime Command Robustness

### A30. Define a standard device command/result model

**Priority.** P2

**Status.** Implemented for the first family. `DeviceCommand` and
`DeviceCommandResult` now live under `src/runtime/`, and `CameraRunner`
routes its existing request methods through `submitCommand()` with deterministic
busy/invalid/unsupported rejection. Covered by architecture contract tests for
overlap rejection and invalid command rejection.

**Depends on.** A00, A22 recommended

**Goal.** Replace ad-hoc runner request methods and simple `m_busy` flags with
a standard command/result shape.

**Implementation notes.**
- Introduce command metadata:
  - command id / correlation id
  - command kind
  - target device id
  - timeout
  - optional payload
- Introduce result metadata:
  - command id
  - success/failure
  - error code/message
  - optional payload
- Start with one family, preferably `CameraRunner`, before applying to all
  runners.
- Keep typed convenience methods, but route internally through the command
  model.

**Acceptance criteria.**
- Overlapping commands are rejected or queued deterministically.
- Timeout and failure reporting is standardized.
- Existing UI actions still work.

**UML updates.**
- Update `uml/03_runtime_threading.puml`.

**Non-conflict note.** Implement family-by-family. Do not change device config
or task role binding in the same step.

---

### A31. Add runner command queue and timeout handling

**Priority.** P2

**Status.** Implemented for `CameraRunner` on 2026-05-30. Added
`DeviceCommandQueue` helper with per-command queue policies, active command
timeout handling (`TimedOut`), structured command lifecycle logging
(`id/kind/target/status/code`), and architecture contract coverage for
`success`, `timeout`, and `busy/queue policy`.

**Depends on.** A30

**Goal.** Make runner request execution deterministic under load or device
latency.

**Implementation notes.**
- Add a small FIFO command queue to the runner base or a helper owned by each
  runner.
- Decide per command whether it can be queued, rejected when busy, or can cancel
  an older command.
- Add timeout handling with clear error reporting.
- Keep direct device calls on the device thread via queued Qt connections.

**Acceptance criteria.**
- Triggering repeated UI actions cannot create ambiguous in-flight operations.
- Logs show command id, command type, target device, and result.
- Tests cover success, timeout, and busy/queue policy.

**UML updates.**
- Update `uml/03_runtime_threading.puml`.

**Non-conflict note.** Do this after the command/result model exists. Avoid
changing task orchestration in the same step except to consume standardized
results.

---

### A32. Introduce runtime recovery policies

**Priority.** P2

**Status.** Implemented on 2026-05-30. Added explicit
`LocalizationRecoveryPolicy` model and policy-driven reconnect handling in
`LocalizationRuntimeController` for camera / primary PLC / vision output roles
with retry scheduling and fault escalation messages containing role + retry
history.

**Depends on.** A22, A30

**Goal.** Move reconnect retry logic and fault escalation into explicit policy
objects.

**Implementation notes.**
- Create policy classes or structs for:
  - camera reconnect
  - PLC reconnect
  - vision output reconnect
- Include max retries, interval, timeout, and final fault behavior.
- Keep policy values configurable later, but start with constants matching the
  intended behavior in `docs/domains/task_localization/task_localization.md`.

**Acceptance criteria.**
- Runtime controller delegates retry decisions to policy objects.
- Fault messages identify the failed device role and retry history.

**UML updates.**
- Update `uml/04_localization_task.puml`.
- Update `uml/03_runtime_threading.puml` if policy is shown near runtime.

**Non-conflict note.** This should not change device factory, UI widget
dispatch, or persistent config unless policy persistence is explicitly in
scope.

---

## Phase 4 - Device Contract Extensibility

### A40. Introduce capability interfaces

**Priority.** P3

**Status.** Implemented in code on 2026-05-30. Added capability interfaces
for optional device surfaces (`IImageSourceDevice`, `IPlcTagProvider`,
`IDigitalIoProvider`, `IWordIoProvider`, `IResultOutputDevice`) and removed
PLC-tag list methods from `IDevice`. `LocalizationSettingWidget` now queries
PLC tag capabilities explicitly instead of assuming every device exposes PLC IO.
Build/test re-verification was blocked in this session by shell approval limits.

**Depends on.** A10, A30 recommended

**Goal.** Reduce pressure on `IDevice` by moving optional surfaces into
capability-specific interfaces.

**Implementation notes.**
- Keep `IDevice` focused on identity, connection lifecycle, status, config,
  JSON, thread detach, and base signals.
- Extract optional interfaces such as:
  - `IImageSourceDevice`
  - `IPlcTagProvider`
  - `IResultOutputDevice`
  - `IDigitalIoProvider`
  - `IWordIoProvider`
- Move or mirror methods such as `getAvailableBits()` and
  `getAvailableWords()` into capability interfaces.
- Update widgets and task logic to query capabilities after resolving device
  roles.

**Acceptance criteria.**
- Camera and VisionOutput devices are no longer forced to expose irrelevant PLC
  IO methods as meaningful API.
- Existing behavior remains intact.
- Unsupported capability is handled as a validation error, not a crash.

**UML updates.**
- Update `uml/02_device_families.puml`.
- Update `uml/04_localization_task.puml` if task validation uses capabilities.

**Non-conflict note.** This is a broad contract refactor. Do not combine it
with factory registry or runtime command queue work.

---

### A41. Split config, runtime state, and diagnostics

**Priority.** P3

**Status.** Implemented for `VisionTcpipDevice` on 2026-05-30. Added
`VisionTcpipRuntimeState` and `VisionTcpipDiagnostics` alongside persisted
`VisionTcpipDeviceCfg`, and updated `VisionTcpipDeviceWidget` to show the
read-only runtime and diagnostic sections in its property browser. JSON
persistence shape remains config-only. Build/test re-verification was blocked
in this session by shell approval limits.

**Depends on.** A30 recommended

**Goal.** Keep persisted configuration separate from volatile runtime state and
operator diagnostics.

**Implementation notes.**
- For each family, define a convention:
  - `*Cfg` or `*Config`: persisted editable settings
  - `*RuntimeState`: connected clients, heartbeat counters, active polling data
  - `*Diagnostics`: health, counters, last errors, timing
- Start with `VisionTcpipDevice`, because it currently has meaningful runtime
  heartbeat/client state.
- Expose runtime state read-only to widgets.

**Acceptance criteria.**
- `toJson()` only writes persisted settings.
- UI status panels read runtime state/diagnostics instead of config.
- Tests confirm runtime state is not persisted accidentally.

**UML updates.**
- Update `uml/02_device_families.puml`.
- Update `uml/06_ui_widgets.puml` if widgets show diagnostics ownership.

**Non-conflict note.** Do one device family at a time. Do not change the JSON
schema for unrelated devices.

---

### A42. Replace factory switches with a device registry

**Priority.** P3

**Status.** Implemented on 2026-05-30. Added `DeviceRegistry` as the source of
truth for supported concrete device sub-types, delegated `DeviceFactory`
dispatch to registry entries, and switched `DeviceManager::getSubDeviceTypeList()`
to registry-backed lists. `AddDeviceWizard` preserves current PLC frame/code UI
by sourcing those protocol options directly instead of overloading subtype list
state. Build/test re-verification was blocked in this session by shell approval
limits.

**Depends on.** A40 recommended

**Goal.** Make new device sub-types pluggable through a registry instead of
expanding central switch statements and manually duplicated subtype lists.

**Implementation notes.**
- Introduce a registry entry containing:
  - top-level `DeviceType`
  - sub-type key
  - display name
  - config JSON key
  - creator callback
  - optional UI widget creator if A10 is mature enough
- Make `DeviceFactory` delegate to the registry.
- Make `DeviceManager::getSubDeviceTypeList()` read from the registry.
- Keep registration colocated near concrete implementation where possible.

**Acceptance criteria.**
- Adding a new concrete device requires one local registration plus concrete
  files, not multiple central switch edits.
- Existing supported devices still load from JSON.
- Unsupported placeholder sub-types are either not registered or registered as
  unavailable with explicit UI handling.

**UML updates.**
- Update `uml/02_device_families.puml`.
- Update `uml/01_project_overview.puml` if registry becomes a first-class
  component.
- Update `uml/07_persistence_sequence.puml` if load dispatch changes.

**Non-conflict note.** Do this after capability interfaces or keep it purely
factory-focused. Do not add a new vendor in the same change.

---

## Phase 5 - Task Lifecycle Formalization

### A50. Add explicit task state machine

**Priority.** P3

**Status.** Implemented in code on 2026-05-31. Added explicit `TaskState`
enum + validated transition rules, wired `ITask` lifecycle methods through the
state machine, surfaced runtime recovery/fault state from
`LocalizationRuntimeController`, and updated `LocalizationTaskWidget` to show
task state directly instead of inferring readiness from device presence alone.
Build/test re-verification is still pending in this session until the dedicated
`build/...` verification run completes.

**Depends on.** A22, A32

**Goal.** Replace implicit lifecycle assumptions with explicit task states and
validated transitions.

**Implementation notes.**
- Define states such as:
  - `Idle`
  - `CommissionStarting`
  - `Commission`
  - `RuntimeStarting`
  - `Ready`
  - `RunningCycle`
  - `Recovering`
  - `Faulted`
  - `Stopping`
- Define transition events and guards.
- Keep `TaskRunner::Phase` as the thread/resource phase unless it becomes
  clearly redundant.
- Surface state changes to UI status lamps and logs.

**Acceptance criteria.**
- Invalid transitions are rejected and logged.
- Runtime faults move task state to `Faulted`.
- UI can display task state without inferring it from device statuses.

**UML updates.**
- Update `uml/03_runtime_threading.puml`.
- Update `uml/04_localization_task.puml`.

**Non-conflict note.** This should come after runtime orchestration and
recovery policies are separated. Avoid changing device contracts here.

---

## Recommended Sequential Order

1. A00 - Add architecture test harness.
2. A01 - Normalize current UML and docs references.
3. A10 - Introduce `DeviceWidgetFactory`.
4. A11 - Rename concrete-only device widgets.
5. A12 - Introduce task device role binding.
6. A20 - Extract `LocalizationSignalMapper`.
7. A21 - Extract `LocalizationPipeline`.
8. A22 - Extract `LocalizationRuntimeController`.
9. A30 - Define standard device command/result model.
10. A31 - Add runner command queue and timeout handling.
11. A32 - Introduce runtime recovery policies.
12. A40 - Introduce capability interfaces.
13. A41 - Split config, runtime state, and diagnostics.
14. A42 - Replace factory switches with a device registry.
15. A50 - Add explicit task state machine.

## Deferred Until Needed

These items should not be started as architecture-only work unless a concrete
feature requires them:

- Implement `VisionSerialDevice`.
- Implement a real robot vendor protocol.
- Add `RobotRunner`.
- Add a second PLC vendor.
- Replace qmake or reorganize the full build system.
