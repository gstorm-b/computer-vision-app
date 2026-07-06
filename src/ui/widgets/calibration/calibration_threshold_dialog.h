#ifndef CALIBRATION_THRESHOLD_DIALOG_H
#define CALIBRATION_THRESHOLD_DIALOG_H

#include <QDialog>

#include <opencv2/core.hpp>

class QCheckBox;
class QSlider;
class QSpinBox;
class QLabel;
class QPushButton;
class QDialogButtonBox;
class ImageViewOnly;

namespace calib {
class CalibrationBoard;
}

// =====================================================================
// CalibrationThresholdDialog
// =====================================================================
//
// Modal dialog that tunes the calibration-board binarization threshold on a
// live camera frame. It drives the SAME binarize()/detect() routines the board
// uses at calibration time, so the preview matches reality.
//
//   - "Auto (Otsu)" checkbox: threshold == -1, Otsu picks the value.
//   - Slider / spin 0..255: fixed manual threshold.
//   - Left preview : binarized image (THRESH_BINARY_INV).
//   - Right preview: detection overlay (detected dots) when detect succeeds.
//   - Status line  : applied threshold + detected dot count + pass/fail.
//
// The board pointer is borrowed (not owned). The dialog temporarily applies the
// trial threshold to the board for previewing; the caller is responsible for
// committing the accepted value (threshold()) and/or restoring the previous one
// after exec().
//
class CalibrationThresholdDialog : public QDialog
{
    Q_OBJECT

public:
    CalibrationThresholdDialog(calib::CalibrationBoard *board,
                               int initialThreshold,
                               QWidget *parent = nullptr);

    // -1 => auto (Otsu); 0..255 => fixed manual threshold.
    int threshold() const;

public slots:
    // Push a freshly grabbed frame (BGR or grayscale) and refresh the previews.
    void setImage(const cv::Mat &image);

signals:
    // Emitted when the user asks for a new frame from the camera.
    void regrabRequested();

private slots:
    void onAutoToggled(bool autoOtsu);
    void onSliderChanged(int value);
    void onSpinChanged(int value);
    void recompute();

private:
    void buildUi(int initialThreshold);
    int  trialThreshold() const;   // -1 when Auto is checked, else slider value

    calib::CalibrationBoard *m_board{nullptr};   // borrowed
    cv::Mat m_image;

    QCheckBox        *m_auto{nullptr};
    QSlider          *m_slider{nullptr};
    QSpinBox         *m_spin{nullptr};
    ImageViewOnly    *m_binView{nullptr};
    ImageViewOnly    *m_overlayView{nullptr};
    QLabel           *m_status{nullptr};
    QPushButton      *m_regrab{nullptr};
    QDialogButtonBox *m_buttons{nullptr};
};

#endif // CALIBRATION_THRESHOLD_DIALOG_H
