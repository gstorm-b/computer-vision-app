# QSS Reusable Widget Implementation Plan

Date: 2026-06-27

Owner: next implementation agent

Source audit: `docs/backlog/qss_reusable_widget_candidates.md`

Design authority: `docs/rules/ui_design_rules.md`

Goal: remove repeated per-form QSS for reusable controls by promoting them to
thin widget subclasses under `src/form/widgets/` and styling them once in the
global theme files `resrc/styles/dark.qss` and `resrc/styles/light.qss`.

## Ground Rules

- Keep `.ui` files structural only. Do not add inline `styleSheet` properties.
- Prefer class selectors in global QSS for reusable variants.
- Keep dynamic state as properties, then repolish after property changes.
- Keep per-form QSS only for layout-local selectors that cannot be globalized.
- Preserve both dark and light behaviour in the same change.
- Register any new source/header files in the relevant qmake project files.

## Phase 1 - Device Connection Controls

Priority: High

Status: Completed on 2026-06-27.
Verification: user-reported app rebuild/compile passed after implementation.

Widgets to introduce:

- `ConnectionStateDot : QLabel`
- `ConnectionStateLabel : QLabel`
- `ConnectionActionButton : QPushButton`
- `SendStateHintLabel : QLabel`

Recommended location:

- `src/form/widgets/connection_state_dot.h/.cpp`
- `src/form/widgets/connection_state_label.h/.cpp`
- `src/form/widgets/connection_action_button.h/.cpp`
- `src/form/widgets/send_state_hint_label.h/.cpp`

Affected forms:

- `src/form/plc/mitsubishi_mc_device_widget.ui`
- `src/form/vision_output/vision_tcpip_device_widget.ui`
- `src/form/vision_output/vision_tcpip_client_device_widget.ui`

Affected QSS:

- `resrc/styles/mitsubishi_mc_device_widget_dark.qss`
- `resrc/styles/mitsubishi_mc_device_widget_light.qss`
- `resrc/styles/vision_tcpip_device_widget_dark.qss`
- `resrc/styles/vision_tcpip_device_widget_light.qss`
- `resrc/styles/vision_tcpip_client_device_widget_dark.qss`
- `resrc/styles/vision_tcpip_client_device_widget_light.qss`
- `resrc/styles/dark.qss`
- `resrc/styles/light.qss`

Implementation steps:

1. Add the four thin subclasses. They should not own business logic; they exist
   to provide stable class names for QSS.
2. Promote existing widgets in the three `.ui` files:
   - `lbl_conn_dot` -> `ConnectionStateDot`
   - `lbl_conn_state` -> `ConnectionStateLabel`
   - `btn_connect` -> `ConnectionActionButton`
   - `lbl_send_hint` -> `SendStateHintLabel` in the two Vision TCP widgets
3. Keep the existing object names so current C++ code keeps compiling.
4. Move repeated rules from per-form QSS into `dark.qss` and `light.qss` using
   class selectors:
   - `ConnectionStateDot`
   - `ConnectionStateDot[connectionState="connected"]`
   - `ConnectionStateDot[connectionState="connecting"]`
   - `ConnectionStateDot[connectionState="disconnected"]`
   - `ConnectionStateLabel[...]`
   - `ConnectionActionButton[...]`
   - `SendStateHintLabel[sendState="ready"]`
   - `SendStateHintLabel[sendState="idle"]`
5. Remove the duplicated object-name selectors from the three per-form QSS
   pairs.
6. Check all existing `setProperty("connectionState", ...)` and
   `setProperty("sendState", ...)` paths still repolish after property changes.
7. If the `connecting` state is not used by Mitsubishi or server TCP widgets,
   the global rule may still support it; unused state support is acceptable.

Acceptance checks:

- The three device widgets build after `.ui` promotion.
- Connect button, state label, and dot still update for connected/disconnected.
- TCP client still renders the connecting state.
- Vision send hint still changes between ready/idle.
- There is no `QLabel#lbl_conn_dot`, `QLabel#lbl_conn_state`,
  `QPushButton#btn_connect`, or `QLabel#lbl_send_hint` selector left in the
  three per-form QSS files unless it is genuinely form-specific and documented.

