#ifndef PROJECT_INFOR_SETTING_H
#define PROJECT_INFOR_SETTING_H

#include <QWidget>
#include "model/project.h"

namespace Ui {
class ProjectInforSetting;
}

/**
 * @file project_infor_setting.h
 * @brief ProjectInforSetting — settings form for a project's name/description and timestamps.
 */

/**
 * @class ProjectInforSetting
 * @brief Settings form showing/editing a project's name and description, and displaying its
 *        created/updated timestamps; edits are written straight back into the bound Project
 *        model.
 */
class ProjectInforSetting : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Loads the generated UI, binds @p proj via setProject(), and connects the
     *        name/description editors to their change handlers.
     * @param[in] proj the project whose data this form displays and edits
     */
    explicit ProjectInforSetting(std::shared_ptr<vc::model::Project> proj, QWidget *parent = nullptr);
    /// Deletes the generated UI object.
    ~ProjectInforSetting();

    /**
     * @brief Rebinds the form to a different project and refreshes the displayed fields; does
     *        nothing if @p proj is null.
     * @param[in] proj the project to bind; ignored if null
     */
    void setProject(std::shared_ptr<vc::model::Project> proj);

private slots:
    /// Repopulates the name, created/updated-at labels, and description fields from m_proj;
    /// does nothing if m_proj is null.
    void refreshProjectInfor();
    /// Writes the name field's current text into m_proj and emits projectModified().
    void onNameModified();
    /// Writes the description field's current plain text into m_proj and emits projectModified().
    void onDescriptionChanged();

signals:
    /// Emitted whenever the bound project's name or description is edited through this form.
    void projectModified();

private:
    Ui::ProjectInforSetting *ui;               ///< Generated UI object owning this form's widgets; destroyed in the destructor.
    std::shared_ptr<vc::model::Project> m_proj;  ///< The project currently bound to and edited by this form.
};

#endif // PROJECT_INFOR_SETTING_H
