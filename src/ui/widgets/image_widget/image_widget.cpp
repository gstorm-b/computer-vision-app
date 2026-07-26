#include "image_widget.h"

#include <QFileDialog>
#include <QMenu>

#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QScrollBar>

/// Debug-only logging helper: when ENABLE_DEBUG_MODE is defined, expands to `qDebug() << msg`;
/// otherwise expands to nothing so the call is compiled out entirely.
#ifdef ENABLE_DEBUG_MODE
#define PRINT_DEBUG_INFO(msg)　qDebug() << msg
#else
#define PRINT_DEBUG_INFO(msg) //
#endif

/// Constructs the image view: creates the backing QGraphicsScene, initializes all ROI/pan/mode
/// state to its defaults, sets the background brush, configures the viewport to fully repaint on
/// every update (avoids ghosting), disables smooth pixmap transform (avoids blur when zoomed in),
/// and builds the right-click context menus via init_mouse_menu().
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

/// Destructor. Currently a no-op: explicit deletion of m_pixmapItem is left commented out since
/// it is owned by m_scene and is cleaned up when the scene is destroyed.
ImageWidget::~ImageWidget() {
  // delete m_pixmapItem;
}

/// Assigns the QSettings instance used to persist things like the last-used file directory. If
/// the widget currently owns an internally-created QSettings (not previously supplied by a
/// caller), that instance is deleted before the new one is installed; ownership of `setting`
/// itself is then considered the caller's (see m_using_user_setting).
/// @param setting settings instance to adopt; a null pointer is ignored (no-op)
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

/// Clears the reference to a caller-supplied QSettings without deleting it (ownership stays with
/// the caller). No-op if the widget isn't currently using an externally supplied QSettings.
void ImageWidget::removeSettings() {
  if ((m_using_user_setting) && (this->m_setting != nullptr)){
    this->m_setting = nullptr;
    m_using_user_setting = false;
  }
}

/// Returns the pixmap currently displayed by the view.
/// @note Dereferences m_pixmapItem without a null check; calling this before any image has been
/// loaded (or after removeImage()) is undefined behavior.
QPixmap ImageWidget::getImage() {
  return m_pixmapItem->pixmap();
}

/// Returns the sub-image of the current pixmap covered by `roi`'s rectangle, aligned to whole
/// pixels and clipped to the pixmap bounds.
/// @param roi region of interest to crop; may be nullptr
/// @return a copy of the cropped pixmap, or a null QPixmap if roi is null or the aligned rect
/// doesn't overlap the image
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

/// Enables or disables showing the right-click context menu on right-mouse-button release.
void ImageWidget::setEnableMouseMenu(bool enable) {
  m_using_mouse_menu = enable;
}

/// Returns whether the right-click context menu is currently enabled.
const bool ImageWidget::isUseMouseMenu() {
  return m_using_mouse_menu;
}

/// Loads an image file from disk into the view: reads it via QImage, creates or updates the
/// pixmap item, expands the scene rect around it, and fits it into the view preserving aspect
/// ratio. Logs and returns without changing state if the file fails to load.
/// @param filePath path to the image file to load
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

/// Loads an in-memory pixmap into the view (e.g. a frame already decoded elsewhere), creating or
/// updating the pixmap item, expanding the scene rect around it, and fitting it into the view
/// preserving aspect ratio.
/// @param pixmap pixmap to display
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

/// Removes the currently displayed pixmap item from the scene and deletes it, then re-fits the
/// (now empty) view. No-op, with a debug log, if no image is currently loaded.
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

/// Begins interactive ROI drawing: clears the current scene selection and switches to
/// IModeDrawing, remembering `roi_type` for when the drag finishes (see draw_endROI). Only takes
/// effect if the widget isn't already in some other interaction mode.
/// @param roi_type the ROI shape to create once drawing completes
void ImageWidget::startDrawROI(ImageWidget::ItemAddType roi_type) {
  if (m_current_mode == IModeNone) {
    // this->setCursor(Qt::CrossCursor);
    m_scene->clearSelection();
    changeInteractMode(IModeDrawing);
    m_draw_roi_type = roi_type;
  }
}

/// Removes and deletes every currently-selected item in the scene (e.g. selected ROIs). No-op if
/// nothing is selected.
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

