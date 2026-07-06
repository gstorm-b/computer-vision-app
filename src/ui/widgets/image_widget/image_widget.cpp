#include "image_widget.h"

#include <QFileDialog>
#include <QMenu>

#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QScrollBar>

#ifdef ENABLE_DEBUG_MODE
#define PRINT_DEBUG_INFO(msg)　qDebug() << msg
#else
#define PRINT_DEBUG_INFO(msg) //
#endif

ImageWidget::ImageWidget(QWidget *parent)
    : QGraphicsView(parent),
    m_scene(new QGraphicsScene(this)),
    m_pixmapItem(nullptr),
    m_setting(nullptr),
    m_using_user_setting(false),
    m_using_mouse_menu(false),
    m_current_mode(IModeNone),
    m_previous_mode(IModeNone),
    m_scene_interacting(false),
    m_has_panned(false),
    m_roi_started(false) {

  this->setScene(m_scene);

  QBrush brush(QColor(0x9c988f));
  this->setBackgroundBrush(brush);

  // set view port update mode to avoid ghosting
  this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  // off smooth pixmap transform to disable blur when zoom in
  this->setRenderHint(QPainter::SmoothPixmapTransform, false);

  init_mouse_menu();
}

ImageWidget::~ImageWidget() {
  // delete m_pixmapItem;
}

void ImageWidget::setSettings(QSettings *setting) {
  if (setting == nullptr) {
    return;
  }

  if ((!m_using_user_setting) && (this->m_setting != nullptr)){
    delete this->m_setting;
  }

  this->m_setting = setting;
  m_using_user_setting = true;
}

void ImageWidget::removeSettings() {
  if ((m_using_user_setting) && (this->m_setting != nullptr)){
    this->m_setting = nullptr;
    m_using_user_setting = false;
  }
}

QPixmap ImageWidget::getImage() {
  return m_pixmapItem->pixmap();
}

QPixmap ImageWidget::getCroppedFromRoi(ItemRoi *roi) {
  if (roi == nullptr) {
    return QPixmap();
  }

  const QPixmap &pixmap = m_pixmapItem->pixmap();

  QRectF save_rect = roi->getRoi();
  QRect roi_int = save_rect.toAlignedRect();
  roi_int = roi_int.intersected(pixmap.rect());
  if (roi_int.isEmpty()) {
    return QPixmap();;
  }

  // QPixmap cropped = pixmap.copy(roi_int);
  return pixmap.copy(roi_int);
}

void ImageWidget::setEnableMouseMenu(bool enable) {
  m_using_mouse_menu = enable;
}

const bool ImageWidget::isUseMouseMenu() {
  return m_using_mouse_menu;
}

void ImageWidget::loadImage(const QString &filePath) {
  QImage image(filePath);
  if (image.isNull()) {
    PRINT_DEBUG_INFO("Failed to load image:" << filePath);
    return;
  }

  QPixmap pixmap = QPixmap::fromImage(image);
  if (!m_pixmapItem) {
    createPixmapItem(pixmap);
    // using FastTransformation to avoid blur image
    m_pixmapItem->setTransformationMode(Qt::FastTransformation);
  } else {
    m_pixmapItem->setPixmap(pixmap);
  }

  m_scene->setSceneRect(pixmap.rect().adjusted(-15, -15, 10, 10));
  // fit image with current window size
  m_pixmap_bounding_rect = m_pixmapItem->boundingRect();
  this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
}

void ImageWidget::loadImage(QPixmap &pixmap) {
  if (!m_pixmapItem) {
    createPixmapItem(pixmap);
    // using FastTransformation to avoid blur image
    m_pixmapItem->setTransformationMode(Qt::FastTransformation);
  } else {
    m_pixmapItem->setPixmap(pixmap);
  }

  m_scene->setSceneRect(pixmap.rect().adjusted(-10, -10, 10, 10));
  // fit image with current window size
  m_pixmap_bounding_rect = m_pixmapItem->boundingRect();
  this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
}

