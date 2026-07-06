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
class ImageViewOnly : public QGraphicsView {
    Q_OBJECT
public:
    explicit ImageViewOnly(QWidget *parent = nullptr);

    void setEnabelPixelModel(bool enable);
    QPixmap getCurrentImage();

    void addROI(int tl_x, int tl_y, int br_x, int br_y, QColor border_color);
    void removeAllROI();
    void loadImageFromPath(QString &path, bool fitsize = true);
    void loadImageOpenCv(cv::Mat &image, bool fitsize = false);
    void loadImage(QPixmap &pixmap, bool fitsize= false);
    void clearCurrentImage();
    bool hadImage();

protected:
    enum InteractMode {
        IModeNone,
        IModeZoom,
        IModePan
    };

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void changeInteractMode(InteractMode mode);
    void backToPreviousMode();
    void changeCursor();
    QString interactMode2String(InteractMode mode);

    bool rightMouseButtonPressed(QMouseEvent *event);
    bool leftMouseButtonPressed(QMouseEvent *event);
    bool rightMouseButtonReleased(QMouseEvent *event);
    bool leftMouseButtonReleased(QMouseEvent *event);

private:
    QGraphicsScene *m_scene{nullptr};
    QGraphicsPixmapItem *m_pixmap_item{nullptr};
    QRectF m_pixmap_bounding_rect;

    bool m_first_time_image_set{false};
    bool m_is_pixel_model{true};

    InteractMode m_current_mode;
    InteractMode m_previous_mode;
    bool m_scene_interacting;

    bool m_has_panned;
    QPoint m_last_pan_point;

    QList<QGraphicsRectItem*> m_roi_items;
};

#endif // IMAGE_VIEW_ONLY_H
