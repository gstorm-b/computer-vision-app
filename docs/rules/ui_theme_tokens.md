# UI Theme Tokens

> **Authority.** This document is the single source of truth for UI colour
> token names, token values, palette rationale, and colour-scheme migration
> workflow. [ui_design_rules.md](ui_design_rules.md) owns how widgets consume
> these tokens through `.ui`, `.qss`, and `ThemeManager`.
>
> **Runtime mirror.** The implemented token table lives in
> [src/core/utils/theme_manager.cpp](../../src/core/utils/theme_manager.cpp) `tokenTable()`.
> This document and `tokenTable()` must stay in 1:1 sync. If they drift, code is
> the implemented truth, but the UI/token task is not closed until both are
> reconciled.

---

## 1. Active Theme

**Theme name**: `hybrid`

**Dark base**: Graphite Vision — cool blue-gray graphite, window `#1a1c20`

**Accent**: Navy Ops Orange — Siemens industrial orange, `#e87c00`

**Rationale**: Cool graphite background creates a temperature-contrast pairing
with warm orange accent — a technique standard in industrial HMI (Siemens TIA
Portal, Beckhoff TwinCAT). State colours remain hue-neutral so alarms are
unambiguous regardless of accent.

---

## 2. Token Tables

`.qss` files reference these colours by the `@{group.token}` placeholder.
`ThemeManager::resolveTokens()` substitutes each placeholder with the active
theme value when a stylesheet is loaded. A `.qss` should not carry a raw hex or
rgba literal that equals a token value — use the token.

The migration of legacy hardcodes onto placeholder form now includes the
device-identity, status-bright, deep accent, and overlay families below. The
only accepted raw exception in the current sweep scope is `#7a1010`
(single-use delete-button shadow). Document any future raw exception explicitly
when touched and keep the design rationale at
[resrc/styles/THEME_PALETTE_DESIGN_BRIEF.md](../../resrc/styles/THEME_PALETTE_DESIGN_BRIEF.md).

### 2.1 Background Tokens

| Token | Role | Dark | Light |
|---|---|---|---|
| `bg.window` | App / page base | `#1a1c20` | `#f2f3f5` |
| `bg.surface` | Panel / sidebar / card | `#22252a` | `#ffffff` |
| `bg.elevated` | Raised containers, dropdowns | `#2c3038` | `#e8eaee` |
| `bg.input` | Inputs, combos, editable fields | `#2c3038` | `#ffffff` |
| `bg.disabled` | Disabled control fill | `#1e2024` | `#eeeff1` |

### 2.2 Border Tokens

| Token | Role | Dark | Light |
|---|---|---|---|
| `border.default` | Standard separators, control edges | `#3a3f48` | `#c8ccd4` |
| `border.strong` | Emphasised borders, section dividers | `#4e5562` | `#a8adb8` |
| `border.focus` | Keyboard / active focus ring | `#e87c00` | `#c06400` |
| `border.disabled` | Disabled control border | `#2e3238` | `#dddfe3` |

### 2.3 Text Tokens

| Token | Role | Dark | Light |
|---|---|---|---|
| `text.primary` | Primary foreground | `#e0e4ea` | `#1a1c20` |
| `text.muted` | Secondary / label text | `#7a8898` | `#5a6878` |
| `text.disabled` | Disabled text | `#4a5260` | `#a0a8b0` |
| `text.on-accent` | Text rendered on accent-coloured fills | `#ffffff` | `#ffffff` |
| `text.link` | Hyperlink / clickable label | `#ffa040` | `#9a5000` |

### 2.4 Accent Tokens

