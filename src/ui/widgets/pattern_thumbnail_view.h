#ifndef PATTERN_THUMBNAIL_VIEW_H
#define PATTERN_THUMBNAIL_VIEW_H

#include <QGraphicsView>
#include <QPixmap>
#include <QPointF>

class QGraphicsScene;
class QGraphicsPixmapItem;

/**
 * @file pattern_thumbnail_view.h
 * @brief PatternThumbnailView — read-only viewer for the selected pattern's image and its
 *        pick geometry.
 */

/**
 * @class PatternThumbnailView
 * @brief Renders a pattern's training image with its pick point, picking orientation and
 *        gripper jaw boxes, and supports pan / zoom / reset — but nothing else.
 *
 * This is a VIEWER, not an editor. There are no handles, no hit-testing and no geometry
 * signals; the pick geometry is authored in the wizards and the property panel. Read-only
 * is enforced by construction rather than by disabling behaviours one at a time: the
 * scene holds a single pixmap item carrying no interaction flags, and the view runs in
 * ScrollHandDrag, so there is no code path that could mutate the pattern.
 *
 * The overlay is painted by @ref pick_overlay, the same code the authoring canvas uses, so
 * the two cannot drift apart. It is drawn in drawForeground() with the world transform
 * reset, which is what keeps the gizmo a constant on-screen size while the jaw boxes —
 * passed in pre-scaled — stay locked to the image.
 *
 * @note Uses FullViewportUpdate: the overlay is drawn outside the scene's coordinate
 *       system, so a partial repaint would clip it against the wrong rectangle.
 * @see pick_overlay, AddPatternImageCanvas
 */
class PatternThumbnailView : public QGraphicsView {
    Q_OBJECT
public:
    /**
     * @brief Constructs an empty viewer; call setPattern() to give it something to show.
     * @param[in] parent parent widget
     */
    explicit PatternThumbnailView(QWidget *parent = nullptr);
    ~PatternThumbnailView() override = default;

    /**
     * @brief Shows @p image with the given pick geometry and fits it to the viewport.
     *
     * Resets the view transform, so selecting a pattern always starts from a predictable
     * framing rather than inheriting the previous pattern's zoom.
     *
     * @param[in] image the pattern's training image
     * @param[in] pick pick point, in image pixel coordinates
     * @param[in] pickAngleDeg picking orientation, in degrees
     * @param[in] boxW jaw width, in image pixels
     * @param[in] boxH jaw height, in image pixels
     * @param[in] boxDist jaw centre-to-centre half-distance, in image pixels
     * @param[in] boxAngleDeg jaw-pair orientation, in degrees
     * @param[in] showBoxes false to omit the jaw pair (pattern has no usable box geometry)
     */
    void setPattern(const QPixmap &image, const QPointF &pick, double pickAngleDeg,
                    double boxW, double boxH, double boxDist, double boxAngleDeg,
                    bool showBoxes);

    /// Clears the image and the overlay, leaving an empty view.
    void clear();

    /// Refits the image to the viewport and drops any pan/zoom the user applied.
    void resetView();

protected:
    /// Paints the pick overlay on top of the image, in viewport coordinates.
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    /// Zooms about the cursor by a fixed factor per wheel notch, clamped to [kMinZoom, kMaxZoom].
    void wheelEvent(QWheelEvent *event) override;
    /// Resets the view on a double-click, matching the wizard canvas's middle-click reset.
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /// Keeps the image fitted while the user has not zoomed or panned.
    void resizeEvent(QResizeEvent *event) override;

private:
    QGraphicsScene      *m_scene{nullptr};       ///< Owns the single pixmap item.
    QGraphicsPixmapItem *m_pixmapItem{nullptr};  ///< The pattern image; carries no interaction flags.

    QPointF m_pick;                 ///< Pick point, in image (== scene) coordinates.
    double  m_pickAngle{0.0};       ///< Picking orientation, in degrees.
    double  m_boxW{0.0};            ///< Jaw width, in image pixels.
    double  m_boxH{0.0};            ///< Jaw height, in image pixels.
    double  m_boxDist{0.0};         ///< Jaw centre-to-centre half-distance, in image pixels.
    double  m_boxAngle{0.0};        ///< Jaw-pair orientation, in degrees.
    bool    m_showBoxes{false};     ///< Whether the jaw pair is drawn at all.

    /// True until the user zooms or pans; while set, a resize refits the image so the
    /// thumbnail fills its pane instead of keeping a framing chosen for the old size.
    bool    m_autoFit{true};
};

#endif // PATTERN_THUMBNAIL_VIEW_H
