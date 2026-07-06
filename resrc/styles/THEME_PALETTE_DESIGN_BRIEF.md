# Theme Palette & Token Design Brief

**Audience.** A design agent (e.g. Claude design mode) deciding the colour palette
for the residual, not-yet-tokenized colours of this app. This brief is
self-contained: read it top to bottom and produce the deliverable in §7.

**Status.** The token *mechanism* and the migration of all existing canonical
colours are **done**. What remains is a **design decision** on the colours listed
in §5–§6. Do not redesign the established tokens in §3 unless explicitly asked.

---

## 1. Product context

- The app is an **industrial machine-vision picking HMI** (operators run a
  camera → match → PLC/robot pick cycle on a shop floor).
- Active theme is **"Hybrid — Graphite Vision + Orange"**:
  - Dark base: cool blue-gray graphite (window `#1a1c20`).
  - Accent: Siemens-style industrial orange (`#e87c00`).
  - Rationale: cool graphite + warm orange is a temperature-contrast pairing
    standard in industrial HMI (Siemens TIA Portal, Beckhoff TwinCAT). Status
    colours stay hue-neutral so alarms read unambiguously regardless of accent.
- Every colour ships in **two themes: dark and light**. There is no "dark only".

## 2. How colours are consumed (output target)

- Stylesheets reference colours as token placeholders: `@{group.token}`
  (e.g. `@{accent.primary}`, `@{state.error}`).
- At load time `ThemeManager::resolveTokens()` substitutes each placeholder with
  the value for the active theme. The single source of truth is the
  `tokenTable()` map in `src/utils/theme_manager.cpp`, mirrored by the table in
  `docs/rules/ui_theme_tokens.md`.
- **So every token you design needs a `{dark, light}` value pair**, and a
  `group.token` name. Values may be `#rrggbb` or `rgba(r,g,b,a)` with `a` in the
  **0.0–1.0** range (do **not** use the legacy 0–255 alpha form).

## 3. Established tokens — DO NOT change (reference for harmony)

These exist and are in use. Match their saturation/lightness so new colours feel
like the same family. (Full table with roles: `docs/rules/ui_theme_tokens.md`.)

| Group | Tokens (dark / light shown for the key ones) |
|---|---|
| Background | `bg.window` #1a1c20/#f2f3f5, `bg.surface` #22252a/#ffffff, `bg.elevated` #2c3038/#e8eaee, `bg.input`, `bg.disabled` |
| Border | `border.default` #3a3f48/#c8ccd4, `border.strong`, `border.focus` #e87c00/#c06400, `border.disabled` |
| Text | `text.primary` #e0e4ea/#1a1c20, `text.muted` #7a8898/#5a6878, `text.disabled`, `text.on-accent` #ffffff, `text.link` |
| Accent | `accent.primary` #e87c00/#c06400, `accent.bright` #ffa040/#e08020, `accent.pressed` #b05a00/#8a4400, `accent.subtle` rgba(232,124,0,0.12)/…, `selection.bg`, `selection.text` |
| State | `state.error` #ff5252/#c0282a, `state.warning` #ffb020/#9a6800, `state.success` #40c870/#1a7a40, `state.info` #40a8e0/#0060a0, plus `state.*.surface` (≈0.10 dark / 0.08 light) |
| Panel accent | `panel.accent.deep/mid/surface/border` (warm graphite tints under nav rail) |

**Constraints for any new colour:**
- Provide both dark and light values.
- WCAG: a status/identity colour is never the *sole* cue — it always pairs with
  an icon or text, so you may prioritise palette harmony over raw contrast for
  large fills, but **text colours must stay legible** (≥ 4.5:1 on their bg for
  body text, ≥ 3:1 for large/bold).
- Keep status hues (error/warning/success/info) unambiguous and distinct from the
  device-identity hues in §5.

---

## 4. Why these colours need a decision (not a mechanical fix)

The residual colours below are **off-palette** — they were authored before the
Hybrid theme and have no §5 token. They cannot be auto-migrated because mapping
them to an existing token, or inventing a new value, **changes how the app
looks** and is a design judgment. There are ~95 distinct raw values; they cluster
into the coherent groups in §5–§6.

---

## 5. Decision A — Device-identity colour family (primary task)

The app colour-codes the four device roles plus a neutral default. This identity
palette appears in three surfaces: the breadcrumb current-segment label, the
left-rail device nav (type label + active-item tint), and the Add-Device wizard
cards. It is currently hardcoded in the old blue/green/purple/gold hues:

