# SignalsMapWidget

> Mapping editor that lets a task config bind its logical signals to
> communication-device tags (PLC addresses/named tags). Lives at
> [src/ui/widgets/signals_map_widget.{h,cpp}](../../../src/ui/widgets/signals_map_widget.h).

## Purpose

A task config (e.g. `TaskLocalizeConfigPrivate`) declares a fixed set of
**logical signal slots** such as `nActiveCamera` (number) or
`bExecuteTrigger` (bool). These slots must each be bound to a concrete
**communication tag** that the PLC (or other comm device) exposes, e.g.
`D200` for words or `M100` for bits.

`SignalsMapWidget` is the editor for that binding. It is one cell wide,
one row per logical signal, three columns:

| # | Column     | Source / role                                                     |
|---|------------|-------------------------------------------------------------------|
| 0 | Signal     | Display label of the logical slot, read-only.                     |
| 1 | Type       | `Number` or `Bool` chip, read-only.                               |
| 2 | Mapped to  | The chosen comm tag for this slot, editable via a `QLineEdit`.    |

The widget is task-agnostic: any config with a similar set of named
slots can reuse it by listing the rows up-front.

## Row identity

Each row carries two strings:

- **internalName** — the unique key the owner uses to write the chosen
  value back into the model. Conventionally matches the field name
  (e.g. `nActiveCamera`, `bMatchingFinished`).
- **displayName** — what column 0 actually shows to the user
  (e.g. "Camera selection", "Matching finished"). Owner typically
  resolves this from `Q_CLASSINFO("<internalName>_name", "…")` of the
  config gadget.

The change signal (`signalMappingChanged`) carries the **internalName**,
not the display label. Owners route writes by stable identifier.

## Tag list ownership

The widget never queries devices itself. The owning page is responsible
for:

1. Watching which comm device the task currently has assigned
   (typically via `task->devicesChanged()` and
   `assignedDevicesOfType(DeviceType::PLC)`).
2. Calling
   PLC tag capability interfaces (`IDigitalIoProvider` /
   `IWordIoProvider`) to get
   the per-type tag lists.
3. Pushing them into the widget with `setBoolTags(…)` /
   `setNumberTags(…)`.

The Mapped-to column's `QCompleter` is fed from those lists.

## Editor behaviour — column 3

- The editor is a `QLineEdit` (no dropdown arrow) with a
  `QClearButton` and a `QCompleter` (case-insensitive, MatchContains).
- On `focusIn`, the completer pops up the full list immediately
  (no need to type a character first).
- Items in the completer popup that are **already assigned to another
  row** are rendered with grey text plus a `"   (used by <display>)"`
  suffix, via a `QStyledItemDelegate` attached to
  `completer->popup()`.
- The user can still type freely. Any value not in the current tag
  list becomes an **orphan**: kept visible, painted with a warning
  style (yellow background, brown text) and a tooltip. The orphan is
  not auto-cleared — see "checkEmpty".

## Conflict handling (cross-row reassignment)

When the user picks a tag already mapped to another row (either via
the completer or by typing it), `editingFinished` runs a
`QMessageBox::question` modal: `"Tag X is currently mapped to Y.
Reassign it to Z?"` (Yes / No).

- **Yes** — the previous owner row's value is cleared, that row is
  flagged warning, `signalMappingChanged(otherName, "")` is emitted
  for the donor, and `signalMappingChanged(thisName, X)` for the new
  owner.
- **No** — the user's edit is reverted to the previous value (guarded
  by `m_suppressEdit` so the revert does not re-enter the handler).

## Orphan handling (tag list changes)

`setBoolTags / setNumberTags` replaces the per-type list. If a row's
current value is no longer in the new list (e.g. user switched comm
device to one with different tags), the row is repopulated:

- the displayed text is preserved (user sees what was there),
- the editor is marked **warning** (yellow background + tooltip
  `"Tag is not in the current comm device's available list."`),
- the value persists in the model — the orphan is intentionally not
  silently cleared.

This is the same warning state produced by free-typed unknown values.

## API surface

