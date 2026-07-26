#ifndef CALIBRATION_BOARD_DIALOG_H
#define CALIBRATION_BOARD_DIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QDialogButtonBox;

/// Small modal dialog that lets the user pick a calibration board from the
/// presets registered in calib::CalibrationBoardFactory::availablePresets().
///
/// Usage:
///   CalibrationBoardDialog dlg(currentPreset, parent);
///   if (dlg.exec() == QDialog::Accepted) {
///       QString preset = dlg.selectedPreset();
///   }
///
class CalibrationBoardDialog : public QDialog
{
    Q_OBJECT

public:
    /// Builds the dialog UI (combo box + details preview + OK/Cancel buttons),
    /// preselecting `currentPreset` in the combo if it matches an available
    /// preset.
    explicit CalibrationBoardDialog(const QString &currentPreset,
                                    QWidget *parent = nullptr);

    /// Returns the preset name currently selected in the combo box, or an
    /// empty string if the combo hasn't been built.
    QString selectedPreset() const;

private slots:
    /// Rebuilds the details preview text (type, total dots, and — for a
    /// FanucIRvisionBoard — grid/spacing/margin) for the currently selected
    /// preset; shows "Unknown preset." if the preset can't be resolved.
    void updatePreview();

private:
    /// Lays out the combo box, details label, and dialog button box; wires
    /// their signals and triggers an initial updatePreview().
    void buildUi(const QString &currentPreset);

    QComboBox        *m_combo{nullptr};    ///< Preset selector, populated from availablePresets().
    QLabel           *m_preview{nullptr};  ///< Word-wrapped label showing the selected preset's details.
    QDialogButtonBox *m_buttons{nullptr};  ///< OK/Cancel buttons.
};

#endif // CALIBRATION_BOARD_DIALOG_H
