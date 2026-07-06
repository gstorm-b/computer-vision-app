#ifndef CALIBRATION_BOARD_DIALOG_H
#define CALIBRATION_BOARD_DIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QDialogButtonBox;

// =====================================================================
// CalibrationBoardDialog
// =====================================================================
//
// Small modal dialog that lets the user pick a calibration board from the
// presets registered in calib::CalibrationBoardFactory::availablePresets().
//
// Usage:
//   CalibrationBoardDialog dlg(currentPreset, parent);
//   if (dlg.exec() == QDialog::Accepted) {
//       QString preset = dlg.selectedPreset();
//   }
//
class CalibrationBoardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationBoardDialog(const QString &currentPreset,
                                    QWidget *parent = nullptr);

    QString selectedPreset() const;

private slots:
    void updatePreview();

private:
    void buildUi(const QString &currentPreset);

    QComboBox        *m_combo{nullptr};
    QLabel           *m_preview{nullptr};
    QDialogButtonBox *m_buttons{nullptr};
};

#endif // CALIBRATION_BOARD_DIALOG_H
