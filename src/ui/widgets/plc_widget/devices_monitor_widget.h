#ifndef DEVICES_MONITOR_WIDGET_H
#define DEVICES_MONITOR_WIDGET_H

#include <QPointer>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QTableWidget;

/// UI widgets that present PLC device state and other live monitor panels.
namespace vc::widgets {

class DeviceRowDelegate;

/// Self-contained monitor for either Mitsubishi M-bit or D-word registers,
/// matching the design handoff `PLC Panel` register table:
///
///    ┌──────────────────────────────────────────────────────────────────┐
///    │ M DEVICES — BIT REGISTERS         16 / 64 ACTIVE   [ filter… ]   │  header
///    ├──────────┬──────────────────────────┬──────────┬─────────────────┤
///    │ ADDRESS  │ DESCRIPTION              │ STATE    │ ACTION          │
///    ├──────────┼──────────────────────────┼──────────┼─────────────────┤
///    │ M2000    │ trigger input            │ [ ON ]   │ ON  OFF  TOGGLE │
///    │ M2001    │ vision busy              │ [ OFF ]  │ ON  OFF  TOGGLE │
///    └──────────┴──────────────────────────┴──────────┴─────────────────┘
///
/// Mode::Bit  → State chip + ON/OFF/TOGGLE per row (emits bitWriteRequested).
/// Mode::Word → Numeric value + value box + WRITE per row
///               (emits wordWriteRequested).
///
/// Rendering is owned by `DeviceRowDelegate` — every interactive control is
/// painted by the delegate itself, no per-row `setCellWidget()` widgets.
/// This guarantees the row sizes (44 px) cleanly match the painted geometry
/// with no native QPushButton bevel-clipping.
class DevicesMonitorWidget : public QWidget {
    Q_OBJECT
public:
    /// Register kind monitored by this widget instance: Bit for M-registers
    /// (ON/OFF), Word for D-registers (signed 16-bit values).
    enum class Mode { Bit, Word };

    /// Constructs the widget for the given register kind, builds the header/table
    /// UI, and applies the current theme's stylesheet.
    /// @param mode Bit or Word — fixed for the widget's lifetime
    explicit DevicesMonitorWidget(Mode mode, QWidget *parent = nullptr);
    ~DevicesMonitorWidget() override;

    /// Returns the register kind (Bit or Word) this widget was constructed with.
    Mode mode() const { return m_mode; }

    /// Sets the header title label text.
    void setTitle(const QString &title);
    /// Sets the header subtitle label text.
    void setSubtitle(const QString &subtitle);

    /// Configures the contiguous device range shown and rebuilds all table rows.
    /// @param start_address first device address in the range
    /// @param amount number of consecutive devices in the range
    void setRange(int start_address, int amount);
    /// Returns the first device address of the currently configured range.
    int  startAddress() const { return m_start; }
    /// Returns the number of devices in the currently configured range.
    int  amount()       const { return m_amount; }
    /// Empties the table and resets the configured range to zero devices.
    void clearRows();

    // Live updates from polling.
    /// Updates the ON/OFF chip for `address` if it maps to a currently visible
    /// row; no-op if the value is unchanged or the address is out of range.
    void setBitState(int address, quint8 value);
    /// Updates the numeric value shown for `address` if it maps to a currently
    /// visible row; no-op if the address is out of range.
    void setWordValue(int address, qint16 value);
    /// Resets every row's state (Bit mode) or value (Word mode) to its default
    /// (off / 0) and refreshes the active-count label.
    void clearAllStatuses();

    // Per-row description / comment.
    /// Enables or disables in-place editing of the Description column, applying
    /// the new edit flag to every existing row.
    void        setCommentEditable(bool editable);
    /// Returns the Description text for `address`, or an empty string if the
    /// address has no corresponding row.
    QString     comment(int address) const;
    /// Sets the Description text (and matching tooltip) for the row at `address`.
    void        setComment(int address, const QString &text);
    /// Returns the formatted device name (e.g. "M2000"/"D0100") for every row in
    /// the currently configured range, in row order.
    QStringList allDeviceNames() const;

signals:
    /// Relayed from the delegate when the user requests writing `value` to the
    /// bit register at `address` via the ON/OFF/TOGGLE controls.
    void bitWriteRequested(int address, quint8 value);
    /// Relayed from the delegate when the user requests writing `value` to the
    /// word register at `address` via the WRITE control.
    void wordWriteRequested(int address, qint16 value);

private slots:
    /// Filters visible rows to those whose formatted address or description
    /// contains the trimmed `text` (case-insensitive); shows all rows if empty.
    void onFilterTextChanged(const QString &text);

private:
    /// Builds the header bar (title/subtitle/count/filter) and the device table,
    /// installs the row delegate, and wires its write signals through to this
    /// widget's own bitWriteRequested/wordWriteRequested signals.
    void setupUi();
    /// Reloads the light/dark QSS resource matching the current theme, resolves
    /// its design tokens, applies it as this widget's stylesheet, and repaints
    /// the table.
    void reloadStyleSheet();
    /// Clears and rebuilds every row for the currently configured range (m_start
    /// / m_amount), setting each row to the fixed 44 px height.
    void rebuildRows();
    /// Creates the four QTableWidgetItems for `row` (Address/Description/State-
    /// or-Value/Action), setting the role data and edit flags each column needs;
    /// actual painting is done by DeviceRowDelegate.
    void buildRow(int row, int address);
    /// Recomputes the header count label: "active / total ACTIVE" in Bit mode,
    /// or "total WORDS" in Word mode.
    void recountActive();

    /// Converts a device address to its row index, or -1 if outside the
    /// currently configured range.
    int     rowOfAddress(int address) const;
    /// Converts a row index back to its device address (m_start + row).
    int     addressOfRow(int row)     const;
    /// Builds the display name for `address` — 'M' or 'D' prefix (by mode)
    /// followed by the zero-padded 4-digit address.
    QString formatName(int address)   const;

    Mode m_mode;                                        ///< Register kind fixed at construction (Bit or Word).
    int  m_start{0};                                     ///< First device address in the configured range.
    int  m_amount{0};                                     ///< Number of devices in the configured range.
    bool m_commentEditable{true};                         ///< Whether the Description column is user-editable.

    QFrame              *m_header        {nullptr};       ///< Header bar: title/subtitle, count label, filter box.
    QLabel              *m_titleLabel    {nullptr};       ///< Header title label.
    QLabel              *m_subtitleLabel {nullptr};       ///< Header subtitle label.
    QLabel              *m_countLabel    {nullptr};       ///< Active/total (Bit) or total (Word) count label.
    QLineEdit           *m_search        {nullptr};       ///< Address/description filter box.
    QTableWidget        *m_table         {nullptr};       ///< Device register table (Address/Description/State-or-Value/Action).
    DeviceRowDelegate   *m_delegate      {nullptr};       ///< Paints the state chip / value box / action controls per row.
};

} // namespace vc::widgets

#endif // DEVICES_MONITOR_WIDGET_H
