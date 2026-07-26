#ifndef SIGNALS_MONITOR_WIDGET_H
#define SIGNALS_MONITOR_WIDGET_H

#include "ui/widgets/controls/flat_list_widget.h"

#include <QWidget>
#include <QVariant>
#include <QList>
#include <QString>

class QListWidgetItem;

namespace vc::widgets::sm_internal { class RowWidget; }

/// Read-only monitor of "task signals" already mapped via SignalsMapWidget.
/// One row per logical signal slot, four columns per row:
///
///   `<signal display name> | <type chip> | <current value> | [Modify]`
///
/// The widget is task-agnostic and device-agnostic. The owner page is
/// responsible for:
///   - appendRow() the schema (mirrors SignalsMapWidget's setup).
///   - setRowTag() with the tag bound to each signal from the task config.
///   - refreshBool() / refreshNumber() with live values keyed by tag-name.
///   - setDeviceConnected() to gate the Modify button.
///   - Listening to requestWriteValue() and routing to the device runner.
///
/// Container is a QListWidget with one custom row widget per item. Column
/// widths are auto-sized to the widest content of each column across all
/// rows, so every column visually aligns even though each row is its own
/// widget. See relayoutColumns().
///
/// Bool rows track OFF->ON->OFF transitions that complete inside 200 ms and
/// surface a "Rising edge" chip next to the ON/OFF chip. The chip auto-
/// hides after 2 s; a new rising-edge event while it is still visible
/// triggers a single blink (hide 500 ms, then show again, hide timer
/// restarts).
class SignalsMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    /// Signal value kind driving row rendering: a numeric readout or an
    /// ON/OFF pill with rising-edge detection.
    enum class Type { Number, Bool };
    Q_ENUM(Type)

    /// Constructs an empty monitor (no rows) and loads the theme stylesheet.
    explicit SignalsMonitorWidget(QWidget *parent = nullptr);
    /// Destroys the widget; row widgets are owned by the internal list.
    ~SignalsMonitorWidget() override;

    /// Appends a new row for `internalName` to the end of the list.
    /// A no-op (with a logged warning) if `internalName` already exists.
    /// @param internalName unique key identifying the row
    /// @param displayName label shown in the name column
    /// @param type value kind (Number or Bool), selects value-column rendering
    void appendRow(const QString &internalName,
                   const QString &displayName,
                   Type type);
    /// Inserts a new row for `internalName` at position `row` (clamped to the
    /// current row count). A no-op (with a logged warning) if `internalName`
    /// already exists.
    /// @param row target index in the row list
    /// @param internalName unique key identifying the row
    /// @param displayName label shown in the name column
    /// @param type value kind (Number or Bool), selects value-column rendering
    void insertRowAt(int row,
                     const QString &internalName,
                     const QString &displayName,
                     Type type);
    /// Removes the row identified by `internalName`; no-op if not found.
    void removeRow(const QString &internalName);
    /// Removes all rows from the monitor.
    void clearRows();

    /// Binds a PLC tag (from the task config / SignalsMapWidget output) to
    /// the row identified by `internalName`. An empty tag makes the row
    /// render "(not mapped)" and disables its Modify button.
    void setRowTag(const QString &internalName, const QString &tag);
    /// Returns the tag currently bound to `internalName`, or an empty
    /// string if the row does not exist.
    QString rowTag(const QString &internalName) const;

    /// Updates the displayed value of a Bool row, keyed by signal name
    /// (internalName), not PLC tag. Logs a warning and ignores the call if
    /// `internalName` is unknown or is not a Bool row.
    void refreshBool(const QString &internalName, bool value);
    /// Updates the displayed value of a Number row, keyed by signal name
    /// (internalName), not PLC tag. Logs a warning and ignores the call if
    /// `internalName` is unknown or is not a Number row.
    void refreshNumber(const QString &internalName, int value);

    /// Sets the device connection state, gating every row's Modify button
    /// (disabled while disconnected). Last-received values are preserved
    /// across reconnect cycles.
    void setDeviceConnected(bool connected);
    /// Returns whether the device is currently marked as connected.
    bool isDeviceConnected() const { return m_deviceConnected; }

signals:
    /// Emitted after the user confirms a new value in the Modify dialog.
    /// The owner routes the write to the device runner; the widget does not
    /// self-update — call refreshBool()/refreshNumber() when the device
    /// echoes the new value.
    /// @param internalName row the new value applies to
    /// @param value the confirmed value to write
    void requestWriteValue(const QString &internalName, const QVariant &value);

private:
    /// Reloads the dark/light QSS stylesheet from resources and applies it,
    /// resolving theme tokens via ThemeManager.
    void reloadStyleSheet();
    /// Recomputes and applies uniform column widths (name/type/value/modify)
    /// across all rows from the widest content in each column.
    void relayoutColumns();
    /// Returns the index of the row with the given internalName, or -1 if
    /// not found.
    int  rowIndexOf(const QString &internalName) const;
    /// Opens the ModifyValueDialog seeded with `row`'s current value and, if
    /// accepted, emits requestWriteValue() with the entered value.
    void openModifyDialog(vc::widgets::sm_internal::RowWidget *row);

    FlatListWidget *m_list{nullptr};  ///< List container hosting one RowWidget per item.
    QList<vc::widgets::sm_internal::RowWidget *> m_rows;  ///< Row widgets, in display order.

    bool m_deviceConnected{false};  ///< Gates every row's Modify button.
};

#endif // SIGNALS_MONITOR_WIDGET_H
