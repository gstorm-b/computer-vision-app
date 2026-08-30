#include "rowhoverdelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

RowHoverDelegate::RowHoverDelegate(QAbstractItemView *view)
    : QStyledItemDelegate(view)
    , m_view(view)
{
    if (!m_view)
        return;

    m_view->setMouseTracking(true);
    m_view->viewport()->installEventFilter(this);
    m_view->setItemDelegate(this);
}

void RowHoverDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;

    // initStyleOption() is called here rather than through
    // QStyledItemDelegate::paint(), because that overload runs it a second
    // time on its own copy and resets backgroundBrush from the model, which
    // would throw the hover tint away.
    initStyleOption(&opt, index);

    if (m_hoveredRow >= 0 && index.row() == m_hoveredRow
        && !(opt.state & QStyle::State_Selected)) {
        // A translucent tint keeps the alternating row colours visible and
        // follows the palette, so it works in light and dark themes alike.
        QColor tint = opt.palette.color(QPalette::Highlight);
        tint.setAlpha(48);
        opt.backgroundBrush = QBrush(tint);
    }

    // The style would otherwise paint its own single cell hover on top.
    opt.state &= ~QStyle::State_MouseOver;

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
}

bool RowHoverDelegate::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && watched == m_view->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            const QPoint pos = static_cast<QMouseEvent *>(event)->position().toPoint();
            setHoveredRow(m_view->indexAt(pos).row());
            break;
        }
        case QEvent::Leave:
        case QEvent::Wheel:
            setHoveredRow(-1);
            break;
        default:
            break;
        }
    }
    return QStyledItemDelegate::eventFilter(watched, event);
}

void RowHoverDelegate::setHoveredRow(int row)
{
    if (row == m_hoveredRow)
        return;
    m_hoveredRow = row;
    if (m_view)
        m_view->viewport()->update();
}