| Token | Role | Dark | Light |
|---|---|---|---|
| `accent.primary` | Primary interactive accent | `#e87c00` | `#c06400` |
| `accent.bright` | Hover / highlight accent | `#ffa040` | `#e08020` |
| `accent.pressed` | Pressed / active deep accent | `#b05a00` | `#8a4400` |
| `accent.pressed.deep` | Extra-deep accent pressed tone | `#7a3c00` | `#5a3800` |
| `accent.subtle` | Translucent accent (hover bg, focus glow) | `rgba(232,124,0,0.12)` | `rgba(192,100,0,0.09)` |
| `selection.bg` | Selected row / item fill | `#3d2e10` | `#fde8c0` |
| `selection.text` | Selected row / item text | `#e0e4ea` | `#1a1c20` |

### 2.5 State Tokens

State colours are confirmed, not proposed. Add to both `dark.qss` and
`light.qss` before shipping the first status-coloured surface.

| Token | Role | Dark | Light |
|---|---|---|---|
| `state.error` | Error / invalid | `#ff5252` | `#c0282a` |
| `state.warning` | Warning / attention | `#ffb020` | `#9a6800` |
| `state.success` | Success / connected / healthy | `#40c870` | `#1a7a40` |
| `state.info` | Informational / neutral status | `#40a8e0` | `#0060a0` |
| `state.error.surface` | Error card / inline alert bg | `rgba(255,82,82,0.10)` | `rgba(192,40,42,0.08)` |
| `state.warning.surface` | Warning card bg | `rgba(255,176,32,0.10)` | `rgba(154,104,0,0.08)` |
| `state.success.surface` | Success card bg | `rgba(64,200,112,0.10)` | `rgba(26,122,64,0.08)` |
| `state.info.surface` | Info card bg | `rgba(64,168,224,0.10)` | `rgba(0,96,160,0.08)` |

Never use a state colour as the sole cue. Always pair it with an icon or text
label. Use `state.*.surface` for large background areas — never solid `state.*`
colour on anything wider than a 3 px indicator bar.

### 2.6 Accent-Tinted Panel Tokens

For nav rail, sidebar headers, or accent panels that need an orange-warm tint
layered on graphite. Use these before introducing any new warm shade inline.

| Token | Role | Dark | Light |
|---|---|---|---|
| `panel.accent.deep` | Deepest accent panel (nav bg) | `#1e1c16` | `#f5ede0` |
| `panel.accent.mid` | Mid accent panel (sidebar header) | `#252118` | `#faebd0` |
| `panel.accent.surface` | Raised accent surface | `#2e2a1e` | `#f5e4c0` |
| `panel.accent.border` | Border within accent panel | `#403a28` | `#d8c89a` |

### 2.7 Device-Identity Tokens

Used for device breadcrumbs, nav labels, wizard cards, and tinted device-active
states. These are semantic role colours, not the primary theme accent.

