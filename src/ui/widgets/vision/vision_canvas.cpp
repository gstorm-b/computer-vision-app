#include "ui/widgets/vision/vision_canvas.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QDateTime>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollBar>
#include <QtMath>

#include "core/utils/theme_manager.h"
#include "ui/widgets/image_widget/item_roi.h"
#include "ui/widgets/image_widget/item_roi_rotated.h"
#include "ui/widgets/vision/vision_geometry.h"

namespace {

QColor tokenColor(const QString &token)
{
    return QColor(ThemeManager::tokenValue(token, ThemeManager::instance()->isDark()));
}

QPen cosmeticPen(const QColor &color, qreal width, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(color, width, style);
    pen.setCosmetic(true);
    return pen;
}

QPen makeOverlayPen(const VisionResultObject &object)
{
    if (object.faulted) {
        return cosmeticPen(tokenColor(QStringLiteral("state.error")),
                           object.sentToOutput ? 3.0 : 2.0);
    }
    if (object.rejected) {
        return cosmeticPen(tokenColor(QStringLiteral("state.warning")), 2.0);
    }
    if (object.sentToOutput) {
        return cosmeticPen(tokenColor(QStringLiteral("state.success")), 3.0);
    }
    return cosmeticPen(tokenColor(QStringLiteral("state.info")), 2.0);
}

QColor colorWithAlphaScale(const QColor &color, qreal scale)
{
    QColor scaled = color;
    scaled.setAlphaF(qBound(0.0, color.alphaF() * scale, 1.0));
    return scaled;
}

QPen makeResultPen(const VisionResultObject &object,
                   bool selected,
                   bool muted,
                   bool hovered)
{
    QPen pen = makeOverlayPen(object);
    if (selected) {
        pen.setWidthF(pen.widthF() + 1.5);
    } else if (hovered) {
        pen.setWidthF(pen.widthF() + 0.8);
    } else if (muted) {
        pen.setColor(colorWithAlphaScale(pen.color(), 0.28));
    }
    return pen;
}

QPen makePickingBoxPen(const QColor &baseColor, bool selected, bool hovered)
{
    QPen pen(baseColor, selected ? 2.4 : (hovered ? 2.0 : 1.6), Qt::DashLine);
    pen.setCosmetic(true);
    return pen;
}

QString overlayLabel(const VisionResultObject &object,
                     const VisionOverlayVisibility &visibility)
{
    QStringList parts;
    if (visibility.showPatternId) {
        parts << QStringLiteral("#%1 %2").arg(object.patternIndex).arg(object.patternName);
    }
    if (visibility.showScore) {
        parts << QStringLiteral("S:%1").arg(object.score, 0, 'f', 3);
    }
    if (visibility.showAngle) {
        parts << QStringLiteral("A:%1").arg(object.pointAngleDeg, 0, 'f', 1);
    }
    return parts.join(QStringLiteral("  "));
}

QGraphicsRectItem *addOverlayChip(QGraphicsScene *scene,
                                  const QPointF &scenePos,
                                  const QString &text,
                                  const QColor &textColor,
                                  qreal zValue = 3.0)
{
    if (!scene || text.isEmpty()) return nullptr;

    auto *textItem = new QGraphicsSimpleTextItem(text);
    textItem->setBrush(QBrush(textColor));

    const QRectF textBounds = textItem->boundingRect();
    constexpr qreal kPaddingX = 6.0;
    constexpr qreal kPaddingY = 3.0;

    auto *chipItem = new QGraphicsRectItem(
        QRectF(0.0, 0.0,
               textBounds.width() + kPaddingX * 2.0,
               textBounds.height() + kPaddingY * 2.0));
    chipItem->setBrush(QColor(0, 0, 0, 176));
    chipItem->setPen(cosmeticPen(QColor(255, 255, 255, 64), 1.0));
    chipItem->setPos(scenePos);
    chipItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    chipItem->setZValue(zValue);
    scene->addItem(chipItem);

    textItem->setParentItem(chipItem);
    textItem->setPos(kPaddingX, kPaddingY);
    return chipItem;
}

QPointF normalizeVector(const QPointF &vector)
{
    const qreal length = std::hypot(vector.x(), vector.y());
    if (length <= 0.0001) return {};
    return QPointF(vector.x() / length, vector.y() / length);
}

QPainterPath arrowPath(const QPointF &direction, qreal length, qreal headSize)
{
    const QPointF unit = normalizeVector(direction);
    if (qFuzzyIsNull(unit.x()) && qFuzzyIsNull(unit.y())) {
        return {};
    }

    const QPointF tip = unit * length;
    const QPointF normal(-unit.y(), unit.x());

    QPainterPath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(tip);
    path.moveTo(tip.x(), tip.y());
    path.lineTo(tip.x() - unit.x() * headSize + normal.x() * headSize * 0.55,
                tip.y() - unit.y() * headSize + normal.y() * headSize * 0.55);
    path.moveTo(tip.x(), tip.y());
    path.lineTo(tip.x() - unit.x() * headSize - normal.x() * headSize * 0.55,
                tip.y() - unit.y() * headSize - normal.y() * headSize * 0.55);
    return path;
}

QGraphicsPathItem *addArrowMarker(QGraphicsScene *scene,
                                  const QPointF &scenePos,
                                  const QPointF &direction,
                                  qreal length,
                                  const QColor &color,
                                  qreal zValue = 3.5)
{
    if (!scene) return nullptr;

    const QPainterPath path = arrowPath(direction, length, qMax<qreal>(4.0, length * 0.28));
    if (path.isEmpty()) return nullptr;

    auto *item = new QGraphicsPathItem(path);
    item->setPen(cosmeticPen(color, 1.8));
    item->setPos(scenePos);
    item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    item->setZValue(zValue);
    scene->addItem(item);
    return item;
}

QColor pickingBoxColor(bool muted)
{
    const QColor color = tokenColor(QStringLiteral("accent.primary"));
    return muted ? colorWithAlphaScale(color, 0.30) : color;
}

QPointF labelAnchorForBounds(const QRectF &bounds)
{
    const qreal aboveY = bounds.top() - 30.0;
    if (aboveY >= 0.0) {
        return QPointF(bounds.left() + 10.0, aboveY);
    }
    return bounds.topLeft() + QPointF(8.0, 8.0);
}

} // namespace

