# AGENT.md

This is the first file an AI/code agent should read when joining
`ncr_picking`.

## Project Snapshot

`ncr_picking` is a Qt/C++ industrial vision-picking application. The first
commercial path is one application with explicit Commission and Runtime modes,
centered on `TaskLocalization`.

First-release integrations:

- Basler GigE camera.
- Mitsubishi MC PLC.
- VisionOutput TCP/IP server/client.
- Advisory RobotKinematics checking for outgoing pick poses.

Deferred integrations include robot communication runner, Huayan robot,
VisionSerial, BaslerUSB, Realsense, custom calibration-board authoring, and the
customer installer implementation.

## Read First

For most work, read only what matches your task. Do not load the whole docs
tree by default.

1. `docs/README.md` for the documentation map and source-of-truth groups.
2. `docs/rules/design_rules.md` for process, ownership, language,
   architecture, and coding conventions.
3. `docs/rules/build_and_verification.md` for the qmake/MSVC build and test
   flow.
3a. `docs/rules/documentation_build.md` for the Doxygen/Graphviz/PlantUML
    generated API reference, and `docs/rules/design_rules.md` §18 for the
    `///` Doxygen comment style all class/method/member comments must follow.
4. `docs/backlog/technical_debt_and_next_steps.md` for the active
   implementation backlog after restructure closeout.
5. `docs/backlog/later_todo_list.md` before flagging or fixing known deferred
   work.

Topic-specific docs:

- Localization runtime: `docs/domains/task_localization/`.
- RobotKinematics integration: `docs/architecture/robot_kinematics_module.md`.
- Product packaging: `docs/product/phase4_product_release_plan.md` and
  `docs/product/customer_installer_packaging.md`.
- UI/QSS implementation: `docs/rules/ui_design_rules.md`.
- UI colour tokens and palette migration: `docs/rules/ui_theme_tokens.md`.
- Current UML: `uml/`.

Historical docs:

- `docs/history/` is traceability only. Do not treat it as current spec unless
  the user explicitly points there.
- `docs/generated/architecture_docs/` is API reference only and may drift;
  prefer source and tests when behavior matters.

## Source-Of-Truth Hierarchy

Use this order when project documents disagree:

1. Source code and tests are the implemented truth.
2. `AGENT.md` is the entrypoint and authority map for agents.
3. Topic source-of-truth docs own their domain:
   - UI structure, QSS workflow, and theming implementation:
     `docs/rules/ui_design_rules.md`;
   - UI token names, values, palette rationale, QPalette mapping, and
     colour-scheme migration workflow: `docs/rules/ui_theme_tokens.md`;
   - build, test, and local environment workflow:
     `docs/rules/build_and_verification.md`;
   - architecture diagrams: `uml/`.
4. `docs/rules/design_rules.md` owns general engineering and architecture rules
   when no topic-specific source-of-truth doc exists.
5. Historical docs and generated API references are supporting context only.

If code and docs disagree, treat code as the current implemented behaviour, then
either update the docs or flag the drift before closing the task.

## Operating Rules

- Code, comments, log strings, and committed docs are English.
- User conversation may be Vietnamese when the user writes Vietnamese.
- Prefer small, verifiable slices over broad rewrites.
- Keep unrelated user changes intact. Do not revert dirty work you did not
  create.
- For explicit implementation requests, proceed with the work and keep the user
  updated. For broad, risky, or ambiguous architecture changes, restate the
  scope and ask before editing.
- If you discover unrelated debt, document it in
  `docs/backlog/later_todo_list.md` or
  `docs/backlog/technical_debt_and_next_steps.md` instead of silently mixing it
  into the current change.
- Do not add backwards-compatibility shims unless the user explicitly asks. The
  project has not shipped to customers yet.

## Module Map And Scope Cards

The build is one `.pri` per module; every module folder carries an
`AGENTS.md` scope card (purpose, allowed dependencies, invariants, verify
commands). Read the scope card before editing a module, and register new
files in that module's `.pri` — never in `ncr_picking.pro`.

| Module | Path | Level |
|---|---|---|
| core (settings/logger/utils) | `src/core/` | 0 |
| device | `src/device/` | 1 |
| calibration | `src/calibration/` | 1 |
| matching | `src/matching/` | 1 |
| model | `src/model/` | 2 |
| runtime | `src/runtime/` | 2 |
| ui (forms + widgets) | `src/ui/` | UI |
| app shell (+translations) | `app/` | top |

