#include "new_task_dialog.h"
#include "ui_new_task_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
// #include

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

NewTaskDialog::~NewTaskDialog() {
    delete ui;
}

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

void NewTaskDialog::onClicked_btn_cancel() {
    this->reject();
}
