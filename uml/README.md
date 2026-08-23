# UML Reference

This folder contains the current UML source for the `ncr_picking` codebase.
It is intended to replace the older `uml_diagram/` folder.

The diagrams are PlantUML source files. They describe the code as it exists
now, not the desired future state. Placeholder enum values and stub classes are
marked explicitly when the implementation is not wired yet.

## Diagram Index

| File | Purpose |
|---|---|
| `01_project_overview.puml` | High-level package/module structure and dependency direction. |
| `02_device_families.puml` | Device base classes, family sub-type dispatch, concrete devices, configs, factory, and manager. |
| `03_runtime_threading.puml` | Task runner, per-device runners, QThread ownership, and runtime coordination relationships. |
| `04_localization_task.puml` | Localization task model, configuration, device assignment, signals, matcher, and pattern manager. |
| `05_matching_calibration.puml` | Matching library and calibration module relationships. |
| `06_ui_widgets.puml` | Main task UI shell, task pages, device widgets, and reusable mapping widgets. |
| `07_persistence_sequence.puml` | Project save/load sequence through repository, task factory, device factory, JSON, and image BLOBs. |
| `08_runtime_state_machines.puml` | Task runner phase, localization task state, and localization runtime cycle state transitions. |
| `09_robot_kinematics.puml` | Current `components/RobotKinematics` component: solver hierarchy, config, results, collision backends, presets, and Vision Output integration. |
| `11_runtime_shell.puml` | Operator runtime executable (`ncr_runtime.exe`): shell window, dock/layout controller, shared startup and hand-off helpers, and its relationship to the commissioning shell. |

There is no `10_*.puml`; the numbering has a gap and that is not a missing file.

## Current Architecture Notes

- The project is a Qt 6 / C++17 / OpenCV application built with qmake.
- It ships as **two peer executables**: `ncr_picking.exe` (`app/`, commissioning) and
  `ncr_runtime.exe` (`runtime_app/`, operator runtime). Neither shell may include
  the other; both link the same `ncr_shared` static library built from `src/`.
- Each device family except `Robot` has a hardware-free `Virtual*` sub-type in
  `src/device/virtual/`. These are **shipped code, not test doubles** — the
  architecture contract test drives these classes rather than its own copies.
  See `docs/domains/virtual_devices/virtual_devices.md`.
- `Project` owns tasks and a `DeviceManager`.
- `DeviceManager` owns device instances as `std::shared_ptr<IDevice>`.
- Device families are grouped by top-level `DeviceType`: `Camera`, `PLC`,
  `VisionOutput`, and `Robot`.
- Each device family has its own sub-type enum and abstract family base.
- `DeviceFactory` dispatches first by `DeviceType`, then by family sub-type.
- `ITask` owns a `TaskRunner`; `TaskRunner` creates one runner per assigned
  supported device.
- Runtime runner support currently exists for `Camera`, `PLC`, and
  `VisionOutput`. `Robot` devices exist as stubs but do not have a runner yet.
- The `components/RobotKinematics` component is the active kinematics source of
  truth. It is an Eigen-based Qt Core library pulled into the app via
  `components/RobotKinematics/robotkinematics.pri`, independent of the
  `vc::device::robot` communications family. Public APIs use SI units internally
  with mm/deg convenience helpers. Current app integration uses the Nachi MZ04D
  preset, optional Coal mesh collision, Vision Output
  `RobotKinematicCheckConfig`, the embedded `RobotKinematicCheckWidget`, and the
  `RobotKinematicPickingChecker` adapter used by Localization runtime matching.
- `TaskLocalization` is the only concrete task currently implemented.
- Pattern data belongs to `PatternGroupManager`, while binary pattern images
  are persisted through `ProjectRepository::project_images`.
- Architecture contract tests live in
  `tests/architecture_contract_test/` and should be updated when these diagrams
  expose a new structural contract.

## Known Placeholders

- `CameraType::Realsense` and `CameraType::BaslerUSB` are enum values but
  `DeviceFactory::createCamera()` currently returns `nullptr` for them.
- `VisionOutputType::VisionSerial` is declared but has no concrete device or
  config implementation.
- `RobotType::Huayan` is declared but has no concrete implementation.
- `KawasakiRobotDevice` and `NachiRobotDevice` are minimum stubs.
- `TaskRunner` does not create a `RobotRunner`.
- There is deliberately no virtual `Robot` sub-type — nothing consumes one.
- `VirtualPlcDevice` records what the runtime *writes* but nothing can drive a
  value *in*, so a task reaches `Ready` and no further (backlog #43).
- `RobotKinematics` is packaged for build-folder runs, but customer installer
  packaging is still open (see `docs/backlog/later_todo_list.md` #27).
- `RobotKinematics` currently exposes the Nachi MZ04D production preset plus
  test/JSON-loaded presets. Kawasaki RS007N and Nachi MZ07 were dropped with the
  old `rkin` module.
- The robot-pickability adapter duplicates some solve/collision wiring from the
  Vision Output device and widget. Consolidating that into a shared facade is a
  later cleanup candidate.
