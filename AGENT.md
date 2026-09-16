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
    generated API reference, and `docs/rules/doc_comment_style.md` for the
    Doxygen comment mechanics (`/** */`, `///`, `///<`, explicit tags) all
    class/method/member doc comments must follow (supersedes design_rules.md §18).
3b. `docs/decisions/` for the owner's standing design rulings (`DR-xxxx`); read
    the index before proposing a behaviour change. `docs/plan/` holds the active
    phase plan.
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
   - owner design rulings: `docs/decisions/`.
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
  - **One approved exception exists:** the pattern-library JSON read accepts the
    two older gripper-geometry shapes (schema v0 and v1) in
    `src/matching/pattern_group_manager.cpp`. Real commissioned picking geometry
    lives in saved projects, so dropping it would mean re-teaching parts on the
    line. It is agreed with the project owner — do not "clean it up". Rationale
    and the full schema history:
    [docs/domains/task_localization/pick_geometry_and_output_contract.md](docs/domains/task_localization/pick_geometry_and_output_contract.md).

## Module Map And Scope Cards

The build is one `.pri` per module; every module folder carries an
`AGENTS.md` scope card (purpose, allowed dependencies, invariants, verify
commands). Read the scope card before editing a module, and register new
files in that module's `.pri` — never in a shell `.pro`.

Those module `.pri` files are consumed by **`src/src.pro`**, which compiles all of
`src/` once into the `ncr_shared` static library that both shells link. A shell
`.pro` no longer lists module sources at all, so a file registered anywhere but
its module `.pri` is not built.

**Resources are the one thing that must NOT move into the library.** Every `.qrc`
is listed at shell level in `qmake/app_common.pri`. A `.qrc` compiled into a
static library is dropped by the linker — nothing references a symbol in the
generated `qrc_*.cpp` — so icons, the QSS theme and `:/i18n` silently resolve to
nothing at runtime, with no build error. Library code may still *use* them; the
resource system is process-global.

| Module | Path | Level |
|---|---|---|
| core (settings/logger/utils) | `src/core/` | 0 |
| device | `src/device/` | 1 |
| calibration | `src/calibration/` | 1 |
| matching | `src/matching/` | 1 |
| model | `src/model/` | 2 |
| runtime | `src/runtime/` | 2 |
| ui (forms + widgets) | `src/ui/` | UI |
| app shell — commissioning (+translations) | `components/app/` | top |
| runtime shell — operator | `runtime_app/` | top |

Include-layering rules (enforced by
`tests/architecture_contract_test::test_module_include_layering_contract`):
lower levels must not include higher ones; `model` and `runtime` may include
each other; `device` may include `calibration` (cameras own a Calibrator);
`ui` may include every non-UI module; the two shells (`components/app/`, `runtime_app/`)
may each include everything **except each other** — they are peers over the same
modules, not a hierarchy; no `../` escapes and no `src/`-prefixed quoted includes
(module includes are rooted at `src/`, e.g. `core/...`, `ui/widgets/...`).

Shared qmake configuration is split in two: `qmake/common_deps.pri` holds what is
needed to **compile** repository code (Qt modules, include roots, OpenCV, Pylon,
ADS, RobotKinematics) and is included by the static library *and* both shells;
`qmake/app_common.pri` holds what only an **executable** can have (resources, the
file version, `DESTDIR`, the link against `ncr_shared`, dependency deployment). A
shell `.pro` adds only its own shell `.pri`, icon and install rules — never a
second copy of either.

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

The one sanctioned exception is `qmake/local_paths.pri`, which is **untracked**
(template: `qmake/local_paths.pri.example`). qmake reads it directly, so a build
from Qt Creator and a build from a terminal resolve the same OpenCV and Pylon
installs without either side exporting anything — an IDE inherits the environment
of whatever launched it, which is how two toolchains end up on two different
installs unnoticed. It is a **fallback**, applied only after the qmake command line
and the environment, so the rule above still holds for everything tracked.

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
- `PUREGEV_ROOT` for the JAI/Pleora eBUS SDK — set by its own installer, and
  normally the only thing needed; `EBUS_INCLUDE_DIR`, `EBUS_LIB_DIR` and
  `EBUS_RUNTIME_DIR` override it for a non-default install
- `VCTOOLS_DEBUG_CRT_DIR` when running Debug test binaries outside Visual Studio
- third-party/component roots such as RobotKinematics dependencies

Important:

