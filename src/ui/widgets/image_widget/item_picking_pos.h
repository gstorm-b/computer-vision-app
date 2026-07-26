#ifndef ITEM_PICKING_POS_H
#define ITEM_PICKING_POS_H

#include <QGraphicsItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

/// Selectable/movable QGraphicsItem marking a picking position: draws an X/Y
/// axis cross with arrowheads at the item's center plus, optionally, a move
/// handle (center dot) and a rotate handle (offset at 45°) that let the user
/// drag-move and drag-rotate the item interactively.
class ItemPickingCenter :  public QObject, public QGraphicsItem {
  Q_OBJECT
    // Q_INTERFACES(Q)
public:
  ItemPickingCenter(QGraphicsItem *parent = nullptr);

  /// Returns a square centered on the item, sized to fully contain the axes,
  /// arrowheads, and handles.
  QRectF boundingRect() const override;
  /// Returns the hit-test/selection shape: the union of the move-handle and
  /// rotate-handle ellipse areas (the axis lines themselves are not hit-tested).
  QPainterPath shape() const override;
  /// Draws the red X axis and green Y axis (each with an arrowhead), and, if
  /// setDrawHandle(true), the blue center move handle and rotate handle dot.
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

  /// Enables or disables drawing (and mouse interaction with) the move/rotate handles.
  void setDrawHandle(bool enable);

  /// Enables or disables the ability to drag-move/drag-rotate the item via its handles.
  void setPosMovable(bool enable);
  /// Returns whether the item currently allows drag-move/drag-rotate interaction.
  const bool isMovable();

  /// Sets the half-length of the X/Y axis lines and repaints.
  void setAxisLength(qreal length);
  /// Sets the size of the axis arrowheads and repaints.
  void setArrowSize(qreal size);

  /// Positions the item so that its local center (boundingRect().center())
  /// lands at `pos` in parent coordinates, then repaints.
  void setPositionInParent(QPointF pos);
  /// Returns m_center mapped to parent coordinates.
  QPointF getPositionInParent() const;
  /// Sets the item's rotation (normalized to [-180, 180]) and repaints.
  void setAngleInParent(qreal angle);
  /// Returns the item's current rotation angle in degrees.
  qreal getAngleInParent() const;

protected:
  /// Starts a move or rotate drag when the press lands in the move-handle or
  /// rotate-handle rect (rotate also captures the rotation origin and press
  /// angle); ignored entirely when not movable or handles are hidden.
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  /// While dragging: moves the item to follow the cursor (move drag), or
  /// updates its rotation from the angle swept since the press (rotate drag).
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  /// Ends the active drag, emitting positionChanged() after a move drag or
  /// angleChanged() after a rotate drag.
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

signals:
  /// Emitted on mouse release after a move drag, with the new parent-relative position.
  void positionChanged(QPointF point);
  /// Emitted on mouse release after a rotate drag, with the new rotation angle in degrees.
  void angleChanged(qreal angle);

private:
  /// Returns the square move-handle rect (fixed 20x20 size) centered on the item.
  QRectF moveHandleRect() const;
  /// Returns the square rotate-handle rect (fixed 10x10 size), offset from
  /// center along the fixed 45° direction by the axis length plus half the handle size.
  QRectF rotateHandleRect() const;
  /// Reduces `angle` into the [-180, 180] range.
  qreal normalizeRotation(qreal angle) const;

private:
  bool m_drawHandle;      ///< Whether the move/rotate handles are drawn and interactive.
  bool m_movable;         ///< Whether drag-move/drag-rotate via the handles is allowed.
  QPointF m_center;       ///< Local-coordinate center point mapped to parent by getPositionInParent().
  QRectF m_moveHandle;    ///< Reserved for a cached move-handle rect (currently unused; see moveHandleRect()).
  QRectF m_rotateHandle;  ///< Reserved for a cached rotate-handle rect (currently unused; see rotateHandleRect()).
  bool m_draggingMove = false;   ///< True while a move drag is in progress.
  bool m_draggingRotate = false; ///< True while a rotate drag is in progress.

  qreal m_rotationAngle = 0;      ///< Last rotation angle reported via angleChanged().
  qreal m_original_rotation;      ///< Item rotation captured at the start of a rotate drag.
  qreal m_valid_rotation;         ///< Last-known-valid rotation during a rotate drag (initialized from m_original_rotation).
  QPointF m_rotation_origin;      ///< Scene-coordinate rotation pivot captured at the start of a rotate drag.
  qreal m_press_angle;            ///< Angle from m_rotation_origin to the mouse position at the start of a rotate drag.

  qreal m_axisLength = 50; ///< Half-length of the drawn X/Y axis lines.
  qreal m_arrowSize = 10;  ///< Size of the axis arrowheads.
  qreal m_centerSize = 5;  ///< Radius of the drawn center dot.
};

#endif // ITEM_PICKING_POS_H
