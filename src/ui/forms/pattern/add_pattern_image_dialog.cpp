#include "add_pattern_image_dialog.h"
#include "ui_add_pattern_image_dialog.h"

#include <QFileDialog>
#include <QFileInfo>

/// Converts a QPixmap to an OpenCV matrix, choosing the conversion by the pixmap's QImage
/// format: Grayscale8 maps to a 1-channel CV_8UC1 clone, RGB888 converts RGB to BGR (CV_8UC3),
/// RGBA8888 converts RGBA to BGRA (CV_8UC4), and any other format is first converted to
/// RGBA8888 before the same RGBA-to-BGRA conversion.
/// @param pixmap the source pixmap
/// @return the converted image, or an empty cv::Mat if `pixmap`'s QImage is null
inline cv::Mat QPixmapToCvMat(const QPixmap& pixmap) {
    QImage qimg = pixmap.toImage();
    if (qimg.isNull()) {
        return cv::Mat();
    }

    switch (qimg.format()) {
    case QImage::Format_Grayscale8: {
        // 1 channel
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC1,
                    const_cast<uchar*>(qimg.bits()),
                    static_cast<size_t>(qimg.bytesPerLine()));
        return mat.clone();
    }

    case QImage::Format_RGB888: {
        // Qt = RGB, OpenCV = BGR
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3,
                    const_cast<uchar*>(qimg.bits()),
                    static_cast<size_t>(qimg.bytesPerLine()));
        cv::Mat matBGR;
        cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);
        return matBGR;
    }

    case QImage::Format_RGBA8888: {
        // Qt = RGBA, OpenCV = BGRA
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC4,
                    const_cast<uchar*>(qimg.bits()),
                    static_cast<size_t>(qimg.bytesPerLine()));
        cv::Mat matBGRA;
        cv::cvtColor(mat, matBGRA, cv::COLOR_RGBA2BGRA);
        return matBGRA;
    }

    default:
        QImage converted = qimg.convertToFormat(QImage::Format_RGBA8888);
        cv::Mat mat(converted.height(), converted.width(), CV_8UC4,
                    const_cast<uchar*>(converted.bits()),
                    static_cast<size_t>(converted.bytesPerLine()));
        cv::Mat matBGRA;
        cv::cvtColor(mat, matBGRA, cv::COLOR_RGBA2BGRA);
        return matBGRA;
    }
}

/// Sets up the generated UI, connects all button handlers, creates the main/crop ImageWidget
/// pages (mouse menu disabled) and adds them to the stacked widget, wires the main view's ROI
/// signal, and seeds m_last_selected_path with the current working directory.
AddPatternImageDialog::AddPatternImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddPatternImageDialog) {
    ui->setupUi(this);

    connect(ui->btn_trigger, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_trigger_clicked);
    connect(ui->btn_choose_image, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_choose_image_clicked);
    connect(ui->btn_set_roi, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_set_roi_clicked);
    connect(ui->btn_crop, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_crop_clicked);
    connect(ui->btn_no_crop, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_no_crop_clicked);
    connect(ui->btn_back, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_back_clicked);
    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &AddPatternImageDialog::btn_cancel_clicked);

    m_image_main_view = new ImageWidget(this);
    m_image_crop_view = new ImageWidget(this);

    ui->stack_wg->addWidget(m_image_main_view);
    ui->stack_wg->addWidget(m_image_crop_view);

    m_image_main_view->setEnableMouseMenu(false);
    m_image_crop_view->setEnableMouseMenu(false);

    connect(m_image_main_view, &ImageWidget::signal_draw_roi_finished,
            this, &AddPatternImageDialog::form_draw_crop_roi_finished);

    m_last_selected_path = QDir::currentPath();
}

/// Deletes the generated UI object.
AddPatternImageDialog::~AddPatternImageDialog() {
    delete ui;
}

/// Resets the dialog to its initial state via prepareShowDialog(), then shows it as a modal
/// dialog and enters its own event loop.
/// @return the QDialog::exec() result code (QDialog::Accepted/Rejected).
int AddPatternImageDialog::showAddPatternDialog() {
    this->prepareShowDialog();

    setModal(true);
    show();
    return this->exec();
}

/// @return m_final_pixmap converted to a cv::Mat via QPixmapToCvMat().
cv::Mat AddPatternImageDialog::getFinalImage() {
    return QPixmapToCvMat(m_final_pixmap);
}

/// Clears any existing crop ROI from the main view, loads `image` into it, and, if the load
/// succeeds, enables the set-ROI and no-crop buttons.
void AddPatternImageDialog::setMainViewImage(QPixmap image) {
    if (m_item_crop_roi != nullptr) {
        m_image_main_view->scene()->removeItem(m_item_crop_roi);
        m_item_crop_roi = nullptr;
    }

    m_image_main_view->loadImage(image);
    if (m_image_main_view->hadImage()) {
        ui->btn_set_roi->setEnabled(true);
        ui->btn_no_crop->setEnabled(true);
    }
}

/// Ignores the Escape key so it cannot close the dialog; all other keys are forwarded to
/// QDialog::keyPressEvent().
void AddPatternImageDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}

/// Requests an externally-supplied image by emitting requestImage() with an empty id.
void AddPatternImageDialog::btn_trigger_clicked() {
    emit this->requestImage("");
}

