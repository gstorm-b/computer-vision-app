#include "calibration_board_dialog.h"

#include "calibration/calibration_board_factory.h"
#include "calibration/fanuc_irvision_board.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

CalibrationBoardDialog::CalibrationBoardDialog(const QString &currentPreset,
                                               QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Select Calibration Board"));
    buildUi(currentPreset);
}

void CalibrationBoardDialog::buildUi(const QString &currentPreset) {
    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_combo = new QComboBox(this);
    for (const std::string &preset : calib::CalibrationBoardFactory::availablePresets()) {
        m_combo->addItem(QString::fromStdString(preset));
    }
    int idx = m_combo->findText(currentPreset);
    if (idx >= 0) {
        m_combo->setCurrentIndex(idx);
    }
    form->addRow(tr("Preset:"), m_combo);

    m_preview = new QLabel(this);
    m_preview->setWordWrap(true);
    form->addRow(tr("Details:"), m_preview);

    root->addLayout(form);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);

    connect(m_combo, &QComboBox::currentTextChanged,
            this, &CalibrationBoardDialog::updatePreview);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updatePreview();
}

QString CalibrationBoardDialog::selectedPreset() const {
    return m_combo ? m_combo->currentText() : QString();
}

void CalibrationBoardDialog::updatePreview() {
    if (!m_combo || !m_preview) {
        return;
    }
    const std::string preset = m_combo->currentText().toStdString();
    auto board = calib::CalibrationBoardFactory::createFromPreset(preset);
    if (!board) {
        m_preview->setText(tr("Unknown preset."));
        return;
    }

    QString text = tr("Type: %1\nTotal dots: %2")
                       .arg(QString::fromStdString(board->typeName()))
                       .arg(board->totalDots());

    if (auto *fanuc = dynamic_cast<const calib::FanucIRvisionBoard *>(board.get())) {
        const auto &p = fanuc->params();
        text += tr("\nGrid: %1 x %2  |  Spacing: %3 mm  |  Margin: %4 mm")
                    .arg(p.rows).arg(p.cols)
                    .arg(p.dotSpacingMm).arg(p.marginMm);
    }
    m_preview->setText(text);
}
