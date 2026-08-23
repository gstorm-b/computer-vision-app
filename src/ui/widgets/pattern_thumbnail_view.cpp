#include "ui/widgets/pattern_thumbnail_view.h"
#include "ui/widgets/pick_overlay_painter.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>

namespace {
/// Zoom factor applied per wheel notch.
constexpr double kZoomStep = 1.15;
/// Zoom bounds, as a multiple of the fitted scale's own units (view transform m11()).
constexpr double kMinZoom  = 0.05;
constexpr double kMaxZoom  = 40.0;
} // namespace

PatternThumbnailView::PatternThumbnailView(QWidget *parent)
    : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_pixmapItem = m_scene->addPixmap(QPixmap{});
    // No ItemIsMovable / ItemIsSelectable / ItemIsFocusable: the item cannot be picked up,
    // selected or focused, which is the read-only guarantee this view exists to make.
    m_pixmapItem->setFlags({});
    m_pixmapItem->setAcceptedMouseButtons(Qt::NoButton);

    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);      // pan; also swallows item interaction
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // The overlay is painted with the world transform reset, so it lives outside the
    // scene's coordinate system. A partial update would clip it against an exposed rect
    // computed in scene space and leave fragments behind.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void PatternThumbnailView::setPattern(const QPixmap &image, const QPointF &pick,
                                      double pickAngleDeg,
                                      double boxW, double boxH, double boxDist,
                                      double boxAngleDeg, bool showBoxes) {
    m_pick       = pick;
    m_pickAngle  = pickAngleDeg;
    m_boxW       = boxW;
    m_boxH       = boxH;
    m_boxDist    = boxDist;
    m_boxAngle   = boxAngleDeg;
    // A jaw pair with no width or height is unset rather than degenerate; drawing it would
    // put two labelled dots on the image and imply geometry the pattern does not have.
    m_showBoxes  = showBoxes && boxW > 0.0 && boxH > 0.0;

    m_pixmapItem->setPixmap(image);
    m_scene->setSceneRect(image.isNull() ? QRectF() : QRectF(image.rect()));

    m_autoFit = true;
    resetView();
}

void PatternThumbnailView::clear() {
    m_pixmapItem->setPixmap(QPixmap{});
    m_scene->setSceneRect(QRectF());
    m_showBoxes = false;
    m_autoFit   = true;
    resetTransform();
    viewport()->update();
}

void PatternThumbnailView::resetView() {
    if (m_pixmapItem->pixmap().isNull()) return;
    resetTransform();
    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    m_autoFit = true;
    viewport()->update();
}

void PatternThumbnailView::drawForeground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (m_pixmapItem->pixmap().isNull()) return;

    painter->save();
    // Drop the view transform so the overlay is drawn in viewport pixels. This is what
    // keeps the gizmo a constant on-screen size at any zoom, exactly as on the wizard
    // canvas — leaving the transform in place would scale the arrows with the image.
    painter->resetTransform();

    const QPointF pw    = mapFromScene(m_pick);
    const double  scale = transform().m11();

    // The jaw boxes are image geometry, so they are pre-scaled and DO grow with the zoom.
    if (m_showBoxes) {
        pick_overlay::drawJawPair(*painter, pw,
                                  m_boxW * scale, m_boxH * scale,
                                  m_boxDist * scale, m_boxAngle);
    }
    // Gizmo before the centre marker: both axes run through the pick point and the marker
    // is what covers the crossing.
    pick_overlay::drawOrientationGizmo(*painter, pw, m_pickAngle,
                                       /*showKnob=*/false, /*knobHeld=*/false);
    pick_overlay::drawPickCrosshair(*painter, pw);

    painter->restore();
}

void PatternThumbnailView::wheelEvent(QWheelEvent *event) {
    if (m_pixmapItem->pixmap().isNull()) return;

    const double factor  = (event->angleDelta().y() > 0) ? kZoomStep : 1.0 / kZoomStep;
    const double current = transform().m11();
    const double target  = current * factor;
    if (target < kMinZoom || target > kMaxZoom) return;

    m_autoFit = false;   // the user has chosen a framing; stop refitting on resize
    scale(factor, factor);
    event->accept();
}

void PatternThumbnailView::mouseDoubleClickEvent(QMouseEvent *event) {
    resetView();
    event->accept();
}

void PatternThumbnailView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    if (m_autoFit && !m_pixmapItem->pixmap().isNull())
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}
