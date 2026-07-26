#ifndef IMAGE_VIEW_ONLY_H
#define IMAGE_VIEW_ONLY_H

#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QSettings>
#include <QPointF>
#include <QGraphicsItemGroup>
#include <QGraphicsPixmapItem>
#include <QAction>

#include <opencv2/opencv.hpp>

// cvs = computer vision
/// Read-only, non-editable QGraphicsView for displaying a single image
/// (loaded from a file path, an OpenCV cv::Mat, or a QPixmap) with Ctrl+wheel
/// zoom, Ctrl+left-drag panning, rectangular ROI overlays drawn on top of the
/// pixmap, and fit-to-view on load / double-middle-click / R key.
class ImageViewOnly : public QGraphicsView {
    Q_OBJECT
public:
    /// Constructs the view: creates the backing QGraphicsScene, sets a dark
    /// grey background brush, full-viewport update mode, and pixel-perfect
    /// (non-smooth) pixmap rendering by default.
    explicit ImageViewOnly(QWidget *parent = nullptr);

    /// Toggles pixel-perfect rendering: when enabled, disables Qt's smooth
    /// pixmap transform so a zoomed-in image shows hard pixel edges instead
    /// of blur.
    void setEnabelPixelModel(bool enable);
    /// Returns the currently displayed pixmap, or a null QPixmap if no image is loaded.
    QPixmap getCurrentImage();

    /// Adds a rectangular ROI outline (corners given as tl/br points) as a
    /// child item of the current pixmap item so it renders on top of the
    /// image; the fill is always a fixed low-alpha green regardless of
    /// `border_color`, which is only used for the 3px border pen.
    /// @param tl_x/tl_y top-left corner of the ROI rectangle
    /// @param br_x/br_y bottom-right corner of the ROI rectangle
    /// @param border_color colour used for the ROI border pen
    void addROI(int tl_x, int tl_y, int br_x, int br_y, QColor border_color);
    /// Removes all previously added ROI rectangles from the scene and clears the tracking list.
    void removeAllROI();
    /// Loads an image from disk at `path` and displays it; logs and does
    /// nothing if the file cannot be read as an image.
    /// @param fitsize when true, refits the view to the image bounds after loading
    void loadImageFromPath(QString &path, bool fitsize = true);
    /// Converts an OpenCV image (8-bit grayscale, BGR, or BGRA) to a QPixmap
    /// via cvMatToQPixmap and displays it; does nothing if `image` is empty.
    /// @param fitsize when true, refits the view to the image bounds after loading
    void loadImageOpenCv(cv::Mat &image, bool fitsize = false);
    /// Displays `pixmap`, creating the scene's pixmap item on first use (with
    /// FastTransformation to avoid blur) or updating it otherwise; updates
    /// the scene rect to the pixmap size and fits the view to it when
    /// `fitsize` is true or this is the very first image ever shown.
    void loadImage(QPixmap &pixmap, bool fitsize= false);
    /// Removes and deletes the current pixmap item (if any) and re-fits the
    /// (now empty) view to the last known bounding rect.
    void clearCurrentImage();
    /// Returns true if an image is currently loaded.
    bool hadImage();

protected:
    /// Current mouse-interaction mode, used to disambiguate plain view
    /// navigation from the Ctrl-driven zoom/pan gestures.
    enum InteractMode {
        IModeNone,
        IModeZoom,
        IModePan
    };

    /// Routes right/left button presses to rightMouseButtonPressed() /
    /// leftMouseButtonPressed() first; falls back to
    /// QGraphicsView::mousePressEvent when neither consumes the event.
    void mousePressEvent(QMouseEvent *event) override;
    /// While panning (IModePan), scrolls the view by the pointer delta since
    /// the last move; always forwards to QGraphicsView::mouseMoveEvent afterward.
    void mouseMoveEvent(QMouseEvent *event) override;
    /// Routes right/left button releases to rightMouseButtonReleased() /
    /// leftMouseButtonReleased() first; falls back to
    /// QGraphicsView::mouseReleaseEvent when neither consumes the event.
    void mouseReleaseEvent(QMouseEvent *event) override;
    /// A middle-button double-click while in IModeNone refits the view to
    /// the current image bounds.
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /// Ctrl+wheel zooms the view (scale factor 1.15 or 0.85 depending on
    /// wheel direction), temporarily switching to IModeZoom for the scale
    /// call and restoring the previous mode afterward; non-Ctrl wheel events
    /// are forwarded to QGraphicsView::wheelEvent.
    void wheelEvent(QWheelEvent *event) override;
    /// Escape cancels the current interact mode; R (while in IModeNone)
    /// refits the view to the image bounds; Delete is accepted but otherwise
    /// unhandled. Always forwards to QGraphicsView::keyPressEvent afterward.
    void keyPressEvent(QKeyEvent *event) override;
    /// Releasing Ctrl while panning (IModePan) clears the last pan point and
    /// returns to IModeNone.
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    /// Switches to `mode` (no-op if already active), remembering the
    /// previous mode and refreshing the cursor and m_scene_interacting flag.
    void changeInteractMode(InteractMode mode);
    /// Restores the interact mode saved by changeInteractMode() (no-op if
    /// it's already current), swapping current/previous and refreshing the
    /// cursor and m_scene_interacting flag.
    void backToPreviousMode();
    /// Sets the view cursor to match m_current_mode (arrow for None, closed
    /// hand for Pan; Zoom currently leaves the cursor unchanged).
    void changeCursor();
    /// Returns a human-readable name for `mode` ("None"/"Zoom"/"Pan", or "Unknown").
    QString interactMode2String(InteractMode mode);

    /// Placeholder right-button-press handler; currently always returns
    /// false (event not consumed).
    bool rightMouseButtonPressed(QMouseEvent *event);
    /// Starts panning (IModePan) when the left button is pressed with Ctrl
    /// held, recording the press position; always returns false so the base
    /// class still processes the event.
    bool leftMouseButtonPressed(QMouseEvent *event);
    /// Placeholder right-button-release handler; currently always returns
    /// false (event not consumed).
    bool rightMouseButtonReleased(QMouseEvent *event);
    /// Ends panning when in IModePan (clears the pan point and reverts to
    /// the previous mode); no-op for the other modes.
    /// @return true if this release completed an actual pan gesture; false
    /// otherwise (including for non-pan modes, where it is always false)
    bool leftMouseButtonReleased(QMouseEvent *event);

private:
    QGraphicsScene *m_scene{nullptr};   ///< Owns the QGraphicsScene backing the view (parented to `this`).
    QGraphicsPixmapItem *m_pixmap_item{nullptr};   ///< The single displayed pixmap item; null until an image is loaded.
    QRectF m_pixmap_bounding_rect;   ///< Cached bounding rect of the current pixmap item, used to refit the view.

    bool m_first_time_image_set{false};   ///< True after the first image has been loaded once.
    bool m_is_pixel_model{true};   ///< When true, disables smooth pixmap transform for hard pixel edges when zoomed.

    InteractMode m_current_mode;    ///< Currently active interact mode.
    InteractMode m_previous_mode;   ///< Interact mode active before the current one, used to restore state.
    bool m_scene_interacting;   ///< True whenever an interact mode other than IModeNone is active.

    bool m_has_panned;   ///< Tracks whether an actual pan movement occurred during the current pan gesture.
    QPoint m_last_pan_point;   ///< Last recorded mouse position used to compute pan deltas.

    QList<QGraphicsRectItem*> m_roi_items;   ///< ROI rectangle items added via addROI(), removed by removeAllROI().
};

#endif // IMAGE_VIEW_ONLY_H
