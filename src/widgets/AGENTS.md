# Module: widgets (UI level)

**Purpose.** Reusable custom widgets: vision canvas/result viewer/ROI editor
(`vision/`), image widget + graphics items (`image_widget/`), property
browser integration (`property_browser/`, including the match-config
adapter), PLC monitor (`plc_widget/`), project/pattern trees, calibration
dialogs (`calibration/`), signal map/monitor widgets.

**May include.** Everything except `app/`. UI modules (`form`, `widgets`,
`libwg`) may include each other. Vendored property-browser headers are
included as `qtpropertybrowser/<name>` (from `3rdparty/`). Enforced by the
architecture contract test.

**Invariants.**
- Do not modify vendored code under `3rdparty/qtpropertybrowser` for app
  features; extend via `property_browser/` (custom managers, adapters).
- Widgets expose data via adapters (e.g. `vision_result_adapter`) instead
  of depending on runtime internals.
- Styling follows the QSS token system (`docs/rules/ui_theme_tokens.md`);
  delegate painting must re-read tokens on theme change.

**Verify.** Root app build + manual UI pass of affected widgets in both
themes.

**Build registration.** `src/widgets/widgets.pri` only.

**Docs.** `docs/domains/signal_map/`,
`docs/backlog/vision_widgets_implementation_plan.md`.
