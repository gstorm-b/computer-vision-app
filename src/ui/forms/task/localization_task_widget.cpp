#include "localization_task_widget.h"
#include "ui_localization_task_widget.h"
#include "model/project.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QInputDialog>
#include <QScrollArea>
#include <QFont>
#include <QFile>

#include <algorithm>

#include "core/utils/windows_helper.h"
#include "runtime/idevice_runner.h"
#include "ui/forms/device_widget_factory.h"
#include "ui/forms/device_widget.h"
#include "device/idevice_config.h"
#include "core/utils/theme_manager.h"
#include "ui/forms/task/localization_dashboard_widget.h"
#include "ui/forms/task/localization_patterns_widget.h"
#include "ui/forms/task/localization_setting_widget.h"

/// Maps a task's current lifecycle state to the visual state of the READY status
/// lamp ("on" while Ready/RunningCycle, "warn" during transitional phases,
/// "error" when Faulted, "off" for Idle/anything else).
/// @param state the task's current TaskState
/// @return one of "on", "warn", "error", "off"
static QString readyLampStateForTask(vc::model::TaskState state)
{
    switch (state) {
    case vc::model::TaskState::Ready:
    case vc::model::TaskState::RunningCycle:
        return QStringLiteral("on");
    case vc::model::TaskState::CommissionStarting:
    case vc::model::TaskState::Commission:
    case vc::model::TaskState::RuntimeStarting:
    case vc::model::TaskState::Recovering:
    case vc::model::TaskState::Stopping:
        return QStringLiteral("warn");
    case vc::model::TaskState::Faulted:
        return QStringLiteral("error");
    case vc::model::TaskState::Idle:
    default:
        return QStringLiteral("off");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ──────────────────────────────────────────────────────────────────────────────
/// Constructs the widget for `task`, wires up the generated Ui form, and runs
/// full initialisation (nav panel, status lamps, device pages, Commission
/// entry) via initWidget().
LocalizationTaskWidget::LocalizationTaskWidget(std::shared_ptr<vc::model::ITask> task,
                                               ads::CDockWidget *dock, QWidget *parent)
    : ITaskWidget(task, dock, parent),
      ui(new Ui::LocalizationTaskWidget)
{
    ui->setupUi(this);
    initWidget();
}

/// Stops all per-device threads before tearing down the widget. This moves
/// devices back to the main thread so DeviceManager can keep using them. The
/// TaskRunner stays alive (owned by ITask) so other task widgets / future
/// runs can re-enter Commission cleanly.
LocalizationTaskWidget::~LocalizationTaskWidget()
{
    // Stop all per-device threads before tearing down the widget.  This
    // moves devices back to the main thread so DeviceManager can keep
    // using them.  The TaskRunner stays alive (owned by ITask) so other
    // task widgets / future runs can re-enter Commission cleanly.
    if (m_localizeTask) m_localizeTask->endCommission();

    delete ui;
}

/// No-op override: this task has no config-to-task push path outside of
/// saveConfig()/setTaskLocalizeConfig().
void LocalizationTaskWidget::loadConfigToTask()  {}
/// No-op override: this task has no config-to-widget pull path implemented.
void LocalizationTaskWidget::loadConfigToWidget() {}

// ──────────────────────────────────────────────────────────────────────────────
//  initWidget
// ──────────────────────────────────────────────────────────────────────────────
/// Full widget initialisation: binds the underlying TaskLocalization and its
/// config, applies the theme stylesheets, builds the property browser / nav
/// panel / status lamps, wires device-manager and task-state signals, hooks
/// the breadcrumb runtime buttons, and begins the Commission phase for the
/// task before showing the Dashboard page.
/// @note starting Commission here spins up a per-device HighPriority QThread
/// for each currently assigned device.
void LocalizationTaskWidget::initWidget()
{
    if (m_task) {
        m_localizeTask = static_cast<vc::model::TaskLocalization*>(m_task.get());
        m_config       = m_localizeTask->taskLocalizeConfig();
    }

    setupThemeReload(QStringLiteral(":/styles/localization_task_widget_dark.qss"),
                     QStringLiteral(":/styles/localization_task_widget_light.qss"));

    // initPropertyBrowser(ui->wg_property_browser);
    initBrowserInWidget(ui->wg_property_browser);
    initNavPanel();
    initStatusLamps();
    // initContentStack();

    // Settings page: comm/output device, camera mapping
    // ui->ledit_comm_device->setReadOnly(true);
    // ui->ledit_output_device->setReadOnly(true);
    // ui->ledit_comm_device->setPlaceholderText(tr("Select communication device..."));
    // ui->ledit_output_device->setPlaceholderText(tr("Select output device..."));

    // connect(ui->tbtn_select_comm_device,   &QToolButton::clicked,
    //         this, &LocalizationTaskWidget::onSelectCommDeviceClicked);
    // connect(ui->tbtn_select_output_device, &QToolButton::clicked,
    //         this, &LocalizationTaskWidget::onSelectOutputDeviceClicked);

    if (m_localizeTask) {
        // updateCameraMappingToWidget();
        // updateCommDeviceInfo();
        // updateOutputDeviceInfo();

        auto *proj = m_localizeTask->project();
        connect(proj->deviceManager().get(), &vc::device::DeviceManager::devicesChanged,
                this, [this]() {
                  // ui->camera_mapping_wg->setCameraList(
                  //     m_localizeTask->project()->deviceManager()->cameraDevicesNameList());
                  // rebuildDeviceNav();
                  onTaskDevicesChanged();
        });
        connect(proj->deviceManager().get(), &vc::device::DeviceManager::deviceModified,
                this, &LocalizationTaskWidget::onAssignedDeviceModified);
        // connect(ui->camera_mapping_wg, &CameraMappingWidget::mappingChanged,
        //         this, &LocalizationTaskWidget::onCameraMappingChanged);

        connect(m_localizeTask, &vc::model::ITask::devicesChanged,
                this, &LocalizationTaskWidget::onTaskDevicesChanged);
        connect(m_localizeTask, &vc::model::ITask::taskStateChanged,
                this, [this](vc::model::TaskState) {
            updateTaskStateUi();
            updateStatusLamps();
            // Runners are created/destroyed across phase transitions, so the
            // per-device connection dots must re-bind to the new runner set.
            wireDeviceNavDots();
        });
    }

    // connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
    //         this, &LocalizationTaskWidget::onPropertyManagerValueChanged);

    // Toolbar buttons in breadcrumb
    connect(ui->tbtn_bc_enterRuntime,    &QToolButton::clicked,
            this, &LocalizationTaskWidget::onTbtnEnterRuntime);
    connect(ui->tbtn_bc_exitRuntime,    &QToolButton::clicked,
            this, &LocalizationTaskWidget::onTbtnExitRuntime);

    // Add device button
    connect(ui->tbtn_add_device, &QToolButton::clicked, this, [this]() {
        emit addDeviceRequested(m_task ? m_task->id() : QString());
    });

    // ── Enter Commission phase ────────────────────────────────────────────
    // Each assigned device gets its own HighPriority QThread so the GUI
    // never blocks while we configure / connect / single-shot.  When the
    // user adds a device later, devicesChanged → syncRunnersWithDevices()
    // registers it; TaskRunner auto-starts the new runner because we are
    // already past Idle.
    if (m_localizeTask) {
        m_localizeTask->beginCommission();
    }

    ui->nav_splitter->setStretchFactor(0, 3);
    ui->nav_splitter->setStretchFactor(1, 7);

    // Dashboard tab (default active)
    showDashboardPage();
    updateTaskStateUi();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Nav panel init
// ──────────────────────────────────────────────────────────────────────────────
/// Builds the left-hand nav panel: sizes the panel per the design handoff,
/// sets fonts/text for the task header labels, styles the add-device button,
/// configures the Dashboard/Patterns/Settings nav buttons (icons + mutually
/// exclusive QButtonGroup + click routing), lays out the scrollable device
/// list container, and styles the breadcrumb labels/buttons before calling
/// rebuildDeviceNav() to populate the device list.
void LocalizationTaskWidget::initNavPanel()
{
    // ── Per design handoff: nav panel is 188px wide ─────────────────────────
    // README.md → "Width 248px sidebar; left device nav 188px".
    if (ui->nav_panel) {
        ui->nav_panel->setMinimumWidth(188);
        ui->nav_panel->setMaximumWidth(260);
    }

    // Task header labels
    QFont sectionFont = ui->lbl_active_title->font();
    sectionFont.setPointSizeF(7.5);
    sectionFont.setBold(true);
    sectionFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    ui->lbl_active_title->setFont(sectionFont);
    ui->lbl_active_title->setText(tr("STATE"));

    QFont nameFont;
    nameFont.setFamily("JetBrains Mono");
    if (!nameFont.exactMatch()) nameFont.setFamily("Consolas");
    nameFont.setPointSizeF(10);
    nameFont.setBold(true);
    ui->lbl_task_name_nav->setFont(nameFont);
    ui->lbl_task_name_nav->setWordWrap(true);
    ui->lbl_task_name_nav->setText(m_task ? m_task->name() : tr("Task"));

    QFont chipFont = ui->lbl_task_type_chip->font();
    chipFont.setPointSizeF(8);
    chipFont.setBold(true);
    chipFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    ui->lbl_task_type_chip->setFont(chipFont);
    ui->lbl_task_type_chip->setText(tr("LOC"));

    // Devices section label
    QFont devSecFont = ui->lbl_devices_section->font();
    devSecFont.setPointSizeF(7.5);
    devSecFont.setBold(true);
    devSecFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    ui->lbl_devices_section->setFont(devSecFont);
    ui->lbl_devices_section->setText(tr("DEVICES"));

    // Add device button
    ui->tbtn_add_device->setIcon(svgIcon(":/resrc/icon/plus_square.svg", 14));
    ui->tbtn_add_device->setToolTip(tr("Add device to task"));

    // Nav buttons — order: Dashboard → Patterns → Settings.  All three
    // live in the .ui (vl_nav_top) so they share the same stylesheet
    // selectors and parent layout.
    ui->btn_nav_dashboard->setText(tr("  Dashboard"));
    ui->btn_nav_dashboard->setIcon(svgIcon(":/resrc/icon/dashboard.svg", 14));
    ui->btn_nav_dashboard->setIconSize({14, 14});

    ui->btn_nav_patterns->setText(tr("  Patterns"));
    ui->btn_nav_patterns->setIcon(svgIcon(":/resrc/icon/target.svg", 14));
    ui->btn_nav_patterns->setIconSize({14, 14});

    ui->btn_nav_settings->setText(tr("  Settings"));
    ui->btn_nav_settings->setIcon(svgIcon(":/resrc/icon/setting.svg", 14));
    ui->btn_nav_settings->setIconSize({14, 14});

    m_navBtnGroup = new QButtonGroup(this);
    m_navBtnGroup->addButton(ui->btn_nav_dashboard);
    m_navBtnGroup->addButton(ui->btn_nav_patterns);
    m_navBtnGroup->addButton(ui->btn_nav_settings);
    m_navBtnGroup->setExclusive(true);

    connect(ui->btn_nav_dashboard, &QPushButton::clicked,
            this, &LocalizationTaskWidget::showDashboardPage);
    connect(ui->btn_nav_patterns,  &QPushButton::clicked,
            this, &LocalizationTaskWidget::showPatternsPage);
    connect(ui->btn_nav_settings,  &QPushButton::clicked,
            this, &LocalizationTaskWidget::showSettingsPage);

    // Device scroll content widget
    m_devListWidget = new QWidget(ui->scroll_devices_content);
    auto *devLayout = new QVBoxLayout(m_devListWidget);
    devLayout->setContentsMargins(8, 0, 8, 4);
    devLayout->setSpacing(1);

    auto *outerLayout = new QVBoxLayout(ui->scroll_devices_content);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_devListWidget);

    // Breadcrumb labels
    QFont bcMonoFont;
    bcMonoFont.setFamily("JetBrains Mono");
    if (!bcMonoFont.exactMatch()) bcMonoFont.setFamily("Consolas");

    bcMonoFont.setPointSizeF(8.5);
    ui->lbl_bc_task->setFont(bcMonoFont);

    QFont bcSepFont = ui->lbl_bc_sep->font();
    bcSepFont.setPointSizeF(13);
    ui->lbl_bc_sep->setFont(bcSepFont);

    bcMonoFont.setPointSizeF(9.5);
    bcMonoFont.setBold(true);
    ui->lbl_bc_current->setFont(bcMonoFont);

    ui->lbl_bc_task->setText(m_task ? m_task->name() : QString());

    // Breadcrumb toolbar buttons
    ui->tbtn_bc_enterRuntime->setIcon(svgIcon(":/resrc/icon/start.svg", 20));
    ui->tbtn_bc_enterRuntime->setToolTip(tr("Start"));

    ui->tbtn_bc_exitRuntime->setIcon(svgIcon(":/resrc/icon/stop.svg", 20));
    ui->tbtn_bc_exitRuntime->setIconSize({20, 20});
    ui->tbtn_bc_exitRuntime->setToolTip(tr("Stop"));

    rebuildDeviceNav();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Status lamps
// ──────────────────────────────────────────────────────────────────────────────
/// Dynamically builds the four status-lamp columns (READY, CAM, PLC, OUT),
/// each a dot + text label stacked vertically, and appends them into
/// `ui->frame_status_lamps`'s layout. Populates m_statusLamps with the
/// created widgets and calls updateStatusLamps() to set their initial state.
void LocalizationTaskWidget::initStatusLamps()
{
    const QStringList labels = { "READY", "CAM", "PLC", "OUT" };

    auto *hl = qobject_cast<QHBoxLayout*>(ui->frame_status_lamps->layout());
    if (!hl) return;

    hl->addStretch();
    for (const auto &label : labels) {
        // Vertical pair: dot + text
        auto *col     = new QWidget(ui->frame_status_lamps);
        auto *colLay  = new QVBoxLayout(col);
        colLay->setContentsMargins(0, 0, 0, 0);
        colLay->setSpacing(3);
        colLay->setAlignment(Qt::AlignHCenter);

        auto *dot = new StatusLampDot(col);
        dot->setFixedSize(10, 10);

        QFont lf;
        lf.setPointSizeF(6.5);
        lf.setBold(true);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        auto *lbl = new StatusTextLabel(col);
        lbl->setText(label);
        lbl->setFont(lf);
        lbl->setAlignment(Qt::AlignCenter);

        colLay->addWidget(dot, 0, Qt::AlignHCenter);
        colLay->addWidget(lbl, 0, Qt::AlignHCenter);

        hl->addWidget(col);
        m_statusLamps.append({dot, lbl});
    }
    hl->addStretch();

    updateStatusLamps();
}

/// Recomputes and applies the four status lamps' "lampState" property
/// (on/warn/error/off) from the currently assigned devices' types (CAM/PLC/OUT
/// presence) and the task's current TaskState (READY lamp, via
/// readyLampStateForTask()), then re-polishes each lamp widget so the
/// stylesheet picks up the new property value.
/// @note the localization task has no robot; the OUT lamp tracks the
/// assigned Vision Output device instead.
void LocalizationTaskWidget::updateStatusLamps()
{
    // States: [0]=READY, [1]=CAM, [2]=PLC, [3]=OUT
    // The localization task has no robot; the fourth lamp tracks the assigned
    // Vision Output device instead.
    bool hasCam = false, hasPlc = false, hasOutput = false;
    QString readyLampState = QStringLiteral("off");

    if (m_localizeTask && m_localizeTask->project()) {
        auto *mgr = m_localizeTask->project()->deviceManager().get();
        for (const QString &id : m_localizeTask->assignedDeviceIds()) {
            auto dev = mgr->deviceById(id);
            if (!dev) continue;
            switch (dev->deviceType()) {
            case vc::device::DeviceType::Camera:       hasCam = true; break;
            case vc::device::DeviceType::PLC:          hasPlc = true; break;
            case vc::device::DeviceType::VisionOutput: hasOutput = true; break;
            default: break;
            }
        }
    }

    auto repolish = [](QWidget *w) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    };

    if (m_localizeTask) {
        readyLampState = readyLampStateForTask(m_localizeTask->taskState());
    }

    for (int i = 0; i < m_statusLamps.size(); ++i) {
        QString state = QStringLiteral("off");
        if (i == 0) {
            state = readyLampState;
        } else if (i == 1) {
            state = hasCam ? QStringLiteral("on") : QStringLiteral("off");
        } else if (i == 2) {
            state = hasPlc ? QStringLiteral("on") : QStringLiteral("off");
        } else if (i == 3) {
            state = hasOutput ? QStringLiteral("on") : QStringLiteral("off");
        }
        m_statusLamps[i].dot->setProperty("lampState", state);
        m_statusLamps[i].label->setProperty("lampState", state);
        repolish(m_statusLamps[i].dot);
        repolish(m_statusLamps[i].label);
    }
}

/// Refreshes the nav-panel state label ("STATE  <NAME>"), its "taskState"
/// stylesheet property, and its tooltip from the underlying task's current
/// TaskState (Idle if there is no bound task), then re-polishes the label.
void LocalizationTaskWidget::updateTaskStateUi()
{
    const vc::model::TaskState state =
        m_localizeTask ? m_localizeTask->taskState() : vc::model::TaskState::Idle;

    ui->lbl_active_title->setText(
        tr("STATE  %1").arg(taskStateDisplayName(state).toUpper()));
    ui->lbl_active_title->setProperty("taskState", taskStateToString(state));
    ui->lbl_active_title->setToolTip(taskStateDisplayName(state));
    ui->lbl_active_title->style()->unpolish(ui->lbl_active_title);
    ui->lbl_active_title->style()->polish(ui->lbl_active_title);
    ui->lbl_active_title->update();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Content stack init
// ──────────────────────────────────────────────────────────────────────────────
/// Currently a no-op placeholder: the dashboard-page insertion logic it once
/// held is commented out below (dashboard construction now happens lazily in
/// showDashboardPage() instead).
void LocalizationTaskWidget::initContentStack()
{
    // showDashboardPage();

    // Insert a dashboard placeholder at index 0
    // page_task_config (from .ui) shifts to index kSettingsPage = 1
    // m_dashboardPage = new QWidget(this);
    // m_dashboardPage->setStyleSheet(QString("background: %1;").arg(ptn::BG));
    // auto *dashLayout = new QVBoxLayout(m_dashboardPage);
    // dashLayout->setAlignment(Qt::AlignCenter);
    // auto *dashLbl = new QLabel(tr("Dashboard"), m_dashboardPage);
    // QFont f = dashLbl->font();
    // f.setPointSize(12);
    // dashLbl->setFont(f);
    // dashLbl->setAlignment(Qt::AlignCenter);
    // dashLbl->setStyleSheet(QString("color: %1;").arg(ptn::TXT3));
    // dashLayout->addWidget(dashLbl);

    // ui->content_stack->insertWidget(kDashboardPage, m_dashboardPage);
    // Now: 0=dashboard, 1=page_task_config (settings)
}

/// Reacts to the task's assigned-device set changing: removes and deletes
/// content-stack pages (and their property-browser entries) for any device
/// that is no longer assigned, resyncs the TaskRunner's per-device runners
/// with the new set (starting/stopping as needed), and rebuilds the device
/// nav list.
void LocalizationTaskWidget::onTaskDevicesChanged() {
    QStringList deviceIds = m_localizeTask->assignedDeviceIds();
    // Iterate from the top down: removeWidget() shifts higher indices, so a
    // forward loop would skip the page that slides into a just-vacated slot
    // (two consecutive removed device pages left a stale widget behind).
    for (int idx = ui->content_stack->count() - 1; idx >= 0; --idx) {
        QWidget *w = ui->content_stack->widget(idx);
        IDeviceWidget *dw = qobject_cast<IDeviceWidget*>(w);
        if (dw) {
            const QString deviceId = dw->deviceId();
            // Only tear down pages for devices that are no longer assigned.
            // These calls used to run for every page found — including still-live
            // ones — desyncing m_devicePages from the stack and orphaning pages.
            if (!deviceIds.contains(deviceId)) {
                ui->content_stack->removeWidget(w);
                w->deleteLater();
                removePropertyBrowserWidget(dw->getPropertyBrowser());
                m_devicePages.remove(deviceId);
            }
        }
    }

    // Sync runners with the new assigned-device set.  TaskRunner will:
    //   - register + start a runner for any newly added device
    //   - unregister + stop the runner for any removed device
    if (m_localizeTask) m_localizeTask->resyncDevices();

    rebuildDeviceNav();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Device nav rebuild
// ──────────────────────────────────────────────────────────────────────────────
/// Clears and rebuilds the device-nav list widget from the task's assigned
/// devices: sorts them by DeviceType (Camera → PLC → VisionOutput → Robot)
/// then by name, creates a DeviceNavItemWidget per device wired to
/// onDeviceNavClicked(), shows a "No devices assigned" placeholder label when
/// the list is empty, and refreshes nav-item styles, status lamps, and the
/// device-nav connection dots afterward.
void LocalizationTaskWidget::rebuildDeviceNav()
{
    // Clear existing items
    QLayout *lay = m_devListWidget->layout();
    while (QLayoutItem *it = lay->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    m_navItems.clear();

    if (!m_localizeTask || !m_localizeTask->project()) return;

    auto *mgr = m_localizeTask->project()->deviceManager().get();

    // Resolve and sort assigned devices by type (Camera → PLC → VisionOutput →
    // Robot, matching the DeviceType enum order), then by name within a type, so
    // the nav order is grouped and stable across rebuilds.
    QList<std::shared_ptr<vc::device::IDevice>> devices;
    for (const QString &devId : m_localizeTask->assignedDeviceIds()) {
        if (auto device = mgr->deviceById(devId))
            devices.append(device);
    }
    std::sort(devices.begin(), devices.end(),
              [](const std::shared_ptr<vc::device::IDevice> &a,
                 const std::shared_ptr<vc::device::IDevice> &b) {
                  if (a->deviceType() != b->deviceType())
                      return a->deviceType() < b->deviceType();
                  return a->name().localeAwareCompare(b->name()) < 0;
              });

    const bool anyDevice = !devices.isEmpty();

    for (const auto &device : devices) {
        const QString devId = device->id();

        auto *item = new DeviceNavItemWidget(m_devListWidget);
        item->setDevice(device);
        connect(item, &DeviceNavItemWidget::activated,
                this, &LocalizationTaskWidget::onDeviceNavClicked);

        m_navItems.insert(devId, item);
        lay->addWidget(item);
    }

    if (!anyDevice) {
        auto *emptyLbl = new QLabel(tr("No devices assigned.\nUse [+] to add."),
                                    m_devListWidget);
        emptyLbl->setProperty("navPart", QStringLiteral("empty"));
        emptyLbl->setAlignment(Qt::AlignCenter);
        QFont f = emptyLbl->font();
        f.setPointSizeF(8.5);
        emptyLbl->setFont(f);
        emptyLbl->setWordWrap(true);
        emptyLbl->setContentsMargins(4, 8, 4, 8);
        lay->addWidget(emptyLbl);
    }

    static_cast<QVBoxLayout*>(lay)->addStretch();
    refreshNavItemStyles();
    updateStatusLamps();
    wireDeviceNavDots();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Device nav connection dots
// ──────────────────────────────────────────────────────────────────────────────
/// (Re)subscribes each device-nav connection dot to its runner's
/// connectStatusChanged signal: drops all previous subscriptions, then for
/// each current nav item looks up its runner via TaskRunner::runnerFor(),
/// seeds the dot from the device's current connectStatus(), and connects the
/// runner's signal to update it going forward. Called after a nav rebuild
/// and on task-state changes, since runners are created/destroyed across
/// phase transitions.
void LocalizationTaskWidget::wireDeviceNavDots()
{
    // Drop previous subscriptions before re-binding (dots may have been
    // recreated by a rebuild, or runners replaced by a phase change).
    for (const auto &conn : std::as_const(m_navDotConns))
        QObject::disconnect(conn);
    m_navDotConns.clear();

    auto *taskRunner = m_localizeTask ? m_localizeTask->taskRunner() : nullptr;

    for (auto it = m_navItems.cbegin(); it != m_navItems.cend(); ++it) {
        const QString &devId = it.key();
        DeviceNavItemWidget *item = it.value();
        if (!item) continue;

        auto *runner = taskRunner ? taskRunner->runnerFor(devId) : nullptr;
        if (!runner || !runner->device()) {
            item->setIndicatorState(QStringLiteral("off"));
            continue;
        }

        // Seed from current status (plain enum read; corrected by the next
        // forwarded change). The runner forwards connectStatusChanged onto the
        // GUI thread, and the nav item is the connection context so it
        // auto-detaches when the item is destroyed.
        item->setConnectStatus(runner->device()->connectStatus());
        m_navDotConns.append(connect(
            runner, &vc::runtime::IDeviceRunner::connectStatusChanged, item,
            [item](vc::device::ConnectStatus status) {
                item->setConnectStatus(status);
            }));
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  Navigation
// ──────────────────────────────────────────────────────────────────────────────
/// Switches the content stack to the Dashboard page, lazily constructing a
/// LocalizationDashboardWidget (or a bare placeholder QWidget if there is no
/// bound task) on first visit. Clears the active device selection, checks
/// the Dashboard nav button, updates the breadcrumb, and hides the property
/// browser.
void LocalizationTaskWidget::showDashboardPage()
{
    // Lazy-construct the patterns widget the first time the user clicks here.
    // This matches the design handoff (`TaskWorkspace.jsx → "Patterns tab"`),
    // and keeps the patterns widget cheap when never viewed.
    if (!m_dashboardPage) {
        if (m_localizeTask) {
            m_dashboardPage = new LocalizationDashboardWidget(m_task, nullptr, this);
        } else {
            m_dashboardPage = new QWidget(this);
        }
        ui->content_stack->insertWidget(kDashboardPage, m_dashboardPage);
    }

    m_activeDeviceId.clear();
    // Navigate by widget pointer (not the fixed kDashboardPage index): pages are
    // inserted lazily in click order, so a page's real index can differ from its
    // preferred slot. setCurrentWidget() is always correct.
    ui->content_stack->setCurrentWidget(m_dashboardPage);
    ui->btn_nav_dashboard->setChecked(true);
    refreshNavItemStyles();
    updateBreadcrumb(tr("Dashboard"), QStringLiteral("accent"));
    ui->wg_property_browser->setVisible(false);
}

/// Switches the content stack to the Settings page, lazily constructing a
/// LocalizationSettingWidget (or a bare placeholder QWidget if there is no
/// bound task) on first visit. Clears the active device selection, checks
/// the Settings nav button, updates the breadcrumb, and hides the property
/// browser.
void LocalizationTaskWidget::showSettingsPage()
{
    // Lazy-construct the patterns widget the first time the user clicks here.
    // This matches the design handoff (`TaskWorkspace.jsx → "Patterns tab"`),
    // and keeps the patterns widget cheap when never viewed.
    if (!m_settingPage) {
        if (m_localizeTask) {
            m_settingPage = new LocalizationSettingWidget(m_task, nullptr, this);
        } else {
            m_settingPage = new QWidget(this);
        }
        ui->content_stack->insertWidget(kSettingsPage, m_settingPage);
    }

    m_activeDeviceId.clear();
    // Navigate by widget pointer (not the fixed kSettingsPage index) — see
    // showDashboardPage() for why the real index can differ from the slot.
    ui->content_stack->setCurrentWidget(m_settingPage);
    ui->btn_nav_settings->setChecked(true);
    refreshNavItemStyles();
    updateBreadcrumb(tr("Settings"), QStringLiteral("muted"));
    // populateBrowser();
    ui->wg_property_browser->setVisible(false);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Patterns page (LocalizationTask only)
// ──────────────────────────────────────────────────────────────────────────────
/// Switches the content stack to the Patterns page (LocalizationTask only),
/// lazily constructing a LocalizationPatternsWidget (or a bare placeholder
/// QWidget if there is no bound task) on first visit. Clears the active
/// device selection, checks the Patterns nav button, updates the breadcrumb,
/// and hides the property browser.
void LocalizationTaskWidget::showPatternsPage()
{
    // Lazy-construct the patterns widget the first time the user clicks here.
    // This matches the design handoff (`TaskWorkspace.jsx → "Patterns tab"`),
    // and keeps the patterns widget cheap when never viewed.
    if (!m_patternsPage) {
        if (m_localizeTask) {
            m_patternsPage = new LocalizationPatternsWidget(m_task, nullptr, this);
        } else {
            m_patternsPage = new QWidget(this);
        }
        ui->content_stack->insertWidget(kPatternsPage, m_patternsPage);
    }

    m_activeDeviceId.clear();
    ui->content_stack->setCurrentWidget(m_patternsPage);
    ui->btn_nav_patterns->setChecked(true);
    refreshNavItemStyles();
    updateBreadcrumb(tr("Patterns"), QStringLiteral("accent"));

    ui->wg_property_browser->setVisible(false);
}

/// Switches the content stack to the config page for `deviceId` (creating it
/// on demand via getOrCreateDeviceConfigPage()), unchecks the Dashboard/
/// Settings nav buttons, updates the breadcrumb with the device's name and
/// device-type colour role, populates the property browser for the device,
/// shows the property browser, and resizes the content stack/scroll area to
/// the page's minimum size hint.
/// @param deviceId id of the device whose config page should become active;
/// no-op if the page cannot be resolved/created
void LocalizationTaskWidget::showDeviceConfigPage(const QString &deviceId)
{
    QWidget *page = getOrCreateDeviceConfigPage(deviceId);
    if (!page) return;

    m_activeDeviceId = deviceId;
    ui->content_stack->setCurrentWidget(page);

    // Uncheck nav buttons
    ui->btn_nav_dashboard->setChecked(false);
    ui->btn_nav_settings->setChecked(false);

    refreshNavItemStyles();

    // Update breadcrumb — use device-type role for colour
    QString label;
    QString role = QStringLiteral("default");
    if (m_localizeTask && m_localizeTask->project()) {
        auto dev = m_localizeTask->project()->deviceById(deviceId);
        if (dev) {
            label = dev->name();
            role  = DeviceNavItemWidget::roleForDeviceType(dev->deviceType());
        }
    }
    updateBreadcrumb(label.isEmpty() ? deviceId : label, role);
    populateBrowser(deviceId);

    ui->wg_property_browser->setVisible(true);

    // QSize property_browser_base_size = ui->wg_property_browser->baseSize();
    // property_browser_base_size.setWidth(300);
    // ui->wg_property_browser->setBaseSize(property_browser_base_size);

    ui->content_stack->setMinimumSize(page->minimumSizeHint());
    ui->scrollArea->setWidgetResizable(true);
}

/// Slot for DeviceNavItemWidget::activated: shows the config page for the
/// clicked device.
void LocalizationTaskWidget::onDeviceNavClicked(const QString &deviceId)
{
    showDeviceConfigPage(deviceId);
}

/// Slot for the breadcrumb "Start" tool button: enters Runtime (non-auto)
/// if the task is currently in the Commission phase.
void LocalizationTaskWidget::onTbtnEnterRuntime() {
    if (m_localizeTask->taskState() == vc::model::TaskState::Commission) {
        m_localizeTask->beginRuntime(false);
    }
}

/// Slot for the breadcrumb "Stop" tool button: ends Runtime if the task is
/// not currently in Commission, then (re-)enters Commission.
void LocalizationTaskWidget::onTbtnExitRuntime() {
    if (m_localizeTask->taskState() != vc::model::TaskState::Commission) {
        m_localizeTask->endRuntime();
    }
    m_localizeTask->beginCommission();
}

/// Updates each device-nav item's selected/highlighted style to reflect
/// whether its device id matches m_activeDeviceId.
void LocalizationTaskWidget::refreshNavItemStyles()
{
    for (auto it = m_navItems.cbegin(); it != m_navItems.cend(); ++it) {
        DeviceNavItemWidget *item   = it.value();
        const bool active = (it.key() == m_activeDeviceId);
        item->setSelected(active);
    }
}

/// Sets the breadcrumb's current-item label text and its "breadcrumbRole"
/// stylesheet property (used to colour it by page/device-type), then
/// re-polishes the label.
/// @param label text to show in the breadcrumb
/// @param role stylesheet role used to colour the breadcrumb (e.g. "accent",
/// "muted", or a device-type role from DeviceNavItemWidget::roleForDeviceType())
void LocalizationTaskWidget::updateBreadcrumb(const QString &label, const QString &role)
{
    ui->lbl_bc_current->setText(label);
    ui->lbl_bc_current->setProperty("breadcrumbRole", role);
    ui->lbl_bc_current->style()->unpolish(ui->lbl_bc_current);
    ui->lbl_bc_current->style()->polish(ui->lbl_bc_current);
    ui->lbl_bc_current->update();
}

/// Reacts to DeviceManager::deviceModified for a device assigned to this
/// task: rebuilds the device nav (to pick up name/type changes), and if the
/// modified device is the currently active one, refreshes the breadcrumb
/// with its (possibly new) name and device-type colour role. No-op if the
/// device isn't assigned to this task or can't be resolved.
/// @param deviceId id of the device that was modified
void LocalizationTaskWidget::onAssignedDeviceModified(const QString &deviceId)
{
    if (!m_localizeTask || !m_localizeTask->hasDevice(deviceId))
        return;

    auto proj = m_localizeTask->project();
    if (!proj)
        return;

    auto dev = proj->deviceById(deviceId);
    if (!dev)
        return;

    rebuildDeviceNav();

    if (m_activeDeviceId == deviceId) {
        updateBreadcrumb(dev->name(),
                         DeviceNavItemWidget::roleForDeviceType(dev->deviceType()));
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────────────────────────────────────
/// Returns the cached content-stack page for `deviceId`, creating it on
/// first request: resolves the device from the project and its runner from
/// TaskRunner::runnerFor() (the widget never creates a worker/thread itself),
/// builds the page via DeviceWidgetFactory::createDeviceWidget() — falling
/// back to a plain "no configuration panel available" placeholder label if
/// the factory returns nullptr — adds it to the content stack, and caches it
/// in m_devicePages.
/// @param deviceId id of the device to get/create a config page for
/// @return the page widget, or nullptr if there is no task/project or the
/// device id can't be resolved
QWidget *LocalizationTaskWidget::getOrCreateDeviceConfigPage(const QString &deviceId)
{
    if (m_devicePages.contains(deviceId))
        return m_devicePages.value(deviceId);

    if (!m_localizeTask || !m_localizeTask->project()) return nullptr;

    auto device = m_localizeTask->project()->deviceById(deviceId);
    if (!device) return nullptr;

    // ── Resolve the typed runner from TaskRunner ─────────────────────────
    // The Widget never creates a worker / thread.  TaskRunner owns the
    // runner (and its QThread); we just look it up here and inject it.
    auto *taskRunner = m_localizeTask->taskRunner();
    vc::runtime::IDeviceRunner *runner =
        taskRunner ? taskRunner->runnerFor(deviceId) : nullptr;

    QWidget *page = DeviceWidgetFactory::createDeviceWidget(device, runner, nullptr, this);
    if (!page) {
        page = new QWidget(this);
        auto *l   = new QVBoxLayout(page);
        auto *lbl = new QLabel(tr("No configuration panel available for this device."), page);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral("font-size: 11pt;"));
        l->addWidget(lbl);
    }

    ui->content_stack->addWidget(page);
    m_devicePages.insert(deviceId, page);
    return page;
}

// ──────────────────────────────────────────────────────────────────────────────
//  Property browser population
// ──────────────────────────────────────────────────────────────────────────────
/// Points the shared property browser at the device page cached for `id`:
/// swaps in that page's own property-browser widget if it implements
/// IDeviceWidget, otherwise falls back to the default (empty) browser. No-op
/// if no page is cached for `id`.
/// @param id id of the device whose page's property browser should be shown
void LocalizationTaskWidget::populateBrowser(const QString &id)
{
    QWidget *w = m_devicePages.value(id, nullptr);
    if (w) {
        IDeviceWidget *dw = qobject_cast<IDeviceWidget*>(w);
        if (dw) {
            changePropertyBrowserWidget(dw->getPropertyBrowser());
        } else {
            changePropertyBrowserDefault();
        }
    }
}

/// Pushes the widget's current m_config back into the bound task via
/// TaskLocalization::setTaskLocalizeConfig(). No-op if there is no bound
/// localization task.
void LocalizationTaskWidget::saveConfig()
{
    if (m_localizeTask) m_localizeTask->setTaskLocalizeConfig(m_config);
}
