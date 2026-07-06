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
`SOURCES`/`HEADERS`/`FORMS` list.

**Docs.** `docs/rules/ui_design_rules.md`, `docs/rules/ui_theme_tokens.md`,
`docs/domains/signal_map/`,
`docs/backlog/vision_widgets_implementation_plan.md`.
