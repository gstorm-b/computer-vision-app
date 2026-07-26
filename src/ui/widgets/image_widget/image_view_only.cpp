#include "image_view_only.h"

#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include "item_roi.h"

/// Converts an OpenCV cv::Mat (8-bit grayscale, BGR, or BGRA) to a QPixmap
/// for display, converting BGR/BGRA to RGB/RGBA order along the way.
/// @param mat source image in CV_8UC1, CV_8UC3 (BGR), or CV_8UC4 (BGRA) format
/// @return the converted QPixmap, or a null QPixmap if `mat`'s type is unsupported
inline QPixmap cvMatToQPixmap(const cv::Mat& mat) {
    QImage qimg;
    if (mat.type() == CV_8UC1) {
        // Grayscale
        qimg = QImage(mat.data,
                      mat.cols,
                      mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_Grayscale8);
    } else if (mat.type() == CV_8UC3) {
        // OpenCV uses BGR, Qt uses RGB
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data,
                      rgb.cols,
                      rgb.rows,
                      static_cast<int>(rgb.step),
                      QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        // BGRA → RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        qimg = QImage(rgba.data,
                      rgba.cols,
                      rgba.rows,
                      static_cast<int>(rgba.step),
                      QImage::Format_RGBA8888).copy();
    } else {
        return QPixmap();
    }

    return QPixmap::fromImage(qimg);
}

/// Creates the backing QGraphicsScene, applies a dark grey background brush,
/// sets FullViewportUpdate (to avoid ghosting), and disables smooth pixmap
/// transform (per m_is_pixel_model) so zoomed pixels stay crisp.
ImageViewOnly::ImageViewOnly(QWidget *parent)
    : QGraphicsView(parent),
    m_scene(new QGraphicsScene(this)),
    m_pixmap_item(nullptr),
    m_first_time_image_set(false),
    m_is_pixel_model(true) {

    this->setScene(m_scene);

    QBrush brush(QColor(0x565656));
    this->setBackgroundBrush(brush);

    // set view port update mode to avoid ghosting
    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    // off smooth pixmap transform to disable blur when zoom in
    this->setRenderHint(QPainter::SmoothPixmapTransform, m_is_pixel_model);

    changeInteractMode(InteractMode::IModeNone);
}

/// Stores `enable` in m_is_pixel_model and applies it as the
/// SmoothPixmapTransform render hint (inverted: pixel-model on means smooth
/// transform off).
void ImageViewOnly::setEnabelPixelModel(bool enable) {
    m_is_pixel_model = enable;
    this->setRenderHint(QPainter::SmoothPixmapTransform, m_is_pixel_model);
}

/// Returns the current pixmap item's pixmap, or a null QPixmap if no image is loaded.
QPixmap ImageViewOnly::getCurrentImage() {
    if (m_pixmap_item == nullptr) {
        return QPixmap();
    }
    return m_pixmap_item->pixmap();
}

/// Builds a QGraphicsRectItem for the ROI rect, parented to m_pixmap_item so
/// it renders on top of the image without a separate scene->addItem() call,
/// then records it in m_roi_items for later removal.
void ImageViewOnly::addROI(int tl_x, int tl_y, int br_x, int br_y, QColor border_color) {
    QRect rect_roi = QRect(QPoint(tl_x, tl_y), QPoint(br_x, br_y));
    QGraphicsRectItem *item = new QGraphicsRectItem(rect_roi, m_pixmap_item);

    QPen pen(border_color);
    QColor bg_color(0, 255, 0, 10);
    pen.setWidth(3);
    item->setPen(pen);
    item->setBrush(bg_color);

    // m_scene->addItem(item);
    m_roi_items.append(item);
}

/// Removes every tracked ROI item from the scene and empties m_roi_items.
void ImageViewOnly::removeAllROI() {
    for (int idx=0;idx<m_roi_items.size();idx++) {
        m_scene->removeItem(m_roi_items.at(idx));
    }
    m_roi_items.clear();
}