namespace vision_canvas_detail {

class VisionCanvasItemBase : public QObject {
    Q_OBJECT
public:
    explicit VisionCanvasItemBase(QObject *parent = nullptr) : QObject(parent) {}
signals:
    void geometryChanged();
    void geometryFinished();
    void selectionChanged();
};

class RoiItemBase : public VisionCanvasItemBase {
    Q_OBJECT
public:
    using VisionCanvasItemBase::VisionCanvasItemBase;
    virtual ~RoiItemBase() = default;

    virtual QGraphicsItem *graphicsItem() = 0;
    virtual VisionRoi roi() const = 0;
};

class VisionRectRoiItem : public RoiItemBase, public ItemRoi {
public:
    explicit VisionRectRoiItem(const VisionRoi &roi,
                               QGraphicsItem *parentItem,
                               bool *ignoreFlag)
        : RoiItemBase(),
          ItemRoi(QRectF(-roi.size.width() * 0.5, -roi.size.height() * 0.5,
                         roi.size.width(), roi.size.height()),
                  parentItem,
                  ignoreFlag),
          m_id(roi.id),
          m_label(roi.label)
    {
        setPos(roi.center);
        if (roi.color.isValid()) {
            setBoundingColorNormal(roi.color);
        }
    }

    VisionRoi roi() const
    {
        VisionRoi roi;
        roi.id = m_id;
        roi.label = m_label;
        roi.shape = VisionRoiShape::AxisAlignedRect;
        roi.center = mapToParent(rect().center());
        roi.size = rect().size();
        roi.visible = isVisible();
        roi.selected = isSelected();
        return roi;
    }

    QGraphicsItem *graphicsItem() override
    {
        return this;
    }

protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemSelectedHasChanged) {
            emit selectionChanged();
        }
        return ItemRoi::itemChange(change, value);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoi::mouseMoveEvent(event);
        emit geometryChanged();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoi::mouseReleaseEvent(event);
        emit geometryChanged();
        emit geometryFinished();
    }

private:
    QString m_id;
    QString m_label;
};

