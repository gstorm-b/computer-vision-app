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

/// File-local helpers for theme-aware pens/colors and for building the small
/// graphics-scene overlay items (label chips, arrow markers) drawn by VisionCanvas.
namespace {

/// Resolves a theme token (e.g. "state.error") to a QColor for the current
/// light/dark theme via ThemeManager.
QColor tokenColor(const QString &token)
{
    return QColor(ThemeManager::tokenValue(token, ThemeManager::instance()->isDark()));
}

/// Builds a QPen with the given color/width/style and marks it cosmetic (constant
/// on-screen width regardless of the view's zoom transform).
QPen cosmeticPen(const QColor &color, qreal width, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(color, width, style);
    pen.setCosmetic(true);
    return pen;
}

/// Picks the base overlay pen color/width for a result object by state: red for
/// faulted (thicker if also sent to output), amber for rejected, green for sent to
/// output, blue otherwise.
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

/// Returns `color` with its alpha multiplied by `scale` (clamped to [0,1]); used to
/// dim/mute overlay colors without changing hue.
QColor colorWithAlphaScale(const QColor &color, qreal scale)
{
    QColor scaled = color;
    scaled.setAlphaF(qBound(0.0, color.alphaF() * scale, 1.0));
    return scaled;
}

/// Derives the on-screen pen for a result object's outline from its base overlay
/// pen (see makeOverlayPen): widens it when selected/hovered, or fades it via
/// colorWithAlphaScale when muted (another object is selected).
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

/// Builds the dashed cosmetic pen used to draw a picking-box polygon, with width
/// stepped up when selected/hovered.
QPen makePickingBoxPen(const QColor &baseColor, bool selected, bool hovered)
{
    QPen pen(baseColor, selected ? 2.4 : (hovered ? 2.0 : 1.6), Qt::DashLine);
    pen.setCosmetic(true);
    return pen;
}

/// Composes the result object's label chip text from the fields the current
/// VisionOverlayVisibility enables: pattern id/name, match score, and point angle.
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

/// Adds a translucent black rounded-rect "chip" with `text` at `scenePos` to
/// `scene` (used for labels, OUT/FAULT markers, and the runtime-signal readout).
/// The chip ignores view transformations so its size stays constant on screen.
/// @return the created rect item (owned by the scene), or nullptr if `scene` is
/// null or `text` is empty.
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

/// Returns `vector` scaled to unit length, or (0,0) if its length is below a small
/// epsilon (avoids division by ~zero).
QPointF normalizeVector(const QPointF &vector)
{
    const qreal length = std::hypot(vector.x(), vector.y());
    if (length <= 0.0001) return {};
    return QPointF(vector.x() / length, vector.y() / length);
}

/// Builds a painter path for a straight arrow of `length` pointing along
/// `direction`, with a V-shaped arrowhead of `headSize`. Returns an empty path if
/// `direction` is a zero vector.
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

/// Adds an arrow-shaped path item (see arrowPath) at `scenePos` to `scene`, used to
/// mark a result object's orientation axes and corner locators. The item ignores
/// view transformations so its size stays constant on screen.
/// @return the created path item (owned by the scene), or nullptr if `scene` is
/// null or the arrow path is empty (zero-length direction).
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

/// Returns the accent color used to draw picking-box outlines, faded when `muted`
/// is true.
QColor pickingBoxColor(bool muted)
{
    const QColor color = tokenColor(QStringLiteral("accent.primary"));
    return muted ? colorWithAlphaScale(color, 0.30) : color;
}

/// Picks the scene position for a label/chip attached to `bounds`: 30px above the
/// top-left corner when that fits on screen, otherwise just inside the top-left
/// corner.
QPointF labelAnchorForBounds(const QRectF &bounds)
{
    const qreal aboveY = bounds.top() - 30.0;
    if (aboveY >= 0.0) {
        return QPointF(bounds.left() + 10.0, aboveY);
    }
    return bounds.topLeft() + QPointF(8.0, 8.0);
}

} // namespace

