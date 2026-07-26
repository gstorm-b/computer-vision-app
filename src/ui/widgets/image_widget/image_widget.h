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


/// QGraphicsView-based image viewer that shows a loaded pixmap and lets the user
/// pan/zoom it and draw/manage ROI (region of interest) and picking-position
/// overlay items on top of it, optionally via a right-click context menu.
class ImageWidget : public QGraphicsView {
  Q_OBJECT

public:
  /// Current mouse/view interaction mode of the widget.
  enum InteractMode {
    IModeNone,    ///< No special interaction; default view/selection behavior.
    IModeZoom,    ///< Ctrl+wheel zoom in progress.
    IModePan,     ///< Ctrl+left-drag panning in progress.
    IModeDrawing  ///< A new ROI/picking-position item is being drawn.
  };

  /// Kind of overlay item being added to the scene.
  enum ItemAddType {
    NormalROI,      ///< Axis-aligned rectangular ROI (ItemRoi).
    RotatedROI,      ///< Rotatable rectangular ROI (ItemRoiRotated).
    PickingPosition  ///< Picking-position marker item.
  };

  explicit ImageWidget(QWidget *parent = nullptr);
  ~ImageWidget();

  /// Installs the QSettings instance used to persist the last save/open
  /// directory. Frees the previously auto-created settings object first,
  /// unless the widget was already using a caller-supplied one.
  void setSettings(QSettings *setting);
  /// Clears the caller-supplied settings pointer set via setSettings(), if any.
  void removeSettings();
  /// Returns the currently loaded pixmap (null pixmap if none is loaded).
  QPixmap getImage();
  /// Returns the region of the loaded image covered by `roi`, cropped and
  /// clipped to the image bounds.
  /// @return an empty QPixmap if `roi` is null or its area does not intersect the image
  QPixmap getCroppedFromRoi(ItemRoi *roi);
  /// Enables or disables showing the right-click context menu on right-mouse release.
  void setEnableMouseMenu(bool enable);
  /// Returns whether the right-click context menu is currently enabled.
  const bool isUseMouseMenu();
  /// Opens a file-open dialog (remembering the last used directory) and loads
  /// the chosen image file into the view.
  void showChooseImageDialog();
  /// Returns true if an image pixmap is currently loaded into the scene.
  bool hadImage();
  /// Deletes every ItemRoi/ItemRoiRotated currently in the scene.
  void removeAllRoi();
  /// Removes `item` from the scene (does not delete it).
  void removeRoi(QGraphicsItem *item);
  /// Creates a new ROI item of type `rtype` at `rect`, parented to the pixmap
  /// item, and emits signal_new_roi_added(). Rejected (and deleted) if the
  /// resulting ROI would be smaller than 10x10, or if no image is loaded.
  void addRoi(ImageWidget::ItemAddType rtype, QRectF rect);
  /// Returns the QGraphicsPixmapItem holding the currently loaded image (nullptr if none).
  QGraphicsPixmapItem* getPixmapItem();
  /// Refits the view on the current pixmap bounds, preserving aspect ratio.
  void fitImageView();

public slots:
  /// Loads the image at `filePath` into the scene, replacing any existing
  /// pixmap, and fits the view to it. No-op (with debug log) if the file
  /// fails to load.
  void loadImage(const QString &filePath);
  /// Loads `pixmap` into the scene, replacing any existing pixmap, and fits
  /// the view to it.
  void loadImage(QPixmap &pixmap);
  /// Removes and deletes the currently loaded pixmap item, then refits the view.
  void removeImage();
  /// Enters drawing mode for a new item of `roi_type`, provided no drawing is
  /// already in progress.
  void startDrawROI(ItemAddType roi_type);
  /// Removes and deletes every currently selected item in the scene.
  void deletedSelectedItems();

signals:
  /// Emitted when a new ROI/picking-position item of `type` is added via addRoi().
  void signal_new_roi_added(QGraphicsItem *roi, ItemAddType type);
  /// Emitted when an interactively drawn ROI/picking-position item of `type`
  /// finishes being drawn by the user (see draw_endROI()).
  void signal_draw_roi_finished(QGraphicsItem *roi, ItemAddType type);

protected:
  /// Dispatches right/left button presses to the button-specific handlers
  /// before falling back to QGraphicsView::mousePressEvent().
  void mousePressEvent(QMouseEvent *event) override;
  /// Updates panning (IModePan) or forwards to draw_updateROI() while drawing
  /// (IModeDrawing); otherwise defers to QGraphicsView::mouseMoveEvent().
  void mouseMoveEvent(QMouseEvent *event) override;
  /// Dispatches right/left button releases to the button-specific handlers
  /// before falling back to QGraphicsView::mouseReleaseEvent().
  void mouseReleaseEvent(QMouseEvent *event) override;
  /// Middle-button double-click refits the view to the image when not
  /// currently interacting; otherwise defers to the base class.
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  /// Ctrl+wheel zooms the view in/out around the current position; plain
  /// wheel events are passed through to QGraphicsView::wheelEvent().
  void wheelEvent(QWheelEvent *event) override;
  /// Escape cancels an in-progress ROI draw and resets to IModeNone; Delete
  /// removes selected items when idle.
  void keyPressEvent(QKeyEvent *event) override;
  /// Releasing Ctrl while panning ends the pan and returns to IModeNone.
  void keyReleaseEvent(QKeyEvent *event) override;

private:
  /// Builds the QActions and the three right-click QMenu variants (no image,
  /// image loaded, ROI selected) used by showRightMouseClickMenu().
  void init_mouse_menu();
  /// Allocates m_pixmapItem for `pixmap` and adds it to the scene.
  void createPixmapItem(QPixmap &pixmap);

