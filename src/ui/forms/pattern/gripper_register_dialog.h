#ifndef GRIPPER_REGISTER_DIALOG_H
#define GRIPPER_REGISTER_DIALOG_H

#include <QDialog>

#include "model/gripper_preset_store.h"

/**
 * @file gripper_register_dialog.h
 * @brief GripperRegisterDialog — modal editor for the task's named gripper geometries.
 */

namespace Ui { class GripperRegisterDialog; }

/**
 * @class GripperRegisterDialog
 * @brief Modal table editor for vc::model::GripperPresetStore: add, edit and remove named
 *        gripper geometries.
 *
 * Edits a working copy. The caller reads presets() only after exec() returns Accepted;
 * cancelling discards everything. Name uniqueness is validated on commit and reported in
 * the dialog rather than through a message box, so a rejected edit does not interrupt the
 * user's flow.
 *
 * @note Parent this to the widget whose QSS should style it — it has no per-form sheet of
 *       its own and inherits the parent's, which is what keeps it theme-correct.
 * @see vc::model::GripperPresetStore
 */
class GripperRegisterDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog and loads `presets` into the table.
     * @param[in] presets the store to edit; copied, never modified in place
     * @param[in] parent  parent widget, also the source of the inherited stylesheet
     */
    explicit GripperRegisterDialog(const vc::model::GripperPresetStore &presets,
                                   QWidget *parent = nullptr);
    ~GripperRegisterDialog() override;

    /// Returns the edited presets. Only meaningful after exec() returns Accepted.
    const vc::model::GripperPresetStore& presets() const { return m_presets; }

private slots:
    /// Appends a blank row with a generated unique name and starts editing it.
    void onAdd();
    /// Removes the selected row, or reports that nothing is selected.
    void onRemove();
    /// Validates every row, rebuilds m_presets, and accepts the dialog when all rows pass.
    void onSave();

private:
    /// Fills the table from m_presets.
    void populate();
    /// Appends one table row for `preset` without touching m_presets.
    void appendRow(const vc::model::GripperPreset &preset);
    /// Shows `text` in the message label; an empty string clears it.
    /// @param[in] text  message to display
    /// @param[in] error true renders it as an error, false as a neutral note
    void showMessage(const QString &text, bool error = true);
    /// Returns a name not currently present in the table, e.g. "Gripper 3".
    QString uniqueDefaultName() const;

    Ui::GripperRegisterDialog *ui;              ///< Generated form.
    vc::model::GripperPresetStore m_presets;    ///< Working copy; replaced wholesale on save.
};

#endif // GRIPPER_REGISTER_DIALOG_H