class VisionRotatedRoiItem : public RoiItemBase, public ItemRoiRotated {
public:
    explicit VisionRotatedRoiItem(const VisionRoi &roi,
                                  QGraphicsItem *parentItem,
                                  bool *ignoreFlag)
        : RoiItemBase(),
          ItemRoiRotated(QRectF(-roi.size.width() * 0.5, -roi.size.height() * 0.5,
                                roi.size.width(), roi.size.height()),
                         parentItem,
                         ignoreFlag),
          m_id(roi.id),
          m_label(roi.label)
    {
        setPos(roi.center);
        setRotation(roi.angleDeg);
    }

    VisionRoi roi() const
    {
        VisionRoi roi;
        roi.id = m_id;
        roi.label = m_label;
        roi.shape = VisionRoiShape::RotatedRect;
        roi.center = mapToParent(rect().center());
        roi.size = rect().size();
        roi.angleDeg = rotation();
        roi.visible = isVisible();
        roi.selected = isSelected();
        return roi;
    }

    QGraphicsItem *graphicsItem() override
    {
        return this;
    }

protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemSelectedHasChanged) {
            emit selectionChanged();
        }
        return ItemRoiRotated::itemChange(change, value);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoiRotated::mouseMoveEvent(event);
        emit geometryChanged();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoiRotated::mouseReleaseEvent(event);
        emit geometryChanged();
        emit geometryFinished();
    }

private:
    QString m_id;
    QString m_label;
};

} // namespace vision_canvas_detail

VisionCanvas::VisionCanvas(QWidget *parent)
    : QGraphicsView(parent),
      m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setBackgroundBrush(tokenColor(QStringLiteral("bg.window")));

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const QString &, bool) {
                setBackgroundBrush(tokenColor(QStringLiteral("bg.window")));
                rebuildAuxiliaryRois();
                rebuildOverlayItems();
            });
}

void VisionCanvas::setImage(const cv::Mat &image)
{
    setImage(vision::pixmapFromMat(image));
}

void VisionCanvas::setImage(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        clearImage();
        return;
    }

    // Only re-fit the view when the incoming image size changes. Successive
    // frames of the same resolution (e.g. a live camera feed) keep the user's
    // current zoom/pan instead of snapping back to fit on every frame.
    const QSize newSize = pixmap.size();
    const bool sizeChanged = (newSize != m_fittedImageSize);

    if (!m_imageItem) {
        m_imageItem = m_scene->addPixmap(pixmap);
        m_imageItem->setTransformationMode(Qt::FastTransformation);
    } else {
        m_imageItem->setPixmap(pixmap);
    }

    m_scene->setSceneRect(pixmap.rect());
    rebuildEditableRois(editableRois());
    rebuildAuxiliaryRois();
    rebuildOverlayItems();

    if (sizeChanged) {
        m_fittedImageSize = newSize;
        fitImageToView();
    }
}

void VisionCanvas::clearImage()
{
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        delete item;
    }
    m_editableItems.clear();

    if (m_imageItem) {
        m_scene->removeItem(m_imageItem);
        delete m_imageItem;
        m_imageItem = nullptr;
    }
    // Reset so the next image always re-fits, even if it happens to match the
    // size of the one that was just cleared.
    m_fittedImageSize = QSize();
    if (m_drawRectItem) {
        delete m_drawRectItem;
        m_drawRectItem = nullptr;
    }
    m_history.clear();
    m_history.append(QVector<VisionRoi>{});
    m_historyIndex = 0;
    emit undoAvailabilityChanged(false, false);
    m_auxiliaryRois.clear();
    m_overlay = VisionResultOverlay();
    m_selectedResultObject = -1;
    m_hoveredResultObject = -1;
    rebuildAuxiliaryRois();
    rebuildOverlayItems();
}

void VisionCanvas::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    syncCursor();
}

void VisionCanvas::setToolMode(VisionToolPalette::ToolMode mode)
{
    const bool changed = (m_toolMode != mode);
    m_toolMode = mode;
    syncCursor();
    if (changed) {
        emit toolModeChanged(m_toolMode);
    }
}

void VisionCanvas::setEditableRois(const QVector<VisionRoi> &rois)
{
    rebuildEditableRois(rois);
    m_history.clear();
    m_history.append(currentSnapshot());
    m_historyIndex = 0;
    emit undoAvailabilityChanged(false, false);
    emitRoisChanged();
}

QVector<VisionRoi> VisionCanvas::editableRois() const
{
    QVector<VisionRoi> rois;
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        if (item) {
            rois.append(vision::normalizedRoi(item->roi()));
        }
    }
    return rois;
}

