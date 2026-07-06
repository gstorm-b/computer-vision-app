#include "new_project_dialog.h"
#include "ui_new_project_dialog.h"

#include <QFileDialog>
#include <QMessageBox>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewProjectDialog) {
    ui->setupUi(this);

    this->setModal(true);

    // connect(ui->btn_select_path, &QPushButton::clicked,
    //         this, &NewProjectDialog::onClicked_btn_select_path);
    connect(ui->btn_finished, &QPushButton::clicked,
            this, &NewProjectDialog::onClicked_btn_finish);
    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &NewProjectDialog::onClicked_btn_cancel);
}

NewProjectDialog::~NewProjectDialog() {
    delete ui;
}

// void NewProjectDialog::onClicked_btn_select_path() {
//     QString filePath = QFileDialog::getSaveFileName(
//         this,
//         tr("New Project"),
//         QCoreApplication::applicationDirPath() + "/untitled.vproj",
//         "Vision project files (*.vproj);;"
//         );

//     if (filePath.isEmpty()) {
//         return;
//     }

//     m_save_file_path = filePath;
//     ui->ledit_save_path->setText(m_save_file_path);
//     ui->btn_finished->setFocus();
// }

void NewProjectDialog::onClicked_btn_finish() {
    if (ui->ledit_task_name->text().isEmpty()) {
        QMessageBox::information(this,
                                 tr("New Project message"),
                                 tr("Please name for new Project!"));
        return;
    }

    // if (ui->ledit_save_path->text().isEmpty()) {
    //     QMessageBox::information(this,
    //                              tr("New Project message"),
    //                              tr("Please select save path!"));
    //     return;
    // }

    // m_save_file_path = ui->ledit_save_path->text();
    // m_name = ui->ledit_task_name->text();
    // m_description = ui->plainText_descript->toPlainText();
    // QFileInfo finfo(m_save_file_path);
    // if ((finfo.completeSuffix() != "vproj") ||
    //     !finfo.isAbsolute()){
    //     QMessageBox::information(this,
    //                              tr("New Project message"),
    //                              tr("File path invalid!"));
    //     return;
    // }

    m_name = ui->ledit_task_name->text();
    m_description = ui->plainText_descript->toPlainText();

    this->accept();
}

void NewProjectDialog::onClicked_btn_cancel() {
    this->reject();
}
