#include "runtime_app/ui/runtime_shell_window.h"
#include "ui_runtime_shell_window.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>  // layout_dock_host, declared in the .ui

#include "runtime_app/src/runtime_layout_controller.h"

#include "core/app_settings/app_settings.h"
#include "core/app_version.h"
#include "core/logger/app_logger.h"
#include "core/utils/shell_handoff.h"
#include "core/utils/theme_manager.h"
#include "model/project_repository.h"
#include "model/task_localization.h"
#include "ui/forms/shell_startup.h"
#include "ui/forms/system_log_form.h"
#include "ui/forms/task/localization_dashboard_widget.h"

using vc::model::Project;
using vc::model::ProjectRepository;
using vc::model::TaskLocalization;

const int RuntimeShellWindow::kMaxTasks = RuntimeLayoutController::kMaxTiles;

/// Builds the menus and the dock host. The project is loaded by beginStartup(), after the
/// window is on screen — see that function for why the two are separated.
RuntimeShellWindow::RuntimeShellWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::RuntimeShellWindow)
{
    ui->setupUi(this);

    ui->lbl_version->setText(tr("Version %1").arg(vc::version::applicationVersion()));

    // Same action as Project → Load…, deliberately. The project-select page is what an
    // operator lands on when there is nothing to run, and the menu bar is not where they
    // will look for the way out of it — the page has a button that says Browse. It has to
    // do something.
    connect(ui->btn_browse, &QPushButton::clicked, this, &RuntimeShellWindow::onLoadProject);

    buildMenus();
    buildDockHost();
    reloadStyleSheet();

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &styleId, bool) {
                syncThemeAction(styleId);
                reloadStyleSheet();
            });

    // What the operator sees while beginStartup() runs. The form starts on the
    // project-select page, so this is the first painted frame — and on a launch that came
    // from the commissioning shell it is the difference between "the switch did nothing"
    // and "it is coming up".
    const QString remembered = AppSettings::instance()->lastRuntimeProjectPath();
    if (!remembered.isEmpty()) {
        ui->lbl_last_path->setText(tr("Last project: %1").arg(remembered));
        ui->lbl_reason->setText(tr("Starting the last project…"));
    }
}

/// Ends runtime on every task before the docks (and the dashboards inside them) go away.
RuntimeShellWindow::~RuntimeShellWindow()
{
    stopTaskRuntimes();
    delete ui;
}

/// Ends runtime on close so device threads stop before the process exits rather than
/// being torn down underneath a running cycle.
void RuntimeShellWindow::closeEvent(QCloseEvent *event)
{
    stopTaskRuntimes();
    QMainWindow::closeEvent(event);
}

/// Wires the menu actions and installs the shared Theme/Language submenus.
void RuntimeShellWindow::buildMenus()
{
    connect(ui->action_load_project,  &QAction::triggered, this, &RuntimeShellWindow::onLoadProject);
    connect(ui->action_close_project, &QAction::triggered, this, &RuntimeShellWindow::onCloseProject);
    connect(ui->action_open_editor,   &QAction::triggered, this, &RuntimeShellWindow::onOpenEditor);
    connect(ui->action_system_log,    &QAction::triggered, this, &RuntimeShellWindow::onSystemLogAction);

    // The tile counts are declared as five checkable actions in the .ui; the exclusivity
    // is the one thing Designer cannot express, so the group is made here. Each action
    // carries its tile count as data() rather than the slot mapping from the action name.
    m_layoutGroup = new QActionGroup(this);
    m_layoutGroup->setExclusive(true);

    const QList<QAction *> layoutActions = {
        ui->action_tiles_1, ui->action_tiles_2, ui->action_tiles_4,
        ui->action_tiles_6, ui->action_tiles_8
    };
    const QList<int> tileCounts = RuntimeLayoutController::supportedTileCounts();
    Q_ASSERT(layoutActions.size() == tileCounts.size());

    for (int i = 0; i < layoutActions.size(); ++i) {
        layoutActions.at(i)->setData(tileCounts.at(i));
        m_layoutGroup->addAction(layoutActions.at(i));
    }
    connect(m_layoutGroup, &QActionGroup::triggered,
            this, &RuntimeShellWindow::onLayoutActionTriggered);

    // Theme and Language are declared in runtime_shell_window.ui — menus and actions both.
    // Only the exclusivity and the behaviour are wired here; Designer can express neither.
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    ui->action_theme_light->setData(QStringLiteral("light"));
    ui->action_theme_dark->setData(QStringLiteral("dark"));
    m_themeGroup->addAction(ui->action_theme_light);
    m_themeGroup->addAction(ui->action_theme_dark);

    connect(m_themeGroup, &QActionGroup::triggered, this, [](QAction *act) {
        ThemeManager::instance()->applyStyle(act->data().toString());
    });

    m_languageGroup = new QActionGroup(this);
    m_languageGroup->setExclusive(true);
    ui->action_language_english->setData(QStringLiteral("en"));
    ui->action_language_japanese->setData(QStringLiteral("ja_JP"));
    m_languageGroup->addAction(ui->action_language_english);
    m_languageGroup->addAction(ui->action_language_japanese);

    connect(m_languageGroup, &QActionGroup::triggered,
            this, &RuntimeShellWindow::onLanguageActionTriggered);

    syncThemeAction(ThemeManager::instance()->currentStyleId());
    // "system" is a legacy value meaning "follow the OS locale"; there is no menu entry for
    // it, so anything that is not ja_JP shows as English — which is what those launches
    // actually produce.
    const QString savedLang = AppSettings::instance()->language();
    ui->action_language_japanese->setChecked(savedLang == QLatin1String("ja_JP"));
    ui->action_language_english->setChecked(savedLang != QLatin1String("ja_JP"));

    setRuntimeActionsEnabled(false);
}

