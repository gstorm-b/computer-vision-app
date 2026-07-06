# Module: model (level 2)

**Purpose.** Project/task domain model and persistence: `Project`,
`ProjectRepository`, task lifecycle (`ITask`, `TaskLocalization`,
`TaskFactory`, task state machine), localization pipeline and
`LocalizationRuntimeController`, signal mapping, robot-kinematic pick
checking (consumes `components/RobotKinematics`).

**May include.** Qt, `core/`, `device/`, `calibration/`, `matching/`,
`runtime/` (model and runtime are the same level and may include each
other), `components/RobotKinematics`.
**Must NOT include.** UI (`ui/`), `app/`.
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

**Verify.** `tests/architecture_contract_test` (the bulk of the suite),
root app build.

**Build registration.** `src/model/model.pri` only.

**Docs.** `docs/domains/task_localization/`.
