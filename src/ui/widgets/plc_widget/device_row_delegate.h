#ifndef DEVICE_ROW_DELEGATE_H
#define DEVICE_ROW_DELEGATE_H

#include <QStyledItemDelegate>

class QAbstractItemView;

namespace vc::widgets {

/// Item delegate that paints every interactive cell of `DevicesMonitorWidget` itself —
/// chip, ON/OFF/TOGGLE buttons, value text and the Word action strip — instead of
/// relying on `setCellWidget()` with native QPushButton / QSpinBox children. The Qt
/// native button bevel + focus ring need ~28 px of usable height plus a safety margin;
/// when the cell widget was forced into a 30/40-px row Qt was clipping the button
/// decoration, so owning the paint loop here gives full control over geometry and
/// pins the row height through sizeHint().
/// @note Data model: each row stores its data on the column-0 / column-2 / column-3
///       `QTableWidgetItem` via the custom roles below (see the Role enum); see
///       `DevicesMonitorWidget` for how they are populated.
/// @note Click handling: editorEvent() reads mouse press/release positions for the
///       action column (column 3) and emits bitWriteRequested() / wordWriteRequested()
///       with the row's address and the appropriate value.
/// @note Word action editing: column 3 in Word mode opens a QSpinBox via
///       createEditor() when the user clicks on the value rect. The committed value
///       is stored in `PendingWriteRole`; the WRITE button then reads it on click.
class DeviceRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    /// Selects which family of PLC data this delegate renders/edits: discrete bit
    /// devices (chip + ON/OFF/TOGGLE) or 16-bit word devices (value + WRITE).
    enum Mode { Bit, Word };

    /// Column indices of the table this delegate paints; the meaning of ColState and
    /// ColAction depends on the active Mode.
    enum Column {
        ColAddress     = 0,   ///< PLC address text (M#### for Bit, D#### for Word).
        ColDescription = 1,   ///< User-editable free-text description (default-drawn).
        ColState       = 2,    ///< chip (Bit) or live value (Word)
        ColAction      = 3,    ///< ON/OFF/TOGGLE (Bit) or [value][WRITE] (Word)
    };

    /// Custom `QTableWidgetItem` data roles used to store each row's PLC state,
    /// populated by `DevicesMonitorWidget` and read back by this delegate.
    enum Role {
        AddressRole      = Qt::UserRole + 100,   ///< int (PLC address)
        BitStateRole     = Qt::UserRole + 101,   ///< bool
        WordValueRole    = Qt::UserRole + 102,   ///< int (treated as qint16)
        PendingWriteRole = Qt::UserRole + 103,   ///< int (treated as qint16)
    };

    /// Identifies the sub-region of the action cell a mouse position/press maps to,
    /// for hit-testing and press-flash tracking.
    enum SubButton {
        SubNone   = -1,   ///< No sub-region hit.
        SubOn     = 0,    ///< Bit-mode ON button.
        SubOff    = 1,    ///< Bit-mode OFF button.
        SubToggle = 2,    ///< Bit-mode TOGGLE button.
        SubWrite  = 3,    ///< Word-mode WRITE button.
        SubValue  = 4,        ///< word value rect (clicking it opens the editor)
    };

    /// Constructs the delegate for the given mode; mode is fixed for the object's
    /// lifetime and determines which of the Bit/Word paint and hit-test paths run.
    explicit DeviceRowDelegate(Mode mode, QObject *parent = nullptr);

    /// Returns the base class's size hint with its height raised to at least
    /// kRowHeight (44 px) so the Bit/Word action buttons have room to render
    /// without clipping their bevel/focus decoration.
    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &idx) const override;

    /// Paints one cell: draws the row background/selection/separator, then dispatches
    /// by column to paintAddress()/paintChip()/paintWordVal()/paintBitAction()/
    /// paintWordAction() depending on the column and the active Mode.
    void  paint(QPainter *p, const QStyleOptionViewItem &opt,
                const QModelIndex &idx) const override;

    /// Handles mouse press/release within the action column (column 3): tracks the
    /// pressed sub-button for the press-flash repaint, and on a matching release
    /// emits bitWriteRequested()/wordWriteRequested() or opens the spinbox editor
    /// for the Word value rect.
    /// @return true if the event was consumed as an action-cell interaction;
    ///         otherwise defers to QStyledItemDelegate::editorEvent().
    bool  editorEvent(QEvent *event, QAbstractItemModel *model,
                      const QStyleOptionViewItem &opt,
                      const QModelIndex &idx) override;

    /// Creates the QSpinBox editor for the Word-mode action cell (range
    /// -32768..32767, no spin buttons, right-aligned, themed via ThemeManager).
    /// @return the new QSpinBox, or nullptr in Bit mode or for any other column.
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &opt,
                          const QModelIndex &idx) const override;

    /// Initializes the spinbox editor from the cell's current PendingWriteRole
    /// value and selects all of its text so typing replaces it outright.
    void setEditorData (QWidget *editor, const QModelIndex &idx) const override;
    /// Commits the spinbox's interpreted value back into the model's
    /// PendingWriteRole for idx.
    void setModelData  (QWidget *editor, QAbstractItemModel *model,
                        const QModelIndex &idx) const override;

    /// Positions/sizes the open editor to exactly cover the word value rect within
    /// the action cell.
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &opt,
                              const QModelIndex &idx) const override;

