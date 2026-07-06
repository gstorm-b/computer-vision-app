# UI Design Rules — ncr_picking

> **Authority.** This document is the single source of truth for UI structure,
> styling workflow, and theming implementation in this project. Colour token
> names, values, palette rationale, and colour-scheme migration workflow live in
> [ui_theme_tokens.md](ui_theme_tokens.md). This document supersedes the
> UI-specific parts of [design_rules.md](design_rules.md) §6 (UI Composition)
> and §15 (Themed Form Stylesheets); those sections now point here. When a UI
> question is not answered by this doc, fall back to `design_rules.md` and then
> ask.
>
> **Audience.** Anyone adding or changing a widget, form, `.ui`, `.qss`, icon,
> or theme. Read §0–§3 before touching any widget; read §4–§6 before touching
> theming or a custom-painted surface.

---

## 0. First principles

Three rules drive everything below. If a decision is ambiguous, pick the
option that best honours these.

1. **Separation of concerns — structure, style, behaviour are three files.**
   Layout structure lives in `.ui`, visual style lives in `.qss`, behaviour and
   wiring live in `.cpp`. A widget that mixes them (inline stylesheet in cpp,
   `new QPushButton` for a layout primitive, geometry hard-coded in code) is a
   defect, not a style preference.

2. **One theming authority.** `ThemeManager` is the *only* mechanism that
   decides colours and the active theme. Every themable surface reacts to it.
   No widget invents its own light/dark switch.

3. **Tokens over literals.** Colours come from
   [ui_theme_tokens.md](ui_theme_tokens.md), never from ad-hoc hex literals
   scattered across `.qss` files.

---

## 1. Layer separation — what goes where

| Concern | Belongs in | Never in |
|---|---|---|
| Widget tree, layouts, spacing, margins, size policies | `.ui` | cpp (no `new QHBoxLayout` for static structure) |
| `objectName` (the styling/anchor handle) | `.ui` | — |
| Colours, fonts, borders, radii, padding, hover/checked states | `.qss` | cpp (`setStyleSheet`), `.ui` (`styleSheet` property) |
| State/role flags that drive variant styling | `.ui` dynamic properties | hard-coded per-state stylesheet strings |
| Signal/slot wiring, data binding, model updates | `.cpp` | — |
| Theme reaction (`reloadStyleSheet`, repolish) | `.cpp` | — |
| Painted (QPainter) surface colours | C++ token source (§6) | `.qss` (QSS cannot style a custom-painted canvas) |

**Rule 1.1 — Layout primitives are authored in `.ui`.** Buttons, labels,
frames, stacks, and their layouts are declared in Qt Designer. Code only wires
them (text, icon, signals, button-group membership). Do not `new` a navigation
button or a static frame in cpp.

**Rule 1.2 — Code-built widgets are the exception, and justified.** Dynamic,
data-driven children (e.g. one row per camera) may be built in code, but the
*container* and its layout still live in the `.ui`, and the built children
still follow the QSS + token rules.

---

## 2. The `.ui` rules

**Rule 2.1 — `objectName` is mandatory on every styled or wired widget.** It is
the QSS selector and the code handle. Use `snake_case` that reflects role and
hierarchy (`btn_nav_dashboard`, `frame_device_header`, `lbl_status_value`).

**Rule 2.2 — The `styleSheet` property must be empty in every `.ui`.** Visual
styling is the `.qss` file's job. An inline `.ui` stylesheet is invisible to a
global theme switch, duplicates the palette, and silently overrides the `.qss`.
This is a hard gate in review.

**Rule 2.3 — Variants are dynamic properties, not separate widgets.** When a
widget needs different looks by state or role (selected card, accent colour,
label role), declare a dynamic property in the `.ui`
(`<property name="selected" stdset="0"><bool>…</bool></property>`) and target it
in QSS via attribute selectors. See §3.4 and §4.4.

**Rule 2.4 — Spacing comes from layouts, not magic numbers in paint code.** Use
layout margins/spacing and size policies. Reserve fixed sizes for genuinely
fixed things (icon buttons), and prefer size policies + minimums otherwise.

**Rule 2.5 — Reusable nav clusters live together in the `.ui`.** Group related
controls (nav bar, toolbar) in one container so a fourth item is a Designer edit,
not a cpp change.

---

## 3. The QSS rules

### 3.1 Two QSS layers — global first

The project has two stylesheet layers. **Prefer the global layer; add a per-form
file only when a widget genuinely needs bespoke styling.**

