#ifndef DEVICE_ROW_DELEGATE_H
#define DEVICE_ROW_DELEGATE_H

#include <QStyledItemDelegate>

class QAbstractItemView;

namespace vc::widgets {

// ─────────────────────────────────────────────────────────────────────────────
//  DeviceRowDelegate
//
//  Paints every interactive cell of `DevicesMonitorWidget` itself — chip,
//  ON/OFF/TOGGLE buttons, value text and the Word action strip — instead of
//  relying on `setCellWidget()` with native QPushButton / QSpinBox children.
//
//  The Qt native button bevel + focus ring need ~28 px of usable height plus
//  a safety margin; when the cell widget was forced into a 30/40-px row Qt
//  was clipping the button decoration.  Owning the paint loop here gives us
//  full control over geometry and pins the row height through `sizeHint()`.
//
//  Data model
//  ──────────
//  Each row stores its data on the column-0 / column-2 / column-3
//  `QTableWidgetItem` via these custom roles.  See `DevicesMonitorWidget`
//  for how they are populated.
//
//  Click handling
//  ──────────────
//  `editorEvent()` reads mouse press/release positions for the action column
//  (column 3) and emits `bitWriteRequested` / `wordWriteRequested` with the
//  row's address and the appropriate value.
//
//  Word action editing
//  ───────────────────
//  Column 3 in Word mode opens a QSpinBox via `createEditor()` when the user
//  clicks on the value rect.  The committed value is stored in
//  `PendingWriteRole`; the WRITE button then reads it on click.
// ─────────────────────────────────────────────────────────────────────────────

class DeviceRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum Mode { Bit, Word };

    enum Column {
        ColAddress     = 0,
        ColDescription = 1,
        ColState       = 2,    // chip (Bit) or live value (Word)
        ColAction      = 3,    // ON/OFF/TOGGLE (Bit) or [value][WRITE] (Word)
    };

    enum Role {
        AddressRole      = Qt::UserRole + 100,   // int (PLC address)
        BitStateRole     = Qt::UserRole + 101,   // bool
        WordValueRole    = Qt::UserRole + 102,   // int (treated as qint16)
        PendingWriteRole = Qt::UserRole + 103,   // int (treated as qint16)
    };

    enum SubButton {
        SubNone   = -1,
        SubOn     = 0,
        SubOff    = 1,
        SubToggle = 2,
        SubWrite  = 3,
        SubValue  = 4,        // word value rect (clicking it opens the editor)
    };

    explicit DeviceRowDelegate(Mode mode, QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &idx) const override;

    void  paint(QPainter *p, const QStyleOptionViewItem &opt,
                const QModelIndex &idx) const override;

    bool  editorEvent(QEvent *event, QAbstractItemModel *model,
                      const QStyleOptionViewItem &opt,
                      const QModelIndex &idx) override;

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &opt,
                          const QModelIndex &idx) const override;

    void setEditorData (QWidget *editor, const QModelIndex &idx) const override;
    void setModelData  (QWidget *editor, QAbstractItemModel *model,
                        const QModelIndex &idx) const override;

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
    QRect bitButtonRect (const QRect &cell, int btnIndex) const;
    QRect wordValueRect (const QRect &cell) const;
    QRect wordWriteRect (const QRect &cell) const;

    SubButton hitTestBit (const QRect &cell, const QPoint &localPos) const;
    SubButton hitTestWord(const QRect &cell, const QPoint &localPos) const;

    // ── Paint helpers ────────────────────────────────────────────────────
    void paintBackground(QPainter *p, const QStyleOptionViewItem &opt,
                         const QModelIndex &idx) const;

    void paintAddress (QPainter *p, const QRect &cell, int address)              const;
    void paintChip    (QPainter *p, const QRect &cell, bool on)                  const;
    void paintWordVal (QPainter *p, const QRect &cell, int value)                const;
    void paintBitAction (QPainter *p, const QRect &cell, int row)                const;
    void paintWordAction(QPainter *p, const QRect &cell, int row, int pending)   const;

    void paintButton(QPainter *p, const QRect &r,
                     const QString &label,
                     bool primary, bool pressed, bool hovered) const;

    // ── Press tracking (so the user gets a momentary press flash) ────────
    int       m_pressedRow{-1};
    SubButton m_pressedBtn{SubNone};

    Mode m_mode;
};

} // namespace vc::widgets

#endif // DEVICE_ROW_DELEGATE_H