## Phase 2 - Flat List Widget

Priority: Medium

Status: Completed on 2026-06-27.
Verification: user-reported app rebuild/compile passed after implementation.

Widget to introduce:

- `FlatListWidget : QListWidget`

Recommended location:

- `src/form/widgets/flat_list_widget.h/.cpp`

Affected widgets:

- `src/widgets/camera_mapping_widget.*`
- `src/widgets/signals_monitor_widget.*`

Affected QSS:

- `resrc/styles/camera_mapping_widget_dark.qss`
- `resrc/styles/camera_mapping_widget_light.qss`
- `resrc/styles/signals_monitor_widget_dark.qss`
- `resrc/styles/signals_monitor_widget_light.qss`
- `resrc/styles/dark.qss`
- `resrc/styles/light.qss`

Implementation steps:

1. Add `FlatListWidget`.
2. Promote `cmwList` in the camera mapping `.ui` to `FlatListWidget`.
3. For `SignalsMonitorWidget`, inspect how `smwList` is created:
   - If it is in `.ui`, promote it there.
   - If it is code-created, instantiate `FlatListWidget` directly.
4. Move common item rules into global QSS:
   - `FlatListWidget::item`
   - `FlatListWidget::item:selected`
   - `FlatListWidget::item:hover`
5. Keep row-specific widgets/delegates in their current classes.
6. Remove the duplicated `QListWidget#cmwList::item`,
   `QListWidget#smwList::item`, selected, and hover rules from per-form QSS.

Acceptance checks:

- Camera mapping rows keep their existing spacing and selection visuals.
- Signals monitor rows keep transparent selected/hover behaviour.
- Per-form QSS no longer carries generic flat-list item chrome.

## Phase 3 - Compact Combo Box

Priority: Medium-low

Status: Completed on 2026-06-27.
Verification: user-reported app rebuild/compile passed after implementation.

Widget to introduce:

- `CompactComboBox : QComboBox`

Recommended location:

- `src/form/widgets/compact_combo_box.h/.cpp`

Affected forms:

- `src/form/add_device_wizard.ui`
- `app/system_log_form.ui`

Affected QSS:

- `resrc/styles/add_device_wizard_dark.qss`
- `resrc/styles/add_device_wizard_light.qss`
- `resrc/styles/system_log_form_dark.qss`
- `resrc/styles/system_log_form_light.qss`
- `resrc/styles/dark.qss`
- `resrc/styles/light.qss`

Implementation steps:

1. Add `CompactComboBox`.
2. Promote `cbx_view_mode` in `system_log_form.ui`.
3. Inspect `add_device_wizard.ui` for combo boxes under `adwConfigStack`.
   Promote only combos that should use the compact density.
4. Move common drop-down border removal into global QSS:
   - `CompactComboBox::drop-down { border: none; }`
5. Decide whether the width difference is intentional:
   - If not intentional, use one width globally.
   - If intentional, add a dynamic property such as `comboDensity="compact18"`
     or keep a documented per-form width override.
6. Remove duplicated drop-down border rules from per-form QSS.

Acceptance checks:

- System log view mode combo still renders correctly in both themes.
- Add-device wizard combo boxes still fit inside their config stack.
- There is no new inline styling in `.ui` or C++.

## Phase 4 - Status Lamps and Device Nav Items

Priority: Watchlist at planning time. Implemented on 2026-06-27 after owner
confirmation.

Status: Completed on 2026-06-27.
Verification: user-reported app rebuild/compile passed after implementation.

Implementation note:

- The codebase already had a separate composite `StatusLamp` widget under
  `src/form/widgets/status_lamp.*` before this phase.
- This phase therefore promoted the remaining raw controls in
  `LocalizationTaskWidget` as:
  - `StatusLampDot : QFrame`
  - `StatusTextLabel : QLabel`
  - `DeviceNavItemWidget : QFrame`
  - `DeviceNavDot : QFrame`
- This keeps the existing composite `StatusLamp` untouched and avoids naming
  collision with the older widget.

Affected files:

