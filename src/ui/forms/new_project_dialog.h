#ifndef NEW_PROJECT_DIALOG_H
#define NEW_PROJECT_DIALOG_H

#include <QDialog>

namespace Ui {
class NewProjectDialog;
}

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);
    ~NewProjectDialog();

    // inline QString getPath() {
    //     return m_save_file_path;
    // }

    inline QString getName() {
        return m_name;
    }

    inline QString getDescription() {
        return m_description;
    }

private:
    // void onClicked_btn_select_path();
    void onClicked_btn_finish();
    void onClicked_btn_cancel();

private:
    Ui::NewProjectDialog *ui;
    // QString m_save_file_path;
    QString m_name;
    QString m_description;
};

#endif // NEW_PROJECT_DIALOG_H