/// Internal helper types backing VisionCanvas's editable ROI items: a QObject-based
/// signal mixin plus the axis-aligned and rotated ROI graphics-item implementations.
namespace vision_canvas_detail {

/// QObject mixin providing the geometry/selection change signals shared by all
/// editable ROI item types (a separate base since the ROI items also derive from a
/// QGraphicsItem-based type via multiple inheritance).
class VisionCanvasItemBase : public QObject {
    Q_OBJECT
public:
    /// Constructs the signal mixin with the given QObject parent.
    explicit VisionCanvasItemBase(QObject *parent = nullptr) : QObject(parent) {}
signals:
    /// Emitted while the item's geometry is being interactively changed (drag in progress).
    void geometryChanged();
    /// Emitted once an interactive geometry change (move/resize) has completed.
    void geometryFinished();
    /// Emitted when the item's QGraphicsItem selection state changes.
    void selectionChanged();
};

/// Abstract interface for an editable ROI backed by a QGraphicsItem: exposes the
/// underlying graphics item and a conversion to the plain VisionRoi data type.
class RoiItemBase : public VisionCanvasItemBase {
    Q_OBJECT
public:
    using VisionCanvasItemBase::VisionCanvasItemBase;
    virtual ~RoiItemBase() = default;

    /// Returns the QGraphicsItem this ROI is implemented as (for scene membership,
    /// deletion, selection, etc.).
    virtual QGraphicsItem *graphicsItem() = 0;
    /// Returns the current geometry/state of this ROI as a plain VisionRoi value.
    virtual VisionRoi roi() const = 0;
};

/// Editable axis-aligned rectangular ROI item: wraps ItemRoi (the resize/move
/// handle behavior) and adds the id/label needed to round-trip to/from VisionRoi.
class VisionRectRoiItem : public RoiItemBase, public ItemRoi {
public:
    /// Constructs the item centered on `roi.center` with the size from `roi.size`,
    /// parented under `parentItem` (the image item) and sharing `ignoreFlag` with
    /// the canvas to suppress feedback while scene interaction is locked.
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

    /// Returns the current geometry as a VisionRoi (axis-aligned shape), reading
    /// center/size/visibility/selection back from the underlying QGraphicsItem.
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

    /// Returns this item as a QGraphicsItem.
    QGraphicsItem *graphicsItem() override
    {
        return this;
    }

protected:
    /// Forwards to ItemRoi::itemChange and additionally emits selectionChanged()
    /// when the item's selection state flips.
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemSelectedHasChanged) {
            emit selectionChanged();
        }
        return ItemRoi::itemChange(change, value);
    }

    /// Forwards to ItemRoi::mouseMoveEvent and emits geometryChanged() so the
    /// canvas can react live while the ROI is being dragged/resized.
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoi::mouseMoveEvent(event);
        emit geometryChanged();
    }

    /// Forwards to ItemRoi::mouseReleaseEvent and emits geometryChanged() followed
    /// by geometryFinished() so the canvas can push an undo snapshot once the drag ends.
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoi::mouseReleaseEvent(event);
        emit geometryChanged();
        emit geometryFinished();
    }

private:
    QString m_id;     ///< Stable identifier carried over from the source VisionRoi.
    QString m_label;  ///< Display label carried over from the source VisionRoi.
};

/// Editable rotated rectangular ROI item: wraps ItemRoiRotated (the resize/rotate
/// handle behavior) and adds the id/label needed to round-trip to/from VisionRoi.
class VisionRotatedRoiItem : public RoiItemBase, public ItemRoiRotated {
public:
    /// Constructs the item centered on `roi.center` with the size from `roi.size`
    /// and initial rotation `roi.angleDeg`, parented under `parentItem` (the image
    /// item) and sharing `ignoreFlag` with the canvas to suppress feedback while
    /// scene interaction is locked.
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

    /// Returns the current geometry as a VisionRoi (rotated shape), reading
    /// center/size/rotation/visibility/selection back from the underlying QGraphicsItem.
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

