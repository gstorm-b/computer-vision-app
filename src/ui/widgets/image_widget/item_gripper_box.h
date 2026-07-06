#ifndef ITEM_GRIPPER_BOX_H
#define ITEM_GRIPPER_BOX_H

#include <QGraphicsItem>
#include <QPainter>

class ItemGripperBox : public QGraphicsItem {
public:
  ItemGripperBox(QGraphicsItem *parent = nullptr);

  qreal distance() const;
  void setDistance(qreal d);

  QSizeF size1() const;
  void setSize1(const QSizeF &s);

  QSizeF size2() const;
  void setSize2(const QSizeF &s);

  qreal placementAngle() const;
  void setPlacementAngle(qreal angleDeg);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
  qreal m_distance;
  QSizeF m_size1;
  QSizeF m_size2;
  qreal m_placementAngle;

  mutable QRectF m_cachedBounding;
  mutable bool m_cacheValid;

  QPointF pointAtAngleAndDistance(qreal angleDeg, qreal dist) const;
};

#endif // ITEM_GRIPPER_BOX_H
