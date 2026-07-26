#ifndef NEW_PROJECT_DIALOG_H
#define NEW_PROJECT_DIALOG_H

#include <QDialog>

namespace Ui {
class NewProjectDialog;
}

/// Modal dialog that collects a name and description for a new project; on success the
/// values are read back via getName()/getDescription().
class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    /// Builds the dialog UI and wires the finish/cancel buttons to their handlers.
    explicit NewProjectDialog(QWidget *parent = nullptr);
    /// Destroys the generated UI object.
    ~NewProjectDialog();

    // inline QString getPath() {
    //     return m_save_file_path;
    // }

    /// Returns the project name entered by the user, captured on finish.
    inline QString getName() {
        return m_name;
    }

    /// Returns the project description entered by the user, captured on finish.
    inline QString getDescription() {
        return m_description;
    }

private:
    // void onClicked_btn_select_path();
    /// Validates that a project name was entered, stores the name/description fields, and
    /// accepts the dialog; shows a message box and returns early if the name is empty.
    void onClicked_btn_finish();
    /// Rejects the dialog without saving any entered values.
    void onClicked_btn_cancel();

private:
    Ui::NewProjectDialog *ui;  ///< Generated UI form for this dialog.
    // QString m_save_file_path;
    QString m_name;  ///< Project name captured from the input field on finish.
    QString m_description;  ///< Project description captured from the input field on finish.
};

#endif // NEW_PROJECT_DIALOG_H
