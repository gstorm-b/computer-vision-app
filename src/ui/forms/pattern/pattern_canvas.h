#ifndef PATTERN_CANVAS_H
#define PATTERN_CANVAS_H

#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <opencv2/core.hpp>

/// Shared canvas widget used by AddPatternWizard / EditPatternWizard to edit the crop, pick, and
/// box geometry over a displayed image.
///
/// Coordinate model: all geometry the host gets/sets (crop, pick, box) is in source-image pixel
/// coordinates. The widget maintains its own view transform (zoom + pan) and renders through it,
/// so geometry stays stable across resizes and zooms.
///
/// View interaction (active in every mode): mouse wheel zooms around the cursor, middle-button
/// drag pans, middle-button double-click resets the view (fits the image to the widget and
/// recentres it), and the image is refit on every setImage() call.
///
/// Modes:
/// - None: image only, no overlay.
/// - Crop: body drag translates the crop rect, four corner handles resize it; drawn with a
///   rule-of-thirds grid and a dim scrim outside the crop.
/// - Pick: click sets the pick point; if a non-empty crop is set, it is drawn read-only so the
///   user can see the crop region.
/// - Box: symmetric jaw pair around the pick point. Box A is interactive (body drag moves it,
///   changing dist + angle; corner handles resize W/H, mirrored onto box B; the rotation handle
///   rotates it), and the pick crosshair itself is draggable -- both jaws follow it rigidly.
///   Boxes may extend beyond the image bounds.
/// - Finish: read-only review of the pick crosshair and jaw pair.
///
/// @note Emits cropChanged(QRect), pickChanged(QPoint, QPoint), and
/// boxChanged(w, h, dist, angleDeg) while the user edits the corresponding geometry.
class AddPatternImageCanvas : public QWidget {
    Q_OBJECT
public:
    /// Active interaction mode; selects which overlay is drawn and which mouse gestures are
    /// enabled (see class-level docs for the behavior of each mode).
    enum Mode { None, Crop, Pick, Box, Finish };

    /// Constructs an empty canvas (no image, 400x280 minimum size) with mouse tracking and strong
    /// focus enabled; paints an opaque background every frame.
    explicit AddPatternImageCanvas(QWidget *parent = nullptr);

    // ── Image source ─────────────────────────────────────────────────────
    /// Sets the source image from a QPixmap and resets the view (zoom/pan) so it fits the widget.
    void setImage(const QPixmap &pix);
    /// Sets the source image from an OpenCV cv::Mat (CV_8UC1/CV_8UC3/CV_8UC4, BGR(A) converted to
    /// RGB(A)) and resets the view so it fits the widget; an empty mat clears the image.
    void setImage(const cv::Mat &mat);
    /// Clears the source image, leaving the canvas blank; does not reset the view.
    void clearImage();
    /// Returns true if a source image is currently set.
    bool hasImage() const { return !m_pix.isNull(); }
    /// Returns the source image size in pixels (empty if no image is set).
    QSize imageSize() const { return m_pix.size(); }

    // ── Mode ─────────────────────────────────────────────────────────────
    /// Switches the active interaction mode and repaints; does not alter any stored crop/pick/box
    /// geometry.
    /// @param m mode to activate
    void setMode(Mode m);
    /// Returns the current interaction mode.
    Mode mode() const { return m_mode; }

    // ── Crop (image pixels) ──────────────────────────────────────────────
    /// Sets the crop rectangle in source-image pixels and repaints.
    /// @param r new crop rect; an empty rect disables the crop overlay (useful when keepOriginal
    /// is on)
    void  setCrop(const QRect &r);
    /// Returns the current crop rectangle in source-image pixels (empty if no crop is set).
    QRect crop() const { return m_crop; }

    // ── Pick (image pixels) ──────────────────────────────────────────────
    /// Sets the pick point (source-image pixels when no crop is set; crop-relative pixels when a
    /// crop is set -- internally combined with the crop origin so pick() returns the correct
    /// coordinates in either case).
    void   setPick(const QPoint &p);
    /// Returns the current pick point.
    /// @return `m_pickCurrentPoint` (crop-relative) when a non-empty crop is set, otherwise
    /// `m_pick` (absolute image pixels).
    QPoint pick() const {
        if (!m_crop.isNull() && !m_crop.isEmpty()) {
            return m_pickCurrentPoint;
        }
        return m_pick;
    }


