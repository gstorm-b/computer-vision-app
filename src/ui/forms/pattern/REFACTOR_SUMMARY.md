# Pattern UI Refactor — Design Handoff Implementation

> **Mục tiêu**: Implement lại Pattern Widget, Add Pattern Wizard và Edit Pattern
> Wizard theo design handoff `ui_scratch/design_handoff_full_project/`.
> Apply theme color tokens, đặt thumbnail dưới pattern library.

---

## 1. Design tokens (`pattern_theme.h`)

Tất cả màu/font lấy từ `README.md` design handoff:

| Token   | Value      | Mô tả                       |
|---------|------------|-----------------------------|
| `BG`    | `#1e1e1e`  | Page background             |
| `SURF`  | `#252526`  | Cards, headers              |
| `SURF2` | `#2d2d2d`  | Elevated modals             |
| `BD`    | `#3c3c3c`  | Default borders             |
| `BD2`   | `#454545`  | Input borders, hover        |
| `TXT`   | `#cccccc`  | Primary text                |
| `TXT2`  | `#9a9a9a`  | Secondary text              |
| `TXT3`  | `#7a7a7a`  | Muted/hint text             |
| `TXT4`  | `#5a5a5a`  | Disabled text               |
| `ACC`   | `#2b8ce8`  | Primary accent (blue)       |
| `OK`    | `#22d17a`  | Success green               |
| `WARN`  | `#f5a623`  | Warning orange              |
| `ERR`   | `#e84040`  | Error red                   |
| `OUTPUT`| `#9cdcfe`  | PLC output ID (light blue)  |
| `LOCK`  | `#6a5acd`  | Edit wizard lock badge      |

**Reusable QSS fragments**: `baseStyleSheet()`, `sectionHeaderStyle()`,
`toolbarStyle()`, `inputStyle()`, `primaryButtonStyle()`, `ghostButtonStyle()`,
`stepBubbleStyle()`.

---

## 2. New files

### `pattern_theme.h`
Centralized design tokens + reusable QSS fragments. Mọi màu trong pattern UI
lấy từ đây.

### `pattern_canvas.h` / `.cpp`
Custom canvas widget dùng chung cho cả 2 wizard. Modes:
- `None`   — chỉ hiển thị image
- `Crop`   — crop rect kéo được + corner handles + rule-of-thirds grid
- `Pick`   — click để đặt pick point + crosshair + ring
- `Box`    — render symmetric jaw pair (read-only)
- `Finish` — render pick + box (review)

Hỗ trợ `setLocked(true)` cho Edit wizard step 1 — overlay tím + lock badge.

### `add_pattern_wizard.h` / `.cpp`  (5-step wizard)

| Step | Nội dung                                                          |
|------|-------------------------------------------------------------------|
| 1    | **Image** — capture from camera / open file + name + number      |
| 2    | **Crop** — drag crop rect, hoặc check "use original frame"        |
| 3    | **Pick Point** — click canvas để set pick X/Y                     |
| 4    | **Picking Box** — symmetric jaw pair: width/height/distance/angle |
| 5    | **Finish** — preview + summary HTML                               |

API:
- Constructor: `(groupName, usedNames, usedNumbers, parent)`
- Signal: `requestCameraImage()` — host phải capture rồi gọi `setCameraImage()`
- Result: `patternName/Number/Image()`, `keepOriginal()`, `cropRect()`,
  `pickX/Y()`, `pickBoxW/H/Dist/Angle()`

Validation step 1 chặn Next nếu name trống / number trùng / chưa có image.

### `edit_pattern_wizard.h` / `.cpp`  (4-step wizard)

| Step | Nội dung                                                          |
|------|-------------------------------------------------------------------|
| 1    | **Identity** — image **locked** (purple scrim + lock badge) + name/number |
| 2    | **Pick Point** — pre-filled từ saved pickX/Y                      |
| 3    | **Picking Box** — pre-filled từ saved box config                  |
| 4    | **Finish** — diff view: changed fields hiển thị `old → new`       |

API:
- Constructor: `(groupName, Pattern initial, usedNames, usedNumbers, parent)`
- `Pattern` struct chứa: name, number, pickX/Y, pickBoxW/H/Dist/Angle, image
- Result: `result()` trả về `Pattern` với new values

Image **không edit được** — phải re-learn pattern để có image mới.

---

