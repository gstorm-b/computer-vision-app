#include "item_picking_pos.h"
#include <QtMath>


/// Constructs the item at local center (0,0), movable and with handles drawn
/// by default; selectable and reports scene-position changes, with hover events enabled.
ItemPickingCenter::ItemPickingCenter(QGraphicsItem *parent)
    : QGraphicsItem(parent), m_center(0, 0),
    m_movable(true),
    m_drawHandle(true) {

  setFlags(ItemIsSelectable |
           ItemSendsScenePositionChanges);
  setAcceptHoverEvents(true);
}

/// Returns a square centered on the item, sized to fully contain the axes,
/// arrowheads, and handles (extent = axis length + arrow size + 20 margin).
QRectF ItemPickingCenter::boundingRect() const {
  qreal extent = m_axisLength + m_arrowSize + 20;
  return QRectF(-extent, -extent, extent * 2, extent * 2);
}

/// Returns the hit-test/selection shape: the union of the move-handle and
/// rotate-handle ellipse areas.
QPainterPath ItemPickingCenter::shape() const {
  QPainterPath path;
  // path.setFillRule(Qt::WindingFill);
  path.addEllipse(moveHandleRect());
  path.addEllipse(rotateHandleRect());
  return path;
}

/// Draws the red X axis and green Y axis (each with an arrowhead), and, if
/// m_drawHandle is set, the blue center dot and rotate-handle ellipse.
void ItemPickingCenter::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing);
  QPointF center = boundingRect().center();

  // X axis
  painter->setPen(QPen(Qt::red, 2));
  painter->drawLine(center + QPointF(-m_axisLength, 0), center + QPointF(m_axisLength, 0));
  QPolygonF xArrow;
  xArrow << center + QPointF(m_axisLength, 0)
         << center + QPointF(m_axisLength - m_arrowSize, -m_arrowSize)
         << center + QPointF(m_axisLength - m_arrowSize, m_arrowSize);
  painter->setBrush(Qt::red);
  painter->drawPolygon(xArrow);

  // Y axis
  painter->setPen(QPen(Qt::green, 2));
  painter->drawLine(center + QPointF(0, m_axisLength), center + QPointF(0, -m_axisLength));
  QPolygonF yArrow;
  yArrow << center + QPointF(0, -m_axisLength)
         << center + QPointF(-m_arrowSize, -m_axisLength + m_arrowSize)
         << center + QPointF(m_arrowSize, -m_axisLength + m_arrowSize);
  painter->setBrush(Qt::green);
  painter->drawPolygon(yArrow);

  if (m_drawHandle) {
    painter->setBrush(Qt::blue);
    painter->drawEllipse(center, m_centerSize, m_centerSize);

    painter->setBrush(Qt::blue);
    painter->drawEllipse(rotateHandleRect());
  }
}

/// Starts a move or rotate drag when the press lands in the move-handle or
/// rotate-handle rect (rotate also captures the rotation origin/press angle
/// via the scene position and sets the transform origin to the item center);
/// falls through to the base class if not movable or handles are hidden.
void ItemPickingCenter::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (!m_movable) {
    event->accept();
    QGraphicsItem::mousePressEvent(event);
    return;
  }

  if (!m_drawHandle) {
    event->accept();
    QGraphicsItem::mousePressEvent(event);
    return;
  }

  if (moveHandleRect().contains(event->pos())) {
    m_draggingMove = true;
    event->accept();

  } else if (rotateHandleRect().contains(event->pos())) {
    m_draggingRotate = true;

    m_original_rotation = rotation();
    m_valid_rotation = m_original_rotation;
    m_rotation_origin = mapToScene(transformOriginPoint());
    // angle determine by center of ROI to mouse clicked position
    m_press_angle = QLineF(m_rotation_origin, event->scenePos()).angle();
    setTransformOriginPoint(boundingRect().center());

    event->accept();
  }
  QGraphicsItem::mousePressEvent(event);
}

