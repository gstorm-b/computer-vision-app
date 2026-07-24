# Figma UI design — `ncr_picking`

This folder is a Figma-style mockup set for `ncr_picking`. The screens are
authored as standalone SVGs so they can be opened in any browser, dragged into
Figma, or imported as frames without a runtime.

The work is organised into two phases:

- `current/` — what the app actually looks like today, reverse-engineered from
  the `.ui`, `.qss`, and theme-token sources of truth.
- `proposed/` — improvements layered on top of the current Hybrid theme. Tokens,
  identity, and the QSS/`ThemeManager` architecture are preserved; only
  structure, density, and a few new primitives change.

## File map

```
figma/
├── current/
│   ├── 00_design_system.svg           Tokens, type, components as-shipped
│   ├── 01_main_window_dashboard.svg   MainWindow + Localization Dashboard
│   ├── 02_patterns_view.svg           Localization Patterns view
│   └── 03_settings_and_wizard.svg     Localization Settings + Add Device wizard
├── proposed/
│   ├── 00_design_system_v2.svg        Type scale, 4-pt grid, density, status, KPI, focus
│   ├── 01_main_window_dashboard_v2.svg Command-bar shell + redesigned Dashboard
│   ├── 02_patterns_view_v2.svg        Inspector-tabs Patterns view with diff overlay
│   ├── 03_runtime_operator_mode.svg   New 1920×1080 Runtime/operator screen with RunBar
│   └── 04_command_palette_and_wizard_v2.svg  Ctrl+K palette + Smart-form device add
└── README.md
```

## Source of truth references

The current-state SVGs were drawn against these in-tree files. They are listed
so anyone updating the mocks can keep them in sync.

| Mock | Code source |
|---|---|
| `current/00_design_system.svg` | `docs/rules/ui_theme_tokens.md`, `resrc/styles/dark.qss`, `src/core/utils/theme_manager.cpp` `tokenTable()` |
| `current/01_main_window_dashboard.svg` | `mainwindow.ui`, `src/ui/forms/task/localization_task_widget.ui`, `src/ui/forms/task/localization_dashboard_widget.ui`, `src/ui/widgets/controls/status_lamp.*`, `src/ui/widgets/signals_monitor_widget.*` |
| `current/02_patterns_view.svg` | `src/ui/forms/task/localization_patterns_widget.ui`, `src/ui/forms/pattern/pattern_canvas.*` |
| `current/03_settings_and_wizard.svg` | `src/ui/forms/task/localization_setting_widget.ui`, `src/ui/forms/add_device_wizard.ui`, `src/ui/widgets/signals_map_widget.*`, `src/ui/widgets/camera_mapping_widget.*` |

## Current-state findings

The current UI is already coherent: there is a strong theme system
(`ThemeManager`), tokens are well-named (`docs/rules/ui_theme_tokens.md`), and
the QSS layering rules are explicit. The mocks make a few problems visible that
are easy to miss while reading code:

1. **Three top bars compete for attention.** Title bar + menu bar +
   `toolBar_file` + `toolBar_task` all live in the top strip but encode
   commands that are conceptually one set (file, task, run). On a 1080p HMI
   that is ~120 px of chrome before any real content.
2. **Status surfaces are scattered.** Connection lamps live inside the
   Dashboard widget, the status bar is a flat orange ribbon at the bottom, and
   per-device lamps appear inside each device card. An operator has three
   places to scan to know "is the system OK right now."
3. **Fault is shown as a small inline card.** When `frame_fault.active=true`
   it changes border colour, but on a wide display the operator can miss it
   entirely. Cycle blocks then look identical to cycle pauses.
4. **No first-class Runtime view.** `AGENT.md` explicitly lists
   "operator runtime validation with a real or simulated Localization cycle"
   as the highest priority. Today the Dashboard is the closest thing, but it
   was designed for commissioning — it has nav, breadcrumbs, an editable
   property browser, and ~14 small cards on one screen.
5. **Density is fixed.** The widgets implicitly target ~24" desktop monitors
   (row heights 28–36 px, mostly 12 pt body). They are cramped on a 19" HMI
   panel and small on a 32" status display.
6. **Add-device is a 4-step modal wizard.** Every value still has to be typed
   in even when most can be probed from a single connection string.

## Proposed changes

Each change keeps the existing token system, the QSS layering rules, and the
"structure in `.ui`, style in `.qss`" contract. None of them require a token
rename or a `tokenTable()` change.

### Shell

- Replace title bar + menu bar + two toolbars with one **Command Bar** that
  holds project breadcrumb, mode toggle (Commission / Runtime), command-palette
  search, run controls, and live connection chips. Frees ~80 px vertical and
  collapses today's 3 status surfaces into 1.