| Layer | File | Scope | Applied by |
|---|---|---|---|
| **Global** | `resrc/styles/dark.qss`, `resrc/styles/light.qss` | App-wide base look of every standard Qt control | `ThemeManager` via `ThemeStyle::qssPath` |
| **Per-form** | `resrc/styles/<form>_<theme>.qss` | One form/widget with styling the global sheet cannot express | the widget's own `reloadStyleSheet()` (§4.2) |

**Rule 3.1 — Default to the global sheet.** A new widget should look correct
from `dark.qss`/`light.qss` alone. Only introduce a per-form pair when the
widget has styling the global selectors genuinely cannot reach. This keeps the
number of QSS files bounded.

**What goes where:**

| Style concern | Layer | Reason |
|---|---|---|
| Standard Qt controls — `QPushButton`, `QLabel`, `QLineEdit`, `QFrame`, `QScrollArea`, `QSplitter`, `QComboBox`, `QSpinBox`, `QCheckBox`, `QGroupBox`, `QTabWidget` | Global `dark.qss` / `light.qss` | One rule governs every instance app-wide |
| Reusable control variants — `GhostButton`, `PrimaryButton`, `NavButton` (subclasses, §3.6) | Global, selector by class name | Variant identity is the class name, not the form |
| Form-specific `objectName` overrides (e.g. a card grid unique to one screen) | Per-form `<form>_dark.qss` | objectName is local to that form; global rules can't target it |
| Domain-specific dynamic property selectors (e.g. `[connectionState]`, `[deviceColor]`) | Per-form | Selector semantics are meaningful only inside that form |
| Complex structural overrides (nav rail, bespoke status chips, accent panels) | Per-form | Layout context is too specific for a global rule |

**Rule 3.2 — Per-form files are a `<form>_dark.qss` + `<form>_light.qss` pair**,
placed in `resrc/styles/` and registered in [resrc.qrc](../../resrc.qrc) with a
`styles/...` alias. Both variants always exist together — never ship dark without
light.

### 3.2 No styling in cpp

**Rule 3.3 — No `setStyleSheet(<literal>)` in cpp.** The only stylesheet code a
widget runs is loading a `.qss` resource in `reloadStyleSheet()` (§4.2).
Inline strings fight the theme switch and force a rebuild for a colour tweak.

### 3.3 Selector conventions

**Rule 3.4 — Style by `objectName`, group related selectors.** One rule per
visual state listing all members
(`QPushButton#btn_a, QPushButton#btn_b { … }` then `…:hover`, `…:checked`), not
one block per widget. Group selectors are how nav clusters stay consistent.

**Rule 3.5 — Remove orphan rules with the widget.** Deleting a widget from a
`.ui` removes its QSS rule in the same change. No dangling selectors.

### 3.4 Variant styling

**Rule 3.6 — Variants use attribute selectors against dynamic properties.**
`QFrame#card[selected="true"] { … }`, `QPushButton#btn[deviceColor="camera"] { … }`.
After mutating the property in code, repolish (§4.4) or the selector reads the
stale value.

### 3.5 Colours

**Rule 3.7 — Reference colours by token, not hex literal.** Every themed colour
is written as a token placeholder `@{group.token}` (e.g. `@{accent.primary}`,
`@{bg.surface}`, `@{state.error}`). `ThemeManager::resolveTokens()` substitutes
each placeholder with the active theme value when the sheet is loaded (global
apply and every per-form `reloadStyleSheet()` route through it). The canonical
token table and governance live in [ui_theme_tokens.md](ui_theme_tokens.md),
mirrored by [src/core/utils/theme_manager.cpp](../../src/core/utils/theme_manager.cpp)
`tokenTable()`. A reviewer rejects a raw hex that corresponds to a token — use
the `@{token}` form instead. Raw hex is allowed only for a genuinely off-palette
one-off, and then only with a comment justifying the exception. Unknown tokens
are left verbatim and logged, so a typo surfaces rather than rendering a wrong
colour.

### 3.6 Reusable control variants via subclassing

Some controls need a different visual role that appears across multiple forms
(ghost button, primary action button, danger button, nav button). These are
styled in the **global** sheet using the class name as the selector — no
per-form QSS file, no property management.

**Rule 3.8 — Reusable variant controls are thin subclasses.**
A variant class inherits only to acquire a stable class name; it adds no logic.
Inherit all constructors with `using Base::Base`. Place the class in
`src/form/widgets/`.

```cpp
// src/form/widgets/ghost_button.h
#pragma once
#include <QPushButton>

class GhostButton : public QPushButton {
    Q_OBJECT
public:
    using QPushButton::QPushButton;
};
```

**Rule 3.9 — Style the variant by class name in the global sheet.**
Qt QSS resolves `GhostButton { … }` to any instance of that class or its
subclasses. Write one block per state and keep it in `dark.qss` / `light.qss`.

