#include "image_view_only.h"

#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include "item_roi.h"

// Convert cv::Mat to QImage
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

void ImageViewOnly::setEnabelPixelModel(bool enable) {
    m_is_pixel_model = enable;
    this->setRenderHint(QPainter::SmoothPixmapTransform, m_is_pixel_model);
}

QPixmap ImageViewOnly::getCurrentImage() {
    if (m_pixmap_item == nullptr) {
        return QPixmap();
    }
    return m_pixmap_item->pixmap();
}

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

void ImageViewOnly::removeAllROI() {
    for (int idx=0;idx<m_roi_items.size();idx++) {
        m_scene->removeItem(m_roi_items.at(idx));
    }
    m_roi_items.clear();
}

void ImageViewOnly::loadImageFromPath(QString &path, bool fitsize) {
    QImage image(path);
    if (image.isNull()) {
        qDebug() << "Image Interact widget: cannot read image at - " << path;
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    this->loadImage(pixmap, fitsize);
}

void ImageViewOnly::loadImageOpenCv(cv::Mat &image, bool fitsize) {
    if (image.empty()) {
        return;
    }
    QPixmap pixmap = cvMatToQPixmap(image);
    this->loadImage(pixmap, fitsize);
}

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

void ImageViewOnly::clearCurrentImage() {
    if (m_pixmap_item != nullptr) {
        m_scene->removeItem(m_pixmap_item);
        delete m_pixmap_item;
        m_pixmap_item = nullptr;
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
    }
}

bool ImageViewOnly::hadImage() {
    return (m_pixmap_item != nullptr);
}

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

void ImageViewOnly::mouseDoubleClickEvent(QMouseEvent *event) {
    if ((m_current_mode == IModeNone) && event->button() == Qt::MiddleButton){
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
        event->accept();
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

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

void ImageViewOnly::changeInteractMode(InteractMode mode) {
    if (mode != m_current_mode) {
        m_previous_mode = m_current_mode;
        m_current_mode = mode;
        changeCursor();
        m_scene_interacting = (m_current_mode != IModeNone) ? true : false;
    }
}

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

bool ImageViewOnly::rightMouseButtonPressed(QMouseEvent *event) {
    return false;
}

bool ImageViewOnly::leftMouseButtonPressed(QMouseEvent *event) {
    if ((event->modifiers() & Qt::ControlModifier)) {
        m_has_panned = false;
        m_last_pan_point = event->pos();
        changeInteractMode(IModePan);
        return false;
    }

    return false;
}

bool ImageViewOnly::rightMouseButtonReleased(QMouseEvent *event) {
    // showRightMouseClickMenu(event);
    return false;
}

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

