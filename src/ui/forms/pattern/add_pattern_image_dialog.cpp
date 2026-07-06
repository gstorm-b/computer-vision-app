#include "add_pattern_image_dialog.h"
#include "ui_add_pattern_image_dialog.h"

#include <QFileDialog>
#include <QFileInfo>

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

AddPatternImageDialog::~AddPatternImageDialog() {
    delete ui;
}

int AddPatternImageDialog::showAddPatternDialog() {
    this->prepareShowDialog();

    setModal(true);
    show();
    return this->exec();
}

cv::Mat AddPatternImageDialog::getFinalImage() {
    return QPixmapToCvMat(m_final_pixmap);
}

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

void AddPatternImageDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}

void AddPatternImageDialog::btn_trigger_clicked() {
    emit this->requestImage("");
}

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

void AddPatternImageDialog::btn_cancel_clicked() {
    this->reject();
}

void AddPatternImageDialog::btn_no_crop_clicked() {
    if (!m_cropped) {
        m_final_pixmap = m_image_main_view->getImage();
        this->accept();
    }
}

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

void AddPatternImageDialog::form_draw_crop_roi_finished(QGraphicsItem *roi, ImageWidget::ItemAddType typee) {
    if (roi == nullptr) {
        return;
    }

    m_item_crop_roi = dynamic_cast<ItemRoi*>(roi);
    ui->btn_crop->setEnabled(true);
}

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