  /// Switches to `mode`, updating m_previous_mode, the cursor, and
  /// m_scene_interacting; no-op if already in `mode`.
  void changeInteractMode(InteractMode mode);
  /// Swaps back to the previously active interaction mode (no-op if it is the
  /// same as the current one).
  void backToPreviousMode();
  /// Sets the view cursor to match m_current_mode (arrow/closed-hand/cross).
  void changeCursor();
  /// Returns a human-readable name for `mode` (used for debug logging).
  QString interactMode2String(InteractMode mode);

  /// Right-button press handler; currently always returns false (event not consumed).
  bool rightMouseButtonPressed(QMouseEvent *event);
  /// Handles left-button press: starts panning under Ctrl, or delegates to
  /// draw_startROI()/draw_endROI() while in drawing mode.
  /// @return true if the event was fully handled and should not propagate further
  bool leftMouseButtonPressed(QMouseEvent *event);
  /// Right-button release handler; shows the context menu if enabled.
  /// @return true if the event was fully handled and should not propagate further
  bool rightMouseButtonReleased(QMouseEvent *event);
  /// Handles left-button release: ends panning (reporting whether an actual
  /// pan occurred) and restores the previous mode.
  /// @return true if the event was fully handled and should not propagate further
  bool leftMouseButtonReleased(QMouseEvent *event);

  /// Picks and executes the appropriate right-click QMenu (ROI-selected,
  /// image-loaded, or empty) at the event position, then performs the action
  /// the user selected (add/delete/save ROI, load/remove image, reset view).
  void showRightMouseClickMenu(QMouseEvent *event);

  /// Begins interactive ROI drawing at the clicked scene position, creating
  /// the temporary dashed-rectangle preview item.
  /// @return false if the click was outside the image or a draw was already in progress
  bool draw_startROI(QMouseEvent *event);
  /// Finishes interactive ROI drawing: discards a zero-size drag, otherwise
  /// creates the real ROI item from the temporary rectangle, emits
  /// signal_draw_roi_finished(), and returns to IModeNone.
  /// @return false if the click was outside the image
  bool draw_endROI(QMouseEvent *event);
  /// Updates the temporary preview rectangle's geometry to track the mouse
  /// while drawing.
  void draw_updateROI(QMouseEvent *event);
  /// Removes and deletes the temporary preview rectangle, aborting an
  /// in-progress ROI draw.
  void draw_cancelROI();

private:
  QGraphicsScene *m_scene;              ///< Scene owning the pixmap item and all ROI/overlay items.
  QGraphicsPixmapItem  *m_pixmapItem;    ///< Currently loaded image item, or nullptr if no image is loaded.
  QRectF m_pixmap_bounding_rect;         ///< Cached bounding rect of m_pixmapItem, used by fit/reset view calls.

  QSettings *m_setting;                 ///< Settings used to persist the last save/open directory.
  bool m_using_user_setting;            ///< True if m_setting was supplied via setSettings() rather than auto-created.

  bool m_using_mouse_menu;              ///< Whether the right-click context menu is enabled.

  InteractMode m_current_mode;          ///< Active interaction mode.
  InteractMode m_previous_mode;         ///< Mode to restore via backToPreviousMode().
  bool m_scene_interacting;             ///< True whenever m_current_mode is not IModeNone.

  bool m_has_panned;                    ///< Set once an actual pan movement occurs during IModePan.
  QPoint m_last_pan_point;              ///< Last mouse position seen while panning, in viewport coordinates.

  bool m_roi_started;                   ///< True while an interactive ROI draw is in progress.
  QGraphicsRectItem *m_temp_roi;        ///< Temporary dashed preview rectangle shown while drawing a ROI.
  QPointF m_roi_start_point;            ///< Scene-coordinate anchor point where the current ROI draw began.
  // ItemRoi *temp_editable_roi;
  ItemAddType m_draw_roi_type;           ///< Item type requested for the ROI currently being drawn.

  /// Right Click Menu
  QMenu *menu_right_mouse;       ///< Context menu shown when no image is loaded.
  QMenu *menu_right_mouse_full;  ///< Context menu shown when an image is loaded and nothing is selected.
  QMenu *menu_right_mouse_roi;   ///< Context menu shown when a ROI item is selected.
  QAction *action_add_roi;      ///< Menu action to start drawing a new ROI.
  QAction *action_delete_roi;   ///< Menu action to delete the selected ROI(s).
  QAction *action_save_roi;     ///< Menu action to crop and save the selected ROI's image region to disk.
  QAction *action_load_img;     ///< Menu action to open the image-choose dialog.
  QAction *action_remove_img;   ///< Menu action to remove the currently loaded image.
  QAction *action_reset_img;    ///< Menu action to refit the view to the loaded image.
};

#endif // IMAGE_WIDGET_H