- `src/form/task/localization_task_widget.cpp`
- `src/form/task/localization_task_widget.h`
- `src/form/widgets/status_lamp_dot.h/.cpp`
- `src/form/widgets/status_text_label.h/.cpp`
- `src/form/widgets/device_nav_item_widget.h/.cpp`
- `src/form/widgets/device_nav_dot.h/.cpp`
- `resrc/styles/localization_task_widget_dark.qss`
- `resrc/styles/localization_task_widget_light.qss`
- `resrc/styles/dark.qss`
- `resrc/styles/light.qss`
- `ncr_picking.pro`

Implementation steps:

1. Because these controls are currently code-created, replace raw `QFrame` and
   `QLabel` creation with the new subclasses where appropriate.
2. Keep existing dynamic properties:
   - `lampState`
   - `navItem`
   - `navActive`
   - `deviceType`
3. Move reusable lamp and nav-item rules into global QSS by class selector.
4. Keep form-specific device-nav layout and any selectors tied to local label
   names in `localization_task_widget_*` until another form needs them.

Acceptance checks:

- Dashboard status lamps still show on/off/warn/error correctly.
- Device nav active/hover/type colours still match current screenshots.
- No layout shift in the localization task sidebar.

## Phase 5 - Signals Monitor Chips

Priority: Watchlist at planning time. Implemented on 2026-06-27 after owner
confirmation.

Status: Completed on 2026-06-27.
Verification: user-reported app rebuild/compile passed after implementation.

Widgets introduced:

- `TypeChipLabel : QLabel`
- `StatePillLabel : QLabel`

Affected files:

- `src/widgets/signals_monitor_widget.cpp`
- `src/form/widgets/type_chip_label.h/.cpp`
- `src/form/widgets/state_pill_label.h/.cpp`
- `resrc/styles/signals_monitor_widget_dark.qss`
- `resrc/styles/signals_monitor_widget_light.qss`
- `resrc/styles/dark.qss`
- `resrc/styles/light.qss`
- `ncr_picking.pro`

Implementation steps:

1. Replace code-created `QLabel` instances for `smwTypeChip` and `smwOnOff`
   with subclasses.
2. Keep existing object names and dynamic properties:
   - `typeKind`
   - `onOffState`
3. Move reusable chip/pill rules into global QSS.
4. Keep signal-row layout rules in the per-form QSS.

Acceptance checks:

- Bool/number type chips keep their colours.
- On/off pills keep their colours and sizing.
- Signals monitor row rendering remains unchanged in both themes.

## Final Verification

Run after each phase:

- `rg -n "setStyleSheet\\(" src` and confirm no new literal styling was added.
- `rg -n "QLabel#lbl_conn_dot|QLabel#lbl_conn_state|QPushButton#btn_connect|QLabel#lbl_send_hint" resrc/styles -g "*device_widget_*.qss"` after Phase 1.
- `rg -n "QListWidget#(cmwList|smwList)::item" resrc/styles -g "*.qss"` after Phase 2.
- `rg -n "QComboBox#cbx_view_mode::drop-down|adwConfigStack QComboBox::drop-down" resrc/styles -g "*.qss"` after Phase 3.
- `rg -n "QFrame#statusDot|QLabel#statusLabel|QFrame#devNavDot|QFrame\\[navItem=\\\"true\\\"\\]" resrc/styles -g "localization_task_widget_*.qss"` after Phase 4.
- `rg -n "QLabel#smwTypeChip|QLabel#smwOnOff" resrc/styles -g "signals_monitor_widget_*.qss"` after Phase 5.
- qmake generator for the app.
- Build the app if local env vars are configured.
- Manually inspect dark and light themes for the touched widgets.

## Suggested Delivery Order

1. Phase 1 only, as a focused PR/change. This removes the biggest duplication
   and validates the subclass/global-QSS pattern.
2. Phase 2 and Phase 3 together if Phase 1 is stable.
3. Phase 4 and Phase 5 only after confirming the team wants to promote
   single-form patterns now rather than waiting for a second consumer.
4. Current status: all five phases in this plan have now been implemented.
5. Latest verification status: user rebuilt the app after Phase 4-5 and
   reported no compile errors.
