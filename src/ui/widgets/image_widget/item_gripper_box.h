#ifndef ITEM_GRIPPER_BOX_H
#define ITEM_GRIPPER_BOX_H

#include <QGraphicsItem>
#include <QPainter>

/// Movable/selectable QGraphicsItem drawing a pair of gripper "finger" squares
/// placed symmetrically at `m_distance` on either side of the item's local
/// origin, along the direction given by `m_placementAngle`.
class ItemGripperBox : public QGraphicsItem {
public:
  ItemGripperBox(QGraphicsItem *parent = nullptr);

  /// Returns the distance from the local origin to each finger square's center.
  qreal distance() const;
  /// Sets the center-to-origin distance for both finger squares, invalidating
  /// the cached bounding rect and repainting if the value actually changed.
  void setDistance(qreal d);

  /// Returns the size of the first finger square (at m_placementAngle).
  QSizeF size1() const;
  /// Sets the size of the first finger square, invalidating the cached
  /// bounding rect and repainting if the value actually changed.
  void setSize1(const QSizeF &s);

  /// Returns the size of the second finger square (at m_placementAngle + 180°).
  QSizeF size2() const;
  /// Sets the size of the second finger square, invalidating the cached
  /// bounding rect and repainting if the value actually changed.
  void setSize2(const QSizeF &s);

  /// Returns the angle (degrees) along which the two finger squares are placed.
  qreal placementAngle() const;
  /// Sets the placement angle in degrees, invalidating the cached bounding
  /// rect and repainting if the value actually changed.
  void setPlacementAngle(qreal angleDeg);

  /// Returns the union of both finger-square rects plus a small margin,
  /// caching the result until geometry-affecting setters invalidate it.
  QRectF boundingRect() const override;
  /// Draws both finger squares as outlined rectangles and a small red dot at
  /// the local origin.
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
  qreal m_distance;       ///< Center-to-origin distance for both finger squares.
  QSizeF m_size1;         ///< Size of the finger square at m_placementAngle.
  QSizeF m_size2;         ///< Size of the finger square at m_placementAngle + 180°.
  qreal m_placementAngle; ///< Placement direction of the two finger squares, in degrees.

  mutable QRectF m_cachedBounding; ///< Cached result of boundingRect(); valid only while m_cacheValid is true.
  mutable bool m_cacheValid;       ///< True when m_cachedBounding reflects the current geometry.

  /// Returns the local-coordinate point at `dist` from the origin along
  /// `angleDeg` (measured counter-clockwise, converted to Qt's y-down axis).
  QPointF pointAtAngleAndDistance(qreal angleDeg, qreal dist) const;
};

#endif // ITEM_GRIPPER_BOX_H
