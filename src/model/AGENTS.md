# Module: model (level 2)

**Purpose.** Project/task domain model and persistence: `Project`,
`ProjectRepository`, task lifecycle (`ITask`, `TaskLocalization`,
`TaskFactory`, task state machine), localization pipeline and
`LocalizationRuntimeController`, signal mapping, robot-kinematic pick
checking (consumes `components/RobotKinematics`).

**May include.** Qt, `core/`, `device/`, `calibration/`, `matching/`,
`runtime/` (model and runtime are the same level and may include each
other), `components/RobotKinematics`.
**Must NOT include.** UI (`ui/`), `components/app/`.
Enforced by the architecture contract test.

**Invariants.**
- `TaskLocalization` owns persistent task/config concerns. Runtime
  orchestration belongs in `LocalizationRuntimeController` and per-device
  runners — do not blur that split.
- The controller runs on the coordinator thread; devices keep per-device
  threads; matching offloads to `matchingRunner`. Revisit only with measured
  latency evidence (decision recorded 2026-06-24).
- Task config JSON carries `version`/`kSchemaVersion`; newer-schema documents
  are refused on load. Imported bindings are validated.
- Device pointers come from `DeviceManager` (`std::shared_ptr`); never reset
  raw pointers into new control blocks.
- A config property's display name lives in `Q_CLASSINFO`, which `lupdate`
  cannot read — so adding one to `TaskLocalizeConfig` (or `ITask`) means adding
  the string to that class's `kDisplayNameSources[]` in the same edit, with the
  context spelled exactly as `staticMetaObject.className()`. Skipping it leaves
  a label that works in English and can never be translated, with no build
  error. The contract test compares both sets and every context.

- **A rejection that only publishes signals has no memory, and the runtime will
  erase it.** `publishInitialReadyOutputs()` sets `bTaskReady`, `bCameraValid`,
  `bPatternValid` true and `bTaskFault` false *unconditionally*, and
  `publishCycleStartOutputs()` clears the fault too. So a setter that publishes
  "invalid" and returns — without dropping `bTaskReady` and without moving
  `m_cycleState` to `Faulted` — leaves the runtime triggerable on stale state, and
  the very next re-arm republishes everything as healthy. Both active-index setters
  shipped that way: an unregistered camera or pattern number left the task Ready
  and valid (owner-reported 2026-09-07). **State first, signals second.**
- **A refusal that is not adopted must be latched, or an unrelated re-arm forgives
  it.** The follow-on to the trap above, and it survived that fix. Both active-index
  setters deliberately do *not* adopt a refused number — the last good camera stays
  bound, which is what lets a later valid write recover with no operator action — so
  the refusal leaves **nothing for `markRuntimeReady()`'s gate to read**: roles are
  healthy, the pattern group validates, the calibration validates, all against the
  *previous* selection. Every re-arm path funnels through that function, so a valid
  write to the *other* index signal, a role reconnecting, or a `bErrorReset` all
  re-armed a task whose commanded camera was still refused, republishing
  `bCameraValid` as true (owner-reported 2026-09-07). Fixed with a per-signal
  rejected latch, cleared **only** by an accepted value for that same signal.
  **Whenever you decline to adopt an input, ask what remembers that you declined.**
- **`markRuntimeReady()` must never fire while `WaitingTriggerReset` is set.** The
  PLC is still holding the trigger high and has not yet read the latched
  `bMatchingFinished` / `bMatchingDetected` / `nDetectedNumber`; re-arming would
  wipe all three mid-handshake. That makes the trigger falling edge the one
  legitimate exit from the state, and it has to leave the state *before* asking to
  re-arm — the fault branch beside it already did, which is why only the success
  branch was wrong.

- **The runtime path can now be exercised with no hardware, and it should be.** A
  virtual PLC's inputs are drivable as of 2026-09-07
  (`IPlcInputSimulator` → `PlcRunner::requestInjectInputValue()`), so a contract
  test can drive the controller through the **real delivery path** — device →
  runner thread boundary → `valueChanged` → `handlePlcValues()` — instead of
  calling `handlePlcValues()` directly. Most cases in the suite still call it
  directly, which is fine for testing controller logic but proves nothing about
  delivery. When a defect could live in the wiring rather than the logic, use
  `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`
  as the model. Every Phase F defect was found by the owner on the cell precisely
  because no suite could reach that path.

**Verify.** `tests/architecture_contract_test` (the bulk of the suite),
root app build.

**Build registration.** `src/model/model.pri` only. That `.pri` is consumed by
`src/src.pro`, which compiles every module **once** into the `ncr_shared` static
library that both shells link. No shell `.pro` lists module sources, so a file
added anywhere else is simply not built.

**Docs.** `docs/domains/task_localization/`.
