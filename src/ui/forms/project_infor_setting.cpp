#include "project_infor_setting.h"
#include "ui_project_infor_setting.h"

/// Sets up the generated UI, binds `proj` (refreshing the displayed fields), and connects the
/// name line-edit's editingFinished and description plain-text-edit's textChanged signals to
/// their respective handlers.
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

/// Deletes the generated UI object.
ProjectInforSetting::~ProjectInforSetting() {
    delete ui;
}

/// Rebinds m_proj to `proj` and refreshes the displayed fields via refreshProjectInfor(); does
/// nothing if `proj` is null (the previously bound project, if any, is left unchanged).
void ProjectInforSetting::setProject(std::shared_ptr<vc::model::Project> proj) {
    if (proj == nullptr) {
        return;
    }

    m_proj = proj;
    refreshProjectInfor();
}

/// Repopulates the name field, created/updated-at labels, and description field from m_proj;
/// does nothing if m_proj is null.
void ProjectInforSetting::refreshProjectInfor() {
    if (m_proj == nullptr) {
        return;
    }

    ui->ledit_project_name->setText(m_proj->name());
    ui->lb_create_at->setText(m_proj->createdAt());
    ui->lb_update_at->setText(m_proj->updatedAt());
    ui->plainText_description->setPlainText(m_proj->description());
}

/// Writes the name line-edit's current text into m_proj and emits projectModified().
void ProjectInforSetting::onNameModified() {
    m_proj->setName(ui->ledit_project_name->text());
    emit projectModified();
}

/// Writes the description plain-text-edit's current text into m_proj and emits
/// projectModified().
void ProjectInforSetting::onDescriptionChanged() {
    m_proj->setDescription(ui->plainText_description->toPlainText());
    emit projectModified();
}
