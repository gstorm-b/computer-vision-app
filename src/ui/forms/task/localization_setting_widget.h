#ifndef LOCALIZATION_SETTING_WIDGET_H
#define LOCALIZATION_SETTING_WIDGET_H

#include <QWidget>

#include "DockWidget.h"

#include "model/task_localization.h"
#include "ui/forms/task_widget.h"

namespace Ui {
class LocalizationSettingWidget;
}

class LocalizationSettingWidget : public ITaskWidget {
    Q_OBJECT

public:
    explicit LocalizationSettingWidget(std::shared_ptr<vc::model::ITask> task,
                                       ads::CDockWidget *dock = nullptr,
                                       QWidget *parent = nullptr);
    ~LocalizationSettingWidget();

    void loadConfigToTask() override;
    void loadConfigToWidget() override;

private:
    // ── Build / wire-up ─────────────────────────────────────────────────
    void initWidget();
    void rebuildDeviceCombos();
    void rebuildCameraList();
    void rebuildCameraWorkspaceList();
    void refreshCommTags(const QString &deviceId);
    void refreshDevicePresentation(const QString &deviceId);
    void pushConfigToTask();

    // ── Camera workspace (ROI) ──────────────────────────────────────────
    void onWorkspaceUseToggled(const QString &cameraId, bool enabled);
    void onWorkspaceConditionToggled(const QString &cameraId, bool enabled);
    void onWorkspaceSetRequested(const QString &cameraId);

    // Push the current workspace state of one camera to its row widget.
    void refreshWorkspaceRow(const QString &cameraId);

private:
    Ui::LocalizationSettingWidget *ui;


    vc::model::TaskLocalization     *m_localizeTask{nullptr};
    vc::model::TaskLocalizeConfig    m_config;
};

#endif // LOCALIZATION_SETTING_WIDGET_H
