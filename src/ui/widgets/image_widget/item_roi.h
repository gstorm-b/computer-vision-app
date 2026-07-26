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

/// Resizable, movable, axis-aligned rectangular region-of-interest item for a
/// QGraphicsScene: renders a dashed bounding box with an optional center cross-hair,
/// shows four corner resize handles when selected, and keeps its position/size clamped
/// inside its parent item's bounding rect.
class ItemRoi : public QGraphicsRectItem {
public:
  /// Custom QGraphicsItem type id returned by type(), for qgraphicsitem_cast<ItemRoi*>().
  enum { Type = UserType + 27 };

  /// Constructs the ROI at `rect` (parent coordinates); `ignore_flag`, if non-null, lets
  /// the owner temporarily suppress all mouse interaction (see mousePressEvent) by setting
  /// *ignore_flag to true.
  ItemRoi(const QRectF &rect,
                 QGraphicsItem *parent = nullptr,
                 bool *ignore_flag = nullptr);
  /// Returns the custom item type id (Type) for qgraphicsitem_cast/type() dispatch.
  int type() const override { return Type; }
  /// Returns the ROI rectangle mapped into the parent item's coordinate system.
  QRectF getRoi();
  /// Declared for returning a pixmap cropped to `roi`'s bounds; not implemented on this
  /// class (the actual crop logic lives in ImageWidget::getCroppedFromRoi).
  QPixmap getCroppedFromRoi(ItemRoi *roi);
  /// Sets the dashed border color used when the ROI is not selected, if `normal` is valid.
  void setBoundingColorNormal(QColor normal);
  /// Sets the dashed border color used when the ROI is selected, if `selected` is valid.
  void setBoundingColorSelected(QColor selected);
  /// Enables/disables drawing the small center cross-hair in paint().
  void setEnableDrawCenter(bool enable);

protected:
  /// Identifies which corner resize handle (or none) is being hit-tested/dragged.
  enum HandlePosition {
    None,        ///< No handle is active/hit.
    TopLeft,     ///< Top-left corner resize handle.
    TopRight,    ///< Top-right corner resize handle.
    BottomLeft,  ///< Bottom-left corner resize handle.
    BottomRight  ///< Bottom-right corner resize handle.
  };

  /// Paints the ROI: an optional center cross-hair, then either the selected-state dashed
  /// rect plus the four corner resize handles, or the normal-state dashed rect.
  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;
  /// Returns rect() expanded to include all four corner handle rects plus a 1px margin;
  /// used by the scene for hit-testing and repaint invalidation.
  QRectF boundingRect() const override;
  /// Returns the precise mouse-hit area: the ROI rect, plus the four corner handle rects
  /// when the item is selected.
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
  /// Returns which corner handle (if any) contains `pos` (item coordinates), or None.
  HandlePosition getHandleAt(const QPointF &pos) const;
  /// Returns the view's current level-of-detail scale factor (clamped to a minimum of
  /// 0.05), or 1.0 if the item has no scene/view yet.
  qreal currentLevelOfDetail() const;
  /// Returns the on-screen-constant handle size (item coordinates), scaled inversely with
  /// the current level of detail and clamped to the range [8,18]/lod.
  qreal effectiveHandleSize() const;

  /// Returns true if `new_rect`, mapped to parent coordinates, is fully contained by the
  /// parent item's bounding rect; false if there is no parent.
  bool isInsideParent(QRectF &new_rect);
  /// Declared hook for correcting `moved_rect` into `corrected`; not implemented on this
  /// class.
  bool correctRectItem(QRectF &moved_rect, QPointF &corrected);

  /// Handles press: honors the external ignore flag, detects a corner-handle grab
  /// (capturing the original rect for resizing), otherwise defers to QGraphicsRectItem
  /// for its normal move-mode handling.
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  /// While a handle is active, resizes rect() from the drag delta (rejecting moves that
  /// would leave the parent bounds or shrink a side below m_handle_size); otherwise defers
  /// to QGraphicsRectItem for its normal move-mode handling.
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  /// Ends an active resize: re-centers the transform origin on the new rect while keeping
  /// the item's on-screen position fixed, then clears the active handle.
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
  bool m_is_draw_center;                ///< Whether paint() draws the center cross-hair.
  QColor m_bounding_color_normal;        ///< Dashed border color used when not selected.
  QColor m_bounding_color_selected;      ///< Dashed border color used when selected.
  /// Minimum width/height allowed when resizing via a corner handle (not the on-screen
  /// handle draw size; see effectiveHandleSize()).
  qreal m_handle_size;
  /// Handle (if any) currently grabbed by the active mouse interaction; None when idle.
  HandlePosition m_current_handle;
  /// rect() captured at mousePressEvent, used as the base for handle-drag resizing.
  QRectF m_original_rect;
  QPointF m_press_pos;  ///< Mouse position (item coordinates) captured at mousePressEvent.

  QPointF m_last_center;  ///< Transform origin captured at mousePressEvent; not read elsewhere.

  bool *m_ignore;  ///< Optional external flag; mouse events are ignored while *m_ignore is true.
};


#endif // ITEM_ROI_H
