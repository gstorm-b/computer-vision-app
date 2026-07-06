#ifndef ITEM_ROI_H
#define ITEM_ROI_H

#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>

class ItemRoi : public QGraphicsRectItem {
public:
  enum { Type = UserType + 27 };

  ItemRoi(const QRectF &rect,
                 QGraphicsItem *parent = nullptr,
                 bool *ignore_flag = nullptr);
  int type() const override { return Type; }
  QRectF getRoi();
  QPixmap getCroppedFromRoi(ItemRoi *roi);
  void setBoundingColorNormal(QColor normal);
  void setBoundingColorSelected(QColor selected);
  void setEnableDrawCenter(bool enable);

protected:
  enum HandlePosition {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
  };

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;
  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                      const QVariant &value) override;
  QRectF handleRect(HandlePosition pos) const;
  HandlePosition getHandleAt(const QPointF &pos) const;
  qreal currentLevelOfDetail() const;
  qreal effectiveHandleSize() const;

  bool isInsideParent(QRectF &new_rect);
  bool correctRectItem(QRectF &moved_rect, QPointF &corrected);

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
  bool m_is_draw_center;
  QColor m_bounding_color_normal;
  QColor m_bounding_color_selected;
  // handle point size
  qreal m_handle_size;
  // offset to draw handle
  HandlePosition m_current_handle;
  // variables for resize
  QRectF m_original_rect;
  QPointF m_press_pos;

  QPointF m_last_center;

  bool *m_ignore;
};


#endif // ITEM_ROI_H
