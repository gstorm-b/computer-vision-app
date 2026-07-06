#ifndef ADD_PATTERN_IMAGE_DIALOG_H
#define ADD_PATTERN_IMAGE_DIALOG_H

#include <QDialog>
#include <opencv2/opencv.hpp>
#include "ui/widgets/image_widget/image_widget.h"
#include "ui/widgets/image_widget/item_roi.h"

namespace Ui {
class AddPatternImageDialog;
}

class AddPatternImageDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddPatternImageDialog(QWidget *parent = nullptr);
    ~AddPatternImageDialog();

    int showAddPatternDialog();
    cv::Mat getFinalImage();

public slots:
    void setMainViewImage(QPixmap image);

signals:
    void requestImage(QString id);

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    void btn_trigger_clicked();
    void btn_choose_image_clicked();
    void btn_set_roi_clicked();
    void btn_cancel_clicked();
    void btn_no_crop_clicked();
    void btn_back_clicked();
    void btn_crop_clicked();

    void form_draw_crop_roi_finished(QGraphicsItem *roi, ImageWidget::ItemAddType typee);

    void prepareShowDialog();

private:
    Ui::AddPatternImageDialog *ui;
    QString m_last_selected_path;

    ImageWidget *m_image_main_view{nullptr};
    ImageWidget *m_image_crop_view{nullptr};

    ItemRoi *m_item_crop_roi{nullptr};
    QPixmap m_item_cropped_pixmap;
    QPixmap m_final_pixmap;

    bool m_cropped;
};

#endif // ADD_PATTERN_IMAGE_DIALOG_H
