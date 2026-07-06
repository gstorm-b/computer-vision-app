#ifndef LOCALIZATION_DASHBOARD_WIDGET_H
#define LOCALIZATION_DASHBOARD_WIDGET_H

#include <QWidget>
#include "DockWidget.h"

#include "model/task_localization.h"
#include "ui/forms/task_widget.h"

namespace Ui {
class LocalizationDashboardWidget;
}

class StatusLamp;
class VisionResultViewerWidget;

class LocalizationDashboardWidget : public ITaskWidget
{
    Q_OBJECT

public:
    explicit LocalizationDashboardWidget(std::shared_ptr<vc::model::ITask> task,
                                         ads::CDockWidget *dock = nullptr,
                                         QWidget *parent = nullptr);

    ~LocalizationDashboardWidget();

    void loadConfigToTask() override;
    void loadConfigToWidget() override;

private:
    void initWidget();
    void setupLamps();

    // ── Device connection lamps (hl_state_connection) ──────────────────────
    // The PLC / Camera / Vision-Output lamps reflect the live link state of the
    // devices currently bound to the task. Each lamp subscribes to its device's
    // runner (IDeviceRunner::connectStatusChanged, forwarded onto the GUI
    // thread); the subscriptions are rebuilt whenever the device bindings or the
    // active camera change.
    void setupConnectionLamps();
    void rebuildConnectionWiring();
    void wireConnectionLamp(StatusLamp *lamp, const QString &deviceId);
    void applyConnectStatusToLamp(StatusLamp *lamp, vc::device::ConnectStatus status);
    // Active camera device for the camera lamp: the device mapped to the live
    // nActiveCamera number, falling back to the first bound camera.
    QString resolveActiveCameraDeviceId() const;

    void pushSignalTagsFromConfig();
    void updateTaskContext();
    void updateTaskStateLabel();
    void setupResultTable();
    void updateCycleResult(const vc::model::LocalizationRuntimeController::CycleResult &result);
    void appendTaskLog(const vc::model::LocalizationRuntimeController::TaskLogEntry &entry);
    void installVisionViewer();
    void syncResultSelectionFromTable();
    void syncResultSelectionFromViewer(int objectIndex);
    void clearResultSelection();
    int resultOverlayIndexForRow(int row) const;

    // Route a single live signal value to its dashboard visual (state lamps,
    // fault panel, or context value label). Monitor-list refresh is handled
    // separately at the call site.
    void applySignalToDashboard(const QString &name, const QVariant &value);
    // Drive the fault panel (frame_fault[active] + value labels + task lamp).
    void setFaultState(bool active, int faultCode);

private:
    Ui::LocalizationDashboardWidget *ui;

    vc::model::TaskLocalization *m_localizeTask{nullptr};
    vc::model::TaskLocalizeConfig m_config;

    int  m_lastFaultCode{0};
    bool m_taskFaultActive{false};
    QMap<QString, QVariant> m_liveSignalValues;
    VisionResultViewerWidget *m_resultViewer{nullptr};

    // Live nActiveCamera number; -1 = unknown (use first bound camera).
    int m_activeCameraNumber{-1};
    // Active runner subscriptions for the connection lamps; dropped/rebuilt on
    // binding or active-camera changes.
    QList<QMetaObject::Connection> m_connectionConns;
};

#endif // LOCALIZATION_DASHBOARD_WIDGET_H