void ImageWidget::removeImage() {
  if (!hadImage()) {
    PRINT_DEBUG_INFO("[IMG ROI Widget] Remove failed, image empty.");
    return;
  }

  m_scene->removeItem(m_pixmapItem);
  delete m_pixmapItem;
  m_pixmapItem = nullptr;
  this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
}

void ImageWidget::startDrawROI(ImageWidget::ItemAddType roi_type) {
  if (m_current_mode == IModeNone) {
    // this->setCursor(Qt::CrossCursor);
    m_scene->clearSelection();
    changeInteractMode(IModeDrawing);
    m_draw_roi_type = roi_type;
  }
}

void ImageWidget::deletedSelectedItems() {
  QList<QGraphicsItem*> selected_items = m_scene->selectedItems();
  if (selected_items.empty()) {
    return;
  }

  for (int idx=0;idx<selected_items.count();idx++) {
    QGraphicsItem *item = selected_items[idx];
    m_scene->removeItem(item);
    delete item;
  }
}

void ImageWidget::mousePressEvent(QMouseEvent *event) {
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

void ImageWidget::mouseMoveEvent(QMouseEvent *event) {
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
    case IModeDrawing:
      draw_updateROI(event);
      return;
      break;
  }

  QGraphicsView::mouseMoveEvent(event);
}

void ImageWidget::mouseReleaseEvent(QMouseEvent *event) {
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

void ImageWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  switch (event->button()) {
    case Qt::MiddleButton:
      if (m_current_mode == IModeNone) {
        this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
      }
      break;
    default:
      break;
  }

  QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageWidget::wheelEvent(QWheelEvent *event) {
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

void ImageWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    if (m_roi_started) {
      m_roi_started = false;
      draw_cancelROI();
    }
    changeInteractMode(IModeNone);
    event->accept();
  }

  if (m_current_mode == IModeNone) {
    switch (event->key()) {
      case Qt::Key_Delete:
        deletedSelectedItems();
        event->accept();
        break;

      case Qt::Key_A:
        // startDrawROI();
        // event->accept();
        break;

      default:
        break;
    }
  }

  QGraphicsView::keyPressEvent(event);
}

void ImageWidget::keyReleaseEvent(QKeyEvent *event) {
  if ((event->key() == Qt::Key_Control) && (m_current_mode == IModePan)) {
    m_last_pan_point = QPoint();
    // this->setCursor(Qt::ArrowCursor);
    changeInteractMode(IModeNone);
    event->accept();
    // return;
  }

  QGraphicsView::keyReleaseEvent(event);
}

void ImageWidget::changeInteractMode(InteractMode mode) {
  if (mode != m_current_mode) {
    m_previous_mode = m_current_mode;
    m_current_mode = mode;
    changeCursor();
    m_scene_interacting = (m_current_mode != IModeNone) ? true : false;

    // PRINT_DEBUG_INFO("[IMG ROI Widget] Chang mode:"
    //          << interactMode2String(m_previous_mode)
    //          << ">>"
    //          << interactMode2String(m_current_mode));
  }
}

void ImageWidget::backToPreviousMode() {
  if (m_previous_mode == m_current_mode) {
    return;
  }

  InteractMode temp_mode = m_current_mode;
  m_current_mode = m_previous_mode;
  m_previous_mode = temp_mode;
  changeCursor();
  m_scene_interacting = (m_current_mode != IModeNone) ? true : false;

  // PRINT_DEBUG_INFO("[IMG ROI Widget] Chang mode (back):"
  //          << interactMode2String(m_previous_mode)
  //          << ">>"
  //          << interactMode2String(m_current_mode));
}

void ImageWidget::changeCursor() {
  switch (m_current_mode) {
    case IModeNone:
      this->setCursor(Qt::ArrowCursor);
      break;
    case IModeZoom:

      break;
    case IModePan:
      this->setCursor(Qt::ClosedHandCursor);
      break;
    case IModeDrawing:
      this->setCursor(Qt::CrossCursor);
      break;
  }
}