void VisionCanvas::setAuxiliaryRois(const QVector<VisionRoi> &rois)
{
    m_auxiliaryRois = rois;
    rebuildAuxiliaryRois();
}

void VisionCanvas::setResultOverlay(const VisionResultOverlay &overlay)
{
    // Overlay visibility is sticky UI state owned by setOverlayVisibility().
    // A result overlay carries data only and must not silently reset which
    // overlay items the user chose to show.
    m_overlay = overlay;
    rebuildOverlayItems();
}

void VisionCanvas::setOverlayVisibility(const VisionOverlayVisibility &visibility)
{
    m_overlayVisibility = visibility;
    rebuildOverlayItems();
}

void VisionCanvas::setSelectedResultObject(int objectIndex)
{
    if (m_selectedResultObject == objectIndex) return;
    m_selectedResultObject = objectIndex;
    if (m_selectedResultObject > 0) {
        m_hoveredResultObject = -1;
    }
    rebuildOverlayItems();
    emit resultObjectSelectionChanged(m_selectedResultObject);
}

void VisionCanvas::clearSelectedResultObject()
{
    setSelectedResultObject(-1);
}

int VisionCanvas::resultObjectAtScenePoint(const QPointF &scenePoint) const
{
    auto matchIndex = [scenePoint](const QVector<VisionResultObject> &objects, int preferredIndex) {
        if (preferredIndex > 0) {
            for (const VisionResultObject &object : objects) {
                if (object.index == preferredIndex
                    && QPolygonF(object.corners).containsPoint(scenePoint, Qt::OddEvenFill)) {
                    return object.index;
                }
            }
        }

        for (auto it = objects.crbegin(); it != objects.crend(); ++it) {
            if (QPolygonF(it->corners).containsPoint(scenePoint, Qt::OddEvenFill)) {
                return it->index;
            }
        }
        return -1;
    };

    const int acceptedHit = matchIndex(m_overlay.acceptedObjects, m_selectedResultObject);
    if (acceptedHit > 0) return acceptedHit;

    if (m_overlayVisibility.showRejectedCandidates) {
        return matchIndex(m_overlay.rejectedObjects, m_selectedResultObject);
    }

    return -1;
}

void VisionCanvas::setHoveredResultObject(int objectIndex)
{
    if (m_hoveredResultObject == objectIndex) return;
    m_hoveredResultObject = objectIndex;

    if (!m_panning) {
        syncCursor();
    }

    rebuildOverlayItems();
}

VisionRoi VisionCanvas::selectedRoi() const
{
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        if (item && item->roi().selected) {
            return item->roi();
        }
    }
    return {};
}

bool VisionCanvas::hasSelectedRoi() const
{
    return !selectedRoi().id.isEmpty();
}

QSize VisionCanvas::imageSize() const
{
    if (!m_imageItem) return {};
    return m_imageItem->pixmap().size();
}

void VisionCanvas::updateSelectedRoi(const VisionRoi &roi)
{
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        if (item && item->roi().id == roi.id) {
            QVector<VisionRoi> rois = editableRois();
            for (VisionRoi &existing : rois) {
                if (existing.id == roi.id) {
                    existing = vision::clampRoiToImage(roi, imageSize());
                    break;
                }
            }
            rebuildEditableRois(rois);
            emitRoisChanged();
            pushUndoSnapshotIfChanged();
            return;
        }
    }
}

void VisionCanvas::deleteSelectedRoi()
{
    if (m_readOnly) return;
    QVector<VisionRoi> rois = editableRois();
    const QString selectedId = selectedRoi().id;
    if (selectedId.isEmpty()) return;
    QVector<VisionRoi> filtered;
    filtered.reserve(rois.size());
    for (const VisionRoi &roi : rois) {
        if (roi.id != selectedId) {
            filtered.append(roi);
        }
    }
    rebuildEditableRois(filtered);
    emitRoisChanged();
    pushUndoSnapshotIfChanged();
}

void VisionCanvas::fitImageToView()
{
    if (m_imageItem) {
        fitInView(m_imageItem->boundingRect(), Qt::KeepAspectRatio);
    }
}

void VisionCanvas::undo()
{
    if (m_historyIndex <= 0) return;
    applyUndoSnapshot(m_historyIndex - 1);
}