    /// Returns this item as a QGraphicsItem.
    QGraphicsItem *graphicsItem() override
    {
        return this;
    }

protected:
    /// Forwards to ItemRoiRotated::itemChange and additionally emits
    /// selectionChanged() when the item's selection state flips.
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemSelectedHasChanged) {
            emit selectionChanged();
        }
        return ItemRoiRotated::itemChange(change, value);
    }

    /// Forwards to ItemRoiRotated::mouseMoveEvent and emits geometryChanged() so
    /// the canvas can react live while the ROI is being dragged/resized/rotated.
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoiRotated::mouseMoveEvent(event);
        emit geometryChanged();
    }

    /// Forwards to ItemRoiRotated::mouseReleaseEvent and emits geometryChanged()
    /// followed by geometryFinished() so the canvas can push an undo snapshot once
    /// the drag ends.
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        ItemRoiRotated::mouseReleaseEvent(event);
        emit geometryChanged();
        emit geometryFinished();
    }

private:
    QString m_id;     ///< Stable identifier carried over from the source VisionRoi.
    QString m_label;  ///< Display label carried over from the source VisionRoi.
};

} // namespace vision_canvas_detail

/// Constructs the canvas: creates and installs the QGraphicsScene, configures the
/// mouse-tracking/focus/viewport-update settings needed for hover highlighting and
/// smooth panning, and re-themes the background plus auxiliary/overlay items
/// whenever ThemeManager reports a theme change.
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

/// Converts `image` to a QPixmap (see vision::pixmapFromMat) and displays it; see
/// the QPixmap overload for fit/zoom behavior.
void VisionCanvas::setImage(const cv::Mat &image)
{
    setImage(vision::pixmapFromMat(image));
}

/// Displays `pixmap` as the canvas background image, creating the pixmap item on
/// first use and otherwise updating it in place. Rebuilds the editable/auxiliary/
/// overlay items against the new image, and re-fits the view only when the
/// pixmap's size differs from the last one shown (so same-size live-feed frames
/// keep the user's current zoom/pan). A null pixmap clears the canvas instead.
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

/// Removes the displayed image and all editable/auxiliary/overlay items, resets
/// the undo history to a single empty snapshot, clears the result-object
/// selection/hover state, and forces the next setImage() call to re-fit the view.
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

/// Switches between edit mode (ROI drawing/dragging) and read-only mode (result
/// object hover/selection); updates the cursor to match.
void VisionCanvas::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    syncCursor();
}

/// Sets the active drawing/interaction tool, updates the cursor, and emits
/// toolModeChanged() if the mode actually changed.
void VisionCanvas::setToolMode(VisionToolPalette::ToolMode mode)
{
    const bool changed = (m_toolMode != mode);
    m_toolMode = mode;
    syncCursor();
    if (changed) {
        emit toolModeChanged(m_toolMode);
    }
}

/// Replaces the set of user-editable ROIs and resets the undo/redo history to a
/// single snapshot of the new state (this is the entry point for loading ROIs from
/// outside the widget, as opposed to interactive edits which push onto the
/// existing history).
void VisionCanvas::setEditableRois(const QVector<VisionRoi> &rois)
{
    rebuildEditableRois(rois);
    m_history.clear();
    m_history.append(currentSnapshot());
    m_historyIndex = 0;
    emit undoAvailabilityChanged(false, false);
    emitRoisChanged();
}

/// Reads the current editable ROIs back from the live graphics items, normalizing
/// each one (see vision::normalizedRoi).
/// @return the current editable ROIs in item order.
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

/// Replaces the read-only auxiliary ROI overlays (e.g. reference regions) drawn
/// alongside the editable ROIs.
void VisionCanvas::setAuxiliaryRois(const QVector<VisionRoi> &rois)
{
    m_auxiliaryRois = rois;
    rebuildAuxiliaryRois();
}

/// Sets the vision-analysis result overlay data (accepted/rejected objects, ROI
/// overlays, runtime signal values) and rebuilds the overlay graphics items.
/// @note Does not change which overlay categories are visible; see the note below
/// on setOverlayVisibility() ownership.
void VisionCanvas::setResultOverlay(const VisionResultOverlay &overlay)
{
    // Overlay visibility is sticky UI state owned by setOverlayVisibility().
    // A result overlay carries data only and must not silently reset which
    // overlay items the user chose to show.
    m_overlay = overlay;
    rebuildOverlayItems();
}

