#include "pattern_canvas.h"
#include "pattern_theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QtMath>
#include <opencv2/imgproc.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    // Handle hit-test slack and visual size — always in WIDGET pixels so
    // handles stay easy to grab regardless of zoom level.
    constexpr int    kHandleHitSlack = 8;
    constexpr double kHandleVisSize  = 11.0;   // square handle, in widget px

    // Rotation handle stand-off distance (image pixels, from box A centre,
    // perpendicular to the connector direction).
    constexpr double kRotateHandleStandoff = 28.0;

    // Zoom limits.
    constexpr double kMinZoom = 0.05;
    constexpr double kMaxZoom = 40.0;
    constexpr double kZoomStep = 1.15;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

AddPatternImageCanvas::AddPatternImageCanvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 280);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// ── Image setters ───────────────────────────────────────────────────────────

void AddPatternImageCanvas::setImage(const QPixmap &pix) {
    m_pix = pix;
    resetView();
    update();
}

void AddPatternImageCanvas::setImage(const cv::Mat &mat) {
    if (mat.empty()) { m_pix = QPixmap(); resetView(); update(); return; }

    QImage img;
    if (mat.type() == CV_8UC1) {
        img = QImage(mat.data, mat.cols, mat.rows,
                     static_cast<int>(mat.step),
                     QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        img = QImage(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step),
                     QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        img = QImage(rgba.data, rgba.cols, rgba.rows,
                     static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888).copy();
    }
    m_pix = QPixmap::fromImage(img);
    resetView();
    update();
}

void AddPatternImageCanvas::clearImage() { m_pix = QPixmap(); update(); }

void AddPatternImageCanvas::setMode(Mode m)             { m_mode = m; update(); }
void AddPatternImageCanvas::setCrop(const QRect &r)     { m_crop = r; update(); }
void AddPatternImageCanvas::setPick(const QPoint &p)    {
    m_pick = p;
    m_pickCurrentPoint = p;
    if (!m_crop.isNull() && !m_crop.isEmpty()) {
        m_pick = p + m_crop.topLeft();
    }
    update();
}

void AddPatternImageCanvas::setBoxConfig(double w, double h, double d, double a) {
    m_boxW = w; m_boxH = h; m_boxDist = d; m_boxAngle = a;
    update();
}
void AddPatternImageCanvas::setLocked(bool locked)      { m_locked = locked; update(); }

// ─────────────────────────────────────────────────────────────────────────────
//  View transform
// ─────────────────────────────────────────────────────────────────────────────

double AddPatternImageCanvas::fitZoom() const {
    if (m_pix.isNull()) return 1.0;
    const double zx = double(width())  / double(m_pix.width());
    const double zy = double(height()) / double(m_pix.height());
    return qBound(kMinZoom, qMin(zx, zy), kMaxZoom);
}

QRectF AddPatternImageCanvas::imageRectInWidget() const {
    return QRectF(m_offset.x(), m_offset.y(),
                  m_pix.width() * m_zoom, m_pix.height() * m_zoom);
}

void AddPatternImageCanvas::resetView() {
    if (m_pix.isNull()) {
        m_zoom = 1.0;
        m_offset = QPointF(0, 0);
        return;
    }
    m_zoom = fitZoom();
    m_offset = QPointF((width()  - m_pix.width()  * m_zoom) / 2.0,
                       (height() - m_pix.height() * m_zoom) / 2.0);
}

void AddPatternImageCanvas::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    // Preserve the user's view (zoom + pan) on widget resize.  setImage()
    // and middle-button double-click are the only paths that refit.
}

// ─────────────────────────────────────────────────────────────────────────────
//  Painting
// ─────────────────────────────────────────────────────────────────────────────

void AddPatternImageCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    // Antialiasing for overlay shapes (crop/box outlines, handles).
    // SmoothPixmapTransform is OFF so the underlying image renders with
    // nearest-neighbour sampling — at high zoom levels users see real pixel
    // boundaries, which matches the pattern-inspection use case.
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // ── Background ──────────────────────────────────────────────────────
    p.fillRect(rect(), QColor("#181818"));

    if (m_pix.isNull()) {
        // Placeholder grid + "no image" text
        p.setPen(QPen(QColor(ptn::BD), 0.5));
        for (int x = 0; x < width(); x += 20)  p.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += 20) p.drawLine(0, y, width(), y);

        p.setPen(QColor(ptn::TXT3));
        p.setFont(QFont("Segoe UI", 10));
        p.drawText(rect(), Qt::AlignCenter, tr("No image"));
        return;
    }

    // ── Draw image through view transform ───────────────────────────────
    const QRectF imgWidget = imageRectInWidget();
    p.drawPixmap(imgWidget, m_pix, QRectF(QPointF(0, 0), QSizeF(m_pix.size())));

    // ── Mode-specific overlay ───────────────────────────────────────────
    switch (m_mode) {
    case None: break;

    case Crop: {
        if (m_crop.isNull() || m_crop.isEmpty()) break;

        const QRectF cropW(imageToWidget(m_crop.topLeft()),
                           QSizeF(m_crop.width() * m_zoom, m_crop.height() * m_zoom));

        // Dim scrim outside the crop, inside the image rect
        QPainterPath outer; outer.addRect(imgWidget);
        QPainterPath inner; inner.addRect(cropW);
        p.setBrush(QColor(0, 0, 0, 170));
        p.setPen(Qt::NoPen);
        p.drawPath(outer.subtracted(inner));

        // Crop rect outline
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(ptn::ACC), 1.5));
        p.drawRect(cropW);

        // Rule-of-thirds (subdued)
        QColor tc(ptn::ACC); tc.setAlpha(85);
        p.setPen(QPen(tc, 1));
        const double w3 = cropW.width()  / 3.0;
        const double h3 = cropW.height() / 3.0;
        p.drawLine(QPointF(cropW.left()  + w3,     cropW.top()),
                   QPointF(cropW.left()  + w3,     cropW.bottom()));
        p.drawLine(QPointF(cropW.left()  + 2 * w3, cropW.top()),
                   QPointF(cropW.left()  + 2 * w3, cropW.bottom()));
        p.drawLine(QPointF(cropW.left(),  cropW.top()    + h3),
                   QPointF(cropW.right(), cropW.top()    + h3));
        p.drawLine(QPointF(cropW.left(),  cropW.top()    + 2 * h3),
                   QPointF(cropW.right(), cropW.top()    + 2 * h3));

        // Corner handles (widget-pixel sized — readable at any zoom)
        const QPointF corners[4] = {
            cropW.topLeft(), cropW.topRight(),
            cropW.bottomLeft(), cropW.bottomRight()
        };
        p.setBrush(QColor(ptn::ACC));
        p.setPen(QPen(QColor("white"), 1.5));
        for (const QPointF &c : corners)
            p.drawRect(QRectF(c.x() - kHandleVisSize / 2,
                              c.y() - kHandleVisSize / 2,
                              kHandleVisSize, kHandleVisSize));
        break;
    }

    case Pick: {
        // Read-only crop overlay (so user knows the crop region)
        if (!m_crop.isNull() && !m_crop.isEmpty()) {
            const QRectF cropW(imageToWidget(m_crop.topLeft()),
                               QSizeF(m_crop.width() * m_zoom,
                                      m_crop.height() * m_zoom));
            QColor scrim(0, 0, 0, 110);
            QPainterPath outer; outer.addRect(imgWidget);
            QPainterPath inner; inner.addRect(cropW);
            p.setBrush(scrim); p.setPen(Qt::NoPen);
            p.drawPath(outer.subtracted(inner));

            // Crop outline (dashed, subdued — signals read-only)
            QColor lock(ptn::ACC); lock.setAlpha(180);
            QPen pen(lock, 1.2, Qt::DashLine);
            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRect(cropW);
        }

        // Pick crosshair (widget coords)
        const QPointF pw = imageToWidget(m_pick);
        QColor warn(ptn::WARN);
        QColor warn2(ptn::WARN); warn2.setAlpha(204);
        p.setPen(QPen(warn2, 1));
        p.drawLine(QPointF(0, pw.y()),         QPointF(width(),  pw.y()));
        p.drawLine(QPointF(pw.x(), 0),         QPointF(pw.x(), height()));

        QColor ringFill(ptn::WARN); ringFill.setAlpha(34);
        p.setBrush(ringFill);
        p.setPen(QPen(warn, 1.5));
        p.drawEllipse(pw, 9, 9);

        // Label — shows crop-relative coords when a crop is set, otherwise
        // raw image coords.  Matches the spec the host returns to callers.
        QPoint displayPick = m_pick;
        if (!m_crop.isNull() && !m_crop.isEmpty())
            displayPick -= m_crop.topLeft();
        p.setPen(warn);
        p.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
        p.drawText(QPointF(pw.x() + 12, pw.y() - 12),
                   QString("PICK (%1, %2)").arg(displayPick.x()).arg(displayPick.y()));
        break;
    }

    case Box:
    case Finish: {
        const QPointF pw = imageToWidget(m_pick);
        const QColor jawColor(ptn::OK);
        QColor jawFill(ptn::OK); jawFill.setAlpha(34);

        auto drawBox = [&](double angleDeg, const QString &label, bool interactive) {
            const double r = qDegreesToRadians(angleDeg);
            const QPointF cImg(m_pick.x() + std::cos(r) * m_boxDist,
                               m_pick.y() + std::sin(r) * m_boxDist);
            const QPointF cW = imageToWidget(cImg);

            // Connector
            p.setPen(QPen(jawColor, 1, Qt::DashLine));
            p.drawLine(pw, cW);

            // Box body — rotated by angleDeg around its centre.  We rotate in
            // widget space, scaling W/H by m_zoom so the box stays "in image
            // pixels" without clipping at the widget edges (boxes are allowed
            // to spill outside the image).
            p.save();
            p.translate(cW);
            p.rotate(angleDeg);
            const double ww = m_boxW * m_zoom;
            const double hh = m_boxH * m_zoom;
            p.setBrush(jawFill);
            p.setPen(QPen(jawColor, 1.5));
            p.drawRect(QRectF(-ww / 2, -hh / 2, ww, hh));
            p.setBrush(jawColor); p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(0, 0), 2.5, 2.5);

            if (interactive && m_mode == Box) {
                // Corner handles (widget-pixel size)
                p.setBrush(QColor(ptn::ACC));
                p.setPen(QPen(QColor("white"), 1.2));
                const QPointF corners[4] = {
                    QPointF(-ww / 2, -hh / 2),
                    QPointF( ww / 2, -hh / 2),
                    QPointF(-ww / 2,  hh / 2),
                    QPointF( ww / 2,  hh / 2),
                };
                for (const QPointF &c : corners)
                    p.drawRect(QRectF(c.x() - kHandleVisSize / 2,
                                      c.y() - kHandleVisSize / 2,
                                      kHandleVisSize, kHandleVisSize));
            }
            p.restore();

            // Label
            p.setPen(jawColor);
            p.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
            p.drawText(QPointF(cW.x() - 4, cW.y() - hh / 2 - 4), label);
        };
        drawBox(m_boxAngle,       "A", /*interactive=*/true);
        drawBox(m_boxAngle + 180, "B", /*interactive=*/false);

        // Rotation handle — small filled disc offset perpendicular to the
        // connector from box A centre, drawn in widget space.
        if (m_mode == Box) {
            const QPointF rotImg = boxRotationHandle();
            const QPointF rotW = imageToWidget(rotImg);
            const QPointF aW   = imageToWidget(boxACentre());
            QColor rot(ptn::WARN);
            p.setPen(QPen(rot, 1.2, Qt::DotLine));
            p.drawLine(aW, rotW);
            p.setBrush(rot);
            p.setPen(QPen(QColor("white"), 1.2));
            p.drawEllipse(rotW, kHandleVisSize / 2, kHandleVisSize / 2);
        }

        // Pick crosshair on top
        p.setPen(QPen(QColor("white"), 1));
        p.drawLine(QPointF(pw.x() - 12, pw.y()), QPointF(pw.x() + 12, pw.y()));
        p.drawLine(QPointF(pw.x(), pw.y() - 12), QPointF(pw.x(), pw.y() + 12));
        p.setBrush(QColor(ptn::WARN)); p.setPen(Qt::NoPen);
        p.drawEllipse(pw, 3, 3);
        break;
    }
    }

    // ── Locked scrim ────────────────────────────────────────────────────
    if (m_locked) {
        QColor scrim(ptn::LOCK); scrim.setAlpha(80);
        p.fillRect(rect(), scrim);

        const int bw = 86, bh = 22;
        QRect badge(width() - bw - 12, 12, bw, bh);
        p.setBrush(QColor(ptn::LOCK));
        p.setPen(QPen(QColor("white"), 1));
        p.drawRoundedRect(badge, 4, 4);
        p.setPen(QColor("white"));
        p.setFont(QFont("Segoe UI", 8, QFont::Bold));
        p.drawText(badge, Qt::AlignCenter, "🔒  LOCKED");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Box geometry helpers
// ─────────────────────────────────────────────────────────────────────────────

QPointF AddPatternImageCanvas::boxACentre() const {
    const double r = qDegreesToRadians(m_boxAngle);
    return QPointF(m_pick.x() + std::cos(r) * m_boxDist,
                   m_pick.y() + std::sin(r) * m_boxDist);
}

QPointF AddPatternImageCanvas::boxRotationHandle() const {
    // Offset perpendicular to the connector direction.  Perpendicular vector
    // for angle θ is (cos(θ+90°), sin(θ+90°)) = (-sin θ, cos θ).
    const double r = qDegreesToRadians(m_boxAngle);
    const QPointF c = boxACentre();
    return QPointF(c.x() + (-std::sin(r)) * (m_boxH / 2 + kRotateHandleStandoff),
                   c.y() + ( std::cos(r)) * (m_boxH / 2 + kRotateHandleStandoff));
}

void AddPatternImageCanvas::boxACorners(QPointF out[4]) const {
    const double r = qDegreesToRadians(m_boxAngle);
    const double co = std::cos(r), si = std::sin(r);
    const QPointF c = boxACentre();
    const double w2 = m_boxW / 2, h2 = m_boxH / 2;
    // local (lx, ly) → image: (c + R * (lx, ly))
    const double lx[4] = { -w2,  w2, -w2,  w2 };
    const double ly[4] = { -h2, -h2,  h2,  h2 };
    for (int i = 0; i < 4; ++i) {
        out[i] = QPointF(c.x() + co * lx[i] - si * ly[i],
                         c.y() + si * lx[i] + co * ly[i]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hit testing
// ─────────────────────────────────────────────────────────────────────────────

AddPatternImageCanvas::CropHandle
AddPatternImageCanvas::hitCropHandle(const QPoint &widgetPos) const {
    if (m_crop.isNull() || m_crop.isEmpty()) return CH_None;

    // Test corners first (widget-pixel slack so they're always reachable).
    const QPointF cs[4] = {
        imageToWidget(m_crop.topLeft()),
        imageToWidget(m_crop.topRight()),
        imageToWidget(m_crop.bottomLeft()),
        imageToWidget(m_crop.bottomRight()),
    };
    for (int i = 0; i < 4; ++i) {
        if (qAbs(widgetPos.x() - cs[i].x()) <= kHandleHitSlack &&
            qAbs(widgetPos.y() - cs[i].y()) <= kHandleHitSlack)
            return CropHandle(i);
    }
    // Body — cursor inside the crop rect (widget coords)
    const QRectF cw(cs[0], cs[3]);
    if (cw.normalized().contains(widgetPos))
        return CH_Body;
    return CH_None;
}

AddPatternImageCanvas::BoxHandle
AddPatternImageCanvas::hitBoxHandle(const QPoint &widgetPos) const {
    // Pick crosshair — highest priority so the picking centre can always be
    // grabbed and dragged (both jaws follow it rigidly).
    {
        const QPointF pw = imageToWidget(m_pick);
        if (QLineF(pw, widgetPos).length() <= kHandleHitSlack + 2)
            return BH_Pick;
    }
    // Rotation handle — widget-pixel proximity to the centre of the knob.
    {
        const QPointF rotW = imageToWidget(boxRotationHandle());
        if (QLineF(rotW, widgetPos).length() <= kHandleHitSlack + 2)
            return BH_Rotate;
    }
    // Corner handles
    QPointF corners[4];
    boxACorners(corners);
    for (int i = 0; i < 4; ++i) {
        const QPointF cw = imageToWidget(corners[i]);
        if (qAbs(widgetPos.x() - cw.x()) <= kHandleHitSlack &&
            qAbs(widgetPos.y() - cw.y()) <= kHandleHitSlack)
            return BoxHandle(i);
    }
    // Body — transform cursor to box-local coords and test against [-W/2, W/2] × [-H/2, H/2]
    const QPointF imgPt = widgetToImage(widgetPos);
    const QPointF c     = boxACentre();
    const double r = qDegreesToRadians(m_boxAngle);
    const double co = std::cos(-r), si = std::sin(-r);
    const double dx = imgPt.x() - c.x();
    const double dy = imgPt.y() - c.y();
    const double lx = co * dx - si * dy;
    const double ly = si * dx + co * dy;
    if (qAbs(lx) <= m_boxW / 2 && qAbs(ly) <= m_boxH / 2)
        return BH_Body;
    return BH_None;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse interaction
// ─────────────────────────────────────────────────────────────────────────────

void AddPatternImageCanvas::mousePressEvent(QMouseEvent *e) {
    if (m_pix.isNull()) return;

    // Middle button — pan (works in every mode, even when locked)
    if (e->button() == Qt::MiddleButton) {
        m_panning         = true;
        m_panStart        = e->pos();
        m_panStartOffset  = m_offset;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (m_locked) return;

    if (m_mode == Crop && e->button() == Qt::LeftButton) {
        const CropHandle h = hitCropHandle(e->pos());
        if (h != CH_None) {
            m_cropDragging      = true;
            m_cropHandle        = h;
            m_cropDragStartImg  = widgetToImage(e->pos());
            m_cropDragStartRect = m_crop;
        }
    } else if (m_mode == Pick && e->button() == Qt::LeftButton) {
        // Pick respects the image bounds (and the crop if one is set).
        QPointF imgPt = widgetToImage(e->pos());
        if (!m_crop.isNull() && !m_crop.isEmpty()) {
            imgPt.setX(qBound<double>(m_crop.left(),  imgPt.x(), m_crop.right()));
            imgPt.setY(qBound<double>(m_crop.top(),   imgPt.y(), m_crop.bottom()));
        } else if (!m_pix.isNull()) {
            imgPt.setX(qBound<double>(0, imgPt.x(), m_pix.width()  - 1));
            imgPt.setY(qBound<double>(0, imgPt.y(), m_pix.height() - 1));
        }
        m_pick = imgPt.toPoint();
        if (!m_crop.isNull() && !m_crop.isEmpty()) {
            m_pickCurrentPoint = m_pick - m_crop.topLeft();
        }
        update();
        emit pickChanged(m_pick, m_pickCurrentPoint);
    } else if (m_mode == Box && e->button() == Qt::LeftButton) {
        const BoxHandle h = hitBoxHandle(e->pos());
        if (h != BH_None) {
            m_boxDragging       = true;
            m_boxHandle         = h;
            m_boxDragStartImg   = widgetToImage(e->pos());
            m_boxDragStartW     = m_boxW;
            m_boxDragStartH     = m_boxH;
            m_boxDragStartDist  = m_boxDist;
            m_boxDragStartAngle = m_boxAngle;
        }
    }
}

void AddPatternImageCanvas::mouseMoveEvent(QMouseEvent *e) {
    // Middle-button pan — handled in every mode.
    if (m_panning) {
        const QPoint d = e->pos() - m_panStart;
        m_offset = m_panStartOffset + QPointF(d);
        update();
        return;
    }
    if (m_pix.isNull() || m_locked) return;

    // ── Crop ────────────────────────────────────────────────────────────
    if (m_mode == Crop) {
        if (m_cropDragging) {
            const QPointF imgNow = widgetToImage(e->pos());
            const QPointF dImg   = imgNow - m_cropDragStartImg;
            QRect r = m_cropDragStartRect;

            auto P = [](const QPointF &p) {
                return QPoint(int(std::round(p.x())), int(std::round(p.y())));
            };

            switch (m_cropHandle) {
            case CH_TL:   r.setTopLeft    (m_cropDragStartRect.topLeft()     + P(dImg)); break;
            case CH_TR:   r.setTopRight   (m_cropDragStartRect.topRight()    + P(dImg)); break;
            case CH_BL:   r.setBottomLeft (m_cropDragStartRect.bottomLeft()  + P(dImg)); break;
            case CH_BR:   r.setBottomRight(m_cropDragStartRect.bottomRight() + P(dImg)); break;
            case CH_Body: r.translate(P(dImg)); break;
            default: break;
            }

            // Enforce minimum size in image pixels and clamp to image bounds.
            if (r.width()  < 8) r.setWidth(8);
            if (r.height() < 8) r.setHeight(8);
            r = r.intersected(QRect(QPoint(0, 0), m_pix.size()));

            if (r != m_crop) {
                m_crop = r;
                update();
                emit cropChanged(m_crop);
            }
        } else {
            const CropHandle h = hitCropHandle(e->pos());
            switch (h) {
            case CH_TL: case CH_BR: setCursor(Qt::SizeFDiagCursor); break;
            case CH_TR: case CH_BL: setCursor(Qt::SizeBDiagCursor); break;
            case CH_Body:           setCursor(Qt::SizeAllCursor);   break;
            default:                setCursor(Qt::ArrowCursor);     break;
            }
        }
        return;
    }

    // ── Pick ────────────────────────────────────────────────────────────
    if (m_mode == Pick) {
        setCursor(Qt::CrossCursor);
        return;
    }

    // ── Box ─────────────────────────────────────────────────────────────
    if (m_mode == Box) {
        if (m_boxDragging) {
            const QPointF imgNow = widgetToImage(e->pos());

            switch (m_boxHandle) {
            case BH_TL: case BH_TR: case BH_BL: case BH_BR: {
                // Convert cursor to box-local axis-aligned coords and use the
                // absolute values as half-sizes — symmetric resize around
                // box A centre (which then propagates to box B).
                const QPointF c = boxACentre();
                const double rR = qDegreesToRadians(m_boxAngle);
                const double co = std::cos(-rR), si = std::sin(-rR);
                const double dx = imgNow.x() - c.x();
                const double dy = imgNow.y() - c.y();
                const double lx = co * dx - si * dy;
                const double ly = si * dx + co * dy;
                const double newW = qMax<double>(8.0, 2.0 * std::abs(lx));
                const double newH = qMax<double>(8.0, 2.0 * std::abs(ly));
                m_boxW = newW;
                m_boxH = newH;
                break;
            }
            case BH_Body: {
                // Translate the box pair — m_boxDist + m_boxAngle re-derived
                // from the cursor position relative to the pick point.
                const double dx = imgNow.x() - m_pick.x();
                const double dy = imgNow.y() - m_pick.y();
                const double dist = std::hypot(dx, dy);
                if (dist > 0.5) {
                    m_boxDist  = dist;
                    m_boxAngle = qRadiansToDegrees(std::atan2(dy, dx));
                }
                break;
            }
            case BH_Rotate: {
                // Rotate around the pick.  We compute the cursor's angle from
                // the pick and offset by 90° so dragging the perpendicular
                // handle puts the box's natural angle along the connector.
                const double dx = imgNow.x() - m_pick.x();
                const double dy = imgNow.y() - m_pick.y();
                if (std::hypot(dx, dy) > 0.5) {
                    m_boxAngle = qRadiansToDegrees(std::atan2(dy, dx)) - 90.0;
                }
                break;
            }
            case BH_Pick: {
                // Drag the picking centre itself — both jaws follow rigidly
                // since their centres are derived from (pick + dist·angle).
                // Clamp to the crop when one is set, otherwise to the image
                // bounds (mirrors Pick mode).
                QPointF imgPt = imgNow;
                if (!m_crop.isNull() && !m_crop.isEmpty()) {
                    imgPt.setX(qBound<double>(m_crop.left(), imgPt.x(), m_crop.right()));
                    imgPt.setY(qBound<double>(m_crop.top(),  imgPt.y(), m_crop.bottom()));
                } else if (!m_pix.isNull()) {
                    imgPt.setX(qBound<double>(0, imgPt.x(), m_pix.width()  - 1));
                    imgPt.setY(qBound<double>(0, imgPt.y(), m_pix.height() - 1));
                }
                m_pick = imgPt.toPoint();
                m_pickCurrentPoint = (!m_crop.isNull() && !m_crop.isEmpty())
                                     ? (m_pick - m_crop.topLeft()) : m_pick;
                break;
            }
            default: break;
            }
            update();
            if (m_boxHandle == BH_Pick)
                emit pickChanged(m_pick, m_pickCurrentPoint);
            else
                emit boxChanged(m_boxW, m_boxH, m_boxDist, m_boxAngle);
        } else {
            const BoxHandle h = hitBoxHandle(e->pos());
            switch (h) {
            case BH_TL: case BH_BR: setCursor(Qt::SizeFDiagCursor); break;
            case BH_TR: case BH_BL: setCursor(Qt::SizeBDiagCursor); break;
            case BH_Rotate:         setCursor(Qt::CrossCursor);     break;
            case BH_Pick:           setCursor(Qt::PointingHandCursor); break;
            case BH_Body:           setCursor(Qt::SizeAllCursor);   break;
            default:                setCursor(Qt::ArrowCursor);     break;
            }
        }
    }
}

void AddPatternImageCanvas::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::MiddleButton) {
        m_panning = false;
        unsetCursor();
        return;
    }
    m_cropDragging = false;
    m_cropHandle   = CH_None;
    m_boxDragging  = false;
    m_boxHandle    = BH_None;
}

void AddPatternImageCanvas::mouseDoubleClickEvent(QMouseEvent *e) {
    // Reset view on middle-button double-click — works in any mode.
    if (e->button() == Qt::MiddleButton) {
        resetView();
        update();
    }
}

void AddPatternImageCanvas::wheelEvent(QWheelEvent *e) {
    if (m_pix.isNull()) return;

    // Zoom around the cursor: keep the image point under the cursor fixed.
    const QPointF mouseW = e->position();
    const QPointF imgPt  = widgetToImage(mouseW);

    const int    delta = e->angleDelta().y();
    const double factor = (delta > 0) ? kZoomStep : 1.0 / kZoomStep;
    const double newZoom = qBound(kMinZoom, m_zoom * factor, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoom)) return;

    m_zoom = newZoom;
    m_offset = mouseW - imgPt * m_zoom;
    update();
}
