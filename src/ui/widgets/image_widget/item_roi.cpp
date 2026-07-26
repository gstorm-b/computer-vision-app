#include "item_roi.h"

#include <QGraphicsScene>
#include <QGraphicsView>

/// Constructs the ROI at `rect`, wires up movable/selectable/geometry-change flags, stores
/// `ignore_flag`, and initializes default colors, handle size, and transform origin.
ItemRoi::ItemRoi(const QRectF &rect,
                 QGraphicsItem *parent,
                 bool *ignore_flag)
    : QGraphicsRectItem(rect, parent),
    m_is_draw_center(false),
    m_current_handle(None) {

  setFlags(QGraphicsItem::ItemIsMovable |
           QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemSendsGeometryChanges);

  m_ignore = ignore_flag;

  m_bounding_color_normal = QColor(0xEC5228);
  m_bounding_color_selected = QColor(0xC68EFD);

  m_handle_size = 10.0;
  this->setTransformOriginPoint(rect.center());
}

/// Returns the ROI rectangle mapped into the parent item's coordinate system.
QRectF ItemRoi::getRoi() {
  return mapRectToParent(rect());
}

/// Sets the dashed border color used when the ROI is not selected, if `normal` is valid.
void ItemRoi::setBoundingColorNormal(QColor normal) {
  if (normal.isValid()) {
    m_bounding_color_normal = normal;
  }
}

/// Sets the dashed border color used when the ROI is selected, if `selected` is valid.
void ItemRoi::setBoundingColorSelected(QColor selected) {
  if (selected.isValid()) {
    m_bounding_color_selected = selected;
  }
}

/// Enables/disables drawing the small center cross-hair in paint().
void ItemRoi::setEnableDrawCenter(bool enable) {
  m_is_draw_center = enable;
}

/// Paints the ROI: an optional center cross-hair, then either the selected-state dashed
/// rect plus the four corner resize handles, or the normal-state dashed rect.
void ItemRoi::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) {
  Q_UNUSED(widget);
  // Draw ROI
  QPen pen(Qt::red);
  pen.setWidth(2);
  pen.setCosmetic(true);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);
  // painter->drawEllipse(this->rect().center(), 5, 5);
  // painter->drawEllipse(this->transformOriginPoint(), 10, 10);

  if (m_is_draw_center) {
    int centerX = this->rect().center().x();
    int centerY = this->rect().center().y();
    const int sizeX = this->rect().width() * 0.06;
    const int sizeY = this->rect().height() * 0.06;
    const int center_size = (sizeX > sizeY) ? sizeY : sizeX;
    painter->drawLine(centerX - center_size, centerY, centerX + center_size, centerY);
    painter->drawLine(centerX, centerY - center_size, centerX, centerY + center_size);
  }

  // if ROI is selected draw Handle point
  if (option->state & QStyle::State_Selected) {
    // bouding box draw
    pen.setColor(m_bounding_color_selected);
    pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->drawRect(rect());

    // draw handle point add 4 corner
    QColor handler_color(0x8F87F1);
    handler_color.setAlpha(100);
    painter->setBrush(handler_color);
    QPen handlePen(Qt::black);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->drawRect(handleRect(TopLeft));
    painter->drawRect(handleRect(TopRight));
    painter->drawRect(handleRect(BottomLeft));
    painter->drawRect(handleRect(BottomRight));
  } else {
    // draw ROI in mode not selected
    pen.setColor(m_bounding_color_normal);
    pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    painter->drawRect(rect());
  }
}

/// boundingRect is consulted when a click event is dispatched, to determine which item is
/// being chosen; returns rect() expanded to include all four corner handle rects plus a
/// 1px margin.
QRectF ItemRoi::boundingRect() const {
  QRectF base = rect();
  // expand bouding box for handle area
  QRectF unionRect = base;
  unionRect = unionRect.united(handleRect(TopLeft));
  unionRect = unionRect.united(handleRect(TopRight));
  unionRect = unionRect.united(handleRect(BottomLeft));
  unionRect = unionRect.united(handleRect(BottomRight));
  // add margin for bouding box
  return unionRect.adjusted(-1, -1, 1, 1);
}

/// shape decides which area belongs to the ROI item for hit-testing: the ROI rect, plus
/// the four corner handle rects when the item is selected.
QPainterPath ItemRoi::shape() const  {
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  // shape
  path.addRect(rect());

  // if item are selected, add handle to shape to expand choosen area
  if (isSelected()) {
    path.addRect(handleRect(TopLeft));
    path.addRect(handleRect(TopRight));
    path.addRect(handleRect(BottomLeft));
    path.addRect(handleRect(BottomRight));
  }
  return path;
}

/// Intercepts ItemPositionChange to clamp the proposed new position so the ROI stays fully
/// inside the parent item's bounding rect.
/// @param change the kind of item change being reported
/// @param value the proposed new value (position, for the change handled here)
/// @return the (possibly corrected) value to apply
QVariant ItemRoi::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) {

  // only override for item position changed
  if (change == QGraphicsItem::ItemPositionChange && parentItem()) {
    QPointF newPos = value.toPointF();

    // map bounding rect to parent coordiante, which is pixmap item
    QRectF rectInParent = mapRectToParent(rect());
    QRectF parentRect = parentItem()->boundingRect();

    // vector offset when move
    QPointF delta = newPos - pos();
    QRectF movedRect = rectInParent.translated(delta);

    // void move outside of parent's bounding box
    QPointF corrected = newPos;

    if (!parentRect.contains(movedRect)) {
      if (movedRect.left() < parentRect.left())
        corrected.rx() += parentRect.left() - movedRect.left();
      if (movedRect.right() > parentRect.right())
        corrected.rx() -= movedRect.right() - parentRect.right();
      if (movedRect.top() < parentRect.top())
        corrected.ry() += parentRect.top() - movedRect.top();
      if (movedRect.bottom() > parentRect.bottom())
        corrected.ry() -= movedRect.bottom() - parentRect.bottom();

      return corrected;
    }
  }

  return QGraphicsRectItem::itemChange(change, value);
}

