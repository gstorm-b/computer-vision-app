#ifndef NEW_TASK_DIALOG_H
#define NEW_TASK_DIALOG_H

#include <QDialog>

namespace Ui {
class NewTaskDialog;
}

class NewTaskDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewTaskDialog(QWidget *parent = nullptr);
    ~NewTaskDialog();
    inline QString getPath() {
        return m_save_file_path;
    }

    inline QString getName() {
        return m_name;
    }

private:
    void onClicked_btn_select_path();
    void onClicked_btn_finish();
    void onClicked_btn_cancel();

private:
    Ui::NewTaskDialog *ui;
    QString m_save_file_path;
    QString m_name;
};

#endif // NEW_TASK_DIALOG_H
