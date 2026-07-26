#ifndef ADD_PATTERN_IMAGE_DIALOG_H
#define ADD_PATTERN_IMAGE_DIALOG_H

#include <QDialog>
#include <opencv2/opencv.hpp>
#include "ui/widgets/image_widget/image_widget.h"
#include "ui/widgets/image_widget/item_roi.h"

namespace Ui {
class AddPatternImageDialog;
}

/// Modal dialog for building a single pattern image: the user either triggers a camera
/// capture (requestImage()/setMainViewImage()) or picks a file from disk, then optionally
/// draws an ROI and crops it; the resulting image is retrieved via getFinalImage().
class AddPatternImageDialog : public QDialog {
    Q_OBJECT

public:
    /// Builds the dialog UI, wires all button handlers, creates the main/crop ImageWidget
    /// pages and adds them to the stacked widget, and initializes the last-selected path to
    /// the current working directory.
    explicit AddPatternImageDialog(QWidget *parent = nullptr);
    /// Destroys the generated UI object.
    ~AddPatternImageDialog();

    /// Resets the dialog to its initial "choose image" state (via prepareShowDialog()), then
    /// shows it modally.
    /// @return the QDialog::exec() result code (QDialog::Accepted/Rejected).
    int showAddPatternDialog();
    /// Converts the final chosen pixmap (either the uncropped source image or the cropped
    /// result) to an OpenCV matrix.
    /// @return the final image as a cv::Mat, in BGR/BGRA/grayscale depending on source format.
    cv::Mat getFinalImage();

public slots:
    /// Loads `image` into the main view (clearing any existing crop ROI first) and, if the
    /// load succeeds, enables the set-ROI and no-crop buttons. Intended to be connected to
    /// an external image-capture source that responds to requestImage().
    void setMainViewImage(QPixmap image);

signals:
    /// Emitted by btn_trigger_clicked() to request an externally-supplied image for `id`.
    void requestImage(QString id);

protected:
    /// Ignores the Escape key (preventing it from closing the dialog); all other keys are
    /// forwarded to QDialog::keyPressEvent().
    void keyPressEvent(QKeyEvent *event);

private:
    /// Requests an image capture by emitting requestImage() with an empty id.
    void btn_trigger_clicked();
    /// Opens a file-open dialog filtered to *.bmp starting at m_last_selected_path; on a
    /// non-empty selection, updates m_last_selected_path, clears any existing crop ROI, loads
    /// the file into the main view, and enables the set-ROI/no-crop buttons on success.
    void btn_choose_image_clicked();
    /// If the main view holds an image, clears any existing crop ROI and starts drawing a new
    /// normal ROI on it.
    void btn_set_roi_clicked();
    /// Rejects the dialog.
    void btn_cancel_clicked();
    /// If not currently in the cropped state, sets the final pixmap to the full main-view
    /// image and accepts the dialog.
    void btn_no_crop_clicked();
    /// Reverts the UI from the crop-preview state back to the main-view state: swaps
    /// back/no-crop button visibility, re-enables choose-image/set-ROI/trigger, switches the
    /// stack back to the main view, clears the cropped flag, and resets the crop button label
    /// to "Crop".
    void btn_back_clicked();
    /// Toggles between previewing and finalizing a crop. If already in the cropped state,
    /// accepts the dialog with the previously cropped pixmap as the final image. Does nothing
    /// further if the main view has no image; otherwise (re-)derives the cropped pixmap from
    /// the current ROI, loads it into the crop view, switches the stack to it, marks the
    /// dialog as cropped, and updates button states/label to "Apply".
    /// @note when already cropped, execution falls through past accept() into the
    ///       re-crop/preview logic below rather than returning immediately.
    void btn_crop_clicked();

    /// Slot for the main view's signal_draw_roi_finished(): ignores a null `roi`, otherwise
    /// stores it (cast to ItemRoi*) as the current crop ROI and enables the crop button.
    void form_draw_crop_roi_finished(QGraphicsItem *roi, ImageWidget::ItemAddType typee);

    /// Resets the dialog to its initial state before display: disables set-ROI/crop/no-crop,
    /// shows the no-crop button (hides back), clears any existing crop ROI and the main-view
    /// image, switches the stack to the main view, and clears the cropped flag/label.
    void prepareShowDialog();

private:
    Ui::AddPatternImageDialog *ui;  ///< Generated UI form for this dialog.
    QString m_last_selected_path;  ///< Directory used to seed the next choose-image file dialog.

    ImageWidget *m_image_main_view{nullptr};  ///< Page showing the source (uncropped) image and its ROI.
    ImageWidget *m_image_crop_view{nullptr};  ///< Page showing the cropped preview image.

    ItemRoi *m_item_crop_roi{nullptr};  ///< Currently drawn crop ROI on the main view, or nullptr if none.
    QPixmap m_item_cropped_pixmap;  ///< Most recently computed crop result.
    QPixmap m_final_pixmap;  ///< Image the dialog will return via getFinalImage() once accepted.

    bool m_cropped;  ///< True while the crop preview is being shown (crop button reads "Apply").
};

#endif // ADD_PATTERN_IMAGE_DIALOG_H
