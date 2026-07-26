#include "device_row_delegate.h"
#include "core/utils/theme_manager.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSpinBox>

/// UI widgets used by the PLC monitor/control views.
namespace vc::widgets {

namespace {

// ── Per-row geometry constants ───────────────────────────────────────────────
constexpr int kRowHeight       = 44;
constexpr int kCellPadX        = 8;
constexpr int kCellPadY        = 8;
constexpr int kBtnHeight       = 28;
constexpr int kBtnSpacing      = 6;
constexpr int kChipHeight      = 24;
constexpr int kChipMinWidth    = 60;
constexpr int kWordValueWidth  = 100;
constexpr int kWordWriteWidth  = 78;

/// Builds the mono-space font used for addresses/values: prefers "JetBrains Mono",
/// falling back to "Consolas" when JetBrains Mono isn't installed (detected via
/// QFontInfo::fixedPitch()).
QFont monoFont(int pointSize, bool bold = false) {
    QFont f("JetBrains Mono");
    if (!QFontInfo(f).fixedPitch()) f = QFont("Consolas");
    f.setPointSize(pointSize);
    f.setBold(bold);
    return f;
}

/// Builds the "Segoe UI" sans-serif font used for button labels, optionally with
/// absolute letter spacing (letterSpacing left at 0 leaves Qt's default spacing).
QFont sansFont(int pointSize, bool bold = false, qreal letterSpacing = 0.0) {
    QFont f("Segoe UI");
    f.setPointSize(pointSize);
    f.setBold(bold);
    if (letterSpacing != 0.0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    return f;
}

/// Resolves a named ThemeManager color token to a QColor for the current
/// light/dark theme.
QColor tokenColor(const char *name) {
    return QColor(ThemeManager::tokenValue(QString::fromLatin1(name),
                                           ThemeManager::instance()->isDark()));
}

} // namespace


/// Constructs the delegate, storing mode for the lifetime of the object.
DeviceRowDelegate::DeviceRowDelegate(Mode mode, QObject *parent)
    : QStyledItemDelegate(parent), m_mode(mode) {}


// ── Geometry helpers ────────────────────────────────────────────────────────

/// Splits the inner (padded) action cell into three button rects for
/// SubOn/SubOff/SubToggle using fixed width ratios (26/30/44) so TOGGLE's
/// longer label has room; returns an empty QRect for any other btnIndex.
QRect DeviceRowDelegate::bitButtonRect(const QRect &cell, int btnIndex) const {
    const QRect inner = cell.adjusted(kCellPadX, kCellPadY,
                                      -kCellPadX, -kCellPadY);
    // 3 buttons share the available width; favour TOGGLE getting more so
    // the label doesn't elide.  Fixed proportions match the painted labels.
    constexpr int kRatioOn     = 26;
    constexpr int kRatioOff    = 30;
    constexpr int kRatioToggle = 44;
    const int totalRatio = kRatioOn + kRatioOff + kRatioToggle;
    const int spacings = 2 * kBtnSpacing;
    const int avail = inner.width() - spacings;
    const int wOn     = (avail * kRatioOn)     / totalRatio;
    const int wOff    = (avail * kRatioOff)    / totalRatio;
    const int wToggle = avail - wOn - wOff;

    const int y = inner.y() + (inner.height() - kBtnHeight) / 2;
    int x = inner.x();

    if (btnIndex == SubOn)     return { x,                                       y, wOn,     kBtnHeight };
    x += wOn + kBtnSpacing;
    if (btnIndex == SubOff)    return { x,                                       y, wOff,    kBtnHeight };
    x += wOff + kBtnSpacing;
    if (btnIndex == SubToggle) return { x,                                       y, wToggle, kBtnHeight };
    return {};
}

/// Returns the rect of the Word-mode value box, anchored at the inner (padded)
/// cell's top-left corner with fixed width kWordValueWidth and height kBtnHeight.
QRect DeviceRowDelegate::wordValueRect(const QRect &cell) const {
    const QRect inner = cell.adjusted(kCellPadX, kCellPadY,
                                      -kCellPadX, -kCellPadY);
    const int y = inner.y() + (inner.height() - kBtnHeight) / 2;
    return { inner.x(), y, kWordValueWidth, kBtnHeight };
}

/// Returns the rect of the Word-mode WRITE button, placed kBtnSpacing to the
/// right of wordValueRect() within the inner (padded) cell.
QRect DeviceRowDelegate::wordWriteRect(const QRect &cell) const {
    const QRect inner = cell.adjusted(kCellPadX, kCellPadY,
                                      -kCellPadX, -kCellPadY);
    const int y = inner.y() + (inner.height() - kBtnHeight) / 2;
    return { inner.x() + kWordValueWidth + kBtnSpacing, y,
             kWordWriteWidth, kBtnHeight };
}

/// Tests localPos against each Bit-mode button rect in turn (ON, then OFF, then
/// TOGGLE) and returns the first match, or SubNone if none contain it.
DeviceRowDelegate::SubButton
DeviceRowDelegate::hitTestBit(const QRect &cell, const QPoint &localPos) const {
    if (bitButtonRect(cell, SubOn)    .contains(localPos)) return SubOn;
    if (bitButtonRect(cell, SubOff)   .contains(localPos)) return SubOff;
    if (bitButtonRect(cell, SubToggle).contains(localPos)) return SubToggle;
    return SubNone;
}

/// Tests localPos against the Word-mode value rect and WRITE button rect and
/// returns the first match (SubValue, then SubWrite), or SubNone if neither
/// contains it.
DeviceRowDelegate::SubButton
DeviceRowDelegate::hitTestWord(const QRect &cell, const QPoint &localPos) const {
    if (wordValueRect(cell).contains(localPos)) return SubValue;
    if (wordWriteRect(cell).contains(localPos)) return SubWrite;
    return SubNone;
}


// ── sizeHint ────────────────────────────────────────────────────────────────

/// Takes the base class's size hint and raises its height to at least
/// kRowHeight (44 px) while leaving the width untouched.
QSize DeviceRowDelegate::sizeHint(const QStyleOptionViewItem &opt,
                                  const QModelIndex &idx) const {
    QSize s = QStyledItemDelegate::sizeHint(opt, idx);
    return { s.width(), qMax(s.height(), kRowHeight) };
}


// ── paint ──────────────────────────────────────────────────────────────────

/// Fills the cell's background (alternating bg.window/bg.surface by row
/// parity, overridden with selection.bg when selected) and draws a
/// border.default separator line along the row's bottom edge.
void DeviceRowDelegate::paintBackground(QPainter *p,
                                        const QStyleOptionViewItem &opt,
                                        const QModelIndex &idx) const {
    p->save();
    const QColor bgBase = tokenColor("bg.window");
    const QColor bgAlt  = tokenColor("bg.surface");
    const bool   alt    = (idx.row() % 2) == 1;
    p->fillRect(opt.rect, alt ? bgAlt : bgBase);

    if (opt.state & QStyle::State_Selected) {
        p->fillRect(opt.rect, tokenColor("selection.bg"));
    }

    // Bottom row separator
    p->setPen(tokenColor("border.default"));
    p->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
    p->restore();
}

/// Draws the address cell's centered, bold mono text: 'M' prefix in Bit mode or
/// 'D' prefix in Word mode, followed by address zero-padded to 4 digits.
void DeviceRowDelegate::paintAddress(QPainter *p, const QRect &cell,
                                     int address) const {
    p->save();
    p->setPen(tokenColor("device.plc"));
    p->setFont(monoFont(10, /*bold*/ true));
    const QChar prefix = m_mode == Bit ? QLatin1Char('M') : QLatin1Char('D');
    const QString text = QString("%1%2").arg(prefix)
                              .arg(address, 4, 10, QChar('0'));
    p->drawText(cell, Qt::AlignCenter, text);
    p->restore();
}

/// Draws the Bit-mode state chip: a rounded pill centered in cell, its width
/// sized to fit "OFF" plus 28px of padding (or kChipMinWidth if larger). Uses
/// the success/green palette with "ON" text when on, otherwise the neutral
/// elevated/muted palette with "OFF" text.
void DeviceRowDelegate::paintChip(QPainter *p, const QRect &cell,
                                  bool on) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const int chipW = qMax(kChipMinWidth,
                           p->fontMetrics().horizontalAdvance("OFF") + 28);
    const QRect r(cell.center().x() - chipW / 2,
                  cell.center().y() - kChipHeight / 2,
                  chipW, kChipHeight);