```cpp
enum class Type { Number, Bool };

void appendRow(internalName, displayName, Type);
void insertRowAt(int row, internalName, displayName, Type);
void removeRow(internalName);
void clearRows();

void setRowValue(internalName, tag);
QString rowValue(internalName) const;

void setNumberTags(QStringList);
void setBoolTags(QStringList);

// Destructive validate: any warning-flagged (orphaned) tag is cleared
// to "" and a signalMappingChanged(name, "") is emitted; returns the
// list of internalNames whose tag is now empty.
QStringList checkEmpty();

// For the popup item delegate — returns the displayName of whichever
// row currently owns `tag`, excluding `excludeRow`, or "" if none.
QString tagOwnerDisplay(QString tag, int excludeRow) const;

signals:
    // Emitted after the user successfully changes a row's tag, or after
    // a conflict reassignment clears the donor row, or after checkEmpty
    // purges an orphan. Argument is the row's internalName.
    void signalMappingChanged(QString internalName, QString tag);
```

The widget is a `QTableWidget` subclass; vertical header is hidden,
column 0 stretches, column 1 fits, column 2 stretches. Row count is
controlled solely through the row-management API — never call
`setRowCount` directly.

## Integration recipe (owner side)

```cpp
// 1. Append rows. Display name comes from Q_CLASSINFO meta.
for (const auto &spec : kSignalRows) {
    ui->signals_map->appendRow(
        spec.internalName,
        displayNameOf(spec.internalName),
        spec.type);
}

// 2. Wire the change signal back into the config model.
connect(ui->signals_map, &SignalsMapWidget::signalMappingChanged,
        this, [this](const QString &name, const QString &tag) {
    writeConfigField(m_config, name, tag);
    pushConfigToTask();
});

// 3. Whenever the comm device assignment changes, refresh tag lists.
const auto bits  = digitalProvider->availableDigitalIoNames();
const auto words = wordProvider->availableWordIoNames();
ui->signals_map->setBoolTags(bits);
ui->signals_map->setNumberTags(words);

// 4. On load: push current model values into the widget.
for (const auto &spec : kSignalRows) {
    ui->signals_map->setRowValue(spec.internalName,
                                 readConfigField(m_config, spec.internalName));
}

// 5. Validate before persisting / starting commission.
QStringList missing = ui->signals_map->checkEmpty();
if (!missing.isEmpty()) {
    // Block save / commission, list the missing names.
}
```

A working example is in
[localization_setting_widget.cpp](../../../src/ui/forms/task/localization_setting_widget.cpp)
(see `kSignalRows`, the lambda wired to `signalMappingChanged`, and
`refreshCommTags`).

## State model summary

```
        ┌───────────────┐  tag in list, no other row uses it
        │ user edits    ├──────────────────────────────────► normal
        │ column 3      │
        └──────┬────────┘
               │ tag in list, another row owns it
               ▼
        ┌───────────────┐  Yes ─► steal: donor row → warning(empty)
        │ confirm modal │
        └──────┬────────┘  No  ─► revert (no model change)
               │
   ┌───────────┼───────────┐
   │           │           │
   │ tag NOT   │           │
   │ in list   │           │
   ▼           ▼           ▼
warning    setBoolTags     checkEmpty()
(orphan)   /Number         purges
            replaces       warning→empty
            list           and emits
                           changed signals
```

## Notes for the future Signal Monitor widget

The future status-monitor widget is expected to *read* the same mapping
the editor produces. To minimise coupling and keep both widgets driven
from one source of truth:

- The mapping is owned by the **task config**
  (`TaskLocalizeConfigPrivate::m_n*` / `m_b*`), not the widget.
- Each entry is `(internalName, tag)`. The monitor widget should
  iterate the same schema (`kSignalRows`) the editor uses, look up the
  tag through `readConfigField`, and subscribe to value updates from
  the assigned comm device.
- `Type` (Number / Bool) determines whether the monitor should
  subscribe via the bit-read or word-read channel of the comm device.
- An empty `tag` value means **unmapped** — the monitor should render
  that row as "(not mapped)" rather than treating empty as a literal
  address.
- The monitor must tolerate the editor's *warning / orphan* state on
  values that haven't been validated yet — `tag` may not exist on the
  current comm device. Surfacing that as a per-row warning is the
  monitor's responsibility; reading from a non-existent address should
  be handled by displaying "(unknown tag)", not by crashing.

When the monitor widget lands, factor the shared schema
(`SignalRowSpec[]` and the `displayNameOf` / `readConfigField` /
`writeConfigField` helpers in
[localization_setting_widget.cpp](../../../src/ui/forms/task/localization_setting_widget.cpp))
into a small header that both widgets include.
