# QSS Reusable Widget Candidates

Date: 2026-06-27

Scope: reviewed per-form QSS files under `resrc/styles/`, excluding the global
theme files `dark.qss` and `light.qss`.

Rule basis: `docs/rules/ui_design_rules.md` says reusable button/control
variants should become thin subclasses under `src/ui/widgets/controls/` and be styled
by class name in the global stylesheet, instead of repeating object-name based
rules in multiple per-form QSS files.

## Strong Candidates

### 1. Device connection controls

Files:

- `resrc/styles/mitsubishi_mc_device_widget_dark.qss`
- `resrc/styles/mitsubishi_mc_device_widget_light.qss`
- `resrc/styles/vision_tcpip_device_widget_dark.qss`
- `resrc/styles/vision_tcpip_device_widget_light.qss`
- `resrc/styles/vision_tcpip_client_device_widget_dark.qss`
- `resrc/styles/vision_tcpip_client_device_widget_light.qss`

Repeated selectors:

- `QLabel#lbl_conn_dot`
- `QLabel#lbl_conn_state`
- `QPushButton#btn_connect`
- `QLabel#lbl_send_hint` in both Vision TCP widgets

Why this should be promoted:

- The same connection-state visual contract is repeated across PLC and Vision
  output device widgets: `connected`, `disconnected`, and for the client widget
  also `connecting`.
- The same dynamic property name, `connectionState`, is already used by all
  three forms.
- The button base style and pressed/hover state rules are duplicated almost
  verbatim.
- Keeping this as object-name QSS makes future device widgets likely to copy the
  same selectors again.

Recommended design:

- Prefer a composite widget such as `DeviceConnectionControlsWidget` or
  `ConnectionStatusBar` that owns the dot, state label, and connect button.
- If a composite is too large for the first pass, introduce thin styled
  subclasses:
  - `ConnectionStateDot : QLabel`
  - `ConnectionStateLabel : QLabel`
  - `ConnectionActionButton : QPushButton`
  - `SendStateHintLabel : QLabel` for Vision output send readiness
- Move the shared QSS from the three per-form files into `dark.qss` and
  `light.qss` using class selectors, e.g. `ConnectionActionButton[...]`.
- Keep only truly form-specific spacing/layout in the per-form QSS files.

Suggested priority: High.

## Medium Candidates

### 2. Transparent/flat list widgets

Files:

- `resrc/styles/camera_mapping_widget_dark.qss`
- `resrc/styles/camera_mapping_widget_light.qss`
- `resrc/styles/signals_monitor_widget_dark.qss`
- `resrc/styles/signals_monitor_widget_light.qss`

Repeated selectors:

- `QListWidget#cmwList::item`
- `QListWidget#cmwList::item:selected`
- `QListWidget#smwList::item`
- `QListWidget#smwList::item:selected`
- `QListWidget#smwList::item:hover`

Why this should be promoted:

- Both widgets suppress default item chrome and selected background.
- This is a reusable visual variant of `QListWidget`, not form-specific domain
  styling.

Recommended design:

- Add a thin `FlatListWidget : QListWidget` or `TransparentListWidget :
  QListWidget`.
- Style item, selected, and hover states globally by class selector.
- Keep per-form delegates or row-content widgets separate.

Suggested priority: Medium. Do this when either list widget is touched next.

### 3. Compact token-backed combo boxes

Files:

- `resrc/styles/add_device_wizard_dark.qss`
- `resrc/styles/add_device_wizard_light.qss`
- `resrc/styles/system_log_form_dark.qss`
- `resrc/styles/system_log_form_light.qss`

Repeated selectors:

- `QStackedWidget#adwConfigStack QComboBox::drop-down`
- `QComboBox#cbx_view_mode::drop-down`

Why this may be promoted:

- Both forms remove the combo drop-down border and use a compact width.
- Width differs slightly (`18px` vs `22px`), so this is a variant candidate,
  not a direct one-to-one duplicate.

Recommended design:

- Add `CompactComboBox : QComboBox`, or use a dynamic property such as
  `comboDensity="compact"`.
- Style the common drop-down border globally; keep width as a size variant if
  both widths are intentionally different.

Suggested priority: Medium-low. Useful if more compact combos appear.

## Watchlist, Not Immediate Refactor

### 4. Localization task status/navigation lamps

Files:

- `resrc/styles/localization_task_widget_dark.qss`
- `resrc/styles/localization_task_widget_light.qss`

Selectors:

- `QFrame#statusDot`
- `QLabel#statusLabel`
- `QFrame#devNavDot`
- `QFrame[navItem="true"]`

Assessment:

- The state model (`lampState`) is reusable, but current duplication is mostly
  inside one form pair.
- Do not promote yet unless another screen needs the same lamp/dot/nav-item
  styling.

Possible future design:

- `StatusLamp : QFrame`
- `StatusTextLabel : QLabel`
- `DeviceNavItemWidget : QFrame`

### 5. Signals monitor chips

Files:

- `resrc/styles/signals_monitor_widget_dark.qss`
- `resrc/styles/signals_monitor_widget_light.qss`

Selectors:

- `QLabel#smwTypeChip`
- `QLabel#smwOnOff`

Assessment:

- These are good chip/badge patterns, but currently only live in one widget.
- Keep local for now. Promote to `TypeChipLabel` / `StatePillLabel` only when a
  second screen needs the same chip semantics.

## Not Candidates From This Pass

- `btn_run_match` in `localization_patterns_widget_*`: only appears in one
  form pair. It may become an `AccentToolButton` later, but the current QSS does
  not show enough cross-form duplication.
- `DevicesMonitorWidget` and `CameraMappingWidget` root transparency rules:
  these are single-purpose cleanup rules, not enough to justify a subclass by
  themselves.