| Role | Current base hue (dark) | Where used |
|---|---|---|
| `device.camera` | `#2b8ce8` (blue) | breadcrumb, nav type label, nav active tint, wizard camera card |
| `device.plc` | `#22d17a` (green) | same set, PLC |
| `device.output` | `#a06cf0` (purple) | same set, vision-output |
| `device.robot` | `#f5a623` (gold) | same set, robot |
| `device.default` | `#6b7ea0` (blue-gray) | fallback / unknown device |

Each role currently uses up to three shades + a tint:
- **base** (text/icon colour) — e.g. camera `#2b8ce8`
- **bright** (hover) and **deep** (pressed) — wizard cards add `#3aa0ff` / `#1f7ed1`
- **tint bg** ≈ 11% alpha and **tint border** ≈ 40% alpha (currently the legacy
  0–255 form, e.g. `rgba(43,140,232,28)` and `rgba(43,140,232,102)`).

**Decide:** for each of the five roles, the harmonized hue (or keep blue/green/
purple/gold but tuned to sit on graphite), and produce these tokens with dark +
light values:
- `device.<role>` (base), `device.<role>.bright`, `device.<role>.deep`
- a standard `device.<role>.tint.bg` and `device.<role>.tint.border` (state in
  0.0–1.0 alpha; pick one canonical bg-alpha and one border-alpha for all roles).

Open question for you to settle: should device-identity hues stay multi-hued
(blue/green/purple/gold), or collapse toward the orange accent + neutrals for a
more unified industrial look? Recommend one and explain the trade-off (recognise-
at-a-glance vs. visual cohesion).

## 6. Decision B — Smaller residual groups

1. **Status "bright" gradient stops.** Status lamps use a radial gradient whose
   *top* stop is a lighter variant of the state colour: success `#2eff98`, error
   `#ff5b5b`, warning `#ffd060` (light theme: `#44e87a`, `#e84040`, …). Decide
   whether to formalise these as `state.<x>.bright` tokens (dark + light) or
   re-derive them from the existing `state.*`.
2. **Tint-alpha standardisation.** Many translucent fills use the legacy 0–255
   alpha (`rgba(...,28)`, `...,89`, `...,100`, `...,102`) or ad-hoc 0.0–1.0
   alphas (0.12/0.18/0.22/0.27/0.30). Decide a small set of canonical alpha
   levels (e.g. subtle-bg, hover-bg, border) and the rule for applying them.
3. **Drift to reconcile.** A few values are near-misses of an existing token and
   are probably bugs: light `#556070` where `text.muted` is `#5a6878`; amber
   `#e8a000` near `accent`. Decide: snap to the token, or keep as a new token.
4. **One-offs.** Genuine single-use decoration: `#922015` (delete-button hover/
   pressed red — a "danger pressed"?), white overlays `rgba(255,255,255,24..96)`
   and black `rgba(0,0,0,16..32)` (elevation/hover overlays in the global sheet),
   `#252830`, `#8899aa`, `#e0d0a0`, `#f0c0c0`, `#9aa0aa`, `#e4e6ea`, `#fde8cc`,
   `#b07000`, `#edf0f3`, `#f5f6f8`, `#5a3800`, `#6e1010`, `#7a1010`. For each:
   map to an existing/new token, or keep raw with a one-line justification.

---

## 7. Deliverable (what to hand back)

A single token table the engineer can paste into `theme_manager.cpp` `tokenTable()`
and `docs/rules/ui_theme_tokens.md`. For every new token:

| Token name | Role / where used | Dark value | Light value | Replaces (raw values) |
|---|---|---|---|---|
| `device.camera` | device identity — camera | `#…` | `#…` | `#2b8ce8`, `rgba(43,140,232,*)` |
| … | … | … | … | … |

Plus a short rationale paragraph for Decision A's hue strategy (§5 open question)
and the canonical alpha levels chosen in §6.2.

**Acceptance:** every listed raw colour in §5–§6 is either (a) covered by a new
token with dark+light values, or (b) explicitly kept raw with a justification.
After the engineer applies it, the review checklist in `docs/rules/ui_design_rules.md`
is run in both dark and light themes (QSS colour changes are visual-only and need
human inspection — there is no automated test for appearance).

## 8. Where the result is applied (for the engineer, not the designer)

1. Add the new tokens to `tokenTable()` in `src/utils/theme_manager.cpp` and the
   table in `docs/rules/ui_theme_tokens.md`.
2. Replace the raw values in `resrc/styles/*.qss` with the new `@{token}` forms
   (heaviest files: `localization_task_widget_*`, `add_device_wizard_*`).
3. If any painted (non-QSS) surface uses the same identity colours, mirror them in
   `src/ui/forms/pattern/pattern_theme.h` (see `docs/rules/ui_design_rules.md`).