/// Sets which overlay categories (rejected candidates, picking boxes, fault/sent
/// markers, runtime signal values, etc.) are drawn, and rebuilds the overlay items
/// to match.
void VisionCanvas::setOverlayVisibility(const VisionOverlayVisibility &visibility)
{
    m_overlayVisibility = visibility;
    rebuildOverlayItems();
}

/// Selects the result object with the given index (clearing any hover state once a
/// real selection is made), rebuilds the overlay to highlight/mute accordingly, and
/// emits resultObjectSelectionChanged(). No-op if `objectIndex` is already selected.
/// @param objectIndex the VisionResultObject::index to select, or a non-positive
/// value to clear the selection
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

/// Clears the current result-object selection (equivalent to setSelectedResultObject(-1)).
void VisionCanvas::clearSelectedResultObject()
{
    setSelectedResultObject(-1);
}

/// Hit-tests `scenePoint` against the result overlay's polygons to find which
/// result object it falls in. Accepted objects are checked first (preferring the
/// currently-selected object if the point still falls inside it, so overlapping
/// polygons don't fight the user's selection), then rejected candidates if the
/// "show rejected" visibility flag is on. Later-drawn (later-index) objects win
/// ties within the same list.
/// @return the matched object's index, or -1 if no polygon contains the point.
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

/// Updates which result object is hover-highlighted, refreshing the cursor (unless
/// currently panning) and rebuilding the overlay to show the hover state. No-op if
/// `objectIndex` is already hovered.
void VisionCanvas::setHoveredResultObject(int objectIndex)
{
    if (m_hoveredResultObject == objectIndex) return;
    m_hoveredResultObject = objectIndex;

    if (!m_panning) {
        syncCursor();
    }

    rebuildOverlayItems();
}

/// Finds the first editable ROI item currently selected in the graphics scene.
/// @return the selected ROI, or a default-constructed (empty-id) VisionRoi if none is selected.
VisionRoi VisionCanvas::selectedRoi() const
{
    for (vision_canvas_detail::RoiItemBase *item : m_editableItems) {
        if (item && item->roi().selected) {
            return item->roi();
        }
    }
    return {};
}

/// Returns whether an editable ROI is currently selected (based on selectedRoi() having a non-empty id).
bool VisionCanvas::hasSelectedRoi() const
{
    return !selectedRoi().id.isEmpty();
}

/// Returns the pixel size of the currently displayed image, or an empty QSize if no image is set.
QSize VisionCanvas::imageSize() const
{
    if (!m_imageItem) return {};
    return m_imageItem->pixmap().size();
}

/// Applies an externally-edited geometry/state for the editable ROI matching
/// `roi.id`: clamps it to the image bounds, rebuilds the editable items from the
/// updated list, emits the roisChanged/selectedRoiChanged signals, and pushes an
/// undo snapshot if the result actually changed. No-op if no ROI with that id exists.
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

/// Removes the currently-selected editable ROI (if any) and pushes an undo
/// snapshot. Does nothing in read-only mode or when no ROI is selected.
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

/// Fits the view's zoom/pan so the full image is visible, preserving aspect ratio. No-op if no image is set.
void VisionCanvas::fitImageToView()
{
    if (m_imageItem) {
        fitInView(m_imageItem->boundingRect(), Qt::KeepAspectRatio);
    }
}

/// Steps the editable-ROI history one entry back, if not already at the oldest entry.
void VisionCanvas::undo()
{
    if (m_historyIndex <= 0) return;
    applyUndoSnapshot(m_historyIndex - 1);
}

/// Steps the editable-ROI history one entry forward, if not already at the newest entry.
void VisionCanvas::redo()
{
    if (m_historyIndex < 0 || m_historyIndex + 1 >= m_history.size()) return;
    applyUndoSnapshot(m_historyIndex + 1);
}

