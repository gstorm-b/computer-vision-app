#ifndef PROJECT_INFOR_SETTING_H
#define PROJECT_INFOR_SETTING_H

#include <QWidget>
#include "model/project.h"

namespace Ui {
class ProjectInforSetting;
}

class ProjectInforSetting : public QWidget {
    Q_OBJECT
public:
    explicit ProjectInforSetting(std::shared_ptr<vc::model::Project> proj, QWidget *parent = nullptr);
    ~ProjectInforSetting();

    void setProject(std::shared_ptr<vc::model::Project> proj);

private slots:
    void refreshProjectInfor();
    void onNameModified();
    void onDescriptionChanged();

signals:
    void projectModified();

private:
    Ui::ProjectInforSetting *ui;
    std::shared_ptr<vc::model::Project> m_proj;
};

#endif // PROJECT_INFOR_SETTING_H