void ImageWidget::init_mouse_menu() {
  action_add_roi = new QAction("Add ROI", this);
  action_delete_roi = new QAction("Delete selected ROIs", this);
  action_save_roi = new QAction("Save ROIs", this);
  action_load_img = new QAction("Load Image", this);
  action_remove_img = new QAction("Remove Image", this);
  action_reset_img = new QAction("Reset transform", this);

  menu_right_mouse = new QMenu();
  menu_right_mouse->addAction(action_load_img);

  menu_right_mouse_full = new QMenu();
  menu_right_mouse_full->addAction(action_add_roi);
  menu_right_mouse_full->addAction(action_load_img);
  menu_right_mouse_full->addAction(action_remove_img);
  menu_right_mouse_full->addAction(action_reset_img);

  menu_right_mouse_roi = new QMenu();
  menu_right_mouse_roi->addAction(action_save_roi);
  menu_right_mouse_roi->addAction(action_delete_roi);
}

void ImageWidget::createPixmapItem(QPixmap &pixmap) {
  // m_pixmapItem = new PixmapBoundingLine(pixmap);
  m_pixmapItem = new QGraphicsPixmapItem(pixmap);
  // m_pixmapItem->setBorderLineWidth(4);
  // m_pixmapItem->setBorderColor(QColor(Qt::green));
  m_scene->addItem(m_pixmapItem);
}

bool ImageWidget::hadImage() {
  return (m_pixmapItem != nullptr);
}

void ImageWidget::removeAllRoi() {
    if (!m_scene) return;

    // ROI items are parented to the pixmap item (not tracked in a list), so walk
    // the scene and delete every ROI item. items() returns a copy, so deleting
    // while iterating is safe; ~QGraphicsItem detaches it from scene + parent.
    const QList<QGraphicsItem *> items = m_scene->items();
    for (QGraphicsItem *item : items) {
        if (dynamic_cast<ItemRoi *>(item) || dynamic_cast<ItemRoiRotated *>(item))
            delete item;
    }
}

void ImageWidget::removeRoi(QGraphicsItem *item) {
    m_scene->removeItem(item);
}

void ImageWidget::addRoi(ImageWidget::ItemAddType rtype, QRectF rect) {
    if (this->m_pixmapItem == nullptr) {
        return;
    }

    if (this->scene()) {
        switch (rtype) {
        case ItemAddType::NormalROI:
        {
            ItemRoi *new_roi = new ItemRoi(rect,
                                           this->m_pixmapItem,
                                           &m_scene_interacting);
            // QPointF temp_pos = m_temp_roi->pos();
            if ((new_roi->rect().width() < 10) ||
                (new_roi->rect().height() < 10)) {
                this->scene()->removeItem(new_roi);
                delete new_roi;
                PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: failed, ROI to small.");
            } else {
                // new_roi->setPos(temp_pos);
                emit signal_new_roi_added(new_roi, m_draw_roi_type);
                PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: finished.");
            }
        }
        break;
        case ItemAddType::RotatedROI:
        {
            ItemRoiRotated *new_roi = new ItemRoiRotated(m_temp_roi->rect(),
                                                         this->m_pixmapItem,
                                                         &m_scene_interacting);
            // QPointF temp_pos = m_temp_roi->pos();
            // this->scene()->removeItem(m_temp_roi);
            // m_temp_roi = nullptr;

            if ((new_roi->rect().width() < 10) ||
                (new_roi->rect().height() < 10)) {
                this->scene()->removeItem(new_roi);
                delete new_roi;
                PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: failed, ROI to small.");
            } else {
                // new_roi->setPos(temp_pos);
                emit signal_new_roi_added(new_roi, m_draw_roi_type);
                PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: finished.");
            }
        }
        break;
        default:
            break;
        }
    }
}

QGraphicsPixmapItem* ImageWidget::getPixmapItem() {
  return m_pixmapItem;
}

void ImageWidget::fitImageView() {
    if (m_pixmapItem == nullptr) {
        return;
    }
    m_pixmap_bounding_rect = m_pixmapItem->boundingRect();
    this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
}

