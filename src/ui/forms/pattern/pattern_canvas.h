#ifndef PATTERN_CANVAS_H
#define PATTERN_CANVAS_H

#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <opencv2/core.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  AddPatternImageCanvas
//
//  Shared canvas widget used by AddPatternWizard / EditPatternWizard.
//
//  Coordinate model
//  ────────────────
//  All geometry the host gets/sets (crop, pick, box) is in **source-image
//  pixels**.  The widget maintains a view transform (zoom + pan) and renders
//  everything through it, so geometry stays stable across resizes / zooms.
//
//  View interaction (active in every mode)
//  ───────────────────────────────────────
//    • Mouse wheel             → zoom around cursor
//    • Middle-button drag      → pan
//    • Middle-button double-click → reset (fit image to widget, recentre)
//    • Image is resized to fit on every setImage() call
//
//  Modes
//  ─────
//    None   — image only.
//    Crop   — body drag to translate, four corner handles to resize.  Rule-of-
//             thirds grid and dim scrim outside.
//    Pick   — click to set pick point.  If a non-empty crop is set, it is drawn
//             read-only so the user knows the crop region.
//    Box    — symmetric jaw pair around the pick point.  Box A is interactive:
//             body drag → move (changes dist + angle), corner handles → resize
//             (W and H, mirrored to box B), rotation handle → rotate.  The pick
//             crosshair itself is draggable too — both jaws follow it rigidly.
//             Boxes may extend beyond the image bounds.
//    Finish — read-only review (pick crosshair + jaw pair).
//
//  Signals
//  ───────
//    cropChanged(QRect)        — emitted while the user edits the crop.
//    pickChanged(QPoint)       — emitted while the user edits the pick.
//    boxChanged(w, h, d, ang)  — emitted while the user edits the jaw pair.
// ─────────────────────────────────────────────────────────────────────────────

class AddPatternImageCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { None, Crop, Pick, Box, Finish };

    explicit AddPatternImageCanvas(QWidget *parent = nullptr);

    // ── Image source ─────────────────────────────────────────────────────
    void setImage(const QPixmap &pix);
    void setImage(const cv::Mat &mat);
    void clearImage();
    bool hasImage() const { return !m_pix.isNull(); }
    QSize imageSize() const { return m_pix.size(); }

    // ── Mode ─────────────────────────────────────────────────────────────
    void setMode(Mode m);
    Mode mode() const { return m_mode; }

    // ── Crop (image pixels) ──────────────────────────────────────────────
    // An empty rect disables the crop overlay (useful when keepOriginal is on).
    void  setCrop(const QRect &r);
    QRect crop() const { return m_crop; }

    // ── Pick (image pixels) ──────────────────────────────────────────────
    void   setPick(const QPoint &p);
    QPoint pick() const {
        if (!m_crop.isNull() && !m_crop.isEmpty()) {
            return m_pickCurrentPoint;
        }
        return m_pick;
    }


    // ── Box config (image-pixel sizes, degrees) ──────────────────────────
    void setBoxConfig(double w, double h, double dist, double angleDeg);

    // ── Locked overlay (Edit wizard step 1 — purple scrim + lock badge) ──
    void setLocked(bool locked);

    // ── View transform ───────────────────────────────────────────────────
    // Reset zoom/pan so the entire image fits inside the widget, centred.
    void resetView();

signals:
    // All signal payloads are in SOURCE-IMAGE pixel coordinates, independent
    // of the current zoom / pan view.  Hosts can safely store them as the
    // canonical geometry without any inverse transform.
    void cropChanged(const QRect &r);                                       // image-pixel rect
    void pickChanged(const QPoint &p, const QPoint &imgp);                  // image-pixel point
    void boxChanged (double w, double h, double dist, double angleDeg);     // image-pixel sizes

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    // ── Hit-test enums ───────────────────────────────────────────────────
    enum CropHandle { CH_None=-1, CH_TL=0, CH_TR=1, CH_BL=2, CH_BR=3, CH_Body=4 };
    enum BoxHandle  { BH_None=-1, BH_TL=0, BH_TR=1, BH_BL=2, BH_BR=3,
                      BH_Body=4, BH_Rotate=5, BH_Pick=6 };

    // ── View transform helpers ───────────────────────────────────────────
    // Image-pixel → widget-pixel
    QPointF imageToWidget(QPointF p) const {
        return QPointF(m_offset.x() + p.x() * m_zoom,
                       m_offset.y() + p.y() * m_zoom);
    }
    QPointF widgetToImage(QPointF p) const {
        return QPointF((p.x() - m_offset.x()) / m_zoom,
                       (p.y() - m_offset.y()) / m_zoom);
    }
    QRectF  imageRectInWidget() const;
    double  fitZoom() const;     // zoom that fits the image into widget

    // ── Crop hit-testing / clamping ──────────────────────────────────────
    CropHandle hitCropHandle(const QPoint &widgetPos) const;

    // ── Box geometry / hit-testing ───────────────────────────────────────
    QPointF boxACentre() const;             // image-pixel centre of box A
    QPointF boxRotationHandle() const;      // image-pixel position of rotation knob
    // Returns the four corners of box A (in image pixels), order: TL TR BL BR.
    void    boxACorners(QPointF out[4]) const;
    BoxHandle hitBoxHandle(const QPoint &widgetPos) const;

    // ── State ────────────────────────────────────────────────────────────
    Mode    m_mode{None};
    QPixmap m_pix;                  // currently displayed image (source size)
    QRect   m_crop{};               // image pixels; empty = no crop
    QPoint  m_pick{0, 0};           // image pixels

    double  m_boxW{120}, m_boxH{80};
    double  m_boxDist{90}, m_boxAngle{0};

    bool    m_locked{false};

    // View transform — image-to-widget mapping
    double  m_zoom{1.0};
    QPointF m_offset{0, 0};         // widget-pixel position of image origin

    // Interaction state
    bool       m_panning{false};
    QPoint     m_panStart;
    QPointF    m_panStartOffset;

    // Crop drag
    bool       m_cropDragging{false};
    CropHandle m_cropHandle{CH_None};
    QPointF    m_cropDragStartImg;   // image-coord cursor at drag start
    QRect      m_cropDragStartRect;

    // Box drag
    bool      m_boxDragging{false};
    BoxHandle m_boxHandle{BH_None};
    QPointF   m_boxDragStartImg;
    double    m_boxDragStartW{0}, m_boxDragStartH{0};
    double    m_boxDragStartDist{0}, m_boxDragStartAngle{0};

    //Pick position
    QPoint    m_pickCurrentPoint;
};

#endif // PATTERN_CANVAS_H