- Add a **slim left rail** (64 px) for cross-section navigation (Tasks,
  Project, Calibration, Logs, Theme, Prefs). The existing `ProjectTreeWidget`
  opens on demand inside the Tasks rail item rather than always occupying
  240 px.
- Promote the per-task `nav_panel` to use **icon + label rows** with a thin
  left accent stripe instead of separate device-color buttons. Same widget
  primitives (QPushButton + dynamic property), just tightened.

### Dashboard

- Hero **"Now" row** with cycle number and run age — answers the first
  operator question.
- **Full-width Fault Banner** replaces the inline fault card. Uses the v2
  `StatusBanner` primitive. Two explicit actions (`Reconnect` / `Acknowledge`).
- **KPI tile spec v2.** Same FrameBox style, but with caption + numeric +
  trend slot (sparkline, gauge, or context line). State expressed by border
  colour AND glyph (per the existing rule "never use colour as sole cue").
- **Side panel** combines result table, cycle-phase timeline, and an activity
  feed of OK / WARN / FAULT pills — a single column to scan during a run.
- Signal monitor moves into a horizontal strip footer; on small windows it
  collapses to a 6-tile "trigger / busy / counter / fault / fault-code /
  queue" set.

### Patterns

- Toolbar split into **Source / Action / State** zones with a clear primary
  (Run Matching). Caption labels above each zone.
- **Floating canvas toolbar** for zoom, ROI, layer toggles — image takes the
  full width.
- **Layer panel** with a `Last-cycle diff` overlay for tuning.
- **Tabbed Inspector** (Threshold / Search / Filters / Output) replaces the
  current side-by-side tree + setting panel; adds a score histogram of the
  last 50 cycles and an inline suggestion card.
- **Result row** with per-match thumbnails and inline score gauges; the
  current text-only KPI strip becomes redundant.

### Runtime / Operator mode (new screen)

A 1920×1080 single-purpose surface for the shop floor. No nav, no settings.

- Hero camera view at ~1180 px wide; matches drawn over the live frame at
  3 px stroke for read-distance.
- Right column is fault banner → 2×2 oversized KPIs → activity feed.
- Persistent **RunBar** footer (160 px tall): Start (green, 220×120 px),
  Stop, E-Stop (deep red), live cycle phase bar, cycles-today counter,
  uptime tile.
- Mode toggle (top right) is the only way back to Commission — keeps an
  unintentional click from disturbing the operator view.

### Interaction primitives

- **Command Palette (Ctrl + K).** Fuzzy search across tasks, devices,
  signals, settings, and recent log lines. Grouped results, keyboard-first.
  Removes the need to remember which menu owns a given action.
- **Add-Device "Smart Form"** replaces the 4-step wizard. Single dialog;
  the connection field triggers a live probe that fills subtype, model,
  serial, and a test frame preview. Cuts the add-device path from ~12 clicks
  to ~3.

### System-level

- **Density modes** (Comfortable / Compact / Touch). Same tokens; only
  control heights and padding change. Selected per machine in
  `app_settings`.
- **Focus ring spec.** `accent.bright`, 2 px, 3 px offset — visible against
  both `bg.surface` and `bg.window`. The current QSS only sets border on
  focused inputs and forgets a focus indicator on buttons.
- **Reusable primitives to add to `src/ui/widgets/controls/`** (thin subclasses,
  styled globally per the §3.6 rule):
  - `PrimaryButton`, `DangerButton`
  - `KpiTile` (caption + numeric + trend slot)
  - `StatusBanner` (success / warn / error / info)
  - `RunBar` (Runtime footer composite)
  - `CommandPalette` (modal Q-search)

## What is intentionally **not** changed

- Token names and values in `docs/rules/ui_theme_tokens.md` — no rename
  needed, no `tokenTable()` migration.
- The "structure in `.ui`, style in `.qss`, behaviour in `.cpp`" contract.
  Every new primitive proposed is a thin `QWidget` / `QPushButton` subclass.
- `ThemeManager` API. Density modes and focus ring fit inside the existing
  `reloadStyleSheet()` flow.
- The hybrid graphite + orange palette — the rationale in
  `docs/rules/ui_theme_tokens.md` §1 still holds.

## How to view

- Open any `.svg` directly in a browser.
- Drag the file into a Figma frame to import it as a native layer (text and
  shapes are editable).
- Each SVG is self-contained — no external assets, fonts, or scripts.

## Suggested rollout order

1. Token-free additions first: new widget primitives
   (`PrimaryButton`, `KpiTile`, `StatusBanner`) and density modes.
2. Dashboard restructure (fault banner, KPI v2, side panel).
3. Patterns inspector and floating canvas toolbar.
4. New Runtime view + RunBar (gated behind the existing mode toggle).
5. Command palette and Smart-form device add.