/// Checks the Theme action matching `styleId`.
///
/// Follows ThemeManager rather than assuming these actions are the only way the theme can
/// change: the commissioning shell writes the same settings file, and applyStyle() can be
/// called from anywhere.
///
/// @warning No QSignalBlocker here, for the same reason as selectLayoutAction(): blocking
///          an action's signals hides the state change from its QActionGroup, which leaves
///          the group's exclusivity bookkeeping pointing at the wrong action.
void RuntimeShellWindow::syncThemeAction(const QString &styleId)
{
    if (m_themeGroup == nullptr) {
        return;
    }
    for (QAction *action : m_themeGroup->actions()) {
        action->setChecked(action->data().toString() == styleId);
    }
}

/// Slot: persists the chosen language and says plainly that it applies on the next start.
/// The translator is installed once, before any widget exists, so there is nothing to
/// re-translate in place.
void RuntimeShellWindow::onLanguageActionTriggered(QAction *action)
{
    AppSettings::instance()->setLanguage(action->data().toString());
    QMessageBox::information(
        this,
        tr("Language Changed"),
        tr("The language change will take effect the next time the application is started."));
}

/// Creates the ADS dock manager inside the form's dock host.
void RuntimeShellWindow::buildDockHost()
{
    ads::CDockManager::setConfigFlags(ads::CDockManager::DefaultOpaqueConfig);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton, false);

    m_dockManager = new ads::CDockManager(ui->wg_dock);
    // Disable ADS's internal stylesheet; the global QSS owns all ads-- rules. Same call and
    // same reason as app/mainwindow.cpp.
    m_dockManager->setStyleSheet(QString());

    // Same widget tree as the commissioning shell's dock host — wg_dock, a QVBoxLayout,
    // the dock manager — but the layout is declared in runtime_shell_window.ui rather than
    // built here, which is what ui_design_rules.md Rule 1.1 requires. (app/mainwindow.cpp
    // still builds its own; that is backlog item #37, not a pattern to copy.)
    //
    // Being a CHILD of wg_dock is not enough: the dock manager also has to be IN a layout,
    // or Qt leaves it at its size hint and never resizes it.
    ui->layout_dock_host->addWidget(m_dockManager);
}

