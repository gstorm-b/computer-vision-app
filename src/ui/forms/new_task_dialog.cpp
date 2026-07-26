#include "new_task_dialog.h"
#include "ui_new_task_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
// #include

/// Sets up the generated UI, makes the dialog modal, and connects the select-path/finish/cancel
/// buttons.
NewTaskDialog::NewTaskDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewTaskDialog) {
    ui->setupUi(this);

    this->setModal(true);

    connect(ui->btn_select_path, &QPushButton::clicked,
            this, &NewTaskDialog::onClicked_btn_select_path);
    connect(ui->btn_finished, &QPushButton::clicked,
            this, &NewTaskDialog::onClicked_btn_finish);
    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &NewTaskDialog::onClicked_btn_cancel);
}

/// Deletes the generated UI object.
NewTaskDialog::~NewTaskDialog() {
    delete ui;
}

/// Prompts for a .vtask save file (default name "untitled.vtask" under the application
/// directory); on a non-empty selection, stores the path, mirrors it into the save-path
/// field, and moves focus to the finish button. Does nothing if the dialog is cancelled.
void NewTaskDialog::onClicked_btn_select_path() {
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("New task"),
        QCoreApplication::applicationDirPath() + "/untitled.vtask",
        "Vision task files (*.vtask);;"
        );

    if (filePath.isEmpty()) {
        return;
    }

    m_save_file_path = filePath;
    ui->ledit_save_path->setText(m_save_file_path);
    ui->btn_finished->setFocus();
}

/// Requires a non-empty task name and save path, and that the save path ends in ".vtask" and
/// is absolute (showing an information message box and returning early on any failing check),
/// then stores the name/path and accepts the dialog.
void NewTaskDialog::onClicked_btn_finish() {
    if (ui->ledit_task_name->text().isEmpty()) {
        QMessageBox::information(this,
                                 tr("New task message"),
                                 tr("Please name for new task!"));
        return;
    }

    if (ui->ledit_save_path->text().isEmpty()) {
        QMessageBox::information(this,
                                 tr("New task message"),
                                 tr("Please select save path!"));
        return;
    }

    m_save_file_path = ui->ledit_save_path->text();
    m_name = ui->ledit_task_name->text();
    QFileInfo finfo(m_save_file_path);
    if ((finfo.completeSuffix() != "vtask") ||
        !finfo.isAbsolute()){
        QMessageBox::information(this,
                                 tr("New task message"),
                                 tr("File path invalid!"));
        return;
    }

    this->accept();
}

/// Rejects the dialog, discarding any entered values.
void NewTaskDialog::onClicked_btn_cancel() {
    this->reject();
}
