#ifndef VISION_CANVAS_H
#define VISION_CANVAS_H

#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <QVector>
#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"
#include "ui/widgets/vision/vision_tool_palette.h"

class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QGraphicsItem;

namespace vision_canvas_detail {
/// Internal (private-implementation) interface for a selectable/editable ROI graphics
/// item hosted on the canvas scene; concrete shapes live in vision_canvas.cpp.
class RoiItemBase;
}

/// Interactive QGraphicsView-based canvas that shows a live/still image (cv::Mat or
/// QPixmap) together with editable ROI shapes and read-only vision result overlays
/// (accepted/rejected matches, picking boxes, fault/output markers). Supports pan/zoom,
/// draw/move/resize of ROIs, ROI undo/redo history, and result-object hover/selection
/// picking in read-only mode.
class VisionCanvas : public QGraphicsView {
    Q_OBJECT

public:
    /// Constructs the canvas: installs its QGraphicsScene, enables mouse tracking and
    /// full-viewport repaint, and applies the current theme background color, refreshing
    /// it (and rebuilding auxiliary/overlay items) whenever ThemeManager::themeChanged fires.
    explicit VisionCanvas(QWidget *parent = nullptr);

    /// Converts `image` to a QPixmap (via vision::pixmapFromMat) and displays it.
    void setImage(const cv::Mat &image);
    /// Displays `pixmap` as the canvas background image. A null pixmap clears the
    /// canvas. Editable/auxiliary/overlay items are rebuilt against the new image, and
    /// the view is re-fit only when the pixmap size differs from the previously shown
    /// size, so same-size frames (e.g. a live camera feed) preserve the user's zoom/pan.
    void setImage(const QPixmap &pixmap);
    /// Removes the displayed image, all editable/draw/overlay items, and resets ROI
    /// undo history, selection, and hover state back to empty.
    void clearImage();

    /// Enables or disables read-only mode (disables ROI drawing/editing and switches
    /// mouse interaction to result-object hover/selection picking) and updates the cursor.
    void setReadOnly(bool readOnly);
    /// Returns whether the canvas is currently read-only.
    bool isReadOnly() const { return m_readOnly; }

    /// Sets the active interaction tool (select/move, pan, draw axis-aligned/rotated
    /// rect), updates the cursor, and emits toolModeChanged() if the mode actually changed.
    void setToolMode(VisionToolPalette::ToolMode mode);
    /// Returns the current interaction tool mode.
    VisionToolPalette::ToolMode toolMode() const { return m_toolMode; }

    /// Replaces all editable ROI items with `rois`, resets the undo/redo history to this
    /// single state, and emits roisChanged()/selectedRoiChanged().
    void setEditableRois(const QVector<VisionRoi> &rois);
    /// Reads back the current editable ROI shapes from the scene, normalized via
    /// vision::normalizedRoi().
    /// @return the current editable ROIs in scene order
    QVector<VisionRoi> editableRois() const;

    /// Replaces the read-only auxiliary ROI outlines (dashed, non-interactive) shown
    /// alongside the editable ROIs and rebuilds their scene items.
    void setAuxiliaryRois(const QVector<VisionRoi> &rois);
    /// Replaces the vision result overlay (accepted/rejected objects, ROI overlays,
    /// runtime signal values) and rebuilds the overlay scene items. Does not touch
    /// overlay visibility, which is owned separately by setOverlayVisibility().
    void setResultOverlay(const VisionResultOverlay &overlay);
    /// Replaces which overlay elements are drawn (score/angle/pattern labels, rejected
    /// candidates, fault/output markers, picking boxes, runtime signal values) and
    /// rebuilds the overlay scene items.
    void setOverlayVisibility(const VisionOverlayVisibility &visibility);
    /// Selects the result object with the given index for highlighting (muting all
    /// others), clears any hover when a real object is selected, rebuilds the overlay,
    /// and emits resultObjectSelectionChanged(). No-op if already selected.
    void setSelectedResultObject(int objectIndex);
    /// Clears the current result-object selection (equivalent to setSelectedResultObject(-1)).
    void clearSelectedResultObject();
    /// Returns the index of the currently selected result object, or -1 if none.
    int selectedResultObject() const { return m_selectedResultObject; }

    /// Returns the ROI data for the currently selected editable item, or a
    /// default-constructed (empty-id) VisionRoi if none is selected.
    VisionRoi selectedRoi() const;
    /// Returns true if an editable ROI item is currently selected.
    bool hasSelectedRoi() const;
    /// Returns the pixel size of the currently displayed image, or an invalid QSize if
    /// no image is set.
    QSize imageSize() const;

