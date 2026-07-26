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

/// Task-configuration widget for a LocalizationTask: a left-hand nav panel
/// (task header, status lamps, Dashboard/Patterns/Settings buttons, and the
/// assigned-device list) driving a right-hand breadcrumb bar plus
/// content_stack that swaps between the Dashboard, Settings, Patterns, and
/// per-device configuration pages.
///
///  Layout:
///    ┌──────────────┬──────────────────────────────────────────┐
///    │  Nav panel   │  Breadcrumb bar (36px)                   │
///    │  (188px)     ├──────────────────────────────────────────┤
///    │  task header │  content_stack                           │
///    │  status lamps│  [0] Dashboard                           │
///    │  [Dashboard] │  [1] Settings                            │
///    │  [Devices]   │  [2+] Device config                      │
///    │  [Settings]  │                                          │
///    └──────────────┴──────────────────────────────────────────┘
class LocalizationTaskWidget : public ITaskWidget {
    Q_OBJECT

public:
    /// Constructs the widget for `task`, wires up the generated Ui form, and runs
    /// full initialisation (nav panel, status lamps, device pages, Commission
    /// entry) via initWidget().
    explicit LocalizationTaskWidget(std::shared_ptr<vc::model::ITask> task,
                                    ads::CDockWidget *dock = nullptr,
                                    QWidget *parent = nullptr);
    /// Ends Commission (stopping all per-device threads and moving devices back
    /// to the main thread) before tearing down the widget; the TaskRunner itself
    /// stays alive, owned by ITask.
    ~LocalizationTaskWidget();

    /// No-op override: this task has no config-to-task push path outside of
    /// saveConfig()/setTaskLocalizeConfig().
    void loadConfigToTask()   override;
    /// No-op override: this task has no config-to-widget pull path implemented.
    void loadConfigToWidget() override;

signals:
    /// Emitted when the user clicks the nav panel's add-device button, carrying
    /// this task's id so the caller knows which task to add the device to.
    void addDeviceRequested(const QString &taskId);

private:
    // ── Initialisation ────────────────────────────────────────────────────
    /// Caches the task/config pointers, wires up theme reload, the property
    /// browser stack, device/task-state signal connections and toolbar buttons,
    /// starts Commission on the task, and shows the Dashboard page.
    void initWidget();
    /// Styles the nav panel: fixes its width, sets the header/section/breadcrumb
    /// fonts and labels, wires the Dashboard/Patterns/Settings button group and
    /// the add-device button, builds the device-list scroll content, and calls
    /// rebuildDeviceNav().
    void initNavPanel();
    /// Builds the READY/CAM/PLC/OUT status-lamp column widgets inside
    /// frame_status_lamps and calls updateStatusLamps() to set their initial state.
    void initStatusLamps();
    /// Placeholder for inserting a dedicated dashboard page into content_stack;
    /// currently a no-op (the dashboard page is built lazily by showDashboardPage()
    /// instead).
    void initContentStack();

    /// Reacts to vc::model::ITask::devicesChanged: removes content_stack pages for
    /// devices no longer assigned (walking the stack top-down so index shifts from
    /// removeWidget() don't skip a page), calls TaskRunner::resyncDevices() via
    /// the task, and rebuilds the device nav.
    void onTaskDevicesChanged();

    // ── Navigation ────────────────────────────────────────────────────────
    /// Clears and repopulates the device-nav list from the task's assigned
    /// devices, sorted by device type then name; shows an empty-state label when
    /// no device is assigned, then refreshes styles/status lamps/nav-dot wiring.
    void rebuildDeviceNav();
    // (Re)subscribe each device-nav connection dot to its runner's
    // connectStatusChanged. Called after a nav rebuild and on task-state
    // changes (runners are created/destroyed across phase transitions).
    void wireDeviceNavDots();
    /// Lazily creates the LocalizationDashboardWidget (once) and switches
    /// content_stack to it, clearing the active device and updating the
    /// breadcrumb/nav-button checked state.
    void showDashboardPage();
    /// Lazily creates the LocalizationPatternsWidget (once) and switches
    /// content_stack to it, clearing the active device and updating the
    /// breadcrumb/nav-button checked state.
    void showPatternsPage();
    /// Lazily creates the LocalizationSettingWidget (once) and switches
    /// content_stack to it, clearing the active device and updating the
    /// breadcrumb/nav-button checked state.
    void showSettingsPage();
    /// Switches content_stack to the (lazily created) config page for
    /// `deviceId`, unchecks the Dashboard/Settings nav buttons, updates the
    /// breadcrumb with the device's name/type role, and populates the property
    /// browser for it.
    void showDeviceConfigPage(const QString &deviceId);
    /// Declared but not implemented/called; nav-button active state is currently
    /// driven by refreshNavItemStyles() and the exclusive m_navBtnGroup instead.
    void setNavButtonActive(QPushButton *btn);
    /// Marks each device-nav item selected/unselected to match m_activeDeviceId.
    void refreshNavItemStyles();
    /// Sets the breadcrumb's current-page label text and "breadcrumbRole" style
    /// property (used for colour), then re-polishes it.
    void updateBreadcrumb(const QString &label, const QString &role = QStringLiteral("accent"));
    /// Recomputes and applies the READY/CAM/PLC/OUT lamp states from the task's
    /// current lifecycle state and the device types among its assigned devices.
    void updateStatusLamps();
    /// Refreshes the nav-panel state label/tooltip/style property from the
    /// task's current TaskState and re-polishes it.
    void updateTaskStateUi();
    /// Rebuilds the device nav and, if `deviceId` is the currently active page,
    /// refreshes the breadcrumb to the device's (possibly changed) name/role.
    /// No-op if the task no longer has `deviceId` assigned.
    void onAssignedDeviceModified(const QString &deviceId);

