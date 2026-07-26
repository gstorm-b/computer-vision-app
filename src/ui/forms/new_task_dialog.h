#ifndef NEW_TASK_DIALOG_H
#define NEW_TASK_DIALOG_H

#include <QDialog>

namespace Ui {
class NewTaskDialog;
}

/// Modal dialog that lets the user pick a save path (.vtask file) and name for a new task;
/// the values are read back via getPath()/getName() once the dialog is accepted.
class NewTaskDialog : public QDialog {
    Q_OBJECT
public:
    /// Builds the dialog UI and wires the select-path/finish/cancel buttons to their handlers.
    explicit NewTaskDialog(QWidget *parent = nullptr);
    /// Destroys the generated UI object.
    ~NewTaskDialog();
    /// Returns the absolute .vtask save path chosen by the user, captured on finish.
    inline QString getPath() {
        return m_save_file_path;
    }

    /// Returns the task name entered by the user, captured on finish.
    inline QString getName() {
        return m_name;
    }

private:
    /// Opens a save-file dialog defaulting to "<appDir>/untitled.vtask"; if a path is chosen,
    /// stores it, displays it in the save-path field, and focuses the finish button.
    void onClicked_btn_select_path();
    /// Validates that a task name and save path were provided and that the save path has a
    /// ".vtask" extension and is absolute (showing an information message box and returning
    /// early on any failure), then stores the name/path and accepts the dialog.
    void onClicked_btn_finish();
    /// Rejects the dialog without saving any entered values.
    void onClicked_btn_cancel();

private:
    Ui::NewTaskDialog *ui;  ///< Generated UI form for this dialog.
    QString m_save_file_path;  ///< Absolute .vtask save path captured from the input field on finish.
    QString m_name;  ///< Task name captured from the input field on finish.
};

#endif // NEW_TASK_DIALOG_H