void VisionCanvas::redo()
{
    if (m_historyIndex < 0 || m_historyIndex + 1 >= m_history.size()) return;
    applyUndoSnapshot(m_historyIndex + 1);
}

void VisionCanvas::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);

    if (event->modifiers() & Qt::ControlModifier) {
        beginPan(event->pos());
        return;
    }

    if (m_readOnly && m_imageItem && event->button() == Qt::LeftButton) {
        const int objectIndex = resultObjectAtScenePoint(mapToScene(event->pos()));
        if (objectIndex > 0) {
            setSelectedResultObject(objectIndex);
            event->accept();
            return;
        }
    }

    if (m_toolMode == VisionToolPalette::ToolMode::Pan) {
        beginPan(event->pos());
        return;
    }

    if (!m_readOnly
        && (m_toolMode == VisionToolPalette::ToolMode::DrawAxisAlignedRect
            || m_toolMode == VisionToolPalette::ToolMode::DrawRotatedRect)
        && event->button() == Qt::LeftButton
        && m_imageItem) {
        beginDraw(clampScenePointToImage(mapToScene(event->pos())));
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void VisionCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        updatePan(event->pos());
        return;
    }

    if (m_drawing) {
        updateDraw(clampScenePointToImage(mapToScene(event->pos())));
        return;
    }

    if (m_readOnly && m_imageItem) {
        setHoveredResultObject(resultObjectAtScenePoint(mapToScene(event->pos())));
    }

    QGraphicsView::mouseMoveEvent(event);
}

void VisionCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning) {
        endPan();
        if (m_readOnly && m_imageItem) {
            setHoveredResultObject(resultObjectAtScenePoint(mapToScene(event->pos())));
        }
        return;
    }

    if (m_drawing) {
        endDraw(clampScenePointToImage(mapToScene(event->pos())));
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
    updateSelectionFromScene();
}

void VisionCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        fitImageToView();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void VisionCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.15 : 0.85;
        scale(factor, factor);
        event->accept();
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void VisionCanvas::leaveEvent(QEvent *event)
{
    if (m_readOnly && !m_panning) {
        setHoveredResultObject(-1);
    }
    QGraphicsView::leaveEvent(event);
}

void VisionCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_panning) {
        endPan();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_toolMode == VisionToolPalette::ToolMode::Pan) {
        setToolMode(VisionToolPalette::ToolMode::SelectMove);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_selectedResultObject >= 0) {
        clearSelectedResultObject();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        deleteSelectedRoi();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void VisionCanvas::rebuildAuxiliaryRois()
{
    for (QGraphicsItem *item : m_auxiliaryItems) {
        delete item;
    }
    m_auxiliaryItems.clear();

    if (!m_imageItem) return;

    for (const VisionRoi &roi : m_auxiliaryRois) {
        if (!roi.visible || !roi.isValid()) continue;
        auto *item = m_scene->addPolygon(
            vision::roiPolygon(roi),
            cosmeticPen(roi.color.isValid() ? roi.color
                                            : tokenColor(QStringLiteral("state.info")),
                        2.0,
                        Qt::DashLine));
        m_auxiliaryItems.append(item);

        if (!roi.label.isEmpty()) {
            const QPolygonF polygon = vision::roiPolygon(roi);
            const QRectF bounds = vision::boundingRectForPoints(polygon.toVector());
            if (auto *chipItem = addOverlayChip(
                    m_scene,
                    labelAnchorForBounds(bounds),
                    roi.label,
                    roi.color.isValid() ? roi.color : QColor(Qt::white))) {
                m_auxiliaryItems.append(chipItem);
            }
        }
    }
}

void VisionCanvas::rebuildOverlayItems()
{
    for (QGraphicsItem *item : m_overlayItems) {
        delete item;
    }
    m_overlayItems.clear();

    if (!m_imageItem) return;

    auto addObjectItems = [this](const QVector<VisionResultObject> &objects, bool respectRejectedToggle) {
        const bool hasSelection = m_selectedResultObject > 0;
        for (const VisionResultObject &object : objects) {
            if (respectRejectedToggle && !m_overlayVisibility.showRejectedCandidates) {
                continue;
            }

            const bool selected = object.index == m_selectedResultObject;
            const bool hovered = !hasSelection && object.index == m_hoveredResultObject;
            const bool muted = hasSelection && !selected;
            const bool showObjectDetails = selected || (!hasSelection && !muted);
            const QPen overlayPen = makeResultPen(object, selected, muted, hovered);
            const QColor overlayColor = overlayPen.color();

            auto *polygonItem = m_scene->addPolygon(QPolygonF(object.corners), overlayPen);
            if (selected) {
                polygonItem->setBrush(QColor(overlayColor.red(), overlayColor.green(), overlayColor.blue(), 28));
            } else if (hovered) {
                polygonItem->setBrush(QColor(overlayColor.red(), overlayColor.green(), overlayColor.blue(), 18));
            } else {
                polygonItem->setBrush(Qt::NoBrush);
            }
            polygonItem->setZValue(selected ? 3.2 : (hovered ? 2.6 : 2.0));
            m_overlayItems.append(polygonItem);

            if (!muted) {
                auto *centerItem = m_scene->addEllipse(-4.0,
                                                       -4.0,
                                                       8.0,
                                                       8.0,
                                                       overlayPen,
                                                       QBrush(overlayColor));
                centerItem->setPos(object.center);
                centerItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
                centerItem->setZValue(selected ? 4.4 : (hovered ? 3.6 : 3.0));
                m_overlayItems.append(centerItem);
            }

            if (showObjectDetails) {
                const qreal angleRad = qDegreesToRadians(object.pointAngleDeg);
                const QPointF xAxis(std::cos(angleRad), std::sin(angleRad));
                const QPointF yAxis(-std::sin(angleRad), std::cos(angleRad));
                const QColor xColor = tokenColor(QStringLiteral("state.success"));
                const QColor yColor = tokenColor(QStringLiteral("state.info"));
                if (auto *xArrow = addArrowMarker(m_scene, object.center, xAxis, 20.0, xColor, 4.1)) {
                    m_overlayItems.append(xArrow);
                }
                if (auto *yArrow = addArrowMarker(m_scene, object.center, yAxis, 16.0, yColor, 4.1)) {
                    m_overlayItems.append(yArrow);
                }
            }

            if (m_overlayVisibility.showPickingBoxes && !object.pickingBoxPolygons.isEmpty() && !muted) {
                for (const QVector<QPointF> &polygonPoints : object.pickingBoxPolygons) {
                    auto *pickingBoxItem = m_scene->addPolygon(
                        QPolygonF(polygonPoints),
                        makePickingBoxPen(pickingBoxColor(false), selected, hovered));
                    pickingBoxItem->setBrush(Qt::NoBrush);
                    pickingBoxItem->setZValue(selected ? 3.9 : (hovered ? 3.3 : 2.5));
                    m_overlayItems.append(pickingBoxItem);
                }
            }

            if (showObjectDetails && object.corners.size() >= 4) {
                const QColor locatorColor = overlayColor;
                const QPointF &topLeftCorner = object.corners.at(0);
                const QPointF &topRightCorner = object.corners.at(1);
                const QPointF &bottomLeftCorner = object.corners.at(3);

                if (auto *verticalLocator = addArrowMarker(
                        m_scene,
                        topLeftCorner,
                        bottomLeftCorner - topLeftCorner,
                        16.0,
                        locatorColor,
                        4.0)) {
                    m_overlayItems.append(verticalLocator);
                }
                if (auto *horizontalLocator = addArrowMarker(
                        m_scene,
                        topLeftCorner,
                        topRightCorner - topLeftCorner,
                        16.0,
                        locatorColor,
                        4.0)) {
                    m_overlayItems.append(horizontalLocator);
                }
            }

            const QString label = overlayLabel(object, m_overlayVisibility);
            if (showObjectDetails && !label.isEmpty()) {
                const QRectF bounds = vision::boundingRectForPoints(object.corners);
                const QPointF labelPos = labelAnchorForBounds(bounds);
                if (auto *chipItem = addOverlayChip(m_scene,
                                                    labelPos,
                                                    label,
                                                    QColor(Qt::white),
                                                    selected ? 4.6 : (hovered ? 3.8 : 3.0))) {
                    m_overlayItems.append(chipItem);
                }
            }

            if (showObjectDetails
                && object.sentToOutput
                && m_overlayVisibility.showSentToOutputMarkers) {
                if (auto *chipItem = addOverlayChip(
                        m_scene,
                        object.center + QPointF(10.0, -28.0),
                        QStringLiteral("OUT"),
                        tokenColor(QStringLiteral("state.success")),
                        4.0)) {
                    m_overlayItems.append(chipItem);
                }
            }

            if (showObjectDetails
                && object.faulted
                && m_overlayVisibility.showFaultMarkers) {
                if (auto *chipItem = addOverlayChip(
                        m_scene,
                        object.center + QPointF(10.0, 10.0),
                        QStringLiteral("FAULT"),
                        tokenColor(QStringLiteral("state.error")),
                        4.0)) {
                    m_overlayItems.append(chipItem);
                }
            }
        }
    };

    addObjectItems(m_overlay.acceptedObjects, false);
    addObjectItems(m_overlay.rejectedObjects, true);

    for (const VisionRoi &roi : m_overlay.roiOverlays) {
        if (!roi.visible) continue;
        const QPolygonF polygon = vision::roiPolygon(roi);
        auto *polygonItem = m_scene->addPolygon(
            polygon,
            cosmeticPen(roi.color.isValid() ? roi.color
                                            : tokenColor(QStringLiteral("state.info")),
                        2.0,
                        Qt::DashLine));
        polygonItem->setBrush(Qt::NoBrush);
        m_overlayItems.append(polygonItem);

        if (!roi.label.isEmpty()) {
            const QRectF bounds = vision::boundingRectForPoints(polygon.toVector());
            if (auto *chipItem = addOverlayChip(
                    m_scene,
                    labelAnchorForBounds(bounds),
                    roi.label,
                    roi.color.isValid() ? roi.color : QColor(Qt::white))) {
                m_overlayItems.append(chipItem);
            }
        }
    }

    if (m_overlayVisibility.showRuntimeSignalValues && !m_overlay.runtimeSignalValues.isEmpty()) {
        QStringList rows;
        for (auto it = m_overlay.runtimeSignalValues.cbegin(); it != m_overlay.runtimeSignalValues.cend(); ++it) {
            rows << QStringLiteral("%1: %2").arg(it.key(), it.value().toString());
        }
        if (auto *chipItem = addOverlayChip(m_scene,
                                            QPointF(8.0, 8.0),
                                            rows.join(QLatin1Char('\n')),
                                            QColor(Qt::white),
                                            4.0)) {
            m_overlayItems.append(chipItem);
        }
    }
}

void VisionCanvas::rebuildEditableRois(const QVector<VisionRoi> &rois)
{
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        delete item;
    }
    m_editableItems.clear();

    if (!m_imageItem) return;

    for (const VisionRoi &roi : rois) {
        VisionRoi normalized = vision::clampRoiToImage(roi, imageSize());
        if (!normalized.isValid()) continue;

        vision_canvas_detail::RoiItemBase *item = nullptr;
        if (normalized.shape == VisionRoiShape::RotatedRect) {
            item = new vision_canvas_detail::VisionRotatedRoiItem(
                normalized, m_imageItem, &m_sceneInteractionLocked);
        } else {
            item = new vision_canvas_detail::VisionRectRoiItem(
                normalized, m_imageItem, &m_sceneInteractionLocked);
        }

        registerEditableItem(item);
        if (normalized.selected) {
            item->graphicsItem()->setSelected(true);
        }
    }
}

