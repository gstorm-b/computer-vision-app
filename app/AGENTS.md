# Module: app (application shell)

**Purpose.** Entry point and top-level composition: `main.cpp`,
`MainWindow` (docking layout, mode switching, menus, project lifecycle UI),
`SystemLogForm`, application translations (`translations/`).

**May include.** Every module — the shell is the top of the dependency
stack. Nothing outside `app/` may include app headers (enforced by the
architecture contract test).

**Invariants.**
- The first release ships ONE executable with explicit Commission and
  Runtime modes — no separate runtime app until the operator flow is
  validated (Phase 4 decision).
- Keep the shell thin: page/widget behavior belongs in `form`/`widgets`;
  domain logic in `model`/`runtime`. MainWindow wires things together.
- Translations: `translations/ncr_picking_ja_JP.ts`, embedded via
  `lrelease`/`embed_translations` (declared in `app/app.pri`).

**Verify.** Root app build (`build\msvc_debug`), then a manual smoke run:
app starts, docking layout restores, mode switch works.

**Build registration.** `app/app.pri` only.
