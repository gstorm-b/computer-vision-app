#ifndef ROWHOVERDELEGATE_H
#define ROWHOVERDELEGATE_H

//
// Item views highlight only the cell under the mouse. This delegate tracks the
// hovered row through the viewport and tints every cell of that row, so the
// whole row lights up like it does in a PLC batch monitor.
//
// Construct it with the view: it installs itself as the item delegate, turns on
// mouse tracking and filters the viewport events. Ownership stays with the view.
//

#include <QStyledItemDelegate>

QT_BEGIN_NAMESPACE
class QAbstractItemView;
QT_END_NAMESPACE

class RowHoverDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RowHoverDelegate(QAbstractItemView *view);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setHoveredRow(int row);

    QAbstractItemView *m_view = nullptr;
    int m_hoveredRow = -1;
};

#endif // ROWHOVERDELEGATE_H