signals:
    /// address: absolute PLC bit address (e.g. M2003 → 2003); value: 0/1.
    void bitWriteRequested(int address, quint8 value);

    /// address: absolute PLC word address (e.g. D2003 → 2003).
    void wordWriteRequested(int address, qint16 value);

private:
    // ── Sub-region geometry (relative to the action cell rect) ───────────
    /// Computes the rect of one Bit-mode action button (ON/OFF/TOGGLE) within cell,
    /// splitting the available width by fixed ratios (26/30/44) so TOGGLE's longer
    /// label doesn't elide.
    /// @return the button's rect, or an empty QRect if btnIndex isn't SubOn/SubOff/SubToggle.
    QRect bitButtonRect (const QRect &cell, int btnIndex) const;
    /// Computes the rect of the Word-mode value box (the spinbox look-alike, and the
    /// area createEditor()'s editor is placed over) within cell.
    QRect wordValueRect (const QRect &cell) const;
    /// Computes the rect of the Word-mode WRITE button, placed to the right of the
    /// value box within cell.
    QRect wordWriteRect (const QRect &cell) const;

    /// Determines which Bit-mode button, if any, contains localPos.
    /// @return SubOn, SubOff, or SubToggle; SubNone if localPos misses all three.
    SubButton hitTestBit (const QRect &cell, const QPoint &localPos) const;
    /// Determines whether localPos falls on the Word-mode value rect or WRITE button.
    /// @return SubValue, SubWrite, or SubNone.
    SubButton hitTestWord(const QRect &cell, const QPoint &localPos) const;

    // ── Paint helpers ────────────────────────────────────────────────────
    /// Paints the row's alternating background (bg.window/bg.surface by idx.row()
    /// parity), the selection highlight when the row is selected, and a bottom
    /// separator line.
    void paintBackground(QPainter *p, const QStyleOptionViewItem &opt,
                         const QModelIndex &idx) const;

    /// Paints the address column: bold mono-font text, centered, showing the 'M'/'D'
    /// prefix (by m_mode) followed by address zero-padded to 4 digits.
    void paintAddress (QPainter *p, const QRect &cell, int address)              const;
    /// Paints the Bit-mode state chip: a rounded pill sized to fit "OFF" plus padding,
    /// centered in cell, colored success/green when on and neutral/muted otherwise.
    void paintChip    (QPainter *p, const QRect &cell, bool on)                  const;
    /// Paints the Word-mode live value as centered, bold mono-font text.
    void paintWordVal (QPainter *p, const QRect &cell, int value)                const;
    /// Paints the three Bit-mode action buttons (ON/OFF/TOGGLE) via paintButton(),
    /// flashing the pressed state for whichever button m_pressedRow/m_pressedBtn
    /// currently tracks for row.
    void paintBitAction (QPainter *p, const QRect &cell, int row)                const;
    /// Paints the Word-mode action cell: a spinbox-styled box showing pending
    /// right-aligned, followed by the WRITE button (flashing pressed state for row).
    void paintWordAction(QPainter *p, const QRect &cell, int row, int pending)   const;

    /// Paints one themed pill button: primary uses the accent palette (distinct
    /// pressed/hovered/idle shades), otherwise a neutral outline button.
    /// @note hovered is accepted for API symmetry but every current call site always
    ///       passes false; this delegate does not track hover state.
    void paintButton(QPainter *p, const QRect &r,
                     const QString &label,
                     bool primary, bool pressed, bool hovered) const;

    // ── Press tracking (so the user gets a momentary press flash) ────────
    int       m_pressedRow{-1};        ///< Row index of the currently pressed sub-button, or -1 if none.
    SubButton m_pressedBtn{SubNone};   ///< Which sub-button within m_pressedRow is pressed.

    Mode m_mode;   ///< Bit or Word; fixed at construction, selects the paint/hit-test/editor behavior.
};

} // namespace vc::widgets

#endif // DEVICE_ROW_DELEGATE_H