/// Returns the square hit/paint rect for corner `pos`, sized by effectiveHandleSize() and
/// centered on that corner of rect().
QRectF ItemRoi::handleRect(HandlePosition pos) const {
  QRectF r = rect();
  QPointF point;
  const qreal handleSize = effectiveHandleSize();
  switch(pos) {
    case TopLeft:
      point = r.topLeft();
      break;
    case TopRight:
      point = r.topRight();
      break;
    case BottomLeft:
      point = r.bottomLeft();
      break;
    case BottomRight:
      point = r.bottomRight();
      break;
    default:
      point = QPointF();
      break;
  }
  // draw rectangle handle with center at corner
  return QRectF(point.x() - handleSize/2, point.y() - handleSize/2,
                handleSize, handleSize);
}

/// Returns which corner handle (if any) contains `pos` (item coordinates), or None.
ItemRoi::HandlePosition ItemRoi::getHandleAt(const QPointF &pos) const {
  if (handleRect(TopLeft).contains(pos))
    return TopLeft;
  if (handleRect(TopRight).contains(pos))
    return TopRight;
  if (handleRect(BottomLeft).contains(pos))
    return BottomLeft;
  if (handleRect(BottomRight).contains(pos))
    return BottomRight;
  return None;
}

/// Returns the view's current level-of-detail scale factor (clamped to a minimum of 0.05),
/// or 1.0 if the item has no scene/view yet.
qreal ItemRoi::currentLevelOfDetail() const
{
  if (scene() && !scene()->views().isEmpty()) {
    return qMax<qreal>(0.05,
                       QStyleOptionGraphicsItem::levelOfDetailFromTransform(
                           scene()->views().first()->viewportTransform()));
  }
  return 1.0;
}

/// Returns the on-screen-constant handle size (item coordinates), scaled inversely with
/// the current level of detail and clamped to the range [8,18]/lod.
qreal ItemRoi::effectiveHandleSize() const
{
  const qreal lod = currentLevelOfDetail();
  return qBound<qreal>(8.0 / lod, 12.0 / lod, 18.0 / lod);
}

/// Returns true if `new_rect`, mapped to parent coordinates, is fully contained by the
/// parent item's bounding rect; false if there is no parent.
bool ItemRoi::isInsideParent(QRectF &new_rect) {
  if (parentItem() != nullptr) {
    QRectF childInParent = this->mapRectToParent(new_rect);
    QRectF parentRect = parentItem()->boundingRect();
    return parentRect.contains(childInParent);
  }
  return false;
}

/// Handles press: honors the external ignore flag, detects a corner-handle grab (capturing
/// the original rect for resizing), otherwise defers to QGraphicsRectItem for its normal
/// move-mode handling.
void ItemRoi::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (m_ignore != nullptr) {
    if (*m_ignore == true) {
      event->ignore();
      return;
    }
  }

  QPointF pos = event->pos();
  m_last_center = this->transformOriginPoint();
  m_current_handle = getHandleAt(pos);
  // check if event emit inside handle area
  if (m_current_handle != None) {
    // if clicked in corner handle, store original rect
    m_original_rect = rect();
    m_press_pos = pos;
    event->accept();
    return;
  }

  // if clicked inside, switch to position changed mode, which is processed in based class
  event->accept();
  QGraphicsRectItem::mousePressEvent(event);
}

/// While a handle is active, resizes rect() from the drag delta (rejecting moves that
/// would leave the parent bounds or shrink a side below m_handle_size); otherwise defers
/// to QGraphicsRectItem for its normal move-mode handling.
void ItemRoi::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (m_ignore != nullptr) {
    if (*m_ignore == true) {
      event->ignore();
      return;
    }
  }

  if (m_current_handle != None) {
    QPointF delta = event->pos() - m_press_pos;
    QRectF newRect = m_original_rect;
    switch(m_current_handle) {
      case TopLeft:
        newRect.setTopLeft(newRect.topLeft() + delta);
        break;
      case TopRight:
        newRect.setTopRight(newRect.topRight() + delta);
        break;
      case BottomLeft:
        newRect.setBottomLeft(newRect.bottomLeft() + delta);
        break;
      case BottomRight:
        newRect.setBottomRight(newRect.bottomRight() + delta);
        break;
      default:
        break;
    }

    // check new position outside parrent
    if (!isInsideParent(newRect)) {
      event->accept();
      return;
    }

    // make sure new rect always large than limited rect
    if (newRect.width() < m_handle_size) {
      event->accept();
      return;
    }

    if (newRect.height() < m_handle_size) {
      event->accept();
      return;
    }

    setRect(newRect);
    event->accept();
    return;
  }

  event->accept();
  QGraphicsRectItem::mouseMoveEvent(event);
}

/// Ends an active resize: re-centers the transform origin on the new rect while keeping
/// the item's on-screen position fixed, then clears the active handle.
void ItemRoi::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (m_ignore != nullptr) {
    if (*m_ignore == true) {
      event->ignore();
      return;
    }
  }

  if (m_current_handle != None) {
    QPointF oldCenterScene = this->mapToScene(this->rect().center());
    this->setTransformOriginPoint(this->rect().center());
    QPointF newCenterScene = this->mapToScene(this->rect().center());
    this->setPos(this->pos() + oldCenterScene - newCenterScene);
  }
  m_current_handle = None;
  event->accept();
  QGraphicsRectItem::mouseReleaseEvent(event);
}