    // ── Box config (image-pixel sizes, degrees) ──────────────────────────
    /// Sets box A's width, height, distance from the pick point, and rotation angle; box B is
    /// derived automatically (mirrored 180 degrees) when painted.
    /// @param w box width, in image pixels
    /// @param h box height, in image pixels
    /// @param dist distance from the pick point to box A's centre, in image pixels
    /// @param angleDeg rotation angle of box A, in degrees
    void setBoxConfig(double w, double h, double dist, double angleDeg);

    // ── Locked overlay (Edit wizard step 1 — purple scrim + lock badge) ──
    /// Shows or hides the locked overlay (dim scrim plus a "LOCKED" badge) and disables all
    /// crop/pick/box editing gestures while locked (middle-button panning still works).
    /// @param locked true to lock the canvas against edits
    void setLocked(bool locked);

    // ── View transform ───────────────────────────────────────────────────
    /// Resets zoom/pan so the entire image fits inside the widget, centred; leaves zoom at 1.0
    /// and the offset at the origin when no image is set.
    void resetView();

signals:
    // All signal payloads are in SOURCE-IMAGE pixel coordinates, independent
    // of the current zoom / pan view.  Hosts can safely store them as the
    // canonical geometry without any inverse transform.
    /// Emitted while the user drags a crop corner handle or the crop body, with the updated rect.
    void cropChanged(const QRect &r);                                       // image-pixel rect
    /// Emitted while the user sets or drags the pick point (Pick mode click/drag, or the Box
    /// mode pick handle).
    /// @param p pick point in absolute source-image pixels (clamped to the crop or image bounds)
    /// @param imgp crop-relative pick point when a crop is set
    void pickChanged(const QPoint &p, const QPoint &imgp);                  // image-pixel point
    /// Emitted while the user drags a box A handle (resize, move, or rotate); box B mirrors box A
    /// automatically.
    void boxChanged (double w, double h, double dist, double angleDeg);     // image-pixel sizes

protected:
    /// Paints the background, the image (through the view transform), the mode-specific overlay
    /// (crop grid, pick crosshair, or box pair), and the locked scrim/badge when locked.
    void paintEvent(QPaintEvent *e) override;
    /// Starts middle-button panning (any mode), or -- depending on the active mode -- begins a
    /// crop-handle drag, sets the pick point immediately, or begins a box-handle drag. Only
    /// panning is honoured while locked.
    void mousePressEvent(QMouseEvent *e) override;
    /// Continues an in-progress pan / crop-drag / box-drag, or updates the hover cursor to match
    /// the handle under the pointer; emits cropChanged/pickChanged/boxChanged as geometry changes.
    void mouseMoveEvent(QMouseEvent *e) override;
    /// Ends middle-button panning or any in-progress crop/box handle drag.
    void mouseReleaseEvent(QMouseEvent *e) override;
    /// Resets the view (fit + recentre) on a middle-button double-click.
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    /// Zooms in/out around the cursor position (mouse wheel), keeping the image point under the
    /// cursor fixed.
    void wheelEvent(QWheelEvent *e) override;
    /// Base-class handling only; the current zoom/pan is deliberately preserved across resizes
    /// (only setImage() and the middle-button double-click refit the view).
    void resizeEvent(QResizeEvent *e) override;

private:
    // ── Hit-test enums ───────────────────────────────────────────────────
    /// Which part of the crop overlay is under the cursor: one of the four corner resize handles,
    /// the draggable body (CH_Body), or nothing (CH_None).
    enum CropHandle { CH_None=-1, CH_TL=0, CH_TR=1, CH_BL=2, CH_BR=3, CH_Body=4 };
    /// Which part of the box A overlay is under the cursor: a corner resize handle, the draggable
    /// body (BH_Body), the rotation knob (BH_Rotate), the pick crosshair (BH_Pick), or nothing
    /// (BH_None).
    enum BoxHandle  { BH_None=-1, BH_TL=0, BH_TR=1, BH_BL=2, BH_BR=3,
                      BH_Body=4, BH_Rotate=5, BH_Pick=6 };