QString ImageWidget::interactMode2String(InteractMode mode) {
  switch (mode) {
    case IModeNone:
      return "None";
    case IModeZoom:
      return "Zoom";
    case IModePan:
      return "Pan";
    case IModeDrawing:
      return "Drawing";
  }
  return "Unknown";
}

bool ImageWidget::rightMouseButtonPressed(QMouseEvent *event) {
  return false;
}

bool ImageWidget::leftMouseButtonPressed(QMouseEvent *event) {
  if ((event->modifiers() & Qt::ControlModifier) && (!m_roi_started)) {
    m_has_panned = false;
    m_last_pan_point = event->pos();
    changeInteractMode(IModePan);
    return false;
  }

  if (m_current_mode == IModeDrawing) {
    if (event->modifiers() != Qt::NoModifier) {
      return false;
    }

    if (!m_roi_started) {
      return draw_startROI(event);
    } else {
      return draw_endROI(event);
    }
  }

  return false;
}

bool ImageWidget::rightMouseButtonReleased(QMouseEvent *event) {
  if (m_using_mouse_menu) {
    showRightMouseClickMenu(event);
  }
  return false;
}

bool ImageWidget::leftMouseButtonReleased(QMouseEvent *event) {
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
    case IModeDrawing:

      break;
    default:
      break;
  }

  return false;
}

void ImageWidget::showRightMouseClickMenu(QMouseEvent *event) {
  QMenu *showMenu = nullptr;

  if (!m_scene->selectedItems().empty()) {
    showMenu = menu_right_mouse_roi;
  } else {
    if (hadImage()) {
      showMenu = menu_right_mouse_full;
    } else {
      showMenu = menu_right_mouse;
    }
  }

  QAction *selectedAction = showMenu->exec(event->globalPosition().toPoint());

  if (selectedAction == action_add_roi) {
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose add new ROI from mouse event.");
    startDrawROI(ItemAddType::RotatedROI);

  } else if (selectedAction == action_delete_roi) {
    deletedSelectedItems();
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose delete selected ROIs from mouse event.");

  } else if (selectedAction == action_save_roi) {
    // deletedSelectedItems();
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose save selected ROIs from mouse event.");

    QPointF scene_pos = mapToScene(event->pos());
    QGraphicsItem *item = this->scene()->itemAt(scene_pos, QTransform());

    if (item) {
      ItemRoi *roi_item = ((ItemRoi*)item);
      if (m_scene->selectedItems().contains(item)) {
        const QPixmap &pixmap = m_pixmapItem->pixmap();

        QRectF save_rect = roi_item->getRoi();
        QRect roi = save_rect.toAlignedRect();
        roi = roi.intersected(pixmap.rect());
        if (roi.isEmpty())
          return;

        QPixmap cropped = pixmap.copy(roi);

        if (m_setting == nullptr) {
          m_setting = new QSettings("DGB", "Image widget");
        }

        QString lastDirectory = m_setting->value("lastDirectory", "").toString();

        QString filePath = QFileDialog::getSaveFileName(this,
                                                        "Save image",
                                                        lastDirectory,
                                                        "Image Files (*.png *.jpg *.jpeg *.bmp)");

        if (!filePath.isEmpty()) {
          QString directory = QFileInfo(filePath).absolutePath();
          m_setting->setValue("lastDirectory", directory);

          cropped.save(filePath, "bmp");
        }
      }
    }

  } else if (selectedAction == action_load_img) {
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose load image from mouse event.");
    showChooseImageDialog();

  } else if (selectedAction == action_remove_img) {
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose remove image from mouse event.");
    this->removeImage();

  } else if (selectedAction == action_reset_img) {
    PRINT_DEBUG_INFO("[IMG ROI Widget] User choose reset image transform from mouse event.");

    if (!hadImage()) {
      PRINT_DEBUG_INFO("[IMG ROI Widget] Rest transform fail, image empty.");
      return;
    }
    this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
  }
}

