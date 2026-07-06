#ifndef LOCALIZATION_TASK_WIDGET_H
#define LOCALIZATION_TASK_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QButtonGroup>

#include "ui/widgets/controls/device_nav_item_widget.h"
#include "ui/widgets/controls/status_lamp_dot.h"
#include "ui/widgets/controls/status_text_label.h"
#include "model/task_localization.h"
#include "ui/forms/task_widget.h"

namespace Ui {
class LocalizationTaskWidget;
}

// ──────────────────────────────────────────────────────────────────────────────
//  LocalizationTaskWidget
//
//  Layout:
//    ┌──────────────┬──────────────────────────────────────────┐
//    │  Nav panel   │  Breadcrumb bar (36px)                   │
//    │  (188px)     ├──────────────────────────────────────────┤
//    │  task header │  content_stack                           │
//    │  status lamps│  [0] Dashboard                           │
//    │  [Dashboard] │  [1] Settings                            │
//    │  [Devices]   │  [2+] Device config                      │
//    │  [Settings]  │                                          │
//    └──────────────┴──────────────────────────────────────────┘
// ──────────────────────────────────────────────────────────────────────────────
class LocalizationTaskWidget : public ITaskWidget {
    Q_OBJECT

public:
    explicit LocalizationTaskWidget(std::shared_ptr<vc::model::ITask> task,
                                    ads::CDockWidget *dock = nullptr,
                                    QWidget *parent = nullptr);
    ~LocalizationTaskWidget();

    void loadConfigToTask()   override;
    void loadConfigToWidget() override;

signals:
    void addDeviceRequested(const QString &taskId);

private:
    // ── Initialisation ────────────────────────────────────────────────────
    void initWidget();
    void initNavPanel();
    void initStatusLamps();
    void initContentStack();

    void onTaskDevicesChanged();

    // ── Navigation ────────────────────────────────────────────────────────
    void rebuildDeviceNav();
    // (Re)subscribe each device-nav connection dot to its runner's
    // connectStatusChanged. Called after a nav rebuild and on task-state
    // changes (runners are created/destroyed across phase transitions).
    void wireDeviceNavDots();
    void showDashboardPage();
    void showPatternsPage();
    void showSettingsPage();
    void showDeviceConfigPage(const QString &deviceId);
    void setNavButtonActive(QPushButton *btn);
    void refreshNavItemStyles();
    void updateBreadcrumb(const QString &label, const QString &role = QStringLiteral("accent"));
    void updateStatusLamps();
    void updateTaskStateUi();
    void onAssignedDeviceModified(const QString &deviceId);

    // ── Helpers ───────────────────────────────────────────────────────────
    QWidget *getOrCreateDeviceConfigPage(const QString &deviceId);

    // ── Old config helpers (settings page) ────────────────────────────────
    void populateBrowser(const QString &id);

private slots:
    // void onCameraMappingChanged(const QMap<int, QString> &mapping);
    void saveConfig();

    void onDeviceNavClicked(const QString &deviceId);
    void onTbtnEnterRuntime();
    void onTbtnExitRuntime();

private:
    // ── Content stack page indices ─────────────────────────────────────────
    static constexpr int kDashboardPage = 0;
    static constexpr int kSettingsPage  = 1;
    static constexpr int kPatternsPage  = 2;
    // Device pages: index 3+

    // ── UI ────────────────────────────────────────────────────────────────
    Ui::LocalizationTaskWidget *ui;

    // ── Model ─────────────────────────────────────────────────────────────
    vc::model::TaskLocalization  *m_localizeTask{nullptr};
    vc::model::TaskLocalizeConfig m_config;

    // bool m_populating_browser{false};
    QStringList m_BitsAddressList;
    QStringList m_WordsAddressList;
    std::shared_ptr<vc::device::IDevice> m_commDevice;

    // ── Nav: status lamps ─────────────────────────────────────────────────
    struct StatusLamp {
        StatusLampDot *dot{nullptr};
        StatusTextLabel *label{nullptr};
    };
    QList<StatusLamp> m_statusLamps; // READY, CAM, PLC, OUT

    // ── Nav: device items ─────────────────────────────────────────────────
    QWidget               *m_devListWidget{nullptr};
    QMap<QString, DeviceNavItemWidget*> m_navItems;     // deviceId → frame
    // Live runner→dot subscriptions for the device-nav connection indicators;
    // dropped and rebuilt by wireDeviceNavDots().
    QList<QMetaObject::Connection> m_navDotConns;

    // ── Content pages ─────────────────────────────────────────────────────
    QWidget               *m_dashboardPage{nullptr};
    QMap<QString, QWidget*> m_devicePages; // deviceId → widget

    // ── Active state ──────────────────────────────────────────────────────
    QString     m_activeDeviceId;          // empty = Dashboard/Settings active
    QPushButton *m_activeNavBtn{nullptr};

    // ── Nav button group for mutual exclusion ─────────────────────────────
    QButtonGroup *m_navBtnGroup{nullptr};

    // ── Patterns page (LocalizationTask only) ─────────────────────────────
    // Per design handoff `Sidebar.jsx`: "Patterns tab — LocalizationTask only".
    // The nav button now lives in the .ui (`ui->btn_nav_patterns`); the
    // content widget below is still lazily instantiated on first click.
    QWidget     *m_patternsPage{nullptr};
    QWidget     *m_settingPage{nullptr};
};

#endif // LOCALIZATION_TASK_WIDGET_H