/// Handles right/left mouse button presses by delegating to rightMouseButtonPressed() /
/// leftMouseButtonPressed(); forwards the event to the base QGraphicsView implementation unless
/// one of those handlers reports it fully handled the event.
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

/// Drives per-mode mouse-move behavior: while panning (IModePan), scrolls the view by the pointer
/// delta since the last move; while drawing (IModeDrawing), updates the in-progress ROI rectangle
/// via draw_updateROI() and returns without forwarding to the base class. Other modes fall
/// through to the base QGraphicsView handling.
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

/// Handles right/left mouse button releases by delegating to rightMouseButtonReleased() /
/// leftMouseButtonReleased(); forwards the event to the base class unless one of those handlers
/// reports it fully handled the event.
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

/// On a middle-button double-click while not otherwise interacting (IModeNone), re-fits the
/// image into the view (resetting zoom/pan). Always forwards the event to the base class
/// afterward.
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

/// While Ctrl is held, treats the wheel as a zoom gesture: temporarily switches to IModeZoom,
/// scales the view by a fixed factor (1.15 to zoom in, 0.85 to zoom out) based on wheel
/// direction, then restores the previous interaction mode and swallows the event. Otherwise
/// forwards to the base QGraphicsView wheel handling.
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

/// On Escape, cancels any in-progress ROI drawing (draw_cancelROI()) and returns to IModeNone.
/// While not interacting (IModeNone), Delete removes selected items via deletedSelectedItems().
/// Every event is forwarded to the base class afterward regardless of handling above.
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

/// Releasing Ctrl while panning (IModePan) clears the last pan point and returns to IModeNone.
/// Always forwards the event to the base class afterward.
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

/// Switches the current interaction mode to `mode` (no-op if already in that mode), remembering
/// the previous mode, updating the cursor, and recomputing m_scene_interacting (true whenever the
/// new mode is not IModeNone).
/// @param mode the interaction mode to switch to
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

/// Swaps back to the previously active interaction mode (no-op if it equals the current mode),
/// updating the cursor and m_scene_interacting accordingly.
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

/// Sets the view's cursor to match the current interaction mode: arrow for IModeNone, closed hand
/// for IModePan, cross for IModeDrawing; IModeZoom leaves the cursor unchanged.
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

/// Creates the context-menu QActions and builds the three right-click QMenu variants (no image,
/// image loaded, ROI selected) used by showRightMouseClickMenu().
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

/// Creates the QGraphicsPixmapItem for `pixmap`, assigns it to m_pixmapItem, and adds it to the
/// scene.
/// @param pixmap the pixmap to wrap and add to the scene
void ImageWidget::createPixmapItem(QPixmap &pixmap) {
  // m_pixmapItem = new PixmapBoundingLine(pixmap);
  m_pixmapItem = new QGraphicsPixmapItem(pixmap);
  // m_pixmapItem->setBorderLineWidth(4);
  // m_pixmapItem->setBorderColor(QColor(Qt::green));
  m_scene->addItem(m_pixmapItem);
}

/// Returns whether an image (pixmap item) is currently loaded.
bool ImageWidget::hadImage() {
  return (m_pixmapItem != nullptr);
}

/// Deletes every ROI item (ItemRoi or ItemRoiRotated) currently in the scene. No-op if the scene
/// doesn't exist.
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

/// Removes `item` from the scene. Does not delete it; the caller retains ownership.
/// @param item item to remove from the scene
void ImageWidget::removeRoi(QGraphicsItem *item) {
    m_scene->removeItem(item);
}

/// Creates and adds a new ROI graphics item to the scene, anchored to the current pixmap item.
/// @param rtype ROI shape to create: NormalROI builds an ItemRoi from `rect`; RotatedROI builds
/// an ItemRoiRotated from m_temp_roi's current rect instead (the `rect` parameter is unused in
/// that branch); any other value (e.g. PickingPosition) is a no-op
/// @param rect rectangle to use for the NormalROI case
/// @note the newly created ROI is discarded if smaller than 10x10; otherwise it is kept and
/// reported via signal_new_roi_added(). No-op if no pixmap is loaded or the widget has no scene.
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

