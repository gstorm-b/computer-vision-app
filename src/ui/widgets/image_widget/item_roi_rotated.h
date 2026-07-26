#ifndef ITEM_ROI_ROTATED_H
#define ITEM_ROI_ROTATED_H

#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>

/// Resizable, movable, and rotatable rectangular region-of-interest item for a
/// QGraphicsScene: renders a dashed bounding box, four corner resize handles, and a
/// dedicated rotation handle above the top edge when selected, and keeps its position
/// and rotation constrained to stay inside its parent item's bounding rect.
class ItemRoiRotated : public QGraphicsRectItem {
public:
  /// Custom QGraphicsItem type id returned by type(), for qgraphicsitem_cast<ItemRoiRotated*>().
  enum { Type = UserType + 28 };

  /// Constructs the ROI at `rect` (parent coordinates); `ignore_flag`, if non-null, lets
  /// the owner temporarily suppress all mouse interaction (see mousePressEvent) by setting
  /// *ignore_flag to true.
  ItemRoiRotated(const QRectF &rect,
                 QGraphicsItem *parent = nullptr,
                 bool *ignore_flag = nullptr);
  /// Returns the custom item type id (Type) for qgraphicsitem_cast/type() dispatch.
  int type() const override { return Type; }
  /// Returns the ROI rectangle mapped into the parent item's coordinate system.
  QRectF getRoi();

protected:
  /// Identifies which handle (corner resize or top rotation handle), if any, is being
  /// hit-tested/dragged.
  enum HandlePosition {
    None,          ///< No handle is active/hit.
    TopLeft,       ///< Top-left corner resize handle.
    TopRight,      ///< Top-right corner resize handle.
    BottomLeft,    ///< Bottom-left corner resize handle.
    BottomRight,   ///< Bottom-right corner resize handle.
    RotateHandle   ///< Rotation handle drawn above the top edge.
  };

  /// Paints the ROI: a center marker, then either the selected-state dashed rect plus the
  /// four corner resize handles and the rotation handle/link line, or the normal-state
  /// dashed rect.
  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;
  /// Returns rect() expanded to include all four corner handle rects and the rotate handle
  /// rect, plus a 1px margin; used by the scene for hit-testing and repaint invalidation.
  QRectF boundingRect() const override;
  /// Returns the precise mouse-hit area: the ROI rect, plus the four corner handle rects
  /// and the rotate handle ellipse when the item is selected.
  QPainterPath shape() const override;
  /// Intercepts ItemPositionChange to clamp the proposed new position so the ROI stays
  /// fully inside the parent item's bounding rect.
  /// @param change the kind of item change being reported
  /// @param value the proposed new value (position, for the change handled here)
  /// @return the (possibly corrected) value to apply
  QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                      const QVariant &value) override;
  /// Returns the square hit/paint rect for corner `pos`, sized by effectiveHandleSize()
  /// and centered on that corner of rect().
  QRectF handleRect(HandlePosition pos) const;
  /// Returns the circular hit/paint rect for the rotation handle, positioned above the
  /// center of the top edge by effectiveRotationHandleOffset() and sized by
  /// effectiveHandleSize().
  QRectF rotateHandleRect() const;
  /// Returns which handle (corner or rotate) contains `pos` (item coordinates), or None.
  HandlePosition getHandleAt(const QPointF &pos) const;
  /// Returns the view's current level-of-detail scale factor (clamped to a minimum of
  /// 0.05), or 1.0 if the item has no scene/view yet.
  qreal currentLevelOfDetail() const;
  /// Returns the on-screen-constant handle size (item coordinates), scaled inversely with
  /// the current level of detail and clamped to the range [10,22]/lod.
  qreal effectiveHandleSize() const;
  /// Returns the on-screen-constant distance from the top edge to the rotation handle
  /// (item coordinates), scaled inversely with the current level of detail and clamped to
  /// the range [18,36]/lod.
  qreal effectiveRotationHandleOffset() const;

  /// Returns true if `new_rect`, mapped to parent coordinates, is fully contained by the
  /// parent item's bounding rect; false if there is no parent.
  bool isInsideParent(QRectF &new_rect);
  /// Declared hook for correcting `moved_rect` into `corrected`; not implemented on this
  /// class.
  bool correctRectItem(QRectF &moved_rect, QPointF &corrected);

  /// Handles press: honors the external ignore flag, detects a rotation-handle grab
  /// (capturing the rotation origin/angle) or a corner-handle grab (capturing the original
  /// rect), otherwise defers to QGraphicsRectItem for its normal move-mode handling.
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  /// While the rotation handle is active, rotates the item to track the mouse angle
  /// (reverting if that would push the rect outside the parent); while a corner handle is
  /// active, resizes rect() from the drag delta (rejecting moves that would leave the
  /// parent bounds or shrink a side below 2*m_handle_size); otherwise defers to
  /// QGraphicsRectItem for its normal move-mode handling.
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  /// Ends an active resize (rotation is excluded): re-centers the transform origin on the
  /// new rect while keeping the item's on-screen position fixed, then clears the active
  /// handle.
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
  /// Minimum half-width/half-height allowed when resizing via a corner handle (compared
  /// as 2*m_handle_size; not the on-screen handle draw size, see effectiveHandleSize()).
  qreal m_handle_size;
  /// Base offset from the top edge to the rotation handle (see effectiveRotationHandleOffset()).
  qreal m_rotation_handle_offset;
  /// Handle (if any) currently grabbed by the active mouse interaction; None when idle.
  HandlePosition m_current_handle;
  /// rect() captured at mousePressEvent, used as the base for handle-drag resizing.
  QRectF m_original_rect;
  QPointF m_press_pos;  ///< Mouse position (item coordinates) captured at mousePressEvent.
  /// Item rotation() captured at mousePressEvent, used as the base for rotation dragging.
  qreal m_original_rotation;
  /// Last rotation that kept the ROI inside the parent's bounding rect; restored if a drag
  /// would move it outside.
  qreal m_valid_rotation;
  /// Scene-space transform origin captured when a rotation drag starts.
  QPointF m_rotation_origin;
  /// Angle (degrees) from m_rotation_origin to the mouse position at the start of a
  /// rotation drag.
  qreal m_press_angle;

  QPointF m_last_center;  ///< Transform origin captured at mousePressEvent; not read elsewhere.

  bool *m_ignore;  ///< Optional external flag; mouse events are ignored while *m_ignore is true.
};

#endif // ITEM_ROI_ROTATED_H