/// Reads the image at `path` into a QImage and, if valid, converts it to a
/// QPixmap and forwards to loadImage(); logs via qDebug() and returns early
/// if the file cannot be read as an image.
void ImageViewOnly::loadImageFromPath(QString &path, bool fitsize) {
    QImage image(path);
    if (image.isNull()) {
        qDebug() << "Image Interact widget: cannot read image at - " << path;
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    this->loadImage(pixmap, fitsize);
}

/// Converts `image` via cvMatToQPixmap() and forwards to loadImage(); does
/// nothing if `image` is empty.
void ImageViewOnly::loadImageOpenCv(cv::Mat &image, bool fitsize) {
    if (image.empty()) {
        return;
    }
    QPixmap pixmap = cvMatToQPixmap(image);
    this->loadImage(pixmap, fitsize);
}

/// Creates the scene's pixmap item on first call (with FastTransformation to
/// avoid blur) or updates its pixmap on subsequent calls; always resizes the
/// scene rect to `pixmap` and refits the view to the new bounding rect when
/// `fitsize` is true or this is the very first image ever shown.
void ImageViewOnly::loadImage(QPixmap &pixmap, bool fitsize) {
    if (!m_pixmap_item) {
        m_pixmap_item = m_scene->addPixmap(pixmap);
        // using FastTransformation to avoid blur image
        m_pixmap_item->setTransformationMode(Qt::FastTransformation);
    } else {
        m_pixmap_item->setPixmap(pixmap);
    }
    m_scene->setSceneRect(pixmap.rect());
    // fit image with current window size
    m_pixmap_bounding_rect = m_pixmap_item->boundingRect();
    if (fitsize || (!m_first_time_image_set)) {
        m_first_time_image_set = true;
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
    }
}

/// Removes and deletes the current pixmap item (if any), then re-fits the
/// view to the last cached bounding rect; no-op if no image is loaded.
void ImageViewOnly::clearCurrentImage() {
    if (m_pixmap_item != nullptr) {
        m_scene->removeItem(m_pixmap_item);
        delete m_pixmap_item;
        m_pixmap_item = nullptr;
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
    }
}

/// Returns true if a pixmap item is currently set.
bool ImageViewOnly::hadImage() {
    return (m_pixmap_item != nullptr);
}

/// Custom mouse press handler: dispatches right/left button presses to
/// rightMouseButtonPressed()/leftMouseButtonPressed() and returns early if
/// either consumes the event; otherwise falls through to the base class.
void ImageViewOnly::mousePressEvent(QMouseEvent *event) {
    // custom handle mouse press event
    switch (event->button()) {
    case Qt::RightButton:
        if (this->rightMouseButtonPressed(event)) {
            return;
        }
        break;
    case Qt::LeftButton:
        if (this->leftMouseButtonPressed(event)) {
            return;
        }
        break;
    default:
        break;
    }

    QGraphicsView::mousePressEvent(event);
}

/// While in IModePan, scrolls the horizontal/vertical scrollbars by the
/// pointer delta since the last recorded pan point; always forwards to the
/// base class afterward.
void ImageViewOnly::mouseMoveEvent(QMouseEvent *event) {

    switch (m_current_mode) {
    case IModeNone:

        break;
    case IModeZoom:

        break;
    case IModePan:
    {
        m_has_panned = true;
        QPoint delta = event->pos() - m_last_pan_point;
        if (!delta.isNull()) {
            this->horizontalScrollBar()->setValue(
                horizontalScrollBar()->value() - delta.x());
            this->verticalScrollBar()->setValue(
                verticalScrollBar()->value() - delta.y());
            m_last_pan_point = event->pos();
        }
    }
    break;
    }

    QGraphicsView::mouseMoveEvent(event);
}

/// Custom mouse release handler: dispatches right/left button releases to
/// rightMouseButtonReleased()/leftMouseButtonReleased() and returns early if
/// either consumes the event; otherwise falls through to the base class.
void ImageViewOnly::mouseReleaseEvent(QMouseEvent *event) {
    // custom handle mouse release event
    switch (event->button()) {
    case Qt::RightButton:
        if (this->rightMouseButtonReleased(event)) {
            return;
        }
        break;
    case Qt::LeftButton:
        if (this->leftMouseButtonReleased(event)) {
            return;
        }
        break;
    default:
        break;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/// A middle-button double-click while in IModeNone refits the view to the
/// current pixmap bounding rect and accepts the event; always forwards to
/// the base class afterward.
void ImageViewOnly::mouseDoubleClickEvent(QMouseEvent *event) {
    if ((m_current_mode == IModeNone) && event->button() == Qt::MiddleButton){
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
        event->accept();
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

/// Ctrl+wheel zooms the view: switches to IModeZoom, scales by 1.15 (wheel
/// up) or 0.85 (wheel down), then restores the previous interact mode and
/// returns without forwarding the event. Without Ctrl, forwards to the base
/// class unchanged.
void ImageViewOnly::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        changeInteractMode(IModeZoom);
        double angle = event->angleDelta().y();
        double factor = (angle > 0) ? 1.15 : 0.85;
        this->scale(factor, factor);
        backToPreviousMode();
        return;
    }

    QGraphicsView::wheelEvent(event);
}

/// Escape always cancels the current interact mode (back to IModeNone). When
/// already in IModeNone, Delete is accepted but otherwise ignored, and R
/// refits the view to the pixmap bounding rect. Always forwards to the base
/// class afterward.
void ImageViewOnly::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        changeInteractMode(IModeNone);
        event->accept();
    }

    if (m_current_mode == IModeNone) {
        switch (event->key()) {
        case Qt::Key_Delete:
            event->accept();
            break;

        case Qt::Key_R:
            this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
            event->accept();
            break;

        default:
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

/// Releasing Ctrl while in IModePan clears the last pan point, returns to
/// IModeNone, and accepts the event; always forwards to the base class afterward.
void ImageViewOnly::keyReleaseEvent(QKeyEvent *event) {
    if ((event->key() == Qt::Key_Control) && (m_current_mode == IModePan)) {
        m_last_pan_point = QPoint();
        // this->setCursor(Qt::ArrowCursor);
        changeInteractMode(IModeNone);
        event->accept();
        // return;
    }

    QGraphicsView::keyReleaseEvent(event);
}

/// If `mode` differs from m_current_mode, saves m_current_mode into
/// m_previous_mode, adopts `mode`, refreshes the cursor, and updates
/// m_scene_interacting; no-op if `mode` is already current.
void ImageViewOnly::changeInteractMode(InteractMode mode) {
    if (mode != m_current_mode) {
        m_previous_mode = m_current_mode;
        m_current_mode = mode;
        changeCursor();
        m_scene_interacting = (m_current_mode != IModeNone) ? true : false;
    }
}

/// Swaps m_current_mode and m_previous_mode back to what they were before
/// the last changeInteractMode() call, then refreshes the cursor and
/// m_scene_interacting; no-op if the two modes are already equal.
void ImageViewOnly::backToPreviousMode() {
    if (m_previous_mode == m_current_mode) {
        return;
    }

    InteractMode temp_mode = m_current_mode;
    m_current_mode = m_previous_mode;
    m_previous_mode = temp_mode;
    changeCursor();
    m_scene_interacting = (m_current_mode != IModeNone) ? true : false;
}

/// Applies the cursor matching m_current_mode: arrow for IModeNone, closed
/// hand for IModePan; IModeZoom currently leaves the cursor unchanged.
void ImageViewOnly::changeCursor() {
    switch (m_current_mode) {
    case IModeNone:
        this->setCursor(Qt::ArrowCursor);
        break;
    case IModeZoom:
        // this->setCursor(Qt::SizeHorCursor);
        break;
    case IModePan:
        this->setCursor(Qt::ClosedHandCursor);
        break;
    }
}

/// Maps `mode` to its display name; returns "Unknown" for any value not
/// handled by the switch.
QString ImageViewOnly::interactMode2String(InteractMode mode) {
    switch (mode) {
    case IModeNone:
        return "None";
    case IModeZoom:
        return "Zoom";
    case IModePan:
        return "Pan";
    }
    return "Unknown";
}

/// Placeholder: currently always returns false (event not consumed).
bool ImageViewOnly::rightMouseButtonPressed(QMouseEvent *event) {
    return false;
}

/// When Ctrl is held, resets m_has_panned, records the press position as
/// m_last_pan_point, and switches to IModePan; always returns false so the
/// base class still processes the press.
bool ImageViewOnly::leftMouseButtonPressed(QMouseEvent *event) {
    if ((event->modifiers() & Qt::ControlModifier)) {
        m_has_panned = false;
        m_last_pan_point = event->pos();
        changeInteractMode(IModePan);
        return false;
    }

    return false;
}

/// Placeholder: currently always returns false (event not consumed).
bool ImageViewOnly::rightMouseButtonReleased(QMouseEvent *event) {
    // showRightMouseClickMenu(event);
    return false;
}

/// In IModePan, clears m_last_pan_point and reverts to the previous mode via
/// backToPreviousMode(), returning true if a real pan occurred
/// (m_has_panned) or false if it was just a click; other modes fall through
/// to the trailing `return false`.
bool ImageViewOnly::leftMouseButtonReleased(QMouseEvent *event) {
    switch (m_current_mode) {
    case IModeNone:
    {

    }
    break;
    case IModeZoom:

        break;
    case IModePan:
    {
        m_last_pan_point = QPoint();
        backToPreviousMode();
        if (m_has_panned) {
            return true;
        } else {
            return false;
        }
    }
    break;
    }

    return false;
}

