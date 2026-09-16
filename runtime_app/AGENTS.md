# Module: runtime_app (operator runtime shell)

**Purpose.** Entry point and top-level composition for `ncr_runtime.exe`, the
operator-facing runtime.

```
runtime_app/
  src/   main.cpp, RuntimeLayoutController      entry point and controllers
  ui/    RuntimeShellWindow (.h/.cpp/.ui)       the window and its form
```

**Includes are rooted at the REPOSITORY, not at this folder** — `#include
"runtime_app/ui/runtime_shell_window.h"`. Writing `"ui/..."` instead would shadow
`src/ui/`, which is already on the include path; which header won would depend on
INCLUDEPATH order, and the contract test would read it as a reference to the
`src/ui` module, so the ambiguity would not even be reported. Asserted by
`test_runtime_shell_is_structured_and_form_driven`.

**May include.** Every `src/` module — this is a shell, top of the dependency
stack. It may **not** include `components/app/` headers, and `components/app/` may not
include these.
The two shells are peers over the same modules, not a hierarchy (enforced by
`tests/architecture_contract_test::test_module_include_layering_contract`).

**Invariants.**
- **Read-only with respect to the project file.** This shell loads and runs; it
  never saves. Commissioning edits belong to `ncr_picking.exe`.
- **No commission affordance.** Reuse `LocalizationDashboardWidget`, never
  `LocalizationTaskWidget` — the latter starts Commission on the task in its
  constructor, which is the opposite of what this executable is for.
- **Tasks enter runtime without a button press.** An operator starts the machine,
  not the software.
- **One process for the whole product.** `SingleInstanceGuard` (in
  `src/core/utils/`) blocks a second launch and raises the running window
  instead. Device ownership is exclusive, and a second process fails in ways that
  look like hardware faults. Since Phase 7 the key is **shared with
  `ncr_picking.exe`** (`vc::shell::ShellHandoff::kInstanceKey`), so the two shells
  exclude each other too — Phase 6's separate keys are gone. Switching is the
  ordered hand-off in `ShellHandoff`, never two processes overlapping.
- **A switch releases the devices before it launches the sibling, or it does not
  switch.** `stopTaskRuntimes()` runs first; the sibling only starts if it
  succeeded. Reversing that order hands a half-released camera to the next
  process, and the symptoms of that are indistinguishable from a hardware fault.
- **Reaching commissioning requires the `Admin` role.** A line worker must not get
  to device and pattern configuration by mis-tapping. Leaving commissioning does
  not require anything — handing authority back is not an escalation.
- **The startup path must never be a dead end.** This shell auto-starts on a
  field machine and owns the screen, so every failure to load a project has to
  land on the project-select page with a message naming the cause — never a
  crash, never a blank window. Protect that property when touching startup.
- **Never hide a task.** A task that fails setup keeps its tile and shows Faulted;
  tasks beyond the 8-tile cap are named in the log and in the status bar. A
  running task nobody can see is worse than a crowded screen.
- **Window structure is authored in `ui/runtime_shell_window.ui`.** Phase 6 shipped
  this window with its whole widget tree `new`-ed in the `.cpp`, against
  `docs/rules/ui_design_rules.md` Rule 1.1, and nobody noticed for a phase. Phase 7
  fixed it and made the rule mechanical: the contract test fails on
  `new QVBoxLayout`/`QStackedWidget`/`QToolBar`/… anywhere under `runtime_app/`.
  The ADS dock manager is the exception — it is not a Designer widget, so it is
  constructed into the form's `wg_dock` host, exactly as `components/app/mainwindow.cpp` does.
- Keep the shell thin: dashboard behaviour lives in `src/ui/`, domain logic in
  `src/model/` and `src/runtime/`.
- **`runtime_app.pri` declares `EXTRA_TRANSLATIONS`, never `TRANSLATIONS`.** Both
  are released and embedded; only `TRANSLATIONS` is updated by lupdate, from the
  declaring project's own sources — and this shell compiles three files. Declaring
  it here marks the rest of the product's strings as vanished, silently. The file
  is owned by `translations/ncr_translations.pro`, which sees every source.
  Guarded by the contract test.

**Verify.** Build *intermediates* go to `build/runtime_build/<build-name>` — one
subfolder per target under the repo-root `build/`, never sharing a directory with
the other shell (they collide on `Makefile`, `.qmake.stash` and generated moc/ui
files). The *executable* lands in `build/bin/<config>` beside `ncr_picking.exe`
with one shared runtime set; that is `DESTDIR` and it is deliberately the same
folder for both shells.

A change under `src/` affects both shells, so prefer `ncr_picking_all.pro` —
building this one alone leaves the editor's executable in `build/bin/` stale
against the same library.

Then a manual smoke run: first launch shows the project-select page; second
launch reloads the remembered project straight into the runtime view; a moved
project file reports that specifically.

**Build registration.** `runtime_app/runtime_app.pri` only — including the
`.ui` under `FORMS`, without which the form is not compiled and the window falls
back to being empty rather than failing to build. Shared qmake config
(Qt modules, `src/*.pri`, ADS, RobotKinematics, resources, deployment) comes from
`qmake/app_common.pri` and must not be copied here.

**Docs.** [docs/domains/runtime_app/runtime_shell.md](../docs/domains/runtime_app/runtime_shell.md).
