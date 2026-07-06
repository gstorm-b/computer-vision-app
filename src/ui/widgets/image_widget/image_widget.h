#ifndef IMAGE_WIDGET_H
#define IMAGE_WIDGET_H

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>

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

#include "ui/widgets/image_widget/item_pixmap_bounding.h"
#include "ui/widgets/image_widget/item_roi.h"
#include "ui/widgets/image_widget/item_roi_rotated.h"


class ImageWidget : public QGraphicsView {
  Q_OBJECT

public:
  enum InteractMode {
    IModeNone,
    IModeZoom,
    IModePan,
    IModeDrawing
  };

  enum ItemAddType {
    NormalROI,
    RotatedROI,
    PickingPosition
  };

  explicit ImageWidget(QWidget *parent = nullptr);
  ~ImageWidget();
  void setSettings(QSettings *setting);
  void removeSettings();
  QPixmap getImage();
  QPixmap getCroppedFromRoi(ItemRoi *roi);
  void setEnableMouseMenu(bool enable);
  const bool isUseMouseMenu();
  void showChooseImageDialog();
  bool hadImage();
  void removeAllRoi();
  void removeRoi(QGraphicsItem *item);
  void addRoi(ImageWidget::ItemAddType rtype, QRectF rect);
  QGraphicsPixmapItem* getPixmapItem();
  void fitImageView();

public slots:
  void loadImage(const QString &filePath);
  void loadImage(QPixmap &pixmap);
  void removeImage();
  void startDrawROI(ItemAddType roi_type);
  void deletedSelectedItems();

signals:
  void signal_new_roi_added(QGraphicsItem *roi, ItemAddType type);
  void signal_draw_roi_finished(QGraphicsItem *roi, ItemAddType type);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;

private:
  void init_mouse_menu();
  void createPixmapItem(QPixmap &pixmap);

  void changeInteractMode(InteractMode mode);
  void backToPreviousMode();
  void changeCursor();
  QString interactMode2String(InteractMode mode);

  bool rightMouseButtonPressed(QMouseEvent *event);
  bool leftMouseButtonPressed(QMouseEvent *event);
  bool rightMouseButtonReleased(QMouseEvent *event);
  bool leftMouseButtonReleased(QMouseEvent *event);

  void showRightMouseClickMenu(QMouseEvent *event);

  bool draw_startROI(QMouseEvent *event);
  bool draw_endROI(QMouseEvent *event);
  void draw_updateROI(QMouseEvent *event);
  void draw_cancelROI();

private:
  QGraphicsScene *m_scene;
  QGraphicsPixmapItem  *m_pixmapItem;
  QRectF m_pixmap_bounding_rect;

  QSettings *m_setting;
  bool m_using_user_setting;

  bool m_using_mouse_menu;

  InteractMode m_current_mode;
  InteractMode m_previous_mode;
  bool m_scene_interacting;

  bool m_has_panned;
  QPoint m_last_pan_point;

  bool m_roi_started;
  QGraphicsRectItem *m_temp_roi;
  QPointF m_roi_start_point;
  // ItemRoi *temp_editable_roi;
  ItemAddType m_draw_roi_type;

  /// Right Click Menu
  QMenu *menu_right_mouse;
  QMenu *menu_right_mouse_full;
  QMenu *menu_right_mouse_roi;
  QAction *action_add_roi;
  QAction *action_delete_roi;
  QAction *action_save_roi;
  QAction *action_load_img;
  QAction *action_remove_img;
  QAction *action_reset_img;
};

#endif // IMAGE_WIDGET_H