    // ── Helpers ───────────────────────────────────────────────────────────
    /// Returns the cached config page for `deviceId`, creating it via
    /// DeviceWidgetFactory (looking up the device's runner from TaskRunner) on
    /// first request and adding it to content_stack; returns nullptr if the
    /// device cannot be resolved.
    QWidget *getOrCreateDeviceConfigPage(const QString &deviceId);

    // ── Old config helpers (settings page) ────────────────────────────────
    /// Switches the shared property browser to the device page's own browser
    /// (or the default browser if the page isn't an IDeviceWidget).
    void populateBrowser(const QString &id);

private slots:
    // void onCameraMappingChanged(const QMap<int, QString> &mapping);
    /// Pushes m_config back to the model via TaskLocalization::setTaskLocalizeConfig().
    void saveConfig();

    /// Slot for DeviceNavItemWidget::activated; shows the config page for the
    /// clicked device.
    void onDeviceNavClicked(const QString &deviceId);
    /// Starts Runtime (non-recovery) when the task is currently in Commission.
    void onTbtnEnterRuntime();
    /// Ends Runtime if not already in Commission, then re-enters Commission.
    void onTbtnExitRuntime();

private:
    // ── Content stack page indices ─────────────────────────────────────────
    static constexpr int kDashboardPage = 0;  ///< Preferred content_stack slot for the dashboard page.
    static constexpr int kSettingsPage  = 1;  ///< Preferred content_stack slot for the settings page.
    static constexpr int kPatternsPage  = 2;  ///< Preferred content_stack slot for the patterns page.
    // Device pages: index 3+

    // ── UI ────────────────────────────────────────────────────────────────
    Ui::LocalizationTaskWidget *ui;  ///< Generated .ui form; owned by this widget, deleted in the destructor.

    // ── Model ─────────────────────────────────────────────────────────────
    vc::model::TaskLocalization  *m_localizeTask{nullptr};  ///< Non-owning cast of m_task; null when constructed without a task.
    vc::model::TaskLocalizeConfig m_config;  ///< Local working copy of the task's config, loaded in initWidget() and pushed back by saveConfig().

    // bool m_populating_browser{false};
    QStringList m_BitsAddressList;  ///< Unused: declared but never populated or read.
    QStringList m_WordsAddressList;  ///< Unused: declared but never populated or read.
    std::shared_ptr<vc::device::IDevice> m_commDevice;  ///< Unused: declared but never assigned or read.

    // ── Nav: status lamps ─────────────────────────────────────────────────
    /// One status-lamp column (dot + text label) shown in the nav panel's
    /// status-lamp row (READY/CAM/PLC/OUT).
    struct StatusLamp {
        StatusLampDot *dot{nullptr};
        StatusTextLabel *label{nullptr};
    };
    QList<StatusLamp> m_statusLamps; ///< Lamp columns in order: READY, CAM, PLC, OUT.

    // ── Nav: device items ─────────────────────────────────────────────────
    QWidget               *m_devListWidget{nullptr};  ///< Scroll-area content widget holding the device-nav item list.
    QMap<QString, DeviceNavItemWidget*> m_navItems;     ///< Device-nav item widgets, keyed by device id.
    // Live runner→dot subscriptions for the device-nav connection indicators;
    // dropped and rebuilt by wireDeviceNavDots().
    QList<QMetaObject::Connection> m_navDotConns;

    // ── Content pages ─────────────────────────────────────────────────────
    QWidget               *m_dashboardPage{nullptr};  ///< Lazily-created dashboard page (LocalizationDashboardWidget), owned by content_stack.
    QMap<QString, QWidget*> m_devicePages; ///< Per-device config pages, keyed by device id.

    // ── Active state ──────────────────────────────────────────────────────
    QString     m_activeDeviceId;          ///< Id of the device whose config page is showing; empty when Dashboard/Settings/Patterns is active.
    QPushButton *m_activeNavBtn{nullptr};  ///< Unused: declared but never assigned or read.

    // ── Nav button group for mutual exclusion ─────────────────────────────
    QButtonGroup *m_navBtnGroup{nullptr};  ///< Makes the Dashboard/Patterns/Settings nav buttons mutually exclusive.

    // ── Patterns page (LocalizationTask only) ─────────────────────────────
    // Per design handoff `Sidebar.jsx`: "Patterns tab — LocalizationTask only".
    // The nav button now lives in the .ui (`ui->btn_nav_patterns`); the
    // content widget below is still lazily instantiated on first click.
    QWidget     *m_patternsPage{nullptr};  ///< Lazily-created patterns page (LocalizationPatternsWidget), owned by content_stack.
    QWidget     *m_settingPage{nullptr};  ///< Lazily-created settings page (LocalizationSettingWidget), owned by content_stack.
};

#endif // LOCALIZATION_TASK_WIDGET_H