/// Returns the pixmap item currently displayed by the view (nullptr if no image is loaded).
QGraphicsPixmapItem* ImageWidget::getPixmapItem() {
  return m_pixmapItem;
}

/// Re-fits the current pixmap into the view, preserving aspect ratio, and refreshes the cached
/// bounding rect used by other fit/reset operations. No-op if no image is loaded.
void ImageWidget::fitImageView() {
    if (m_pixmapItem == nullptr) {
        return;
    }
    m_pixmap_bounding_rect = m_pixmapItem->boundingRect();
    this->fitInView(m_pixmap_bounding_rect, Qt::KeepAspectRatio);
}

/// Returns a human-readable name for `mode`, used for debug logging.
/// @param mode the interaction mode to describe
/// @return "None"/"Zoom"/"Pan"/"Drawing" for known modes, "Unknown" otherwise
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

/// Right-button press is not specially handled here; right-click actions are dispatched on
/// release instead (see rightMouseButtonReleased()).
/// @return always false
bool ImageWidget::rightMouseButtonPressed(QMouseEvent *event) {
  return false;
}

/// Handles a left mouse-button press depending on modifiers and current mode: Ctrl+click starts
/// panning (unless an ROI draw is already in progress); while in drawing mode with no modifiers,
/// marks the first ROI corner (draw_startROI()) or completes the ROI (draw_endROI()) depending on
/// whether drawing has already started.
/// @return true if drawing fully consumed the click (draw_startROI()/draw_endROI() result);
/// false otherwise, including the Ctrl+pan case (which still updates view state but lets the
/// base view also process the event)
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

/// Shows the right-click context menu (showRightMouseClickMenu()) if it is enabled
/// (m_using_mouse_menu); otherwise no-op.
/// @return always false
bool ImageWidget::rightMouseButtonReleased(QMouseEvent *event) {
  if (m_using_mouse_menu) {
    showRightMouseClickMenu(event);
  }
  return false;
}

/// Handles left-button release per current mode: for IModePan, clears the pan point, restores
/// the previous interaction mode, and reports whether an actual pan occurred; other modes take no
/// action here.
/// @return true if a pan just completed with actual movement (m_has_panned); false otherwise
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

/// Builds and executes the appropriate right-click context menu (ROI-selected / image-loaded /
/// no-image variant) at the event's global position, then dispatches whichever action the user
/// picked: add ROI, delete selected ROIs, save the selected ROI's cropped image to disk
/// (prompting for a save path and remembering the chosen directory), load a new image, remove the
/// current image, or reset the view transform.
/// @param event the mouse event that triggered the menu (used for its screen position)
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

/// Prompts the user with a file-open dialog (defaulting to the last used directory, filtered to
/// common image types) and, if a file is chosen, remembers its directory in QSettings and loads
/// it via loadImage(). Lazily creates an internal QSettings("DGB", "Image widget") if none has
/// been set yet.
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

/// Marks the first corner of a new ROI being drawn at the event's scene position and creates the
/// temporary dashed-outline rectangle item used while dragging.
/// @return true if this press started a new ROI drag; false if the click was outside the current
/// pixmap or a drag was already in progress
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

/// Finishes the in-progress ROI drag at the event's scene position: if the release is at the
/// same point as the start (no actual drag), the temporary rectangle is discarded; otherwise a
/// real ItemRoi or ItemRoiRotated (matching m_draw_roi_type) is created from the temporary
/// rectangle, discarded if smaller than 10x10, and otherwise kept and reported via
/// signal_draw_roi_finished(). Leaves interaction mode at IModeNone on completion.
/// @return false if the release point is outside the pixmap (drag left unresolved); true
/// otherwise, including when the resulting ROI was rejected for being too small
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

/// Updates the temporary ROI rectangle's extent to span from the drag start point to the event's
/// current scene position (normalized so width/height stay positive). No-op if the pointer is
/// outside the pixmap.
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

/// Cancels an in-progress ROI drag: stops mouse tracking and removes/deletes the temporary
/// rectangle item.
void ImageWidget::draw_cancelROI() {
  m_roi_started = false;
  setMouseTracking(false);
  this->scene()->removeItem(m_temp_roi);
  delete m_temp_roi;
  m_temp_roi = nullptr;
}