void VisionCanvas::registerEditableItem(vision_canvas_detail::RoiItemBase *item)
{
    if (!item) return;
    m_editableItems.append(item);
    connect(item, SIGNAL(geometryChanged()), this, SLOT(onEditableItemGeometryChanged()));
    connect(item, SIGNAL(geometryFinished()), this, SLOT(onEditableItemGeometryFinished()));
    connect(item, SIGNAL(selectionChanged()), this, SLOT(onEditableItemSelectionChanged()));
}

void VisionCanvas::updateSelectionFromScene()
{
    VisionRoi roi = selectedRoi();
    emit selectedRoiChanged(roi);
}

void VisionCanvas::emitRoisChanged()
{
    emit roisChanged(editableRois());
    emit selectedRoiChanged(selectedRoi());
}

void VisionCanvas::pushUndoSnapshotIfChanged()
{
    if (m_historyLocked) return;

    const QVector<VisionRoi> snapshot = currentSnapshot();
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size()
        && m_history[m_historyIndex] == snapshot) {
        return;
    }

    while (m_history.size() > m_historyIndex + 1) {
        m_history.removeLast();
    }
    m_history.append(snapshot);
    m_historyIndex = m_history.size() - 1;
    emit undoAvailabilityChanged(m_historyIndex > 0, false);
}