/// Loads and applies this window's stylesheet for the active theme.
///
/// One sheet for both themes: every colour in it is a @{token} and tokens resolve per
/// theme, so a light and a dark copy would be identical. The RESOLVED output still differs,
/// which is why this runs again on every theme change.
void RuntimeShellWindow::reloadStyleSheet()
{
    QFile f(QStringLiteral(":/styles/runtime_shell_window.qss"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_DEV_ERR << "Runtime shell stylesheet could not be opened.";
        return;
    }
    // Applied to the project-select page, not to the whole window. Every rule in the sheet
    // targets a widget on that page, and a stylesheet set on a QMainWindow re-resolves
    // styling for its entire subtree — including the menu bar, the status bar and the ADS
    // docks, none of which this sheet says anything about and all of which the global
    // theme already styles.
    ui->page_project_select->setStyleSheet(
        ThemeManager::instance()->resolveTokens(QString::fromUtf8(f.readAll())));
}

/// Loads the remembered project once the window is up. See the header for why this is not
/// part of the constructor.
void RuntimeShellWindow::beginStartup()
{
    // Paint the window before blocking the GUI thread for seconds. show() alone only
    // queues the first paint, and Windows delivers WM_PAINT when the queue is otherwise
    // empty — which it will not be again until the project is loaded and every device is
    // open. Without this the window is in front but blank, which reads as a hang.
    // repaint() is synchronous and processes no events, so it cannot re-enter anything.
    repaint();

    // Project → Load… and Project → Open Editor both open modal dialogs, and a modal dialog
    // spins a nested event loop that can dispatch a second menu trigger straight back into
    // startProject() while this one is still inside stopTaskRuntimes() — which iterates
    // m_runningTaskIds and m_taskDocks and clears them only afterwards. That is a
    // use-after-clear on both containers. It was unreachable while this ran in the
    // constructor with no window on screen; showing the window first makes it reachable, so
    // the two actions are held closed for the duration.
    //
    // setEnabled(), never signal blocking: blocking a QAction's signals breaks the
    // QActionGroup bookkeeping, which is a separate defect this shell has already had once.
    setStartupControlsEnabled(false);
    tryStartRememberedProject();
    setStartupControlsEnabled(true);
}

/// Reads the remembered project path and routes to the runtime page or the select page.
/// The three failure modes get three different messages on purpose: "never ran one",
/// "the file has moved" and "the file would not load" call for three different actions,
/// and a single generic message costs the operator the time to work out which it is.
void RuntimeShellWindow::tryStartRememberedProject()
{
    const QString remembered = AppSettings::instance()->lastRuntimeProjectPath();

    if (remembered.isEmpty()) {
        showProjectSelect(tr("No project has been run yet. Choose one to begin."));
        return;
    }

    ui->lbl_last_path->setText(tr("Last project: %1").arg(remembered));

    if (!QFileInfo::exists(remembered)) {
        LOG_USER_WARN << "Remembered runtime project is missing." << remembered;
        showProjectSelect(tr("The last project file could not be found. It may have been "
                             "moved, renamed, or its drive is not connected."));
        return;
    }

    if (!startProject(remembered)) {
        showProjectSelect(tr("The last project file exists but could not be loaded: %1")
                              .arg(ProjectRepository::lastMsg()));
    }
}

/// Slot: opens a file dialog next to the remembered project and starts whatever is chosen.
void RuntimeShellWindow::onLoadProject()
{
    const QString remembered = AppSettings::instance()->lastRuntimeProjectPath();
    const QString startDir = remembered.isEmpty() ? QString()
                                                  : QFileInfo(remembered).absolutePath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        startDir,
        tr("Vision Project (*.vproj)"));

    if (path.isEmpty()) {
        return;
    }

    if (!startProject(path)) {
        QMessageBox::critical(this,
                              tr("Open Project error"),
                              tr("Cannot open project:\n%1").arg(ProjectRepository::lastMsg()));
        showProjectSelect(tr("The selected project could not be loaded: %1")
                              .arg(ProjectRepository::lastMsg()));
    }
}

