#include "project_infor_setting.h"
#include "ui_project_infor_setting.h"

ProjectInforSetting::ProjectInforSetting(std::shared_ptr<vc::model::Project> proj,
                                         QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProjectInforSetting) {
    ui->setupUi(this);

    setProject(proj);

    connect(ui->ledit_project_name, &QLineEdit::editingFinished,
            this, &ProjectInforSetting::onNameModified);
    connect(ui->plainText_description, &QPlainTextEdit::textChanged,
            this, &ProjectInforSetting::onDescriptionChanged);
}

ProjectInforSetting::~ProjectInforSetting() {
    delete ui;
}

void ProjectInforSetting::setProject(std::shared_ptr<vc::model::Project> proj) {
    if (proj == nullptr) {
        return;
    }

    m_proj = proj;
    refreshProjectInfor();
}

void ProjectInforSetting::refreshProjectInfor() {
    if (m_proj == nullptr) {
        return;
    }

    ui->ledit_project_name->setText(m_proj->name());
    ui->lb_create_at->setText(m_proj->createdAt());
    ui->lb_update_at->setText(m_proj->updatedAt());
    ui->plainText_description->setPlainText(m_proj->description());
}

void ProjectInforSetting::onNameModified() {
    m_proj->setName(ui->ledit_project_name->text());
    emit projectModified();
}

void ProjectInforSetting::onDescriptionChanged() {
    m_proj->setDescription(ui->plainText_description->toPlainText());
    emit projectModified();
}
