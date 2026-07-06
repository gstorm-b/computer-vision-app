# Module: core (level 0)

**Purpose.** Foundation shared by every module: application settings
(`app_settings/`), file logger (`logger/`), theme manager + QSS token
resolver, meta/gadget helpers, settings keys, Windows API helpers (`utils/`).

**Public surface.** `app_settings/app_settings.h`, `logger/app_logger.h`,
`utils/theme_manager.h`, `utils/meta_utils.h`, `utils/windows_helper.h`,
`qgadget_macro.h`, `setting_keys.h`.

**May include.** Qt, standard library, other `core/` headers only.
**Must NOT include.** Any other module (`device/`, `matching/`, `model/`,
`runtime/`, `ui/`, `app/`). Enforced by
`tests/architecture_contract_test` (include-layering contract).

**Invariants.**
- `ThemeManager::resolveTokens()` substitutes `@{group.token}` placeholders in
  every loaded QSS sheet from the canonical `tokenTable()` in
  `theme_manager.cpp`. Token names/values are specified in
  `docs/rules/ui_theme_tokens.md` — keep both in sync.
- `vc::gadget_meta` (qgadget_macro.h) is the shared gadget meta-property
  helper; do not fork per-widget copies.

**Verify.** Root app build + `tests/architecture_contract_test`.

**Build registration.** Add new files to `src/core/core.pri` only.