/// Slot: stops every running task and returns to the project-select page.
///
/// Never leaves a blank window: this shell owns the screen on a field machine, so the way
/// out of a project has to land somewhere the operator can act from.
void RuntimeShellWindow::onCloseProject()
{
    if (!m_project) {
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Close project"),
        tr("This will stop all running tasks and release the camera, the PLC connection "
           "and the output port.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    LOG_USER_INFO << "Runtime shell closing project." << m_projectPath;
    stopTaskRuntimes();
    m_project.reset();
    m_projectPath.clear();

    showProjectSelect(tr("The project was closed. Choose one to begin again."));
}

/// Slot: hands off to the commissioning shell.
///
/// The release step is stopTaskRuntimes(), which calls endRuntime() on every task this
/// shell started — that is what actually gives the camera, the PLC socket and the
/// vision-output port back. It runs before the sibling is launched, because the sibling
/// reaches for the same hardware and a half-released device fails in ways that look
/// exactly like a fault.
///
/// Requesting commissioning **does** need Admin; requestShellSwitch() handles that, along
/// with the confirmation that all running tasks will stop.
void RuntimeShellWindow::onOpenEditor()
{
    vc::ui::requestShellSwitch(this, vc::shell::ShellKind::OperatorRuntime, [this]() -> bool {
        stopTaskRuntimes();
        return true;
    });
}

/// Slot: shows and raises the system-log dock, creating it on first use.
///
/// Created lazily rather than at startup: an operator watching dashboards has no use for
/// it, and a dock that exists takes a tile's worth of screen from the tasks.
void RuntimeShellWindow::onSystemLogAction()
{
    if (m_systemLogDock == nullptr) {
        m_systemLogDock = new ads::CDockWidget(tr("System log"), this);
        m_systemLogDock->setWidget(new SystemLogForm(m_systemLogDock));
        m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_systemLogDock);
    }

    m_systemLogDock->toggleView(true);
    m_systemLogDock->setAsCurrentTab();
    m_systemLogDock->setFocus();
}

/// Slot: re-applies the tile layout for the newly chosen tile count.
void RuntimeShellWindow::onLayoutActionTriggered(QAction *action)
{
    Q_UNUSED(action)
    applyCurrentLayout();
}

/// Loads `path`, enters runtime, and shows the runtime page. The remembered path is
/// written only after the load succeeds, so a bad file never becomes the one the next
/// launch tries first.
bool RuntimeShellWindow::startProject(const QString &path)
{
    stopTaskRuntimes();

    auto project = std::make_shared<Project>();
    if (!ProjectRepository::load(path, *project)) {
        LOG_USER_ERR << "Runtime shell could not load project."
                     << path << ProjectRepository::lastMsg();
        return false;
    }

    m_project = project;
    m_projectPath = path;
    AppSettings::instance()->setLastRuntimeProjectPath(path);
    ui->lbl_last_path->setText(tr("Last project: %1").arg(path));

    startTaskRuntimes();
    ui->stack_pages->setCurrentWidget(ui->page_runtime);
    setRuntimeActionsEnabled(true);

    LOG_USER_INFO << "Runtime shell started project." << path;
    return true;
}

/// Shows page 0 with `reason` explaining why.
void RuntimeShellWindow::showProjectSelect(const QString &reason)
{
    ui->lbl_reason->setText(reason);
    ui->stack_pages->setCurrentWidget(ui->page_project_select);
    setRuntimeActionsEnabled(false);
    statusBar()->clearMessage();
}

/// Enables the actions that only mean anything while a project is running.
void RuntimeShellWindow::setRuntimeActionsEnabled(bool enabled)
{
    ui->action_close_project->setEnabled(enabled);
    ui->menu_layout->setEnabled(enabled);
}

/// Enables the controls that must not run while beginStartup() is in progress.
///
/// All three reach a modal dialog, and a modal dialog's nested event loop can dispatch a
/// second trigger into a project start/stop that has not finished. btn_browse is in the list
/// because it is the same slot as Project → Load… and it sits on the page that is visible
/// during exactly this window.
///
/// System log, Theme and Language are deliberately left live — none of them touch the
/// project or the devices, and an operator watching a slow start is exactly who wants the
/// log.
void RuntimeShellWindow::setStartupControlsEnabled(bool enabled)
{
    ui->action_load_project->setEnabled(enabled);
    ui->action_open_editor->setEnabled(enabled);
    ui->btn_browse->setEnabled(enabled);
}

/// Creates one dock + dashboard per localization task and enters runtime on each.
///
/// A task whose setup fails goes Faulted and its dashboard shows that; the tile is kept
/// deliberately. Hiding a failed task would leave an operator looking at a screen that
/// says nothing is wrong while a station sits dead.
void RuntimeShellWindow::startTaskRuntimes()
{
    if (!m_project) {
        return;
    }

    // QMap iteration is by task id, so "display order" is id order — stable across
    // launches, which is what matters for an operator learning the tile positions.
    const auto &tasks = m_project->getCurrentTasks();
    int shown = 0;
    QStringList dropped;

    for (auto it = tasks.cbegin(); it != tasks.cend(); ++it) {
        const std::shared_ptr<vc::model::ITask> &task = it.value();
        if (!task) {
            continue;
        }

        auto localization = std::dynamic_pointer_cast<TaskLocalization>(task);
        if (!localization) {
            continue;  // Only localization tasks have an operator dashboard.
        }

        if (shown >= kMaxTasks) {
            dropped.append(localization->name());
            continue;
        }

        auto *dock = new ads::CDockWidget(localization->name(), this);
        dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
        dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetMovable, true);

        auto *dashboard = new LocalizationDashboardWidget(task, dock, dock);
        dock->setWidget(dashboard);

        m_taskDocks.append(dock);
        m_runningTaskIds.append(localization->id());
        shown += 1;

        // Enter runtime with no button press. This is the whole point of the executable:
        // an operator starts the machine, not the software.
        localization->beginRuntime();
    }

    if (!dropped.isEmpty()) {
        // Never truncate silently — a task that is not on screen is a task nobody is
        // watching, and the operator has to know which ones.
        LOG_USER_WARN << "Runtime shell task cap reached."
                      << "shown=" << shown
                      << "dropped=" << dropped.join(QStringLiteral(", "));
    }

    const QString capNotice = dropped.isEmpty()
                                  ? QString()
                                  : tr("  —  NOT SHOWN (max %1): %2")
                                        .arg(kMaxTasks)
                                        .arg(dropped.join(QStringLiteral(", ")));
    statusBar()->showMessage(tr("%1  —  %2 task(s) running%3")
                                 .arg(QFileInfo(m_projectPath).fileName())
                                 .arg(shown)
                                 .arg(capNotice));

    selectLayoutAction(RuntimeLayoutController::smallestFittingTileCount(shown));
    applyCurrentLayout();
}