    /// Applies `roi` (clamped to the image bounds) to the editable ROI matching its id,
    /// rebuilds the editable items, emits roisChanged(), and records an undo snapshot.
    void updateSelectedRoi(const VisionRoi &roi);
    /// Removes the currently selected editable ROI, rebuilds the items, emits
    /// roisChanged(), and records an undo snapshot. No-op in read-only mode or if
    /// nothing is selected.
    void deleteSelectedRoi();
    /// Fits the displayed image into the viewport preserving aspect ratio.
    void fitImageToView();
    /// Reverts the editable ROIs to the previous undo-history snapshot, if any.
    void undo();
    /// Reapplies the next undo-history snapshot (redo), if any.
    void redo();

signals:
    /// Emitted whenever the set of editable ROIs changes, carrying the full current list.
    void roisChanged(QVector<VisionRoi> rois);
    /// Emitted whenever the selected editable ROI changes, carrying its current data (or
    /// an empty VisionRoi if nothing is selected).
    void selectedRoiChanged(VisionRoi roi);
    /// Emitted after any undo-history change, reporting whether undo/redo are currently available.
    void undoAvailabilityChanged(bool canUndo, bool canRedo);
    /// Emitted whenever the selected result object (read-only mode) changes.
    void resultObjectSelectionChanged(int objectIndex);
    /// Emitted whenever the active tool mode changes.
    void toolModeChanged(VisionToolPalette::ToolMode mode);

protected:
    /// Clears the hovered result object when the mouse leaves the viewport (read-only,
    /// not panning) before delegating to the base implementation.
    void leaveEvent(QEvent *event) override;
    /// Dispatches left-button presses to pan-start, result-object picking (read-only),
    /// or ROI-draw-start depending on modifiers/tool mode; otherwise forwards to the base
    /// QGraphicsView handler for default rubber-band/selection behavior.
    void mousePressEvent(QMouseEvent *event) override;
    /// Continues an active pan or in-progress ROI draw, or updates the hovered result
    /// object in read-only mode; otherwise forwards to the base handler.
    void mouseMoveEvent(QMouseEvent *event) override;
    /// Ends an active pan or ROI draw and refreshes hover/selection state accordingly;
    /// otherwise forwards to the base handler and re-syncs the selected-ROI signal from
    /// the scene's selection.
    void mouseReleaseEvent(QMouseEvent *event) override;
    /// Fits the image to the view on a middle-button double-click; otherwise forwards to
    /// the base handler.
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /// Ctrl+wheel zooms the view in/out by a fixed step factor; otherwise forwards to the
    /// base handler for normal scrolling.
    void wheelEvent(QWheelEvent *event) override;
    /// Handles Escape (cancel pan / exit pan tool / clear result selection) and Delete
    /// (remove selected ROI); otherwise forwards to the base handler.
    void keyPressEvent(QKeyEvent *event) override;

private:
    /// Recreates the dashed auxiliary ROI outlines (and their label chips) from
    /// m_auxiliaryRois, discarding the previous scene items.
    void rebuildAuxiliaryRois();
    /// Recreates all vision result overlay scene items (accepted/rejected object
    /// polygons, center markers, axis/locator arrows, picking boxes, labels, fault/output
    /// chips, ROI overlays, and the runtime-signal-values chip) from m_overlay and
    /// m_overlayVisibility, applying selection/hover mute/highlight styling.
    void rebuildOverlayItems();
    /// Recreates the editable ROI graphics items from `rois` (clamped to the image
    /// bounds, invalid ones skipped), restoring per-item selection and reconnecting their signals.
    void rebuildEditableRois(const QVector<VisionRoi> &rois);
    /// Appends `item` to m_editableItems and connects its geometryChanged/
    /// geometryFinished/selectionChanged signals to this canvas's slots.
    void registerEditableItem(vision_canvas_detail::RoiItemBase *item);
    /// Re-emits selectedRoiChanged() with the current scene selection (used after the
    /// base QGraphicsView mouse-release updates item selection).
    void updateSelectionFromScene();
    /// Emits roisChanged() with the current editable ROIs and selectedRoiChanged() with
    /// the current selection.
    void emitRoisChanged();
    /// Appends a new undo-history snapshot if the current editable-ROI state differs
    /// from the last recorded one, truncating any redo tail; no-op while history
    /// recording is locked (during undo/redo replay).
    void pushUndoSnapshotIfChanged();
    /// Rebuilds the editable ROIs from history entry `index` (with history recording
    /// locked to avoid re-recording), updates the history cursor, and emits
    /// undoAvailabilityChanged()/roisChanged().
    void applyUndoSnapshot(int index);
    /// Returns a copy of the current editable ROIs with each item's `selected` flag
    /// cleared, used as a normalized undo-history entry.
    QVector<VisionRoi> currentSnapshot() const;
    /// Hit-tests `scenePoint` against accepted result objects (preferring the currently
    /// selected object's polygon) and, if rejected candidates are shown, against rejected objects.
    /// @return the matching object's index, or -1 if none hit
    int resultObjectAtScenePoint(const QPointF &scenePoint) const;
    /// Updates the hovered result object, refreshes the cursor (unless panning), and
    /// rebuilds the overlay items to reflect the new hover state. No-op if unchanged.
    void setHoveredResultObject(int objectIndex);
    /// Returns the scene bounding rect of the displayed image, or an empty rect if no image is set.
    QRectF imageBounds() const;
    /// Clamps `scenePoint` to the displayed image's bounds (returned unchanged if no
    /// image is set), used to keep ROI drawing/editing within the image.
    QPointF clampScenePointToImage(const QPointF &scenePoint) const;
    /// Updates the viewport cursor to reflect the current pan/tool-mode/hover state.
    void syncCursor();
    /// Starts a viewport pan from `pos` and switches the cursor to the closed-hand shape.
    void beginPan(const QPoint &pos);
    /// Scrolls the view by the delta between `pos` and the last recorded pan point.
    void updatePan(const QPoint &pos);
    /// Ends the active pan and restores the cursor.
    void endPan();
    /// Starts drawing a new ROI rectangle at `imagePoint`, creating or resetting the
    /// dashed preview rect item.
    void beginDraw(const QPointF &imagePoint);
    /// Updates the in-progress draw preview rectangle to span from the draw start point to `imagePoint`.
    void updateDraw(const QPointF &imagePoint);
    /// Finishes the in-progress ROI draw: discards the preview rect and, if the
    /// resulting rectangle is at least 4x4 scene units, appends a new axis-aligned or
    /// rotated ROI (per the active tool mode) with an auto-generated id/label, rebuilds
    /// the editable items, emits roisChanged(), and records an undo snapshot.
    void endDraw(const QPointF &imagePoint);

private slots:
    /// Re-emits the current ROI/selection state while an editable item's geometry is
    /// being dragged (connected to RoiItemBase::geometryChanged).
    void onEditableItemGeometryChanged();
    /// Re-emits the current ROI/selection state and records an undo snapshot once an
    /// editable item's geometry edit completes (connected to RoiItemBase::geometryFinished).
    void onEditableItemGeometryFinished();
    /// Re-emits selectedRoiChanged() when an editable item's scene selection state
    /// changes (connected to RoiItemBase::selectionChanged).
    void onEditableItemSelectionChanged();

private:
    QGraphicsScene *m_scene{nullptr};             ///< Scene backing this view; owned (parented) by the canvas.
    QGraphicsPixmapItem *m_imageItem{nullptr};    ///< Displayed background image item, or null when no image is set.
    QSize m_fittedImageSize;                      ///< Size of the image last auto-fit to the view; used to detect resolution changes across setImage() calls.
    QGraphicsRectItem *m_drawRectItem{nullptr};   ///< Dashed preview rectangle shown while drawing a new ROI.
    VisionToolPalette::ToolMode m_toolMode{VisionToolPalette::ToolMode::SelectMove}; ///< Active interaction tool.
    bool m_readOnly{false};                       ///< When true, disables ROI editing and enables result-object picking.
    bool m_panning{false};                        ///< True while a viewport pan gesture is in progress.
    QPoint m_lastPanPoint;                        ///< Last mouse position seen during an active pan, in viewport coordinates.
    bool m_drawing{false};                        ///< True while a new ROI rectangle is being drawn.
    QPointF m_drawStart;                          ///< Scene-space anchor point of the ROI currently being drawn.
    QVector<VisionRoi> m_auxiliaryRois;           ///< Read-only auxiliary ROI outlines shown alongside the editable ROIs.
    VisionResultOverlay m_overlay;                ///< Current vision result overlay data (accepted/rejected objects, ROI overlays, runtime signal values).
    VisionOverlayVisibility m_overlayVisibility;  ///< Which overlay elements are currently drawn.
    QVector<QGraphicsItem *> m_auxiliaryItems;    ///< Scene items created for m_auxiliaryRois; torn down and rebuilt on change.
    QVector<QGraphicsItem *> m_overlayItems;      ///< Scene items created for m_overlay/m_overlayVisibility; torn down and rebuilt on change.
    QList<vision_canvas_detail::RoiItemBase *> m_editableItems; ///< Interactive ROI items currently on the scene.
    int m_selectedResultObject{-1};               ///< Index of the selected result object in read-only mode, or -1 if none.
    int m_hoveredResultObject{-1};                ///< Index of the hovered result object in read-only mode, or -1 if none.
    bool m_sceneInteractionLocked{false};          ///< Shared flag passed to ROI items to suppress interaction (e.g. while rebuilding).
    QVector<QVector<VisionRoi>> m_history;        ///< ROI undo/redo snapshots.
    int m_historyIndex{-1};                       ///< Index of the current snapshot within m_history.
    bool m_historyLocked{false};                  ///< True while replaying a snapshot, to suppress recording a new undo entry.
};

#endif // VISION_CANVAS_H
