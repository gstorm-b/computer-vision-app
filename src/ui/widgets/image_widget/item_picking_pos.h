#ifndef ITEM_PICKING_POS_H
#define ITEM_PICKING_POS_H

#include <QGraphicsItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

class ItemPickingCenter :  public QObject, public QGraphicsItem {
  Q_OBJECT
    // Q_INTERFACES(Q)
public:
  ItemPickingCenter(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  QPainterPath shape() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

  void setDrawHandle(bool enable);

  void setPosMovable(bool enable);
  const bool isMovable();

  void setAxisLength(qreal length);
  void setArrowSize(qreal size);

  void setPositionInParent(QPointF pos);
  QPointF getPositionInParent() const;
  void setAngleInParent(qreal angle);
  qreal getAngleInParent() const;

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
  void positionChanged(QPointF point);
  void angleChanged(qreal angle);

private:
  QRectF moveHandleRect() const;
  QRectF rotateHandleRect() const;
  qreal normalizeRotation(qreal angle) const;

private:
  bool m_drawHandle;
  bool m_movable;
  QPointF m_center;
  QRectF m_moveHandle;
  QRectF m_rotateHandle;
  bool m_draggingMove = false;
  bool m_draggingRotate = false;

  qreal m_rotationAngle = 0;
  qreal m_original_rotation;
  qreal m_valid_rotation;
  QPointF m_rotation_origin;
  qreal m_press_angle;

  qreal m_axisLength = 50;
  qreal m_arrowSize = 10;
  qreal m_centerSize = 5;
};

#endif // ITEM_PICKING_POS_H