| Token | Role | Dark | Light |
|---|---|---|---|
| `device.camera` | Camera base | `#3a9bd9` | `#1f6fb8` |
| `device.camera.bright` | Camera hover/highlight | `#5cb3e8` | `#3a8ad0` |
| `device.camera.deep` | Camera pressed/deep | `#2278b8` | `#155a98` |
| `device.camera.tint.bg` | Camera tinted fill | `rgba(58,155,217,0.12)` | `rgba(31,111,184,0.08)` |
| `device.camera.tint.bd` | Camera tinted border | `rgba(58,155,217,0.40)` | `rgba(31,111,184,0.36)` |
| `device.plc` | PLC base | `#3ac98a` | `#1a8a55` |
| `device.plc.bright` | PLC hover/highlight | `#5cdca2` | `#28a468` |
| `device.plc.deep` | PLC pressed/deep | `#22a070` | `#0e6a40` |
| `device.plc.tint.bg` | PLC tinted fill | `rgba(58,201,138,0.12)` | `rgba(26,138,85,0.08)` |
| `device.plc.tint.bd` | PLC tinted border | `rgba(58,201,138,0.40)` | `rgba(26,138,85,0.36)` |
| `device.output` | Vision-output base | `#a87de0` | `#7a4ec0` |
| `device.output.bright` | Vision-output hover/highlight | `#bf98ea` | `#9168d0` |
| `device.output.deep` | Vision-output pressed/deep | `#8458c0` | `#5e389a` |
| `device.output.tint.bg` | Vision-output tinted fill | `rgba(168,125,224,0.12)` | `rgba(122,78,192,0.08)` |
| `device.output.tint.bd` | Vision-output tinted border | `rgba(168,125,224,0.40)` | `rgba(122,78,192,0.36)` |
| `device.robot` | Robot base | `#e0b020` | `#a07000` |
| `device.robot.bright` | Robot hover/highlight | `#f0c850` | `#b88810` |
| `device.robot.deep` | Robot pressed/deep | `#b88a10` | `#7a5400` |
| `device.robot.tint.bg` | Robot tinted fill | `rgba(224,176,32,0.12)` | `rgba(160,112,0,0.08)` |
| `device.robot.tint.bd` | Robot tinted border | `rgba(224,176,32,0.40)` | `rgba(160,112,0,0.36)` |
| `device.default` | Fallback / unknown base | `#7a8ca8` | `#4a5868` |
| `device.default.bright` | Fallback hover/highlight | `#94a4be` | `#5e6c80` |
| `device.default.deep` | Fallback pressed/deep | `#5a6c88` | `#36404e` |
| `device.default.tint.bg` | Fallback tinted fill | `rgba(122,140,168,0.12)` | `rgba(74,88,104,0.08)` |
| `device.default.tint.bd` | Fallback tinted border | `rgba(122,140,168,0.40)` | `rgba(74,88,104,0.36)` |

### 2.8 State Bright/Deep Helper Tokens

These complement the state tokens when a control needs a lighter gradient stop
or a deeper danger pressed colour.

| Token | Role | Dark | Light |
|---|---|---|---|
| `state.success.bright` | Brighter success stop | `#2eff98` | `#44c878` |
| `state.error.bright` | Brighter error stop | `#ff8a8a` | `#e84848` |
| `state.warning.bright` | Brighter warning stop | `#ffd060` | `#d09020` |
| `state.error.deep` | Danger pressed/deep | `#922015` | `#6e1010` |

### 2.9 Overlay Tokens

Theme-agnostic overlay helpers for dock-button hover/pressed states, scrims,
and elevation tints.

| Token | Role | Dark | Light |
|---|---|---|---|
| `overlay.scrim.weak` | Weak black scrim | `rgba(0,0,0,0.08)` | `rgba(0,0,0,0.08)` |
| `overlay.scrim.med` | Medium black scrim | `rgba(0,0,0,0.16)` | `rgba(0,0,0,0.16)` |
| `overlay.scrim.strong` | Strong black scrim | `rgba(0,0,0,0.32)` | `rgba(0,0,0,0.32)` |
| `overlay.tint.weak` | Weak white tint | `rgba(255,255,255,0.04)` | `rgba(255,255,255,0.04)` |
| `overlay.tint.med` | Medium white tint | `rgba(255,255,255,0.10)` | `rgba(255,255,255,0.10)` |
| `overlay.tint.strong` | Strong white tint | `rgba(255,255,255,0.24)` | `rgba(255,255,255,0.24)` |

### 2.10 Canonical Tint-Alpha Scale

Alpha-only guidance for new tinted families; prefer these levels before
inventing a fresh translucent literal.

| Level | Dark Alpha | Light Alpha | Use |
|---|---|---|---|
| `tint.alpha.subtle` | 0.10 | 0.08 | resting tinted fill |
| `tint.alpha.hover` | 0.18 | 0.14 | pointer-hover on tinted surface |
| `tint.alpha.active` | 0.28 | 0.22 | selected / pressed tinted fill |
| `tint.alpha.border` | 0.40 | 0.36 | outline of a tinted surface |

---

## 3. Token Governance

**Rule 3.1 — A colour used twice is a token.** The second time a hex appears,
promote it to this document and reference the token. A single-use decoration may
stay inline only with a comment justifying the exception.