```qss
/* dark.qss */
GhostButton {
    background: transparent;
    border: 1px solid #3a3f48;   /* border.default */
    color: #e0e4ea;               /* text.primary */
    border-radius: 5px;
    padding: 6px 16px;
}
GhostButton:hover  { background: rgba(232,124,0,0.12); border-color: #e87c00; }
GhostButton:pressed { background: rgba(176,90,0,0.18); }
GhostButton:disabled { color: #4a5260; border-color: #2e3238; }
```

**Rule 3.10 — Promote the widget in Qt Designer.**
Right-click the button in Designer → *Promote to…* → enter the class name
(`GhostButton`) and header path (`form/widgets/ghost_button.h`). The `.ui`
stays structure-only; no `styleSheet` property is set.

---

## 4. Theming system (`ThemeManager`)

### 4.1 One authority, extensible

`ThemeManager` (singleton, [src/core/utils/theme_manager.h](../../src/core/utils/theme_manager.h))
owns the active style. `light` and `dark` are pre-registered; additional styles
(e.g. `high_contrast`) are added via `registerStyle(ThemeStyle)`. A `ThemeStyle`
carries an `isDark` flag (drives icon variant), an optional `QPalette`, and a
global `qssPath`.

**Rule 4.1 — Add a theme by registering a `ThemeStyle`, not by branching in
widgets.** Widgets must keep working for any registered style; they switch on
`isDark()`, never on a hard-coded style id.

### 4.2 The `reloadStyleSheet()` contract

A widget that ships a per-form `.qss` pair follows this exact contract:

- In its init, call `reloadStyleSheet()` once, then subscribe to
  `ThemeManager::themeChanged` to call it again on toggle.
- `reloadStyleSheet()` picks the variant from `ThemeManager::instance()->isDark()`,
  loads the resource, and applies it to the widget.

```cpp
// In the constructor / initWidget(), after ui->setupUi(this):
reloadStyleSheet();
connect(ThemeManager::instance(), &ThemeManager::themeChanged,
        this, [this](const QString &, bool) { reloadStyleSheet(); });

// Member:
void MyWidget::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/my_widget_dark.qss")
        : QStringLiteral(":/styles/my_widget_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}
```

**Rule 4.2 — The method is named `reloadStyleSheet()`.** Do not invent
per-widget names (`applyTheme()`, `restyle()`, …). Uniform naming makes the
pattern greppable and reviewable. (Existing non-conforming widgets are flagged
for migration in [later_todo_list.md](../backlog/later_todo_list.md).)

**Rule 4.3 — Subscribe on init, and let Qt disconnect on destroy.** The lambda
captures `this`; because the receiver context object is `this`, the connection
dies with the widget. Always pass `this` as the context, never a dangling
context.

### 4.3 Icons follow the theme

**Rule 4.4 — Theme-aware icons go through the theme-aware icon helper.**
In this codebase that means `svgIcon(basePath)` for ordinary `QIcon` consumers,
or `ThemeManager::themedIcon(basePath)` when code explicitly needs the resolved
resource path. Icons named `foo.svg` carry light-on-dark paths; `foo_dark.svg`
carry dark-on-light paths. When no `_dark` sibling exists, the base asset is
treated as theme-neutral and reused in both themes. Any widget that converts an
icon to a `QPixmap` and stores it in a label must refresh that pixmap on
`themeChanged`, because a stored pixmap does not re-resolve variants by itself.

### 4.4 Repolish after a property change

**Rule 4.5 — `setProperty()` on a styled widget must be followed by a repolish.**
Qt does not re-evaluate QSS on property change:

```cpp
w->setProperty("selected", true);
w->style()->unpolish(w);
w->style()->polish(w);
w->update();
```

Wrap this in a small `repolish(QWidget*)` helper and call it from every variant
toggle.

---

## 5. Design tokens

Colour token names, values, active palette rationale, QPalette mapping, and
colour-scheme migration workflow live in
[ui_theme_tokens.md](ui_theme_tokens.md). This file only defines how UI code and
QSS consume those tokens.

Use token placeholders in QSS (`@{group.token}`), keep painted surfaces aligned
with the same semantic token names, and update both `ui_theme_tokens.md` and
`ThemeManager::tokenTable()` when changing token values.

---

## 6. Non-QSS theming — custom-painted surfaces

QSS cannot style a `QPainter`-rendered canvas (e.g. the pattern canvas). Those
surfaces read their colours from a C++ token source.