/// Routes a mouse press to panning (Ctrl+click or Pan tool), result-object
/// selection (read-only mode, left click), ROI drawing (rect/rotated-rect tool),
/// or the base QGraphicsView handling, in that priority order.
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

/// Continues an in-progress pan or ROI draw, or (in read-only mode) updates the
/// hovered result object; otherwise forwards to the base QGraphicsView handling.
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

/// Ends an in-progress pan or ROI draw (updating the hover state after a pan), or
/// forwards to the base QGraphicsView handling and refreshes the ROI selection
/// from the scene.
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

/// Middle-button double-click fits the image to the view; other buttons fall
/// through to the base QGraphicsView handling.
void VisionCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        fitImageToView();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

/// Ctrl+wheel zooms the view in/out by a fixed 15% step per notch; plain wheel
/// falls through to the base QGraphicsView handling (scrolling).
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

/// Clears the hovered result object when the mouse leaves the viewport in
/// read-only mode (unless currently panning).
void VisionCanvas::leaveEvent(QEvent *event)
{
    if (m_readOnly && !m_panning) {
        setHoveredResultObject(-1);
    }
    QGraphicsView::leaveEvent(event);
}

/// Handles canvas shortcuts: Escape cancels an in-progress pan, exits the Pan tool
/// back to select/move, or clears the result-object selection (in that priority
/// order); Delete removes the selected ROI. Falls through to the base
/// QGraphicsView handling otherwise.
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

/// Discards and rebuilds the auxiliary-ROI overlay graphics items (dashed polygon
/// plus label chip per visible, valid auxiliary ROI) from m_auxiliaryRois. No-op if
/// no image is currently displayed.
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