/// While dragging: moves the item to follow the cursor (move drag), or
/// recomputes and applies the normalized rotation from the angle swept since
/// the press (rotate drag).
void ItemPickingCenter::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (m_draggingMove) {
    setPos(mapToParent(event->pos()));
    event->accept();

  } else if (m_draggingRotate) {
    qreal currentAngle = QLineF(m_rotation_origin, event->scenePos()).angle();
    qreal angleDiff = currentAngle - m_press_angle;
    qreal new_angle = m_original_rotation - angleDiff;
    new_angle = normalizeRotation(new_angle);
    // setRotation(m_original_rotation - angleDiff);
    setRotation(new_angle);
    // qInfo() << "rotation changed" << this->rotation();

    event->accept();
  }
  QGraphicsItem::mouseMoveEvent(event);
}

/// Ends the active drag, emitting positionChanged() after a move drag or
/// recording/emitting angleChanged() after a rotate drag.
void ItemPickingCenter::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (m_draggingMove) {
    emit positionChanged(getPositionInParent());
  } else if (m_draggingRotate) {
    // m_rotationAngle += m_valid_rotation;
    m_rotationAngle = this->rotation();
    emit angleChanged(m_rotationAngle);
  }

  m_draggingMove = false;
  m_draggingRotate = false;
  QGraphicsItem::mouseReleaseEvent(event);
}

/// Returns the square move-handle rect (fixed 20x20 size) centered on the item.
QRectF ItemPickingCenter::moveHandleRect() const {
  QPointF center = boundingRect().center();
  qreal handleSize = 20.0;

  return QRectF(center.x() - handleSize / 2,
                center.y() - handleSize / 2,
                handleSize,
                handleSize);
}

/// Returns the square rotate-handle rect (fixed 10x10 size), offset from
/// center along the fixed 45° direction by the axis length plus half the handle size.
QRectF ItemPickingCenter::rotateHandleRect() const {
  QPointF center = boundingRect().center();
  qreal handleSize = 10.0;
  qreal offset = m_axisLength + handleSize / 2;

  qreal angleRad = qDegreesToRadians(45.0);
  QPointF direction(qCos(angleRad), -qSin(angleRad));

  QPointF handleCenter = center + direction * offset;

  return QRectF(handleCenter.x() - handleSize / 2,
                handleCenter.y() - handleSize / 2,
                handleSize,
                handleSize);
}

/// Reduces `angle` into the [-180, 180] range.
qreal ItemPickingCenter::normalizeRotation(qreal angle) const {
  angle = std::fmod(angle, 360.0);
  if (angle < 0)
    angle += 360.0;

  if (angle > 180.0)
    angle -= 360.0;

  return angle;
}

/// Enables or disables drawing (and mouse interaction with) the move/rotate handles.
void ItemPickingCenter::setDrawHandle(bool enable) {
  m_drawHandle = enable;
}

/// Enables or disables the ability to drag-move/drag-rotate the item via its handles.
void ItemPickingCenter::setPosMovable(bool enable) {
  m_movable = enable;
}

/// Returns whether the item currently allows drag-move/drag-rotate interaction.
const bool ItemPickingCenter::isMovable() {
  return m_movable;
}

/// Sets the half-length of the X/Y axis lines and repaints.
void ItemPickingCenter::setAxisLength(qreal length) {
  m_axisLength = length;
  update();
}

/// Sets the size of the axis arrowheads and repaints.
void ItemPickingCenter::setArrowSize(qreal size) {
  m_arrowSize = size;
  update();
}

/// Positions the item so that its local center (boundingRect().center())
/// lands at `pos` in parent coordinates, then repaints.
void ItemPickingCenter::setPositionInParent(QPointF pos) {
  QPointF localCenter = boundingRect().center();
  QPointF newPos = pos - localCenter;
  setPos(newPos);
  update();
}

/// Returns m_center mapped to parent coordinates.
QPointF ItemPickingCenter::getPositionInParent() const {
  return mapToParent(m_center);
}

/// Sets the item's rotation (normalized to [-180, 180]) and repaints.
void ItemPickingCenter::setAngleInParent(qreal angle) {
  angle = normalizeRotation(angle);
  setRotation(angle);
  update();
}

/// Returns the item's current rotation angle in degrees.
qreal ItemPickingCenter::getAngleInParent() const {
  return this->rotation();
}