- Call `vcvars64.bat` directly. Do not redirect its output.
- Run `qmake` from the build directory before compiling.
- **Both executables are linked into one folder: `build\bin\<config>`**, together
  with a single shared runtime set and `robot_assets\`. That is `DESTDIR`, set
  once in `qmake/app_common.pri`. Build layout and deploy layout are different
  questions: intermediates must stay apart, binaries must come together.
- Intermediates follow the `.pro` owner:
  - **both shells at once (preferred): `ncr_picking_all.pro` →
    `%NCR_PICKING_ROOT%\build\all\<build-name>`.** Use the same directory from
    the CLI and from Qt Creator so they share one incremental state;
  - shared static library `src\src.pro`: `ncr_shared.lib` →
    `%NCR_PICKING_ROOT%\build\shared_lib\<config>`;
  - app shell `ncr_picking.pro`: `%NCR_PICKING_ROOT%\build\<build-name>`;
  - runtime shell `runtime_app\ncr_runtime.pro`:
    `%NCR_PICKING_ROOT%\build\runtime_build\<build-name>`;
  - tests: `build\<build-name>` next to the test `.pro`;
  - examples/components: `build\<build-name>` next to that `.pro`.
- Do not put test/example/component build outputs under root `build\`.
  Root `build\` holds the umbrella build plus one subfolder per individually
  built shell, and root release/package verification. Two shells must never
  share a build directory **when built individually** — they collide on
  `Makefile`, `.qmake.stash` and the generated moc/ui files. The umbrella is the
  safe way to share one directory: qmake gives each subproject its own uniquely
  named makefile.
- A change under `src/` affects both shells. Build through
  `ncr_picking_all.pro` so one cannot be left stale against the other.
- A build directory is not an install image: it mixes intermediates with the
  deployed runtime. Ship from `dist\` — see
  `docs/rules/build_and_verification.md` and `docs/product/install_image.md`.
- For `tests/architecture_contract_test`, run
  `nmake /nologo -f Makefile.Debug compiler_moc_source_make_all` before the
  normal `nmake /nologo`.
- Use `QT_QPA_PLATFORM=minimal` when running Qt tests headlessly.
- Parallel compilation is **opt-in**: add `CONFIG+=multicore` at qmake time for
  MSVC `/MP` (measured 10m50s → 4m08s on a 16-core machine, clean rebuild). Off by
  default because it depends on the machine having RAM to match its cores, and it
  oversubscribes when combined with `jom`. Cap it with `NCR_JOBS=<n>`. See
  `qmake/multicore.pri`.
- Qt Creator may use `jom`, but from Codex CLI use the qmake + nmake fallback
  unless the `jom` PATH inheritance issue has been solved.

See `docs/rules/build_and_verification.md` for exact commands.

## Current Release Decision

**Phase 4 is ON HOLD as of 2026-07-28.** The product needs further development
before a first release is meaningful, so the release track is deferred: no
customer installer work, no release-candidate hardening, and the Phase 4 release
gate is not a current acceptance criterion. Do not start packaging or
release-gate work because the rest of the backlog looks clear — resuming
Phase 4 is a product decision by the user.

The release *shape* Phase 4 chose still stands and should not be re-litigated
when the track resumes, **except for the runtime-executable decision, which
Phase 6 superseded on 2026-08-19 by the project owner's explicit choice**:

- ~~one `ncr_picking.exe`~~ → **two executables**: `ncr_picking.exe`
  (commissioning) and `ncr_runtime.exe` (operator runtime), built from the same
  `src/` modules through `qmake/app_common.pri`;
- explicit Commission and Runtime modes;
- ~~no separate runtime executable until the operator flow is validated~~ →
  the separate runtime executable exists as of Phase 6.

The original reasoning is kept above rather than deleted: it is still the right
default for a single-shell product, and a future agent should understand that
the split was a deliberate decision, not drift. Rationale, shell boundaries and
the operator startup flow:
[docs/domains/runtime_app/runtime_shell.md](docs/domains/runtime_app/runtime_shell.md).

Build-folder deployment copies RobotKinematics DLLs and `robot_assets/`. That is
a developer convenience, not packaging, and a customer installer will still need
its own manifest and clean-machine smoke test whenever the track reopens.

See `docs/product/phase4_product_release_plan.md` and the "Release Track Status"
section of `docs/backlog/technical_debt_and_next_steps.md`.

## Highest Priority Next Work

Start with `docs/backlog/technical_debt_and_next_steps.md`, skipping anything
marked on hold. Active slices are:

- Phase 10 (runtime core redesign): start from `docs/plan/phase_10/` — the
  charter is published there at Checkpoint 0; until then the working draft is
  `temp_docs/02_phase10_charter_and_work_breakdown.md`. Phase 9 is closed with
  carried items; see `docs/backlog/technical_debt_and_next_steps.md`.
- feature development toward the capabilities the product still lacks — this is
  the reason Phase 4 was deferred, and the user sets its scope;
- runtime matching latency and UI responsiveness measurement, which feeds the
  threading-model revisit criteria;
- Doxygen doc-comment sweep across headers, following
  `docs/rules/doc_comment_style.md`;
- QSS token mechanism and remaining UI cleanup.

Operator runtime validation stays useful as development validation whenever the
runtime path is touched, but it is no longer a release gate.

## Do Not

- Do not use `docs/history/` as current design guidance.
- Do not treat `QMAKE_POST_LINK` build-folder copying as customer packaging.
- Do not mark roadmap items verified without build/test/manual evidence.
- Do not run destructive git or filesystem commands without explicit user
  permission.
- Do not create broad abstractions before a second concrete implementation
  proves the shape.