/// Discards and rebuilds every result-overlay graphics item (accepted/rejected
/// object outlines, center markers, orientation/corner-locator arrows, picking-box
/// polygons, label chips, OUT/FAULT markers, ROI overlays, and the runtime-signal
/// readout chip) from m_overlay and the current selection/hover state and
/// visibility flags. No-op if no image is currently displayed.
void VisionCanvas::rebuildOverlayItems()
{
    for (QGraphicsItem *item : m_overlayItems) {
        delete item;
    }
    m_overlayItems.clear();

    if (!m_imageItem) return;

    /// Draws one result-object list (accepted or rejected candidates) into the
    /// overlay: outline polygon, center dot, orientation arrows, picking boxes,
    /// corner locators, label chip, and OUT/FAULT markers, styled by
    /// selected/hovered/muted state.
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

/// Discards and rebuilds the editable ROI graphics items from `rois`: each ROI is
/// clamped to the image bounds, invalid ROIs are skipped, and the concrete item
/// type (rotated vs. axis-aligned) is chosen from VisionRoi::shape. No-op if no
/// image is currently displayed.
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

/// Adds `item` to the tracked editable items and wires its geometry/selection
/// signals to the corresponding onEditableItem* slots. No-op if `item` is null.
void VisionCanvas::registerEditableItem(vision_canvas_detail::RoiItemBase *item)
{
    if (!item) return;
    m_editableItems.append(item);
    connect(item, SIGNAL(geometryChanged()), this, SLOT(onEditableItemGeometryChanged()));
    connect(item, SIGNAL(geometryFinished()), this, SLOT(onEditableItemGeometryFinished()));
    connect(item, SIGNAL(selectionChanged()), this, SLOT(onEditableItemSelectionChanged()));
}

/// Re-reads the selected ROI from the scene and emits selectedRoiChanged() (used
/// after a mouse release so external listeners see the graphics scene's actual
/// selection state).
void VisionCanvas::updateSelectionFromScene()
{
    VisionRoi roi = selectedRoi();
    emit selectedRoiChanged(roi);
}

/// Emits roisChanged() with the current editable ROI list and selectedRoiChanged()
/// with the current selection.
void VisionCanvas::emitRoisChanged()
{
    emit roisChanged(editableRois());
    emit selectedRoiChanged(selectedRoi());
}

/// Appends the current editable-ROI state to the undo history, truncating any
/// redo entries beyond the current position, unless history recording is locked
/// (see applyUndoSnapshot) or the state is identical to the current history entry.
/// Emits undoAvailabilityChanged() when a snapshot is pushed.
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

/// Rebuilds the editable ROIs from history entry `index` and moves the history
/// cursor there, locking pushUndoSnapshotIfChanged() out for the duration so the
/// rebuild does not itself get recorded as a new undo step. Emits
/// undoAvailabilityChanged() and the ROI-changed signals. No-op if `index` is out
/// of range.
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

/// Returns the current editable ROIs with selection state cleared, so undo/redo
/// history entries are compared and restored independent of which ROI happened to
/// be selected.
QVector<VisionRoi> VisionCanvas::currentSnapshot() const
{
    QVector<VisionRoi> snapshot = editableRois();
    for (VisionRoi &roi : snapshot) {
        roi.selected = false;
    }
    return snapshot;
}

/// Returns the displayed image's bounding rect in scene coordinates, or an empty
/// rect if no image is set.
QRectF VisionCanvas::imageBounds() const
{
    return m_imageItem ? m_imageItem->boundingRect() : QRectF();
}

/// Clamps `scenePoint` to the displayed image's bounds, so ROI drawing can't start
/// or extend outside the image. Returns `scenePoint` unchanged if there is no image.
QPointF VisionCanvas::clampScenePointToImage(const QPointF &scenePoint) const
{
    const QRectF bounds = imageBounds();
    if (bounds.isNull()) return scenePoint;
    return QPointF(qBound(bounds.left(), scenePoint.x(), bounds.right()),
                   qBound(bounds.top(), scenePoint.y(), bounds.bottom()));
}

/// Sets the viewport cursor to match current interaction state, in priority order:
/// closed hand while panning, open hand in the Pan tool, crosshair in a draw tool,
/// pointing hand when hovering a result object in read-only mode, otherwise the
/// default arrow.
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

/// Starts an interactive pan from viewport position `pos` and updates the cursor.
void VisionCanvas::beginPan(const QPoint &pos)
{
    m_panning = true;
    m_lastPanPoint = pos;
    syncCursor();
}

/// Scrolls the view by the delta between `pos` and the last recorded pan point.
void VisionCanvas::updatePan(const QPoint &pos)
{
    const QPoint delta = pos - m_lastPanPoint;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    m_lastPanPoint = pos;
}

/// Ends the current interactive pan and updates the cursor.
void VisionCanvas::endPan()
{
    m_panning = false;
    syncCursor();
}

/// Starts drawing a new ROI rectangle anchored at `imagePoint`, creating (or
/// resetting) the zero-size dashed preview rect item.
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

/// Resizes the in-progress draw preview rect to span from the draw start point to `imagePoint`.
void VisionCanvas::updateDraw(const QPointF &imagePoint)
{
    if (!m_drawRectItem) return;
    m_drawRectItem->setRect(QRectF(m_drawStart, imagePoint).normalized());
}

/// Finishes drawing: removes the preview rect and, if the drawn rectangle is at
/// least 4x4 pixels, appends a new VisionRoi (axis-aligned or rotated, per the
/// active tool mode) with an auto-generated id/label, then rebuilds the editable
/// ROIs, emits the ROI-changed signals, and pushes an undo snapshot. Rectangles
/// smaller than 4x4 are discarded as accidental clicks.
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

/// Slot: forwards a live geometry change from any editable ROI item to emitRoisChanged().
void VisionCanvas::onEditableItemGeometryChanged()
{
    emitRoisChanged();
}

/// Slot: called when an editable ROI item's interactive geometry change
/// completes; re-emits the ROI-changed signals and pushes an undo snapshot.
void VisionCanvas::onEditableItemGeometryFinished()
{
    emitRoisChanged();
    pushUndoSnapshotIfChanged();
}

/// Slot: called when any editable ROI item's selection state changes; re-emits
/// selectedRoiChanged() with the current selection.
void VisionCanvas::onEditableItemSelectionChanged()
{
    emit selectedRoiChanged(selectedRoi());
}

#include "vision_canvas.moc"
