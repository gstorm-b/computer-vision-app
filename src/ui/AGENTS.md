# Module: ui (UI level)

**Purpose.** The entire user interface. Merged (2026-07) from the former
`form`, `widgets`, and `libwg` modules into one module with two subtrees:

- `forms/` — dialogs, wizards, and device/task configuration pages.
  Domain subgroups: `camera/`, `pattern/`, `plc/`, `task/`,
  `vision_output/`, plus top-level dialogs (add-device wizard, new
  project/task, project info).
- `widgets/` — reusable custom widgets. Domain subgroups: `vision/`
  (canvas, result viewer, ROI editor), `image_widget/` (image view +
  graphics items), `property_browser/` (property browser + custom managers +
  match-config adapter), `plc_widget/` (device monitor), `calibration/`
  (calibration dialogs/table). Also `controls/` (small composed
  status/connection controls, formerly `form/widgets`) and generic
  primitives at the top level (`group_frame`, `validating_line_edit`,
  `no_wheel_*`, `clamp`, `lamp_button`, ...).

**Where does X go?** A dialog/wizard/page → `ui/forms/`. A reusable widget →
`ui/widgets/` (in a domain subgroup if one fits; `controls/` for small
status/connection controls). This two-rule split replaces the old
form/widgets/form-widgets/libwg overlap.

**May include.** Qt, OpenCV, every non-UI module (`core`, `device`,
`calibration`, `matching`, `model`, `runtime`), other `ui/` headers, and the
vendored property browser as `qtpropertybrowser/<name>` (from `3rdparty/`).
**Must NOT be included by** any non-UI module — the UI is the top of `src/`,
below only the `app` shell. Enforced by
`tests/architecture_contract_test::test_module_include_layering_contract`.

**Invariants.**
- UI structure in `.ui`, behavior in `.cpp`, styling in `.qss`. Avoid inline
  `setStyleSheet()` unless `docs/rules/ui_design_rules.md` allows it.
- QSS colors use `@{group.token}` design tokens
  (`docs/rules/ui_theme_tokens.md`); do not hardcode hex. Delegate painting
  must re-read tokens on theme change.
- Do not modify vendored `3rdparty/qtpropertybrowser`; extend via
  `widgets/property_browser/` (custom managers, adapters).
- `DeviceWidgetFactory` dispatches per device subtype — a new device subtype
  must add its UI dispatch here (see AGENT.md guardrails).
- Pages talk to `TaskLocalization` / `LocalizationRuntimeController` APIs;
  they must not reach into devices directly (use runtime runners).

**Verify.** Root app build (`build\msvc_debug`) + a manual UI pass of the
affected surface in both Commission and Runtime modes and both themes.

**Build registration.** `src/ui/ui.pri` only. New files: add to the matching
`SOURCES`/`HEADERS`/`FORMS` list. That `.pri` is consumed by `src/src.pro`, which
compiles every module **once** into the `ncr_shared` static library that both
shells link. No shell `.pro` lists module sources, so a file added anywhere else
is simply not built.

**Resources are the exception, and moving them here breaks things silently.**
Every `.qrc` — `resrc.qrc` (icons, QSS themes), `ads.qrc`, `qtpropertybrowser.qrc`
— is listed at **shell** level in `qmake/app_common.pri`, never in a module `.pri`
and never in `src/src.pro`. Qt registers a resource through a static initialiser
in the generated `qrc_*.cpp`; inside a static library nothing references a symbol
in that object file, so the linker drops it and the resource **does not exist at
runtime**. Icons, the QSS theme and `:/i18n` all resolve to nothing, with **no
build error and no warning** — you find out by looking at an unstyled window.

Library code may freely *use* those resources: the Qt resource system is
process-global, so what the executable registers is visible here. Only the
registration has to stay in the shells. `src/src.pro` ends with a bare
`RESOURCES =` that takes back any `.qrc` an included `.pri` added on its own; do
not "tidy" that away either.

**Docs.** `docs/rules/ui_design_rules.md`, `docs/rules/ui_theme_tokens.md`,
`docs/domains/signal_map/`,
`docs/backlog/vision_widgets_implementation_plan.md`.