/// Opens a *.bmp file-open dialog starting at m_last_selected_path; on a non-empty selection,
/// updates m_last_selected_path to the chosen file's directory, clears any existing crop ROI,
/// loads the file into the main view, and enables the set-ROI/no-crop buttons if the load
/// succeeded. Does nothing if the dialog is cancelled.
void AddPatternImageDialog::btn_choose_image_clicked() {
    QString file_path = QFileDialog::getOpenFileName(this,
                                                     tr("Select pattern image"),
                                                     m_last_selected_path,
                                                     tr("Image (*.bmp)"));

    if (file_path.isEmpty()) {
        return;
    }

    QFileInfo file_info(file_path);
    m_last_selected_path = file_info.absolutePath();

    if (m_item_crop_roi != nullptr) {
        m_image_main_view->scene()->removeItem(m_item_crop_roi);
        m_item_crop_roi = nullptr;
    }

    m_image_main_view->loadImage(file_path);
    if (m_image_main_view->hadImage()) {
        ui->btn_set_roi->setEnabled(true);
        ui->btn_no_crop->setEnabled(true);
    }
}

/// If the main view has a loaded image, clears any existing crop ROI and starts drawing a new
/// normal ROI on it; does nothing if no image is loaded.
void AddPatternImageDialog::btn_set_roi_clicked() {
    if (!m_image_main_view->hadImage()) {
        return;
    }

    if (m_item_crop_roi != nullptr) {
        m_image_main_view->scene()->removeItem(m_item_crop_roi);
        m_item_crop_roi = nullptr;
    }

    m_image_main_view->startDrawROI(ImageWidget::NormalROI);
}

/// Rejects the dialog, discarding any selected/cropped image.
void AddPatternImageDialog::btn_cancel_clicked() {
    this->reject();
}

/// If not currently showing the crop preview, takes the full main-view image as the final
/// image and accepts the dialog. Does nothing while the crop preview is active.
void AddPatternImageDialog::btn_no_crop_clicked() {
    if (!m_cropped) {
        m_final_pixmap = m_image_main_view->getImage();
        this->accept();
    }
}

/// Reverts from the crop-preview state back to the main-view state: hides the back button and
/// shows the no-crop button, re-enables choose-image/set-ROI/trigger, switches the stack back
/// to the main view, clears the cropped flag, and resets the crop button label to "Crop".
void AddPatternImageDialog::btn_back_clicked() {
    ui->btn_back->setVisible(false);
    ui->btn_no_crop->setVisible(true);
    ui->btn_choose_image->setEnabled(true);
    ui->btn_set_roi->setEnabled(true);
    ui->btn_trigger->setEnabled(true);
    ui->stack_wg->setCurrentWidget(m_image_main_view);
    m_cropped = false;
    ui->btn_crop->setText(tr("Crop"));
}

/// Toggles between previewing and finalizing a crop. If already in the cropped state, sets
/// the final image to the previously computed cropped pixmap and accepts the dialog. Then, if
/// the main view has no image, returns; otherwise (re-)derives the cropped pixmap from the
/// current ROI, loads it into the crop view and fits the view to it, switches the stack to the
/// crop view, marks the dialog as cropped, updates the crop button label to "Apply", shows the
/// back button (hiding no-crop), and disables choose-image/trigger/set-ROI.
/// @note when already cropped, execution falls through past accept() into the recompute/
///       preview logic below instead of returning immediately.
void AddPatternImageDialog::btn_crop_clicked() {
    if (m_cropped) {
        m_final_pixmap = m_item_cropped_pixmap;
        this->accept();
    }

    if (!m_image_main_view->hadImage()) {
        return;
    }

    m_item_cropped_pixmap = m_image_main_view->getCroppedFromRoi(m_item_crop_roi);
    m_image_crop_view->loadImage(m_item_cropped_pixmap);
    m_image_crop_view->fitImageView();

    ui->stack_wg->setCurrentWidget(m_image_crop_view);
    m_cropped = true;
    ui->btn_crop->setText(tr("Apply"));
    ui->btn_back->setVisible(true);
    ui->btn_no_crop->setVisible(false);
    ui->btn_choose_image->setEnabled(false);
    ui->btn_trigger->setEnabled(false);
    ui->btn_set_roi->setEnabled(false);
}

/// Handles the main view's signal_draw_roi_finished(): ignores a null `roi`, otherwise stores
/// it (cast to ItemRoi*) as the current crop ROI and enables the crop button.
void AddPatternImageDialog::form_draw_crop_roi_finished(QGraphicsItem *roi, ImageWidget::ItemAddType typee) {
    if (roi == nullptr) {
        return;
    }

    m_item_crop_roi = dynamic_cast<ItemRoi*>(roi);
    ui->btn_crop->setEnabled(true);
}

/// Resets the dialog to its initial display state: disables set-ROI/crop/no-crop, hides the
/// back button and shows no-crop, clears any existing crop ROI and removes the main-view
/// image, switches the stack to the main view, and clears the cropped flag and crop button
/// label ("Crop").
void AddPatternImageDialog::prepareShowDialog() {
    ui->btn_set_roi->setEnabled(false);
    ui->btn_crop->setEnabled(false);
    ui->btn_no_crop->setEnabled(false);
    ui->btn_back->setVisible(false);
    ui->btn_no_crop->setVisible(true);

    if (m_item_crop_roi != nullptr) {
        m_image_main_view->scene()->removeItem(m_item_crop_roi);
        m_item_crop_roi = nullptr;
    }
    m_image_main_view->removeImage();

    ui->stack_wg->setCurrentWidget(m_image_main_view);

    m_cropped = false;
    ui->btn_crop->setText(tr("Crop"));
}