**Rule 6.1 — Painted surfaces pull colours from a theme token header, keyed off
`ThemeManager::isDark()`**, and repaint on `themeChanged`. See
[src/form/pattern/pattern_theme.h](../../src/form/pattern/pattern_theme.h) for the
existing pattern. New painted surfaces mirror it; they do not embed raw
`QColor(0x…)` literals in `paintEvent`.

**Rule 6.2 — Keep C++ token names aligned with `ui_theme_tokens.md`.** A painted
"accent" should equal `accent.primary` for the active theme, so painted and QSS
surfaces match.

---

## 7. Standard themed-widget skeleton

Copy this when adding a widget that needs a per-form theme. It encodes every
rule above.

```cpp
// my_widget.cpp
MyWidget::MyWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MyWidget) {
    ui->setupUi(this);            // structure from my_widget.ui (Rule 1.1, 2.x)

    // ... signal/slot wiring (behaviour) ...

    reloadStyleSheet();           // Rule 4.2
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) { reloadStyleSheet(); });
}

void MyWidget::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/my_widget_dark.qss")
        : QStringLiteral(":/styles/my_widget_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}

// Variant toggle (Rule 2.3, 3.6, 4.5):
void MyWidget::setSelected(QWidget *card, bool on) {
    card->setProperty("selected", on);
    card->style()->unpolish(card);
    card->style()->polish(card);
    card->update();
}
```

Checklist for the matching files:
- `my_widget.ui` — structure only, every styled widget has an `objectName`,
  `styleSheet` property empty, variants as dynamic properties.
- `resrc/styles/my_widget_dark.qss` + `_light.qss` — registered in `resrc.qrc`,
  colours are `ui_theme_tokens.md` tokens, selectors grouped by state.

---

## 8. Naming conventions

| Artifact | Convention | Example |
|---|---|---|
| `objectName` | `snake_case`, role-first | `btn_nav_settings`, `lbl_device_name` |
| Per-form QSS files | `<form>_<theme>.qss` | `signals_monitor_widget_dark.qss` |
| QSS resource alias | `styles/<file>` | `:/styles/signals_monitor_widget_dark.qss` |
| Dynamic property | `lowerCamel` flag/role | `selected`, `deviceColor`, `labelRole` |
| Theme reload method | exactly `reloadStyleSheet()` | — |
| Token name | `group.semantic` from `ui_theme_tokens.md` | `accent.primary`, `bg.surface` |
| Variant widget subclass | `PascalCase`, role-based | `GhostButton`, `PrimaryButton`, `NavButton`, `DangerButton` |
| Variant widget source location | `src/form/widgets/<name>.h` + `.cpp` | `src/form/widgets/ghost_button.h` |

---

## 9. Review checklist (PR gate for UI changes)

A UI change is not ready to merge unless all of these hold:

- [ ] Structure is in `.ui`; no layout primitives `new`-ed in cpp.
- [ ] Every styled/wired widget has a meaningful `objectName`.
- [ ] No `styleSheet` property set in any `.ui`.
- [ ] No `setStyleSheet(<literal>)` in cpp — only resource loads in
      `reloadStyleSheet()`.
- [ ] If a per-form `.qss` was added, both `_dark` and `_light` exist and are
      registered in `resrc.qrc`.
- [ ] All colours are `ui_theme_tokens.md` tokens; no new raw hex (or a justifying comment).
- [ ] Theme reaction uses `reloadStyleSheet()` + `themeChanged` subscription
      with `this` as context.
- [ ] Variant styling uses dynamic properties + attribute selectors, with a
      repolish after every `setProperty`.
- [ ] Theme-aware icons go through `svgIcon()` or `themedIcon()`; direct
      `QPixmap` consumers refresh on `themeChanged`.
- [ ] Custom-painted surfaces read colours from a theme token header, not raw
      `QColor` literals, and repaint on `themeChanged`.
- [ ] Reusable button/control variants are thin subclasses in `src/form/widgets/`,
      styled by class name in the global sheet — no per-form QSS added just for
      a button style.
- [ ] Widget renders correctly in **both** dark and light before review.

---

## 10. Relationship to other docs

- [design_rules.md](design_rules.md) §6, §15 — superseded by this doc for
  UI/QSS; kept as short pointers.
- [ui_theme_tokens.md](ui_theme_tokens.md) — source of truth for token names,
  values, palette rationale, QPalette mapping, and colour-scheme migration
  workflow.
- [later_todo_list.md](../backlog/later_todo_list.md) — follow-up UI conformance work that
  is orthogonal to the token table itself, for example the remaining
  theme-aware icon audit.
- The `qt-ui-design` agent skill may be used to audit a screen against these
  rules.