Include-layering rules (enforced by
`tests/architecture_contract_test::test_module_include_layering_contract`):
lower levels must not include higher ones; `model` and `runtime` may include
each other; `device` may include `calibration` (cameras own a Calibrator);
`ui` may include every non-UI module; only `app/` may include everything; no
`../` escapes and no `src/`-prefixed quoted includes (module includes are
rooted at `src/`, e.g. `core/...`, `ui/widgets/...`).

Prebuilt third-party install trees under `3rdparty/` (Eigen, Coal, Boost,
Assimp) are provisioned locally and NOT tracked in git — see
`3rdparty/README.md` before assuming a fresh clone can build.

## Architecture Guardrails

- Source code and tests are the highest source of truth for implemented
  behavior.
- `TaskLocalization` owns persistent task/config concerns. Runtime orchestration
  belongs in `LocalizationRuntimeController` and per-device runners.
- Do not call device methods directly across runtime threads. Use
  `CameraRunner`, `PlcRunner`, `VisionOutputRunner`, or a new runner for the
  relevant family.
- Device families follow abstract base plus concrete subtype:
  `CameraDevice`, `PlcDevice`, `VisionOutputDevice`, `RobotDevice`.
- Every new device subtype must update enum/string conversion, factory
  dispatch, UI dispatch, persistence, and tests.
- UI structure belongs in `.ui`, behavior in `.cpp`, and styling in `.qss`.
  Avoid inline `setStyleSheet()` unless the UI rules explicitly allow it.
- Update `uml/` and docs when changing architecture boundaries.

## Build And Test

This is a qmake project. In command-line MSVC verification, use qmake first and
`nmake /nologo` as the reliable fallback.

Local machine paths must come from environment variables. Do not add new
hard-coded absolute paths for Qt, OpenCV, Basler Pylon, Visual Studio, or
third-party libraries.

Expected local variables:

- `NCR_PICKING_ROOT`
- `QT_MSVC_DIR`
- `VCVARS`
- `OPENCV_ROOT`, `OPENCV_BIN`, `OPENCV_INCLUDE_DIR`, `OPENCV_LIB_DIR`
- `OPENCV_WORLD_RELEASE`, `OPENCV_WORLD_DEBUG` when the OpenCV world library
  names differ from qmake defaults
- `PYLON_ROOT`, `PYLON_RUNTIME_DIR`, `PYLON_INCLUDE_DIR`, `PYLON_LIB_DIR`
- `PYLON_BASE_LIB` when the Basler Pylon import-library name differs from the
  qmake default
- `VCTOOLS_DEBUG_CRT_DIR` when running Debug test binaries outside Visual Studio
- third-party/component roots such as RobotKinematics dependencies

Important:

- Call `vcvars64.bat` directly. Do not redirect its output.
- Run `qmake` from the build directory before compiling.
- Build output location follows the `.pro` owner:
  - root app `ncr_picking.pro`: `%NCR_PICKING_ROOT%\build\<build-name>`;
  - tests: `build\<build-name>` next to the test `.pro`;
  - examples/components: `build\<build-name>` next to that `.pro`.
- Do not put test/example/component build outputs under root `build\`.
  Root `build\` is reserved for the root application and root release/package
  verification.
- For `tests/architecture_contract_test`, run
  `nmake /nologo -f Makefile.Debug compiler_moc_source_make_all` before the
  normal `nmake /nologo`.
- Use `QT_QPA_PLATFORM=minimal` when running Qt tests headlessly.
- Qt Creator may use `jom`, but from Codex CLI use the qmake + nmake fallback
  unless the `jom` PATH inheritance issue has been solved.

See `docs/rules/build_and_verification.md` for exact commands.

## Current Release Decision

Phase 4 chose a single-app first release:

- one `ncr_picking.exe`;
- explicit Commission and Runtime modes;
- no separate runtime executable until the operator flow is validated.

Customer installer work is still open. Build-folder deployment copies
RobotKinematics DLLs and `robot_assets/`, but a customer installer must have
its own manifest and clean-machine smoke test.

## Highest Priority Next Work

Start with `docs/backlog/technical_debt_and_next_steps.md`. The most important
next slices are:

- operator runtime validation with a real or simulated Localization cycle;
- customer installer prototype and clean-machine smoke verification;
- `TaskLocalization` ownership/threading debt around active-camera switching
  and shared device pointers;
- schema/version validation for task config and imported bindings;
- QSS token mechanism and remaining UI cleanup.

## Do Not

- Do not use `docs/history/` as current design guidance.
- Do not treat `QMAKE_POST_LINK` build-folder copying as customer packaging.
- Do not mark roadmap items verified without build/test/manual evidence.
- Do not run destructive git or filesystem commands without explicit user
  permission.
- Do not create broad abstractions before a second concrete implementation
  proves the shape.
