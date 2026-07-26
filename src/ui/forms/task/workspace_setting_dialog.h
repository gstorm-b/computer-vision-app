#ifndef WORKSPACE_SETTING_DIALOG_H
#define WORKSPACE_SETTING_DIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QRectF>
#include <QString>

#include "ui/widgets/image_widget/image_widget.h"
#include "ui/widgets/image_widget/item_roi.h"

namespace Ui {
/// Qt Designer-generated form for WorkspaceSettingDialog (buttons + host stack widget).
class WorkspaceSettingDialog;
}

/// Dialog for defining up to two axis-aligned ROIs on one reference image: the working ROI
/// (crop region, drawn green) and the condition ROI (task-dependent region, drawn orange).
/// Both are optional and live in the same image's pixel coordinates.
///
/// The image comes from a file ("Choose image") or a camera grab ("Grab", which the owner
/// serves by handling requestGrab() and calling setMainViewImage()). On accept, resultImage()
/// holds the image the ROIs were drawn on, resultRoi() holds the working ROI and
/// resultConditionRoi() the condition ROI (each empty when not drawn), all in image-pixel
/// coordinates.
///
/// @note Mirrors AddPatternImageDialog: the ImageWidget is embedded in code; the .ui
/// only carries the buttons + the host stack.
class WorkspaceSettingDialog : public QDialog {
    Q_OBJECT

public:
    /// Constructs the dialog: embeds the ImageWidget into the .ui's host stack, builds the
    /// ROI colour legend overlay, and wires all button/ROI signals.
    explicit WorkspaceSettingDialog(QWidget *parent = nullptr);
    /// Destroys the dialog and deletes the generated UI.
    ~WorkspaceSettingDialog() override;

    /// Sets the text shown in the dialog's title label.
    void setTitleText(const QString &title);

    /// Preloads an existing workspace: loads `image` into the view and draws `workingRoi`/
    /// `conditionRoi` if non-empty.
    /// @param image reference image to display; ignored if null
    /// @param workingRoi initial working ROI in image-pixel coordinates; skipped if empty
    /// @param conditionRoi initial condition ROI in image-pixel coordinates; skipped if empty
    void setInitial(const QPixmap &image, const QRectF &workingRoi,
                    const QRectF &conditionRoi);

    bool    hasResult()          const { return m_hasResult; }          ///< True once onSaveClicked() has committed a result.
    QPixmap resultImage()        const { return m_resultImage; }        ///< Image the ROIs were drawn on, valid only after hasResult().
    QRectF  resultRoi()          const { return m_resultRoi; }          ///< Accepted working ROI in image-pixel coordinates (empty if not drawn).
    QRectF  resultConditionRoi() const { return m_resultConditionRoi; } ///< Accepted condition ROI in image-pixel coordinates (empty if not drawn).

public slots:
    /// Loads `image` into the view as the reference image, supplied by the owner after a
    /// requestGrab() (or any external image source); resets any existing ROIs.
    void setMainViewImage(QPixmap image);

signals:
    /// Emitted when the user clicks "Grab"; the owner should respond with setMainViewImage().
    void requestGrab();

protected:
    /// Swallows the Escape key so a stray press does not close the dialog while editing an ROI.
    void keyPressEvent(QKeyEvent *event) override;

private:
    /// Which ROI kind an about-to-be-created ROI item should be assigned to.
    enum class RoiKind { None, Working, Condition };

    /// Opens a file dialog and loads the chosen image as the reference image, resetting ROIs.
    void onChooseImageClicked();
    /// Requests a camera grab by emitting requestGrab().
    void onGrabClicked();
    /// Drops any existing working ROI and starts drawing a new one.
    void onSetWorkingRoiClicked();
    /// Removes the current working ROI, if any.
    void onClearWorkingRoiClicked();
    /// Drops any existing condition ROI and starts drawing a new one.
    void onSetConditionRoiClicked();
    /// Removes the current condition ROI, if any.
    void onClearConditionRoiClicked();
    /// Validates that at least one ROI was drawn, captures the result (image + ROIs), and
    /// accepts the dialog; shows a warning message box instead if nothing can be saved.
    void onSaveClicked();

    /// Capture a freshly created ROI (drawn or preloaded) into the pending kind.
    void onRoiCreated(QGraphicsItem *item);
    /// Apply the kind-specific bounding colour (working = green, condition = orange).
    void styleRoi(ItemRoi *roi, RoiKind kind);
    /// Drop all ROIs and reset tracked pointers (used when the image changes).
    void resetRois();
    /// Enables/disables the set/clear/save buttons based on whether an image is loaded and
    /// which ROIs currently exist.
    void updateButtons();

    Ui::WorkspaceSettingDialog *ui;             ///< Generated form; owns the button/host-stack widgets.
    ImageWidget *m_view{nullptr};                ///< Embedded reference-image view; child of `ui`'s stack, not owned separately.
    QString      m_lastDir;                      ///< Directory remembered from the last "Choose image" file dialog.

    RoiKind  m_pendingKind{RoiKind::None};        ///< Kind to assign to the next ROI reported via onRoiCreated().
    ItemRoi *m_workingRoi{nullptr};                ///< Currently drawn working ROI item, or null if none.
    ItemRoi *m_conditionRoi{nullptr};              ///< Currently drawn condition ROI item, or null if none.

    bool    m_hasResult{false};                   ///< Set true by onSaveClicked() once a result has been committed.
    QPixmap m_resultImage;                        ///< Image captured at save time.
    QRectF  m_resultRoi;                          ///< Working ROI captured at save time, in image-pixel coordinates.
    QRectF  m_resultConditionRoi;                  ///< Condition ROI captured at save time, in image-pixel coordinates.
};

#endif // WORKSPACE_SETTING_DIALOG_H
