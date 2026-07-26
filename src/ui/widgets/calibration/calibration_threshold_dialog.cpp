#include "calibration_threshold_dialog.h"

#include "calibration/calibration_board.h"
#include "ui/widgets/image_widget/image_view_only.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>
#include <vector>

namespace {
/// Fixed width/height (in pixels) used for both preview panes.
constexpr int kPreviewSize = 360;
} // namespace

/// Constructs the dialog: stores the borrowed board pointer, builds the UI seeded from
/// `initialThreshold`, and runs an initial recompute() (previews stay blank until setImage()
/// delivers the first frame).
CalibrationThresholdDialog::CalibrationThresholdDialog(calib::CalibrationBoard *board,
                                                       int initialThreshold,
                                                       QWidget *parent)
    : QDialog(parent), m_board(board)
{
    setWindowTitle(tr("Tune Detection Threshold"));
    buildUi(initialThreshold);
    recompute();
}

/// Assembles the dialog layout: two preview panes (binarized / detection overlay) with
/// captions, a status label, the Auto checkbox + threshold slider/spin + Re-grab button row,
/// and the OK/Cancel button box; wires all control signals to their handlers.
/// @param initialThreshold seed value: -1 selects Auto (Otsu) with 128 as the manual fallback,
/// 0..255 selects manual mode pre-set to that value
void CalibrationThresholdDialog::buildUi(int initialThreshold)
{
    auto *root = new QVBoxLayout(this);

    // --- Preview row: binarized | detection overlay ---
    auto *previews = new QHBoxLayout();

    auto makePreview = [this, previews](const QString &title) -> ImageViewOnly * {
        auto *col = new QVBoxLayout();
        auto *cap = new QLabel(title, this);
        cap->setAlignment(Qt::AlignHCenter);
        auto *view = new ImageViewOnly(this);
        view->setMinimumSize(kPreviewSize, kPreviewSize);
        col->addWidget(cap);
        col->addWidget(view, 1);
        previews->addLayout(col);
        return view;
    };

    m_binView     = makePreview(tr("Binarized (threshold)"));
    m_overlayView = makePreview(tr("Detected dots"));
    root->addLayout(previews, 1);

    // --- Status ---
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    // --- Controls ---
    auto *controls = new QHBoxLayout();
    const bool autoMode = initialThreshold < 0;
    const int  manualValue = autoMode ? 128 : initialThreshold;

    m_auto = new QCheckBox(tr("Auto (Otsu)"), this);
    m_auto->setChecked(autoMode);

    auto *lbl = new QLabel(tr("Threshold:"), this);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 255);
    m_slider->setValue(manualValue);
    m_slider->setEnabled(!autoMode);

    m_spin = new QSpinBox(this);
    m_spin->setRange(0, 255);
    m_spin->setValue(manualValue);
    m_spin->setEnabled(!autoMode);

    m_regrab = new QPushButton(tr("Re-grab"), this);

    controls->addWidget(m_auto);
    controls->addWidget(lbl);
    controls->addWidget(m_slider, 1);
    controls->addWidget(m_spin);
    controls->addWidget(m_regrab);
    root->addLayout(controls);

    // --- OK / Cancel ---
    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);

    connect(m_auto,   &QCheckBox::toggled,  this, &CalibrationThresholdDialog::onAutoToggled);
    connect(m_slider, &QSlider::valueChanged, this, &CalibrationThresholdDialog::onSliderChanged);
    connect(m_spin,   qOverload<int>(&QSpinBox::valueChanged),
            this, &CalibrationThresholdDialog::onSpinChanged);
    connect(m_regrab, &QPushButton::clicked,
            this, &CalibrationThresholdDialog::regrabRequested);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/// Returns -1 when the Auto (Otsu) checkbox is checked, otherwise the current slider value
/// (or 128 if the slider hasn't been created yet).
int CalibrationThresholdDialog::trialThreshold() const
{
    if (m_auto && m_auto->isChecked()) return -1;
    return m_slider ? m_slider->value() : 128;
}

/// Returns the dialog's current threshold selection (delegates to trialThreshold()).
int CalibrationThresholdDialog::threshold() const
{
    return trialThreshold();
}

/// Clones `image` into m_image and recomputes both preview panes and the status line.
void CalibrationThresholdDialog::setImage(const cv::Mat &image)
{
    m_image = image.clone();
    recompute();
}

/// Enables/disables the manual slider and spin box based on `autoOtsu` and recomputes the
/// previews.
void CalibrationThresholdDialog::onAutoToggled(bool autoOtsu)
{
    if (m_slider) m_slider->setEnabled(!autoOtsu);
    if (m_spin)   m_spin->setEnabled(!autoOtsu);
    recompute();
}

/// Mirrors the slider's new value onto the spin box (blocking its signal to avoid feedback)
/// and recomputes the previews.
void CalibrationThresholdDialog::onSliderChanged(int value)
{
    if (m_spin && m_spin->value() != value) {
        const QSignalBlocker blocker(m_spin);
        m_spin->setValue(value);
    }
    recompute();
}

/// Mirrors the spin box's new value onto the slider (blocking its signal to avoid feedback)
/// and recomputes the previews.
void CalibrationThresholdDialog::onSpinChanged(int value)
{
    if (m_slider && m_slider->value() != value) {
        const QSignalBlocker blocker(m_slider);
        m_slider->setValue(value);
    }
    recompute();
}

/// Re-runs binarize()/detect() on the last grabbed frame at the current trial threshold and
/// refreshes both preview panes and the status line; a no-op until an image has been pushed
/// via setImage().
/// @note Applies the trial threshold to `m_board` as a side effect (setBinarizeThreshold), so
/// the previews reflect exactly what calibration would see.
void CalibrationThresholdDialog::recompute()
{
    if (!m_board || !m_binView || !m_overlayView || !m_status) {
        return;
    }

    if (m_image.empty()) {
        m_binView->clearCurrentImage();
        m_overlayView->clearCurrentImage();
        m_status->setText(tr("Grab a frame from the camera to preview."));
        return;
    }

    const int trial = trialThreshold();
    m_board->setBinarizeThreshold(trial);

    double used = 0.0;
    cv::Mat binary = m_board->binarize(m_image, &used);
    // fitsize=false keeps the current zoom/pan while the user drags the slider.
    m_binView->loadImageOpenCv(binary, false);

    std::vector<cv::Point2f> allPts;
    std::vector<cv::Point2f> corners;
    cv::Mat overlay;
    const bool ok = m_board->detect(m_image, allPts, &corners, &overlay);
    cv::Mat overlayShow = overlay.empty() ? m_image : overlay;
    m_overlayView->loadImageOpenCv(overlayShow, false);

    const QString modeStr = (trial < 0)
        ? tr("Auto (Otsu = %1)").arg(static_cast<int>(std::lround(used)))
        : tr("Manual = %1").arg(trial);

    if (ok) {
        m_status->setText(tr("Threshold: %1  |  Detect: OK (%2 / %3 dots)")
                              .arg(modeStr)
                              .arg(static_cast<int>(allPts.size()))
                              .arg(m_board->totalDots()));
    } else {
        m_status->setText(tr("Threshold: %1  |  Detect: FAILED — adjust the threshold")
                              .arg(modeStr));
    }
}
