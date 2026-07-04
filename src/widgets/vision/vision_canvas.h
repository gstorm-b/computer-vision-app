#ifndef VISION_CANVAS_H
#define VISION_CANVAS_H

#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <QVector>
#include <opencv2/core/mat.hpp>

#include "widgets/vision/vision_overlay_types.h"
#include "widgets/vision/vision_tool_palette.h"

class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QGraphicsItem;

namespace vision_canvas_detail {
class RoiItemBase;
}

class VisionCanvas : public QGraphicsView {
    Q_OBJECT

public:
    explicit VisionCanvas(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    void setImage(const QPixmap &pixmap);
    void clearImage();

    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }

    void setToolMode(VisionToolPalette::ToolMode mode);
    VisionToolPalette::ToolMode toolMode() const { return m_toolMode; }

    void setEditableRois(const QVector<VisionRoi> &rois);
    QVector<VisionRoi> editableRois() const;

    void setAuxiliaryRois(const QVector<VisionRoi> &rois);
    void setResultOverlay(const VisionResultOverlay &overlay);
    void setOverlayVisibility(const VisionOverlayVisibility &visibility);
    void setSelectedResultObject(int objectIndex);
    void clearSelectedResultObject();
    int selectedResultObject() const { return m_selectedResultObject; }

    VisionRoi selectedRoi() const;
    bool hasSelectedRoi() const;
    QSize imageSize() const;

    void updateSelectedRoi(const VisionRoi &roi);
    void deleteSelectedRoi();
    void fitImageToView();
    void undo();
    void redo();

signals:
    void roisChanged(QVector<VisionRoi> rois);
    void selectedRoiChanged(VisionRoi roi);
    void undoAvailabilityChanged(bool canUndo, bool canRedo);
    void resultObjectSelectionChanged(int objectIndex);
    void toolModeChanged(VisionToolPalette::ToolMode mode);

protected:
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void rebuildAuxiliaryRois();
    void rebuildOverlayItems();
    void rebuildEditableRois(const QVector<VisionRoi> &rois);
    void registerEditableItem(vision_canvas_detail::RoiItemBase *item);
    void updateSelectionFromScene();
    void emitRoisChanged();
    void pushUndoSnapshotIfChanged();
    void applyUndoSnapshot(int index);
    QVector<VisionRoi> currentSnapshot() const;
    int resultObjectAtScenePoint(const QPointF &scenePoint) const;
    void setHoveredResultObject(int objectIndex);
    QRectF imageBounds() const;
    QPointF clampScenePointToImage(const QPointF &scenePoint) const;
    void syncCursor();
    void beginPan(const QPoint &pos);
    void updatePan(const QPoint &pos);
    void endPan();
    void beginDraw(const QPointF &imagePoint);
    void updateDraw(const QPointF &imagePoint);
    void endDraw(const QPointF &imagePoint);

private slots:
    void onEditableItemGeometryChanged();
    void onEditableItemGeometryFinished();
    void onEditableItemSelectionChanged();

private:
    QGraphicsScene *m_scene{nullptr};
    QGraphicsPixmapItem *m_imageItem{nullptr};
    QSize m_fittedImageSize;
    QGraphicsRectItem *m_drawRectItem{nullptr};
    VisionToolPalette::ToolMode m_toolMode{VisionToolPalette::ToolMode::SelectMove};
    bool m_readOnly{false};
    bool m_panning{false};
    QPoint m_lastPanPoint;
    bool m_drawing{false};
    QPointF m_drawStart;
    QVector<VisionRoi> m_auxiliaryRois;
    VisionResultOverlay m_overlay;
    VisionOverlayVisibility m_overlayVisibility;
    QVector<QGraphicsItem *> m_auxiliaryItems;
    QVector<QGraphicsItem *> m_overlayItems;
    QList<vision_canvas_detail::RoiItemBase *> m_editableItems;
    int m_selectedResultObject{-1};
    int m_hoveredResultObject{-1};
    bool m_sceneInteractionLocked{false};
    QVector<QVector<VisionRoi>> m_history;
    int m_historyIndex{-1};
    bool m_historyLocked{false};
};

#endif // VISION_CANVAS_H