**Rule 3.2 — Renaming or re-valuing a token is a two-source operation.** Update
this document and the matching entry in
[src/core/utils/theme_manager.cpp](../../src/core/utils/theme_manager.cpp) `tokenTable()`.
`.qss` files reference the token by name (`@{token}`), so token-backed colours
resolve everywhere on the next load.

**Rule 3.3 — New token families need a semantic reason.** Add a new group only
when existing tokens cannot describe the UI role. Prefer semantic names
(`state.warning.surface`) over visual names (`yellow.transparent`).

---

## 4. QPalette Contract

`ThemeManager::buildDarkPalette()` and `ThemeManager::buildLightPalette()` must
stay aligned with the token table. The palette is the fallback colour source for
widgets not explicitly covered by QSS, and it is the colour source for
`palette()` references inside the ADS docking stylesheet
(`focus_highlighting.css`).

| QPalette role | Token | Why it matters |
|---|---|---|
| `Window` | `bg.window` | Default fill for QWidget / QMainWindow / content areas |
| `Base` | `bg.window` (dark) / `bg.surface` (light) | Input field base background |
| `Button` | `bg.elevated` | Fusion style button fill |
| `AlternateBase` | `bg.elevated` | Alternating rows in table/tree views |
| `Light` | `bg.surface` | ADS `focus_highlighting.css` uses `palette(light)` for `ads--CDockWidget` background |
| `Midlight` | `bg.elevated` | Fusion style intermediate tones |
| `Dark` | `border.default` | ADS `focus_highlighting.css` uses `palette(dark)` for splitter and sidebar borders |
| `Mid` | `border.default` | Fusion style separator rendering |
| `WindowText` / `Text` / `ButtonText` | `text.primary` | Default text colour for all widgets |
| `Highlight` | `accent.primary` | Selection highlight colour throughout the app |
| `HighlightedText` | `text.on-accent` (`#ffffff`) | Text on accent-coloured selection |
| `ToolTipBase` | `bg.surface` | Tooltip background |
| `ToolTipText` | `text.primary` | Tooltip text |
| `Disabled / WindowText` | `text.disabled` | Disabled control text |
| `Shadow` | near-black / near-white | Drop shadows; rarely visible |

**ADS note.** Qt Advanced Docking System loads `focus_highlighting.css`
internally via `CDockManager::setStyleSheet()` (widget-level, higher priority
than `qApp->setStyleSheet()`). To let the global QSS own all ADS colours, call
`m_dockManager->setStyleSheet(QString())` immediately after constructing the
`CDockManager`. This clears ADS's widget-level stylesheet and lets `ads--` rules
in `dark.qss` / `light.qss` take effect. Even so, `palette()` references are
still resolved from the active QPalette, so this mapping must be correct or ADS
widget backgrounds will not match the tokens.

---

## 5. Changing The Colour Scheme

When switching to a new colour palette (for example replacing Graphite Vision +
Orange with another theme family), update the following locations together.
Touching only one layer will leave visible mismatches.

1. Update the token table in this document.
2. Update `src/core/utils/theme_manager.cpp` `tokenTable()` to match this document.
3. Update `buildDarkPalette()` and `buildLightPalette()` per the QPalette
   mapping in this document.
4. Replace only residual raw hex in `resrc/styles/dark.qss`,
   `resrc/styles/light.qss`, and per-form QSS pairs. Token-backed colours are
   handled by step 2.
5. If the accent changed, re-derive `panel.accent.*` tokens and audit the
   `LocalizationTaskWidget` nav-panel QSS sections.
6. If the accent changed, update
   [src/ui/forms/pattern/pattern_theme.h](../../src/ui/forms/pattern/pattern_theme.h) and
   any other C++ painted-surface token headers.
7. Build and test both dark and light in the running app. QSS cannot be fully
   validated by the type-checker or test suite; visual inspection is mandatory.