## 3. Files đã sửa

### `localization_patterns_widget.h`
- Thêm public slots: `editPattern(groupNumber, patternNumber)`, `editSelectedPattern()`
- Thêm forward decl: `class AddPatternWizard *m_activeAddWizard`

### `localization_patterns_widget.cpp`

**Theme application**:
- Apply `ptn::baseStyleSheet()` cho widget gốc
- Override toolbar stylesheet với token values (#1e1e1e BG, #22d17a OK, ...)
- Style các section labels qua `ptn::sectionHeaderStyle()`
- Theme thumbnail QGraphicsView với `#181818` background + `BD` border

**Layout fix — thumbnail dưới pattern library**:
```cpp
// Reparent wg_thumb_container từ pane_inspector → libraryLayout
ui->wg_thumb_container->setParent(nullptr);
ui->libraryLayout->addWidget(ui->wg_thumb_container);
```
Tránh phải sửa 943 dòng UI XML.

**Add Pattern flow** — `onTreeAddPatternRequested`:
- Cũ: `AddPatternImageDialog` (1 step) + `AddPatternDialog` (name/index)
- Mới: `AddPatternWizard` 5-step duy nhất
- Sau Accept: tạo MatchPattern, áp dụng crop nếu cần, set pick position vào
  pattern config, set picking box vào group config

**Edit Pattern flow** — `editPattern(int g, int p)`:
- Tìm group + pattern theo number
- Build `EditPatternWizard::Pattern initial` từ saved config
- Show `EditPatternWizard`
- Sau Accept: apply name/number/pick → pattern config, picking box → group config

**Camera image routing** — `setCameraImage()`:
1. `m_activeAddWizard` (mới — wizard mở)
2. `m_addPatternImageDialog` (legacy)
3. Default — main monitor

### `ncr_picking.pro`
Thêm vào SOURCES:
```
src/form/pattern/pattern_canvas.cpp
src/form/pattern/add_pattern_wizard.cpp
src/form/pattern/edit_pattern_wizard.cpp
```
Thêm vào HEADERS:
```
src/form/pattern/pattern_theme.h
src/form/pattern/pattern_canvas.h
src/form/pattern/add_pattern_wizard.h
src/form/pattern/edit_pattern_wizard.h
```

---

## 4. Layout sau refactor

```
┌─────────────────────┬─────────────────────┬───────────────────┐
│  Pane 1: Monitor    │  Pane 2: Library    │  Pane 3: Inspector│
│                     │                     │                   │
│  Toolbar            │  PATTERN LIBRARY    │  MATCH CONFIG     │
│  ─ Trigger          │  ┌───────────────┐  │  ┌──────────────┐ │
│  ─ Open Image       │  │ ▸ Group_1   3p│  │  │ Property     │ │
│  ─ Workspace        │  │   ● Pattern_A │  │  │ browser      │ │
│  ─ Run Match        │  │   ● Pattern_B │  │  │              │ │
│                     │  │ ▸ Group_2   2p│  │  │              │ │
│  Camera View        │  └───────────────┘  │  │              │ │
│                     │                     │  │              │ │
│  KPIs               │  ─── separator ───  │  │              │ │
│                     │                     │  │              │ │
│  Result Table       │  SELECTED PATTERN   │  │              │ │
│                     │  ┌───────────────┐  │  │              │ │
│                     │  │  THUMBNAIL    │  │  │              │ │
│                     │  │  (140-220 px) │  │  │              │ │
│                     │  └───────────────┘  │  │              │ │
│                     │  No pattern        │  │              │ │
│                     │  selected           │  └──────────────┘ │
└─────────────────────┴─────────────────────┴───────────────────┘
```

**Trước refactor**: thumbnail nằm ở Pane 3 trên cùng, pattern library ở Pane 2.
**Sau refactor**: thumbnail nằm ngay dưới pattern library trong Pane 2 — đúng
như yêu cầu của user và design handoff.

---

## 5. Wizards visual

### Add Pattern Wizard (920×640)
```
┌──────────────────────────────────────────────────────────┐
│ New Pattern Wizard                                    ✕  │
│ Group: Part_A · step 1 of 5 — Capture or load source     │
├──────────────────────────────────────────────────────────┤
│ ① Image  ② Crop  ③ Pick Point  ④ Picking Box  ⑤ Finish  │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ┌──────────────────────┐  Pattern Name                  │
│  │                      │  [____________]                │
│  │   Image preview      │  Pattern Number                │
│  │   (CW × CH)          │  [____________]                │
│  │                      │  ─────────────                 │
│  │                      │  Image Source                  │
│  │                      │  [📷 Capture from Camera ]    │
│  │                      │  [📁 Open from File       ]   │
│  └──────────────────────┘                                │
│                                                          │
├──────────────────────────────────────────────────────────┤
│ ✓ Image ready · proceed to crop  [Cancel] [← Back] [Next →]│
└──────────────────────────────────────────────────────────┘
```

### Edit Pattern Wizard (920×620)
- Step 1 (Identity): canvas hiển thị image + scrim tím + 🔒 LOCKED badge
- Step 2-3: same as Add wizard
- Step 4 (Finish): diff view với HTML rich text — fields đổi: `old strikethrough → new bold ok-color`

---

## 6. Cách sử dụng

### Add Pattern (existing flow)
User click `+ Pattern` button trong tree → trigger `addPatternRequested(groupIndex)`
→ `onTreeAddPatternRequested(groupIndex)` mở `AddPatternWizard`.

### Edit Pattern (new — wire-up tự chọn)
Có 2 cách invoke wizard:

```cpp
// 1. Programmatic edit cho pattern cụ thể
patternsWidget->editPattern(groupNumber, patternNumber);

// 2. Edit pattern đang chọn trong tree
patternsWidget->editSelectedPattern();
```

Recommended wire-ups:
- Toolbar button "Edit Selected" → `editSelectedPattern()`
- Right-click context menu trên pattern row → `editPattern(g, p)`
- Pattern tree double-click → `editPattern(g, p)` (sau khi sửa
  `PatternTreeWidget` để forward double-click thay vì rename)
- Keyboard shortcut F2 → `editSelectedPattern()`

---

## 7. Để build sau khi áp dụng

```
qmake ncr_picking.pro
make
```

Wizards sẽ được link như deps mới của `localization_patterns_widget.cpp`.

---

## 8. Files cần để ý sau

1. **Camera capture image** — wizard yêu cầu host gọi `setCameraImage(mat)`.
   Đã wire trong `LocalizationPatternsWidget::setCameraImage()` để forward
   tới `m_activeAddWizard` nếu wizard đang mở.

2. **Coordinate mapping** — wizard work in display pixels (560×380). Khi áp
   dụng vào MatchPattern thực, cần map về pixel coordinates của image gốc
   (theo camera calibration scale factor).

3. **PatternTreeWidget edit signal** — hiện chưa có `editPatternRequested`
   signal trong tree. Để có UI button edit trên mỗi pattern row (như design),
   cần thêm signal trong `pattern_tree_widget.h/.cpp` và emit từ
   `PatternItemWidget`.

---

_Implementation hoàn tất theo design handoff `ui_scratch/design_handoff_full_project`._

---

## 10. Update — `PatternSettingPanel` (under-tree inline editor)

Thêm widget mới `PatternSettingPanel` (`pattern_setting_panel.{h,cpp}`) đặt
DƯỚI pattern library tree trong Pane 2, mirror đúng `PatternSetting` từ
`PatternManager.jsx`.

### Layout sau refactor (Pane 2)

```
┌─────── PATTERN LIBRARY ──────┐
│ ▸ Group_1                    │  ← top: tree (resizable)
│   ● Pattern_A                │
│   ● Pattern_B                │
│ ▸ Group_2                    │
│ ▸ Group_3                    │
├──── Vertical Splitter ───────┤
│ ┌─ #N PatternName  ●LEARNED ┐│  ← bottom: PatternSettingPanel
│ │ ┌──────────────────────┐  ││
│ │ │   thumbnail          │  ││
│ │ └──────────────────────┘  ││
│ │ [▶ Trigger&Learn] [📁]   ││
│ ├─ MATCH SETTINGS ──────────┤│
│ │ Pattern Number   #N       ││
│ │ Min Score        [0.85]   ││
│ │ Angle            [0°]     ││
│ │ Tolerance Angle  [180°]   ││
│ │ Max Overlap      [0.10]   ││
│ │ Pick X           [123]    ││
│ │ Pick Y           [456]    ││
│ ├─ EDGE MATCH CONFIG ───────┤│
│ │ Thresh Lower / Upper      ││
│ │ Kernel Size  [3 ▾]        ││
│ │ Blur W × H                ││
│ │ Greediness                ││
│ │ Invert Binary  [○━]       ││
│ │ Sub-pixel      [○━]       ││
│ │ Stop at L1     [○━]       ││
│ └───────────────────────────┘│
└──────────────────────────────┘
```

### 3 trạng thái nội bộ (QStackedWidget)

1. **Empty** — "Select a group or pattern" (centered hint)
2. **Group only** — Group Config + Picking Box props
3. **Pattern selected** — Title + thumbnail + Trigger&Learn/Open Image + Match Settings + Edge Match Config

### Signals → Host

```cpp
void learnRequested();                                      // →  trigger camera capture
void openImageRequested();                                  // →  file dialog
void patternFieldChanged(const QString &key, QVariant v);   // pattern config edits
void groupFieldChanged  (const QString &key, QVariant v);   // group config edits
```

Host (`LocalizationPatternsWidget`) routes:
- `learnRequested` → `onTriggerCameraClicked()`
- `openImageRequested` → `onChooseImageClicked()`
- `patternFieldChanged` → `onSettingPanelPatternFieldChanged()` → applies via `MatchPattern::setConfig`
- `groupFieldChanged` → `onSettingPanelGroupFieldChanged()` → applies via `PatternGroupManager::setGroupConfig`
  (MatchGroup::setConfig is private; manager validates uniqueness)

### Files

| File                                | Vai trò                                              |
|-------------------------------------|------------------------------------------------------|
| `pattern_setting_panel.h`           | Class declaration with empty/group/pattern stacked pages |
| `pattern_setting_panel.cpp`         | Implementation: makePlaceholderPixmap (grid+reticle thumb), buildPropRow, toggle, all editor wiring |

### Files đã sửa thêm

| File                                 | Thay đổi                                              |
|--------------------------------------|-------------------------------------------------------|
| `localization_patterns_widget.h`     | Thêm member `m_settingPanel`, slots `onSettingPanelPatternFieldChanged/GroupFieldChanged` |
| `localization_patterns_widget.cpp`   | (1) Hide legacy `wg_thumb_container` — replaced by setting panel; (2) Wrap [tree, PatternSettingPanel] in vertical QSplitter inside `pane_library`; (3) Wire selection from `selectGroup/selectPattern/clearSelection` → `m_settingPanel->setSelection()`; (4) Implement field-change slots that update MatchPattern via `pat->setConfig()` and MatchGroup via `m_patternManager->setGroupConfig()`; (5) Forward `updatePatternThumb` → `m_settingPanel->setPatternThumbnail()` |
| `ncr_picking.pro`                    | Thêm `pattern_setting_panel.{cpp,h}`                 |

### Bố cục cuối cùng (toàn widget)

```
┌─────────────────────┬─────────────────────┬───────────────────┐
│  Pane 1: Monitor    │  Pane 2 (resizable) │  Pane 3: Property │
│                     │                     │  Browser          │
│  Toolbar            │  PATTERN LIBRARY    │  ┌──────────────┐ │
│  Camera View        │  ┌─ Tree ─────────┐ │  │  Group       │ │
│  KPI strip          │  │ ▸ Group_1     │ │  │  Settings    │ │
│  Result Table       │  │   ● Pat_A     │ │  │              │ │
│                     │  │   ● Pat_B     │ │  │  Pattern     │ │
│                     │  └───────────────┘ │  │  Identity    │ │
│                     │  ─── splitter ───  │  │              │ │
│                     │  PATTERN SETTING   │  │  Match       │ │
│                     │  ┌─ thumb ───────┐ │  │  Parameters  │ │
│                     │  │               │ │  │              │ │
│                     │  └───────────────┘ │  │  Edge Config │ │
│                     │  [Trigger&Learn]  │  │              │ │
│                     │  Match Settings   │  └──────────────┘ │
│                     │   Min Score [..]  │                   │
│                     │   Angle     [..]  │                   │
│                     │  Edge Config      │                   │
│                     │   ...             │                   │
└─────────────────────┴─────────────────────┴───────────────────┘
```

Pane 2 và Pane 3 **đều có** props editor — design intent: pane 2 là context-specific
inline editor (always visible khi có selection), pane 3 là collapsible schema browser
(filterable, có search bar, Apply/Revert).

---

## 11. Update — Bug fixes & Result Table

### Bugs đã sửa (action chưa hoạt động đúng)

| Bug | Mô tả | Fix |
|-----|-------|-----|
| Tree empty on init | `wireManagerSignals` chỉ connect signals, không pull state hiện tại của manager → tree trống dù project có sẵn groups | Thêm `rebuildTreeFromManager()` gọi đầu tiên trong `wireManagerSignals` — pull `m_patternManager->groups()` + patterns vào tree |
| `groupRemoved` không xóa khỏi tree | Slot chỉ rebuild combo + count, không gọi `tree->removeGroup` | Thêm `ui->treeWidget_patternEditor->removeGroup(removed.m_groupIndex)` + clear panel selection nếu group bị xóa đang được chọn |
| `patternRemoved` không xóa khỏi tree | Slot chỉ updateGroupsCount | Thêm `tree->removePattern(group->number(), removed.m_patternIndex)` + cập nhật panel selection |
| `groupChanged` không refresh panel | Manager fire signal khi config đổi nhưng PatternSettingPanel không refresh | Connect `groupChanged` → nếu group đang chọn → `m_settingPanel->setSelection(group, currentPattern)` |
| `patternChanged` không refresh panel | Tương tự | Connect `patternChanged` → nếu pattern đang chọn → `m_settingPanel->setSelection(group, pattern)` |
| `groupAdded` chỉ thêm group, mất pre-existing patterns | `MatchGroupConfig gr` mới chỉ có name/number — patterns rỗng | Mirror toàn bộ `group->patterns()` vào `gr.patterns` trước khi gọi `tree->addGroup(gr)` |

### Result Table (mới)

Thêm `QTableWidget` 8 columns ngay dưới KPI strip trong Pane 1 — mirror đúng
`MonitorPane > Result Table` từ design handoff.

```
┌─ MATCH RESULTS ─────────────────────────────────────────────────────┐
│ #  | Pat # | Pattern Name        | Score    | X     | Y    | Angle | OK │
│ 1  | #1    | Face_Front          | 0.967    | 823.4 | 512.1| 12.4° | OK │  ← green score (≥0.85)
│ 2  | #2    | Face_Side           | 0.881    | 614.2 | 480.9| 45.2° | OK │
│ 3  | #1    | Face_Front          | 0.812    | 420.7 | 310.5|178.1° | OK │  ← orange score (≥0.70)
│ 4  | #3    | Face_Corner         | 0.541    | 290.0 | 180.3| 92.0° | NG │  ← red score
└──────────────────────────────────────────────────────────────────────┘
```

API:
- `buildResultTable()` — gọi 1 lần trong `initWidget`. Tạo table + section header,
  insert vào cuối `monitorLayout` (sau frame_kpi).
- `populateResultTable(const mtc::MatchResult &result)` — gọi sau mỗi run
  matching (trong `onRunMatchingClicked`). Iterate `result.Objects` và fill
  rows: `pattern_index`, `pattern_name`, `matched_Score`, `point_Center.x/y`,
  `matched_Angle`. Score color-coded theo design (≥0.85 OK green, ≥0.70 WARN
  orange, else ERR red).
- `clearResultTable()` — gọi trong `resetKpis` để table trống khi KPIs reset.

Style: dark theme tokens (background `ptn::BG`, header `ptn::SURF`, selected row
`rgba(43,140,232,28)`), giống design handoff.

### Scrollable Pattern Setting

`PatternSettingPanel` group + pattern pages giờ wrap trong `QScrollArea` —
tránh việc các property rows bị cắt khi panel quá ngắn.

### Files đã sửa thêm (turn này)

| File | Thay đổi |
|------|----------|
| `localization_patterns_widget.h` | Thêm `rebuildTreeFromManager`, `buildResultTable`, `populateResultTable`, `clearResultTable`, member `m_resultTable` |
| `localization_patterns_widget.cpp` | (1) `wireManagerSignals` — initial population + sync remove + connect groupChanged/patternChanged → panel refresh + groupAdded mirror patterns; (2) `buildResultTable` + populate/clear; (3) Wire vào `onRunMatchingClicked` (call populateResultTable) và `resetKpis` (call clearResultTable) |
| `pattern_setting_panel.cpp` | Wrap group + pattern pages trong QScrollArea (helper `wrapInScroll`) |
