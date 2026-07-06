# Module: form (UI level)

**Purpose.** Dialogs, wizards, and task/device pages: add-device wizard,
pattern wizards + canvas, PLC/camera/vision-output device widgets,
localization task pages (`task/`), project dialogs, plus the small
composed controls under `form/widgets/`.

**May include.** Everything except `app/`. UI modules (`form`, `widgets`,
`libwg`) may include each other. Enforced by the architecture contract test.

**Invariants.**
- UI structure belongs in `.ui` files, behavior in `.cpp`, styling in
  `.qss`. Avoid inline `setStyleSheet()` unless
  `docs/rules/ui_design_rules.md` explicitly allows it.
- QSS colors use `@{group.token}` design tokens
  (`docs/rules/ui_theme_tokens.md`); do not hardcode new hex values.
- `DeviceWidgetFactory` dispatches per device subtype — a new device
  subtype must add its UI dispatch here (see AGENT.md guardrails).
- Task pages talk to `TaskLocalization` / `LocalizationRuntimeController`
  APIs; they must not reach into devices directly.

**Verify.** Root app build + manual UI pass of the affected page
(Commission and Runtime modes, light/dark themes).

**Build registration.** `src/form/form.pri` only.

**Open question (C6).** `form/widgets` vs `widgets/` vs `libwg/` overlap;
a future consolidation into a single `src/ui/` tree is on the backlog —
do not start it as a side effect of another change.
