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

/// Modal dialog that tunes the calibration-board binarization threshold on a live camera
/// frame. It drives the SAME binarize()/detect() routines the board uses at calibration time,
/// so the preview matches reality.
///   - "Auto (Otsu)" checkbox: threshold == -1, Otsu picks the value.
///   - Slider / spin 0..255: fixed manual threshold.
///   - Left preview : binarized image (THRESH_BINARY_INV).
///   - Right preview: detection overlay (detected dots) when detect succeeds.
///   - Status line  : applied threshold + detected dot count + pass/fail.
/// @note The board pointer is borrowed (not owned). The dialog temporarily applies the trial
/// threshold to the board for previewing; the caller is responsible for committing the
/// accepted value (threshold()) and/or restoring the previous one after exec().
class CalibrationThresholdDialog : public QDialog
{
    Q_OBJECT

public:
    /// Constructs the dialog, wiring the preview panes and controls to `board` and seeding the
    /// initial slider/spin value and Auto checkbox from `initialThreshold`, then runs an
    /// initial recompute() (previews stay blank until setImage() delivers the first frame).
    /// @param board calibration board whose binarize()/detect() drive the previews (borrowed, not owned)
    /// @param initialThreshold seed value: -1 selects Auto (Otsu), 0..255 selects manual with that value
    CalibrationThresholdDialog(calib::CalibrationBoard *board,
                               int initialThreshold,
                               QWidget *parent = nullptr);

    /// Returns the currently selected threshold (mirrors trialThreshold()).
    /// @return -1 for Auto (Otsu); 0..255 for the fixed manual value
    int threshold() const;

public slots:
    /// Clones a freshly grabbed frame (BGR or grayscale) into m_image and refreshes both
    /// preview panes via recompute().
    void setImage(const cv::Mat &image);

signals:
    /// Emitted when the user clicks "Re-grab" to request a fresh frame from the camera.
    void regrabRequested();

private slots:
    /// Handles the "Auto (Otsu)" checkbox toggling: enables/disables the manual slider and
    /// spin box accordingly, then recomputes the previews.
    /// @param autoOtsu true when Auto (Otsu) is now checked
    void onAutoToggled(bool autoOtsu);
    /// Handles slider movement: mirrors `value` onto the spin box (blocking its signal to
    /// avoid feedback) and recomputes the previews.
    void onSliderChanged(int value);
    /// Handles spin box edits: mirrors `value` onto the slider (blocking its signal to avoid
    /// feedback) and recomputes the previews.
    void onSpinChanged(int value);
    /// Re-runs binarize()/detect() on the last grabbed frame at the current trial threshold
    /// and refreshes both preview panes and the status line; a no-op until an image has been
    /// pushed via setImage().
    void recompute();

private:
    /// Builds the dialog's layout (preview panes, status label, threshold controls, OK/Cancel
    /// buttons) and wires their signals; seeds the slider/spin/Auto state from `initialThreshold`.
    void buildUi(int initialThreshold);
    /// Returns -1 when Auto (Otsu) is checked, otherwise the current slider value.
    int  trialThreshold() const;

    calib::CalibrationBoard *m_board{nullptr};   ///< Borrowed pointer to the board being tuned (not owned).
    cv::Mat m_image;  ///< Last frame pushed via setImage(); empty until the first grab.

    QCheckBox        *m_auto{nullptr};        ///< "Auto (Otsu)" checkbox.
    QSlider          *m_slider{nullptr};      ///< Manual threshold slider (0..255).
    QSpinBox         *m_spin{nullptr};        ///< Manual threshold spin box, kept in sync with m_slider.
    ImageViewOnly    *m_binView{nullptr};     ///< Left preview pane: binarized image at the trial threshold.
    ImageViewOnly    *m_overlayView{nullptr}; ///< Right preview pane: detection overlay (or raw frame on failure).
    QLabel           *m_status{nullptr};      ///< Status line: applied threshold, detected dot count, pass/fail.
    QPushButton      *m_regrab{nullptr};      ///< "Re-grab" button; emits regrabRequested() when clicked.
    QDialogButtonBox *m_buttons{nullptr};     ///< OK/Cancel button box.
};

#endif // CALIBRATION_THRESHOLD_DIALOG_H