    QColor bg, fg, border;
    if (on) {
        bg     = tokenColor("state.success.surface");
        border = tokenColor("device.plc.tint.bd");
        fg     = tokenColor("state.success");
    } else {
        bg     = tokenColor("bg.elevated");
        border = tokenColor("border.default");
        fg     = tokenColor("text.muted");
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p->fillPath(path, bg);
    p->setPen(QPen(border, 1));
    p->drawPath(path);

    p->setPen(fg);
    QFont f = monoFont(8, /*bold*/ true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    p->setFont(f);
    p->drawText(r, Qt::AlignCenter, on ? "ON" : "OFF");
    p->restore();
}

/// Draws the Word-mode live value as centered, bold 11pt mono text.
void DeviceRowDelegate::paintWordVal(QPainter *p, const QRect &cell,
                                     int value) const {
    p->save();
    p->setPen(tokenColor("text.primary"));
    p->setFont(monoFont(11, /*bold*/ true));
    p->drawText(cell, Qt::AlignCenter, QString::number(value));
    p->restore();
}

/// Draws one rounded pill-shaped button with `label` centered inside `r`.
/// primary buttons use the accent palette (pressed/hovered/idle shades of
/// accent color, text.on-accent label); non-primary buttons use a neutral
/// outline (bg.elevated/bg.window fill, border.default/accent.primary border,
/// text.muted/text.primary label) depending on pressed/hovered.
void DeviceRowDelegate::paintButton(QPainter *p, const QRect &r,
                                    const QString &label,
                                    bool primary, bool pressed,
                                    bool hovered) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    QColor bg, border, fg;
    if (primary) {
        bg     = pressed ? tokenColor("accent.pressed")
               : hovered ? tokenColor("accent.bright")
                         : tokenColor("accent.primary");
        border = bg;
        fg     = tokenColor("text.on-accent");
    } else {
        bg     = pressed ? tokenColor("bg.window") : tokenColor("bg.elevated");
        border = hovered ? tokenColor("accent.primary")
                         : tokenColor("border.default");
        fg     = hovered ? tokenColor("text.primary")
                         : tokenColor("text.muted");
    }

    QPainterPath path;
    path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p->fillPath(path, bg);
    p->setPen(QPen(border, 1));
    p->drawPath(path);

    p->setPen(fg);
    QFont f = sansFont(8, /*bold*/ true, 0.6);
    p->setFont(f);
    p->drawText(r, Qt::AlignCenter, label);
    p->restore();
}

/// Draws the three Bit-mode action buttons (ON/OFF/TOGGLE) via paintButton(),
/// marking whichever one matches m_pressedRow/m_pressedBtn for row as pressed.
void DeviceRowDelegate::paintBitAction(QPainter *p, const QRect &cell,
                                       int row) const {
    static const struct { SubButton id; const char *label; } kBtns[] = {
        { SubOn,     "ON"     },
        { SubOff,    "OFF"    },
        { SubToggle, "TOGGLE" },
    };
    for (const auto &b : kBtns) {
        const QRect br = bitButtonRect(cell, b.id);
        const bool pressed = (m_pressedRow == row && m_pressedBtn == b.id);
        paintButton(p, br, QString::fromLatin1(b.label),
                    /*primary*/ false, pressed, /*hovered*/ false);
    }
}

/// Draws the Word-mode action cell: a spinbox-styled value box (bg.input fill,
/// border.default border) showing pending right-aligned in mono font, followed
/// by the WRITE button via paintButton() (marked pressed when m_pressedRow/
/// m_pressedBtn match row/SubWrite).
void DeviceRowDelegate::paintWordAction(QPainter *p, const QRect &cell,
                                        int row, int pending) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // Value box — looks like a spinbox, content shows current pending value
    const QRect valueRect = wordValueRect(cell);
    QPainterPath valPath;
    valPath.addRoundedRect(QRectF(valueRect).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    p->fillPath(valPath, tokenColor("bg.input"));
    p->setPen(QPen(tokenColor("border.default"), 1));
    p->drawPath(valPath);

    p->setPen(tokenColor("text.primary"));
    p->setFont(monoFont(10));
    p->drawText(valueRect.adjusted(8, 0, -8, 0),
                Qt::AlignVCenter | Qt::AlignRight,
                QString::number(pending));
    p->restore();

    // Write button
    const QRect btnRect = wordWriteRect(cell);
    const bool pressed = (m_pressedRow == row && m_pressedBtn == SubWrite);
    paintButton(p, btnRect, QStringLiteral("WRITE"),
                /*primary*/ true, pressed, /*hovered*/ false);
}

/// Paints one cell: fills the row background/selection/separator, then
/// dispatches by column — ColAddress to paintAddress(), ColDescription to a
/// default-style left-aligned text draw of Qt::DisplayRole, ColState to
/// paintChip()/paintWordVal(), and ColAction to paintBitAction()/
/// paintWordAction() — depending on the active Mode.
void DeviceRowDelegate::paint(QPainter *p,
                              const QStyleOptionViewItem &opt,
                              const QModelIndex &idx) const {
    paintBackground(p, opt, idx);

    const int col = idx.column();

    if (col == ColAddress) {
        const int address = idx.data(AddressRole).toInt();
        paintAddress(p, opt.rect, address);
        return;
    }

    if (col == ColDescription) {
        // Default text drawing — the QTableWidgetItem already holds the
        // user's editable description string under Qt::DisplayRole.
        p->save();
        p->setPen(tokenColor("text.primary"));
        p->setFont(monoFont(10));
        p->drawText(opt.rect.adjusted(10, 0, -10, 0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    idx.data(Qt::DisplayRole).toString());
        p->restore();
        return;
    }

    if (col == ColState) {
        if (m_mode == Bit) {
            paintChip(p, opt.rect, idx.data(BitStateRole).toBool());
        } else {
            paintWordVal(p, opt.rect, idx.data(WordValueRole).toInt());
        }
        return;
    }

    if (col == ColAction) {
        if (m_mode == Bit) {
            paintBitAction(p, opt.rect, idx.row());
        } else {
            paintWordAction(p, opt.rect, idx.row(),
                            idx.data(PendingWriteRole).toInt());
        }
        return;
    }
}


// ── Mouse handling ──────────────────────────────────────────────────────────

/// Handles mouse press/release within the action column (column 3). Ignores
/// any other column or event type (deferring to the base class). On press,
/// hit-tests the appropriate sub-buttons for the active Mode and records
/// m_pressedRow/m_pressedBtn for the press-flash repaint. On release, the
/// action only fires if the release lands on the same row and sub-button that
/// was pressed: Bit mode emits bitWriteRequested() (1 for ON, 0 for OFF, the
/// inverse of the current BitStateRole for TOGGLE); Word mode emits
/// wordWriteRequested() with the cell's PendingWriteRole for SubWrite, or
/// opens the spinbox editor via QAbstractItemView::edit() for SubValue.
/// @return true when the action-column interaction was handled; otherwise
///         defers to QStyledItemDelegate::editorEvent().
bool DeviceRowDelegate::editorEvent(QEvent *event,
                                    QAbstractItemModel *model,
                                    const QStyleOptionViewItem &opt,
                                    const QModelIndex &idx) {
    if (idx.column() != ColAction)
        return QStyledItemDelegate::editorEvent(event, model, opt, idx);

    auto *me = (event->type() == QEvent::MouseButtonPress
                || event->type() == QEvent::MouseButtonRelease)
                   ? static_cast<QMouseEvent *>(event)
                   : nullptr;
    if (!me) return QStyledItemDelegate::editorEvent(event, model, opt, idx);

    const QPoint pos = me->pos();
    const QRect  cell = opt.rect;

    auto repaint = [&]() {
        if (auto *view = qobject_cast<QAbstractItemView *>(parent()))
            view->viewport()->update(cell);
    };

    if (m_mode == Bit) {
        const SubButton btn = hitTestBit(cell, pos);
        if (event->type() == QEvent::MouseButtonPress) {
            if (btn == SubNone) return false;
            m_pressedRow = idx.row();
            m_pressedBtn = btn;
            repaint();
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const int row = m_pressedRow;
            const SubButton pressed = m_pressedBtn;
            m_pressedRow = -1;
            m_pressedBtn = SubNone;
            repaint();
            if (pressed == SubNone || row != idx.row()) return false;
            if (pressed != btn)                          return false;

            const int address = idx.sibling(idx.row(), ColAddress)
                                    .data(AddressRole).toInt();
            switch (btn) {
            case SubOn:     emit bitWriteRequested(address, 1); break;
            case SubOff:    emit bitWriteRequested(address, 0); break;
            case SubToggle: {
                const bool cur = idx.sibling(idx.row(), ColState)
                                     .data(BitStateRole).toBool();
                emit bitWriteRequested(address, cur ? 0 : 1);
                break;
            }
            default: break;
            }
            return true;
        }
    } else {
        const SubButton btn = hitTestWord(cell, pos);
        if (event->type() == QEvent::MouseButtonPress) {
            if (btn == SubNone) return false;
            m_pressedRow = idx.row();
            m_pressedBtn = btn;
            repaint();
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const int row = m_pressedRow;
            const SubButton pressed = m_pressedBtn;
            m_pressedRow = -1;
            m_pressedBtn = SubNone;
            repaint();
            if (pressed == SubNone || row != idx.row()) return false;
            if (pressed != btn)                          return false;

            const int address = idx.sibling(idx.row(), ColAddress)
                                    .data(AddressRole).toInt();
            if (btn == SubWrite) {
                const qint16 v = static_cast<qint16>(
                                     idx.data(PendingWriteRole).toInt());
                emit wordWriteRequested(address, v);
                return true;
            }
            if (btn == SubValue) {
                // Open the spinbox editor on the same cell.
                if (auto *view = qobject_cast<QAbstractItemView *>(parent()))
                    view->edit(idx);
                return true;
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, opt, idx);
}


// ── Editor (spinbox for Word value) ─────────────────────────────────────────

/// Creates the qint16-ranged (-32768..32767), button-less, right-aligned
/// QSpinBox used to edit the Word-mode pending value, themed from
/// ThemeManager's bg.input/text.primary/border.default/accent.primary tokens.
/// @return the new QSpinBox; nullptr if not in Word mode or idx isn't the
///         action column (leaves the cell non-editable).
QWidget *DeviceRowDelegate::createEditor(QWidget *parent,
                                         const QStyleOptionViewItem &,
                                         const QModelIndex &idx) const {
    if (m_mode != Word || idx.column() != ColAction) return nullptr;

    auto *spin = new QSpinBox(parent);
    spin->setRange(-32768, 32767);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    spin->setStyleSheet(QString(
        "QSpinBox { background-color: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px;"
        "  padding: 0 6px; font: 10pt 'JetBrains Mono'; }"
        "QSpinBox:focus { border-color: %4; }"
    ).arg(ThemeManager::tokenValue(QStringLiteral("bg.input"),
                                   ThemeManager::instance()->isDark()),
          ThemeManager::tokenValue(QStringLiteral("text.primary"),
                                   ThemeManager::instance()->isDark()),
          ThemeManager::tokenValue(QStringLiteral("border.default"),
                                   ThemeManager::instance()->isDark()),
          ThemeManager::tokenValue(QStringLiteral("accent.primary"),
                                   ThemeManager::instance()->isDark())));
    spin->setFocusPolicy(Qt::StrongFocus);
    return spin;
}

/// Seeds the spinbox editor with the cell's current PendingWriteRole value and
/// selects all its text; no-op if editor isn't a QSpinBox.
void DeviceRowDelegate::setEditorData(QWidget *editor,
                                      const QModelIndex &idx) const {
    if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        spin->setValue(idx.data(PendingWriteRole).toInt());
        spin->selectAll();
    }
}

/// Forces the spinbox to interpret any in-progress typed text, then writes its
/// value into the model's PendingWriteRole for idx; no-op if editor isn't a
/// QSpinBox.
void DeviceRowDelegate::setModelData(QWidget *editor,
                                     QAbstractItemModel *model,
                                     const QModelIndex &idx) const {
    if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        spin->interpretText();
        model->setData(idx, spin->value(), PendingWriteRole);
    }
}

/// Sizes/positions the open editor to exactly cover wordValueRect() for the
/// cell's rect; no-op if editor is null.
void DeviceRowDelegate::updateEditorGeometry(QWidget *editor,
                                             const QStyleOptionViewItem &opt,
                                             const QModelIndex &) const {
    if (!editor) return;
    editor->setGeometry(wordValueRect(opt.rect));
}

} // namespace vc::widgets
