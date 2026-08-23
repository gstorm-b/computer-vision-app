# Module: core (level 0)

**Purpose.** Foundation shared by every module: application settings
(`app_settings/`), access control (`auth/`), file logger (`logger/`), theme
manager + QSS token resolver, the shell identity/hand-off helper, meta/gadget
helpers, settings keys, Windows API helpers (`utils/`).

**Public surface.** `app_settings/app_settings.h`, `auth/access_control.h`,
`auth/admin_credential_provider.h`, `logger/app_logger.h`,
`utils/theme_manager.h`, `utils/meta_utils.h`, `utils/shell_handoff.h`,
`utils/single_instance_guard.h`, `utils/windows_helper.h`, `qgadget_macro.h`,
`setting_keys.h`.

**May include.** Qt, standard library, other `core/` headers only.
**Must NOT include.** Any other module (`device/`, `matching/`, `model/`,
`runtime/`, `ui/`, `app/`). Enforced by
`tests/architecture_contract_test` (include-layering contract).

**Invariants.**
- `single_instance_guard.*` calls `AllowSetForegroundWindow()` on Windows, so
  `core.pri` links `user32`. `tests/architecture_contract_test` lists src sources
  individually rather than including the module `.pri` files, so it declares the
  same link dependency separately — keep the two in sync.
- `ThemeManager::resolveTokens()` substitutes `@{group.token}` placeholders in
  every loaded QSS sheet from the canonical `tokenTable()` in
  `theme_manager.cpp`. Token names/values are specified in
  `docs/rules/ui_theme_tokens.md` — keep both in sync.
- `vc::gadget_meta` (qgadget_macro.h) is the shared gadget meta-property
  helper; do not fork per-widget copies.
- **`AppSettings::filePath()` is product-scoped, never application-scoped.** Both
  shells share one `settings.dat`; deriving the path from
  `QCoreApplication::applicationName()` gave them one file each and is what made a
  theme chosen in the editor invisible to the runtime. Asserted by
  `test_settings_path_is_product_scoped_not_application_scoped`.
- **`auth/` never stores or returns a password.** `IAdminCredentialProvider` answers
  `verify(password)` and nothing else — no getter, no "show current password". That
  asymmetry is what lets a hardware dongle implement the same interface without ever
  handing a secret to the process. The settings-backed provider keeps a salted SHA-256
  hash with a fresh salt per write.
- **`AccessRole` starts at `Operator` in every process and is never persisted.** No
  "stay logged in": an unattended station left in Admin is the failure this exists to
  prevent.
- **`ShellHandoff::kInstanceKey` is ONE key for BOTH shells.** They own the same camera,
  PLC socket and output port, so only one of the two applications runs at a time. Do not
  reintroduce a per-shell key "so I can run them side by side" — that is the exact
  configuration whose symptoms look like hardware faults. Asserted by
  `test_both_shells_take_the_same_instance_key`.
- **`applicationName()` is not translated.** It is an identity: `QLockFile` records it and
  a losing launch compares against it to name the running shell. A name that changed with
  the UI language would break that in Japanese and nowhere else.

**Verify.** Root app build + `tests/architecture_contract_test`.

**Build registration.** Add new files to `src/core/core.pri` only. That `.pri` is
consumed by `src/src.pro`, which compiles every module **once** into the
`ncr_shared` static library that both shells link. No shell `.pro` lists module
sources, so a file added anywhere else is simply not built.