    // ── View transform helpers ───────────────────────────────────────────
    /// Converts a source-image pixel coordinate to widget-pixel coordinates using the current
    /// zoom/pan (`m_zoom`, `m_offset`).
    QPointF imageToWidget(QPointF p) const {
        return QPointF(m_offset.x() + p.x() * m_zoom,
                       m_offset.y() + p.y() * m_zoom);
    }
    /// Converts a widget-pixel coordinate back to source-image pixel coordinates; inverse of
    /// imageToWidget().
    QPointF widgetToImage(QPointF p) const {
        return QPointF((p.x() - m_offset.x()) / m_zoom,
                       (p.y() - m_offset.y()) / m_zoom);
    }
    /// Returns the image's bounding rect in widget-pixel coordinates at the current zoom/pan.
    QRectF  imageRectInWidget() const;
    /// Returns the zoom level, clamped to [kMinZoom, kMaxZoom], that fits the whole image inside
    /// the widget; 1.0 when no image is set.
    double  fitZoom() const;

    // ── Crop hit-testing / clamping ──────────────────────────────────────
    /// Returns which crop handle (a corner or the body) is under `widgetPos`, or CH_None if the
    /// crop is empty/unset or the position doesn't hit any handle.
    CropHandle hitCropHandle(const QPoint &widgetPos) const;

    // ── Box geometry / hit-testing ───────────────────────────────────────
    /// Returns box A's centre, in image pixels: the pick point offset by `m_boxDist` along
    /// `m_boxAngle`.
    QPointF boxACentre() const;
    /// Returns the rotation handle's position, in image pixels: offset from box A's centre,
    /// perpendicular to the pick-to-centre connector, by `m_boxH / 2 + kRotateHandleStandoff`.
    QPointF boxRotationHandle() const;
    /// Computes the four corners of box A, in image pixels and rotated by `m_boxAngle`.
    /// @param out destination array of 4 points (order TL, TR, BL, BR); overwritten
    void    boxACorners(QPointF out[4]) const;
    /// Returns which box A handle is under `widgetPos` (checked in priority order: pick crosshair,
    /// rotation knob, corners, then body), or BH_None if none is hit.
    BoxHandle hitBoxHandle(const QPoint &widgetPos) const;

    // ── State ────────────────────────────────────────────────────────────
    Mode    m_mode{None};           ///< Current interaction mode.
    QPixmap m_pix;                  ///< Currently displayed image, at source resolution.
    QRect   m_crop{};               ///< Crop rectangle, in image pixels; empty means no crop.
    QPoint  m_pick{0, 0};           ///< Pick point, in absolute image pixels.

    double  m_boxW{120}, m_boxH{80};        ///< Box A width/height, in image pixels.
    double  m_boxDist{90}, m_boxAngle{0};   ///< Box A distance from the pick point (image px) and rotation angle (degrees).

    bool    m_locked{false};        ///< True while the locked overlay is shown and edits are disabled.

    // View transform — image-to-widget mapping
    double  m_zoom{1.0};            ///< Current zoom factor (image pixels -> widget pixels).
    QPointF m_offset{0, 0};         ///< Widget-pixel position of the image origin (pan offset).

    // Interaction state
    bool       m_panning{false};    ///< True while a middle-button pan drag is in progress.
    QPoint     m_panStart;          ///< Widget-pixel cursor position where the current pan began.
    QPointF    m_panStartOffset;    ///< `m_offset` value captured when the current pan began.

    // Crop drag
    bool       m_cropDragging{false};  ///< True while a crop handle/body drag is in progress.
    CropHandle m_cropHandle{CH_None};  ///< Which crop handle is being dragged.
    QPointF    m_cropDragStartImg;   ///< Image-pixel cursor position where the current crop drag began.
    QRect      m_cropDragStartRect;    ///< `m_crop` value captured when the current crop drag began.

    // Box drag
    bool      m_boxDragging{false};    ///< True while a box A handle drag is in progress.
    BoxHandle m_boxHandle{BH_None};    ///< Which box A handle is being dragged.
    QPointF   m_boxDragStartImg;       ///< Image-pixel cursor position where the current box drag began.
    double    m_boxDragStartW{0}, m_boxDragStartH{0};        ///< Box A width/height captured when the current drag began.
    double    m_boxDragStartDist{0}, m_boxDragStartAngle{0}; ///< Box A distance/angle captured when the current drag began.

    //Pick position
    QPoint    m_pickCurrentPoint;   ///< Crop-relative pick point (see pick()).
};

#endif // PATTERN_CANVAS_H