/// Ends runtime on every task this shell started and drops its docks. Idempotent: the
/// destructor and closeEvent() both call it, and a project switch calls it again.
void RuntimeShellWindow::stopTaskRuntimes()
{
    if (m_project) {
        for (const QString &taskId : m_runningTaskIds) {
            auto task = m_project->taskById(taskId);
            if (task) {
                task->endRuntime();
            }
        }
    }
    m_runningTaskIds.clear();

    for (ads::CDockWidget *dock : m_taskDocks) {
        if (dock != nullptr) {
            dock->deleteLater();
        }
    }
    m_taskDocks.clear();
}

/// Checks the Layout action matching `tileCount`.
///
/// @warning Do not wrap this in a QSignalBlocker. QActionGroup tracks its checked action
///          through QAction::changed, so blocking an action while checking it leaves the
///          group believing nothing is checked — checkedAction() then returns nullptr and
///          applyCurrentLayout() silently docks nothing, which is how the runtime page
///          came up empty with the task docks stranded outside the dock manager.
///          Blocking is unnecessary anyway: setChecked() emits toggled()/changed(), never
///          triggered(), and QActionGroup::triggered is the only signal wired to a slot.
void RuntimeShellWindow::selectLayoutAction(int tileCount)
{
    if (m_layoutGroup == nullptr) {
        return;
    }
    for (QAction *action : m_layoutGroup->actions()) {
        action->setChecked(action->data().toInt() == tileCount);
    }
}

/// Applies the tile layout for the currently checked Layout action.
///
/// A missing checked action never means "dock nothing". This shell owns the screen on a
/// field machine, and a dock that was created but never added to the dock manager is not
/// invisible — it is drawn at its size hint in the window's top-left corner, over the menu
/// bar. Falling back to the smallest grid that fits keeps the tasks where the operator can
/// see them; the error line keeps the menu-state defect visible instead of absorbing it.
void RuntimeShellWindow::applyCurrentLayout()
{
    if (m_layoutGroup == nullptr || m_taskDocks.isEmpty()) {
        return;
    }

    const QAction *checked = m_layoutGroup->checkedAction();
    int tileCount = 0;
    if (checked != nullptr) {
        tileCount = checked->data().toInt();
    } else {
        tileCount = RuntimeLayoutController::smallestFittingTileCount(m_taskDocks.size());
        LOG_DEV_ERR << "Runtime layout: no tile count is checked in the Layout menu."
                    << "Falling back to tiles=" << tileCount;
    }

    RuntimeLayoutController::applyLayout(m_dockManager, m_taskDocks, tileCount);
}