void ImageWidget::showChooseImageDialog() {
  if (m_setting == nullptr) {
    m_setting = new QSettings("DGB", "Image widget");
  }

  QString lastDirectory = m_setting->value("lastDirectory", "").toString();

  QString filePath = QFileDialog::getOpenFileName(this,
                                                  "Choose image",
                                                  lastDirectory,
                                                  "Image Files (*.png *.jpg *.jpeg *.bmp)");

  if (!filePath.isEmpty()) {
    QString directory = QFileInfo(filePath).absolutePath();
    m_setting->setValue("lastDirectory", directory);

    loadImage(filePath);
  }
}

bool ImageWidget::draw_startROI(QMouseEvent *event) {
  if (!m_pixmapItem->contains(mapToScene(event->pos()))) {
    return false;
  }

  if (!m_roi_started) {
    m_roi_started = true;
    setMouseTracking(true);

    PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: first point marked.");

    m_roi_start_point = mapToScene(event->pos());
    m_temp_roi = new QGraphicsRectItem();
    m_temp_roi->setPen(QPen(Qt::red, 2, Qt::DashLine));
    m_temp_roi->setBrush(QBrush(Qt::transparent));
    m_temp_roi->setRect(QRectF(m_roi_start_point, QSizeF(0, 0)));

    if (this->scene()) {
      this->scene()->addItem(m_temp_roi);
    }
    return true;
  }
  return false;
}

bool ImageWidget::draw_endROI(QMouseEvent *event) {
  if (!m_pixmapItem->contains(mapToScene(event->pos()))) {
    return false;
  }

  m_roi_started = false;
  setMouseTracking(false);

  if (this->mapToScene(event->pos()) == m_roi_start_point) {
    this->scene()->removeItem(m_temp_roi);
    delete m_temp_roi;
    m_temp_roi = nullptr;
    return true;
  }


  if (this->scene()) {
    switch (m_draw_roi_type) {
      case ItemAddType::NormalROI:
      {
        ItemRoi *new_roi = new ItemRoi(m_temp_roi->rect(),
                                       this->m_pixmapItem,
                                       &m_scene_interacting);
        QPointF temp_pos = m_temp_roi->pos();
        this->scene()->removeItem(m_temp_roi);
        delete m_temp_roi;
        m_temp_roi = nullptr;

        if ((new_roi->rect().width() < 10) ||
            (new_roi->rect().height() < 10)) {
          this->scene()->removeItem(new_roi);
          delete new_roi;
          PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: failed, ROI to small.");
        } else {
          new_roi->setPos(temp_pos);
          emit signal_draw_roi_finished(new_roi, m_draw_roi_type);
          PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: finished.");
        }
      }
        break;
      case ItemAddType::RotatedROI:
      {
        ItemRoiRotated *new_roi = new ItemRoiRotated(m_temp_roi->rect(),
                                                     this->m_pixmapItem,
                                                     &m_scene_interacting);
        QPointF temp_pos = m_temp_roi->pos();
        this->scene()->removeItem(m_temp_roi);
        delete m_temp_roi;
        m_temp_roi = nullptr;

        if ((new_roi->rect().width() < 10) ||
            (new_roi->rect().height() < 10)) {
          this->scene()->removeItem(new_roi);
          delete new_roi;
          PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: failed, ROI to small.");
        } else {
          new_roi->setPos(temp_pos);
          emit signal_draw_roi_finished(new_roi, m_draw_roi_type);
          PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: finished.");
        }
      }
        break;
      default:
        break;
    }


  }

  changeInteractMode(IModeNone);
  return true;
}

void ImageWidget::draw_updateROI(QMouseEvent *event) {
  if (!m_pixmapItem->contains(mapToScene(event->pos()))) {
    return;
  }

  QPointF currentPos = mapToScene(event->pos());
  QRectF newRect(m_roi_start_point, currentPos);

  newRect = newRect.normalized();
  m_temp_roi->setRect(newRect);
  PRINT_DEBUG_INFO("[IMG ROI Widget] Add new ROI: change position." << newRect);
}

void ImageWidget::draw_cancelROI() {
  m_roi_started = false;
  setMouseTracking(false);
  this->scene()->removeItem(m_temp_roi);
  delete m_temp_roi;
  m_temp_roi = nullptr;
}