void VisionCanvas::applyUndoSnapshot(int index)
{
    if (index < 0 || index >= m_history.size()) return;
    m_historyLocked = true;
    rebuildEditableRois(m_history.at(index));
    m_historyLocked = false;
    m_historyIndex = index;
    emit undoAvailabilityChanged(m_historyIndex > 0, m_historyIndex + 1 < m_history.size());
    emitRoisChanged();
}

QVector<VisionRoi> VisionCanvas::currentSnapshot() const
{
    QVector<VisionRoi> snapshot = editableRois();
    for (VisionRoi &roi : snapshot) {
        roi.selected = false;
    }
    return snapshot;
}

QRectF VisionCanvas::imageBounds() const
{
    return m_imageItem ? m_imageItem->boundingRect() : QRectF();
}

QPointF VisionCanvas::clampScenePointToImage(const QPointF &scenePoint) const
{
    const QRectF bounds = imageBounds();
    if (bounds.isNull()) return scenePoint;
    return QPointF(qBound(bounds.left(), scenePoint.x(), bounds.right()),
                   qBound(bounds.top(), scenePoint.y(), bounds.bottom()));
}

void VisionCanvas::syncCursor()
{
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (m_toolMode == VisionToolPalette::ToolMode::Pan) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (m_toolMode == VisionToolPalette::ToolMode::DrawAxisAlignedRect
        || m_toolMode == VisionToolPalette::ToolMode::DrawRotatedRect) {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_readOnly && m_hoveredResultObject > 0) {
        setCursor(Qt::PointingHandCursor);
        return;
    }

    setCursor(Qt::ArrowCursor);
}

void VisionCanvas::beginPan(const QPoint &pos)
{
    m_panning = true;
    m_lastPanPoint = pos;
    syncCursor();
}

void VisionCanvas::updatePan(const QPoint &pos)
{
    const QPoint delta = pos - m_lastPanPoint;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    m_lastPanPoint = pos;
}

void VisionCanvas::endPan()
{
    m_panning = false;
    syncCursor();
}

void VisionCanvas::beginDraw(const QPointF &imagePoint)
{
    m_drawing = true;
    m_drawStart = imagePoint;
    if (!m_drawRectItem) {
        m_drawRectItem = m_scene->addRect(QRectF(imagePoint, imagePoint),
                                          cosmeticPen(tokenColor(QStringLiteral("accent.primary")),
                                                      2.0,
                                                      Qt::DashLine));
    } else {
        m_drawRectItem->setRect(QRectF(imagePoint, imagePoint));
    }
}

void VisionCanvas::updateDraw(const QPointF &imagePoint)
{
    if (!m_drawRectItem) return;
    m_drawRectItem->setRect(QRectF(m_drawStart, imagePoint).normalized());
}

void VisionCanvas::endDraw(const QPointF &imagePoint)
{
    m_drawing = false;
    const QRectF rect = QRectF(m_drawStart, imagePoint).normalized();
    if (m_drawRectItem) {
        delete m_drawRectItem;
        m_drawRectItem = nullptr;
    }

    if (rect.width() < 4.0 || rect.height() < 4.0) return;

    QVector<VisionRoi> rois = editableRois();
    VisionRoi roi;
    roi.id = QStringLiteral("roi_%1").arg(QDateTime::currentMSecsSinceEpoch());
    roi.label = QStringLiteral("ROI %1").arg(rois.size() + 1);
    roi.shape = m_toolMode == VisionToolPalette::ToolMode::DrawRotatedRect
                    ? VisionRoiShape::RotatedRect
                    : VisionRoiShape::AxisAlignedRect;
    roi.center = rect.center();
    roi.size = rect.size();
    roi.angleDeg = 0.0;
    rois.append(roi);
    rebuildEditableRois(rois);
    emitRoisChanged();
    pushUndoSnapshotIfChanged();
}

void VisionCanvas::onEditableItemGeometryChanged()
{
    emitRoisChanged();
}

void VisionCanvas::onEditableItemGeometryFinished()
{
    emitRoisChanged();
    pushUndoSnapshotIfChanged();
}

void VisionCanvas::onEditableItemSelectionChanged()
{
    emit selectedRoiChanged(selectedRoi());
}

#include "vision_canvas.moc"
