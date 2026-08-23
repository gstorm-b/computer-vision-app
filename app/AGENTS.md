# Module: app (application shell)

**Purpose.** Entry point and top-level composition for the **commissioning**
application `ncr_picking.exe`: `main.cpp`, `MainWindow` (docking layout, mode
switching, menus, project lifecycle UI), `SystemLogForm`, application
translations (`translations/`).

**May include.** Every `src/` module — the shell is the top of the dependency
stack. It may **not** include `runtime_app/` headers, and nothing outside `app/`
may include app headers (enforced by the architecture contract test).

**Invariants.**
- ~~The first release ships ONE executable with explicit Commission and
  Runtime modes — no separate runtime app until the operator flow is
  validated (Phase 4 decision).~~
  **Superseded by Phase 6 (2026-08-19, project owner's decision):** the operator
  runtime now ships as a second executable, `runtime_app/ncr_runtime.pro`. This
  shell keeps its explicit Commission and Runtime modes and remains the only one
  that can edit and save a project. Rationale and shell boundaries:
  [../docs/domains/runtime_app/runtime_shell.md](../docs/domains/runtime_app/runtime_shell.md).
- Keep the shell thin: page/widget behavior belongs in `form`/`widgets`;
  domain logic in `model`/`runtime`. MainWindow wires things together.
- Translations: `translations/ncr_picking_ja_JP.ts`, embedded via
  `lrelease`/`embed_translations` (declared in `app/app.pri`).

**Verify.** Build intermediates go to `build\<build-name>` (or `build\all\<build-name>`
through the umbrella); the executable and its runtime always land in
`build\bin\<config>` beside `ncr_runtime.exe`. Then a manual smoke run: app
starts, docking layout restores, mode switch works.

A change under `src/` affects **both** shells, so prefer `ncr_picking_all.pro` —
building this one alone leaves the runtime shell's executable in `build\bin\`
untouched and stale against the same library.

**Build registration.** `app/app.pri` only. Shared qmake config (Qt modules,
`src/*.pri`, ADS, RobotKinematics, resources, deployment) comes from
`qmake/app_common.pri` and is shared with `runtime_app/` — do not copy it back
into `ncr_picking.pro`.
