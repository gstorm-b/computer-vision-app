#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>

#include "app_settings/app_settings.h"

#include "windows_helper.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "DockSplitter.h"
#include "DockWidget.h"
#include "FloatingDockContainer.h"

// #include "form/navigate_form.h"
#include "form/new_project_dialog.h"
#include "form/add_device_wizard.h"

#include <QInputDialog>

#include "device/device_factory.h"
#include "device/camera/camera_device.h"
#include "device/camera/camera_basler_gige.h"
#include "device/plc/mc_protocol_device.h"

#include "form/camera/basler_camera_widget.h"
#include "form/plc/mitsubishi_mc_device_widget.h"

#include "model/task_localization.h"
#include "form/task/localization_task_widget.h"
#include "form/task/localization_dashboard_widget.h"
#include "form/task/localization_patterns_widget.h"

using namespace ads;
using Project     = vc::model::Project;
using ProjectRepo = vc::model::ProjectRepository;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    createToolBarActions();
    createMainContents();
    updateAccessLevelLabel(false);

    setProjectEnabled(false);
    updateWindowTitle();

    m_lastFolderDir   = AppSettings::instance()->lastFolderAccessDir();
    m_projectFilePath = QString();
    m_isDirty         = false;

    ui->main_splitter->setStretchFactor(0, 0);
    ui->main_splitter->setStretchFactor(1, 1);

    connect(ui->proj_treeview_wg, &ProjectTreeWidget::deviceDoubleClicked,
            this, &MainWindow::onDeviceDoubleClicked);
    connect(ui->proj_treeview_wg, &ProjectTreeWidget::taskDoubleClicked,
            this, &MainWindow::onTaskDoubleClicked);
    connect(ui->proj_treeview_wg, &ProjectTreeWidget::addDeviceToTaskRequested,
            this, &MainWindow::onAddDeviceToTaskRequested);
    connect(ui->proj_treeview_wg, &ProjectTreeWidget::moveDeviceRequested,
            this, &MainWindow::onMoveDeviceRequested);
    connect(ui->proj_treeview_wg, &ProjectTreeWidget::deleteDeviceRequested,
            this, &MainWindow::onDeleteDeviceRequested);
    connect(ui->proj_treeview_wg, &ProjectTreeWidget::deleteTaskRequested,
            this, &MainWindow::onDeleteTaskRequested);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QMessageBox::StandardButton resBtn = QMessageBox::question(
        this, windowTitle(),
        tr("Would you like to save configuration before exiting?"),
        QMessageBox::Save | QMessageBox::Cancel | QMessageBox::Close,
        QMessageBox::Cancel);

    switch (resBtn) {
    case QMessageBox::Save:
        onSaveProject();
        event->accept();
        handleCloseEvent();
        return;
    case QMessageBox::Close:
        event->accept();
        handleCloseEvent();
        break;
    case QMessageBox::Cancel:
        event->ignore();
        return;
    default:
        break;
    }
}

void MainWindow::handleCloseEvent() {
    AppSettings::instance()->setLastFolderAccessDir(m_lastFolderDir);
    if (m_project)
        m_project.reset();
}

void MainWindow::createToolBarActions() {
    QToolBar *tbar = ui->toolBar_file;
    tbar->setFloatable(false);
    tbar->setAllowedAreas(Qt::ToolBarArea::TopToolBarArea);
    tbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tbar->setIconSize(QSize(20, 20));

    m_actNewProject = tbar->addAction("New Project");
    m_actNewProject->setIcon(svgIcon(":/resrc/icon/file-create.svg"));

    m_actOpenProject = tbar->addAction("Open Project");
    m_actOpenProject->setIcon(svgIcon(":/resrc/icon/file-open.svg"));

    m_actSaveProject = tbar->addAction("Save Project");
    m_actSaveProject->setIcon(svgIcon(":/resrc/icon/file-save.svg"));

    m_actCloseProject = tbar->addAction("Close Project");
    m_actCloseProject->setIcon(svgIcon(":/resrc/icon/file-close.svg"));

    connect(m_actNewProject,   &QAction::triggered, this, &MainWindow::onNewProject);
    connect(m_actOpenProject,  &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(m_actSaveProject,  &QAction::triggered, this, &MainWindow::onSaveProject);
    connect(m_actCloseProject, &QAction::triggered, this, &MainWindow::onCloseProject);

    QToolBar *tbarTask = ui->toolBar_task;
    tbarTask->setFloatable(false);
    tbarTask->setAllowedAreas(Qt::ToolBarArea::TopToolBarArea);
    tbarTask->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_actCaptureImage = tbarTask->addAction("Capture Image");
    m_actCaptureImage->setIcon(svgIcon(":/resrc/icon/file-capture.svg"));

    connect(ui->actionNew_Pproject,   &QAction::triggered, this, &MainWindow::onNewProject);
    connect(ui->actionOpen_Project,   &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(ui->actionSave_Project,   &QAction::triggered, this, &MainWindow::onSaveProject);
    connect(ui->actionSaveAs_Project, &QAction::triggered, this, &MainWindow::onSaveAsProject);
    connect(ui->actionClose_Project,  &QAction::triggered, this, &MainWindow::onCloseProject);

    connect(ui->actionSystemLog,          &QAction::triggered, this, &MainWindow::onSystemLogAction);
    connect(ui->actionPrivilege_Standard, &QAction::triggered, this, &MainWindow::onPrivilegeStandard);
    connect(ui->actionPrivilege_Admin,    &QAction::triggered, this, &MainWindow::onPrivilegeAdmin);

    // Theme submenu — lists all registered styles with exclusive selection
    ui->menuView->addSeparator();
    m_menuTheme   = ui->menuView->addMenu(tr("Theme"));
    m_actGrpTheme = new QActionGroup(this);
    m_actGrpTheme->setExclusive(true);

    const QString currentStyleId = ThemeManager::instance()->currentStyleId();
    for (const ThemeStyle &style : ThemeManager::instance()->styles()) {
        QAction *act = m_menuTheme->addAction(style.displayName);
        act->setCheckable(true);
        act->setChecked(style.id == currentStyleId);
        act->setData(style.id);
        m_actGrpTheme->addAction(act);
    }

    connect(m_actGrpTheme, &QActionGroup::triggered, this, [](QAction *act) {
        ThemeManager::instance()->applyStyle(act->data().toString());
    });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    connect(ThemeManager::instance(), &ThemeManager::styleRegistered,
            this, &MainWindow::onThemeStyleRegistered);

    // Language submenu
    m_menuLanguage   = ui->menuView->addMenu(tr("Language"));
    m_actGrpLanguage = new QActionGroup(this);
    m_actGrpLanguage->setExclusive(true);

    struct LangEntry { QString id; QString display; };
    const QList<LangEntry> languages = {
        { QStringLiteral("en"),    tr("English") },
        { QStringLiteral("ja_JP"), QString::fromUtf8("日本語") },
    };

    const QString savedLang    = AppSettings::instance()->language();
    const QString effectiveLang = (savedLang == QLatin1String("ja_JP")) ? savedLang
                                                                        : QStringLiteral("en");
    for (const LangEntry &entry : languages) {
        QAction *act = m_menuLanguage->addAction(entry.display);
        act->setCheckable(true);
        act->setChecked(entry.id == effectiveLang);
        act->setData(entry.id);
        m_actGrpLanguage->addAction(act);
    }

    connect(m_actGrpLanguage, &QActionGroup::triggered,
            this, &MainWindow::onLanguageAction);
}

void MainWindow::createMainContents() {
    CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
    CDockManager::setAutoHideConfigFlags({CDockManager::DefaultAutoHideConfig});

    ads::CDockManager::setConfigFlags(ads::CDockManager::DefaultOpaqueConfig);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasCloseButton,     false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasUndockButton,    false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::DockAreaHasTabsMenuButton,  false);

    m_dockManager = new ads::CDockManager(ui->wg_dock);
    m_dockManager->setStyleSheet(QString()); // disable ADS internal stylesheet; global QSS owns all ads-- rules

    QVBoxLayout *layout = new QVBoxLayout(ui->wg_dock);
    layout->addWidget(m_dockManager);
    layout->contentsMargins().setBottom(2);
    layout->contentsMargins().setTop(2);
    layout->contentsMargins().setLeft(2);
    layout->contentsMargins().setRight(2);
    ui->wg_dock->setLayout(layout);

    m_systemLogDock = std::make_shared<ads::CDockWidget>(tr("System log"));
    m_systemLogForm = new SystemLogForm();
    m_systemLogDock->setWidget(m_systemLogForm);

    m_anchorDock.reset(new ads::CDockWidget("CentralAnchor"));
    m_anchorDock->setFeature(ads::CDockWidget::NoTab,               true);
    m_anchorDock->setFeature(ads::CDockWidget::DockWidgetClosable,  false);
    m_anchorDock->setFeature(ads::CDockWidget::DockWidgetMovable,   false);
    m_anchorDock->setFeature(ads::CDockWidget::DockWidgetFocusable, false);
    m_anchorDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
    m_anchorDock->setFeature(ads::CDockWidget::DockWidgetPinnable,  false);
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, m_anchorDock.get());
}

void MainWindow::updateAccessLevelLabel(bool isAdmin) {
    ui->menuAccessLevel->setTitle(isAdmin ? tr("Admin") : tr("Standard"));
}

void MainWindow::updateWindowTitle() {
    if (!m_project) {
        setWindowTitle(tr("NCR-PICKING (No Project opened)"));
        m_currentProjectName = QString();
        return;
    }

    m_currentProjectName = m_project->name();
    const QString dirtyMark = (m_isDirty || m_projectFilePath.isEmpty())
                              ? QStringLiteral("*") : QString();

    if (m_currentProjectName.isEmpty()) {
        setWindowTitle(tr("NCR-PICKING (No Project opened)"));
        return;
    }
    setWindowTitle(tr("Project: %1").arg(m_currentProjectName) + dirtyMark);
}

void MainWindow::setProjectEnabled(bool enabled) {
    ui->toolBar_task->setEnabled(enabled);
}

bool MainWindow::confirmCloseProject() {
    QMessageBox::StandardButton resBtn = QMessageBox::question(
        this, windowTitle(),
        tr("Would you like to save the project before close it?"),
        QMessageBox::Save | QMessageBox::Cancel | QMessageBox::Close,
        QMessageBox::Cancel);

    switch (resBtn) {
    case QMessageBox::Save:
        onCloseProject();
        return true;
    case QMessageBox::Close:
        onCloseProject();
        return true;
    case QMessageBox::Cancel:
        return false;
    default:
        return false;
    }
}

bool MainWindow::isProjectOpen() const {
    return (m_project.get() != nullptr);
}

void MainWindow::saveToFile(const QString &path, bool saveAs) {
    const bool isNew = (!QFile::exists(path) || saveAs);
    const QString timeStr = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (isNew) {
        if (!ProjectRepo::createNew(path, m_project->name(), timeStr)) {
            QMessageBox::critical(this,
                                  tr("Save Project error"),
                                  tr("Cannot create Project file: %1")
                                      .arg(ProjectRepo::lastMsg()));
            return;
        }
    }

    QFileInfo finfo(path);
    if (finfo.completeSuffix() != QLatin1String("vproj") || !finfo.isAbsolute()) {
        QMessageBox::critical(this, tr("Save Project error"), tr("File format invalid!"));
        return;
    }

    m_project->setUpdatedAt(timeStr);
    if (!ProjectRepo::save(path, *m_project)) {
        QMessageBox::critical(this,
                              tr("Save Project error"),
                              tr("Cannot save Project: %1").arg(ProjectRepo::lastMsg()));
        return;
    }

    LOG_USER_INFO << "Project saved: " << path;
    m_projectFilePath = path;
    m_isDirty         = false;
    updateWindowTitle();
    addRecentPath(path);
}

void MainWindow::addRecentPath(const QString &path) {
    if (!m_recentPaths.contains(path))
        m_recentPaths.push_front(path);
}

bool MainWindow::maybeSave() {
    if (!(m_isDirty || (m_projectFilePath.isEmpty() && m_project)))
        return true;

    const auto ret = QMessageBox::question(
        this,
        tr("Would you want to change?"),
        tr("Project [%1] has some changed.\nWould you want to save the project?")
            .arg(m_project ? m_project->name() : "error"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)
        return (onSaveProject(), !m_isDirty);

    if (ret == QMessageBox::Discard)
        return true;

    return false;
}

void MainWindow::refreshUIForProject(const QString &path) {
    m_projectFilePath = path;
    m_isDirty         = false;
    updateWindowTitle();
    addRecentPath(path);
    ui->proj_treeview_wg->changeProjectName(m_project->name());
    ui->proj_treeview_wg->refreshTree();
}

void MainWindow::connectProjectSignals() {
    if (!m_project) return;

    connect(m_project.get(), &vc::model::Project::projectModificationOccurred,
            this, &MainWindow::onProjectModified);

    connect(m_project->deviceManager().get(), &vc::device::DeviceManager::deviceCreated,
            this, &MainWindow::onDeviceCreated);
    connect(m_project->deviceManager().get(), &vc::device::DeviceManager::deviceDeleted,
            this, &MainWindow::onDeviceDeleted);
    // Device creation/deletion changes refresh the tree
    connect(m_project->deviceManager().get(), &vc::device::DeviceManager::devicesChanged,
            ui->proj_treeview_wg, &ProjectTreeWidget::refreshTree);
    // Device metadata changes such as rename also refresh the tree
    connect(m_project->deviceManager().get(), &vc::device::DeviceManager::deviceModified,
            ui->proj_treeview_wg, &ProjectTreeWidget::refreshTree);

    connect(m_project.get(), &vc::model::Project::taskCreated,
            this, &MainWindow::onTaskCreated);
    connect(m_project.get(), &vc::model::Project::taskDeleted,
            this, &MainWindow::onTaskDeleted);
    connect(m_project.get(), &vc::model::Project::tasksChanged,
            ui->proj_treeview_wg, &ProjectTreeWidget::refreshTree);

    // Connect existing tasks' device-assignment changes (project loaded from file)
    for (const auto &taskPair : m_project->getCurrentTasks()) {
        connect(taskPair.get(), &vc::model::ITask::devicesChanged,
                ui->proj_treeview_wg, &ProjectTreeWidget::refreshTree);
    }
}

void MainWindow::createProjectInfoDock() {
    m_projectInfoWidget = new ProjectInforSetting(m_project);
    m_projectInfoDock.reset(new ads::CDockWidget(tr("Project infromation")));
    m_projectInfoDock->setWidget(m_projectInfoWidget);

    m_projectInfoDock->setFeature(ads::CDockWidget::DockWidgetClosable,  false);
    m_projectInfoDock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
    m_projectInfoDock->setFeature(ads::CDockWidget::DockWidgetMovable,   false);
    m_projectInfoDock->setFeature(ads::CDockWidget::DockWidgetPinnable,  false);
    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, m_projectInfoDock.get(),
                                 m_anchorDock->dockAreaWidget());

    connect(m_projectInfoWidget, &ProjectInforSetting::projectModified,
            this, &MainWindow::onProjectModified);
}

void MainWindow::removeProjectInfoDock() {
    if (!m_projectInfoDock) return;
    m_dockManager->removeDockWidget(m_projectInfoDock.get());
    m_projectInfoDock.reset();
}

void MainWindow::closeAllDockTabs() {
    // remove all devices docks
    QMap<QString, UIDeviceEntry>::const_iterator devIt = m_deviceEntries.cbegin();
    while (devIt != m_deviceEntries.cend()) {
        if (devIt.value().dockWidget) {
            m_dockManager->removeDockWidget(devIt.value().dockWidget);
        }            
        ++devIt;
    }
    m_deviceEntries.clear();

    // remove all tasks docks and its children docks
    QMap<QString, UITaskEntry>::const_iterator taskIt = m_taskEntries.cbegin();
    while (taskIt != m_taskEntries.cend()) {
        QMap<QString, UITaskEntry>::const_iterator childIt = taskIt.value().children.cbegin();
        while (childIt != taskIt.value().children.cend()) {
            if (childIt.value().dockWidget) {
            m_dockManager->removeDockWidget(childIt.value().dockWidget);
            }
            ++childIt;
        }

        if (taskIt.value().dockWidget) {
            m_dockManager->removeDockWidget(taskIt.value().dockWidget);
        }
        ++taskIt;
    }
    m_taskEntries.clear();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onNewProject() {
    if (!maybeSave()) return;
    if (m_project) onCloseProject();

    NewProjectDialog *dialog = new NewProjectDialog(this);
    dialog->exec();
    if (dialog->result() == QDialog::Rejected) return;

    const QString timeStr = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_project = std::make_shared<Project>();
    m_project->setName(dialog->getName());
    m_project->setDescription(dialog->getDescription());
    m_project->setCreatedAt(timeStr);
    m_project->setUpdatedAt(timeStr);
    ui->proj_treeview_wg->setProject(m_project);

    refreshUIForProject(QString());
    createProjectInfoDock();
    connectProjectSignals();
}

void MainWindow::onOpenProject() {
    if (!maybeSave()) return;
    if (m_project) onCloseProject();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        m_lastFolderDir,
        "Vision Project (*.vproj);;");

    if (path.isEmpty()) return;

    m_lastFolderDir = QFileInfo(path).absolutePath();

    auto project = std::make_shared<Project>();
    if (!ProjectRepo::load(path, *project)) {
        QMessageBox::critical(this,
                              tr("Open Project error"),
                              tr("Cannot open Project: \n") + ProjectRepo::lastMsg());
        return;
    }

    m_project = project;
    ui->proj_treeview_wg->setProject(m_project);
    refreshUIForProject(path);
    createProjectInfoDock();
    connectProjectSignals();
}

void MainWindow::onSaveProject() {
    if (!m_project) return;
    if (m_projectFilePath.isEmpty()) {
        onSaveAsProject();
        return;
    }
    saveToFile(m_projectFilePath);
}

void MainWindow::onSaveAsProject() {
    if (!m_project) return;

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save project as"),
        m_lastFolderDir + "/untitled.vproj",
        "Vision Project (*.vproj);;");

    if (path.isEmpty()) return;

    m_lastFolderDir = QFileInfo(path).absolutePath();
    saveToFile(path, true);
}

void MainWindow::onCloseProject() {
    if (!m_project) return;

    ui->proj_treeview_wg->clearProject();
    m_project.reset();
    removeProjectInfoDock();
    closeAllDockTabs();
    updateWindowTitle();

    m_projectFilePath = QString();
    m_isDirty         = false;
}

void MainWindow::onProjectModified() {
    m_isDirty = true;
    updateWindowTitle();
    ui->proj_treeview_wg->changeProjectName(m_project->name());
}

void MainWindow::onSystemLogAction() {
    if (!m_dockManager->findDockWidget("System log")) {
        m_dockManager->addDockWidget(ads::CenterDockWidgetArea, m_systemLogDock.get(),
                                     m_anchorDock->dockAreaWidget());
        return;
    }
    if (m_systemLogDock->isClosed()) {
        m_dockManager->addDockWidget(ads::CenterDockWidgetArea, m_systemLogDock.get(),
                                     m_anchorDock->dockAreaWidget());
    }
    m_systemLogDock->toggleView(true);
    m_systemLogDock->setAsCurrentTab();
    m_systemLogDock->setFocus();
}

void MainWindow::onPrivilegeStandard() {
    updateAccessLevelLabel(false);
}

void MainWindow::onPrivilegeAdmin() {
    updateAccessLevelLabel(true);
}

void MainWindow::onThemeChanged(const QString &styleId, bool isDark) {
    Q_UNUSED(isDark)
    for (QAction *act : m_actGrpTheme->actions()) {
        QSignalBlocker blocker(act);
        act->setChecked(act->data().toString() == styleId);
    }
    if (m_project)
        ui->proj_treeview_wg->refreshTree();
}

void MainWindow::onThemeStyleRegistered(ThemeStyle style) {
    for (QAction *act : m_actGrpTheme->actions()) {
        if (act->data().toString() == style.id) return;
    }
    QAction *act = m_menuTheme->addAction(style.displayName);
    act->setCheckable(true);
    act->setChecked(style.id == ThemeManager::instance()->currentStyleId());
    act->setData(style.id);
    m_actGrpTheme->addAction(act);
}

void MainWindow::onLanguageAction(QAction *act) {
    AppSettings::instance()->setLanguage(act->data().toString());
    QMessageBox::information(this,
        tr("Language Changed"),
        tr("The language change will take effect the next time the application is started."));
}

void MainWindow::onProjectInfoDock() {
}

void MainWindow::onDeviceDoubleClicked(const QString &id) {
    const auto it = m_deviceEntries.constFind(id);
    const bool found = (it != m_deviceEntries.constEnd());

    std::shared_ptr<vc::device::IDevice> device = m_project->deviceById(id);
    if (!device) return;

    const QString dockName = QString("%1 - %2").arg(device->name(), device->id());

    if (!found && !m_dockManager->findDockWidget(dockName)) {
        onDeviceCreated(id);
        return;
    }

    if (!found) return;

    const UIDeviceEntry &entry = it.value();
    if (entry.dockWidget->isClosed()) {
        m_dockManager->addDockWidget(ads::CenterDockWidgetArea, entry.dockWidget,
                                     m_anchorDock->dockAreaWidget());
    }
    entry.dockWidget->toggleView(true);
    entry.dockWidget->setAsCurrentTab();
    entry.dockWidget->setFocus();
}

void MainWindow::onTaskDoubleClicked(const QString &id, const QString &widgetName) {
    const auto it = m_taskEntries.constFind(id);
    const bool found = (it != m_taskEntries.constEnd());

    std::shared_ptr<vc::model::ITask> task = m_project->taskById(id);
    if (!task) return;

    if (!found && !m_dockManager->findDockWidget(task->name())) {
        onTaskCreated(id);
        return;
    }

    if (!found) return;

    const UITaskEntry &taskEntry = it.value();
    ads::CDockWidget *dockWidget = nullptr;
    if (widgetName.isEmpty()) {
        dockWidget = taskEntry.dockWidget;
    }

    if (!dockWidget) return;

    if (dockWidget->isClosed()) {
        m_dockManager->addDockWidget(ads::CenterDockWidgetArea, dockWidget,
                                     m_anchorDock->dockAreaWidget());
    }
    dockWidget->toggleView(true);
    dockWidget->setAsCurrentTab();
    dockWidget->setFocus();
}

void MainWindow::onDeviceCreated(const QString &id) {
    if (!m_project) {
        LOG_DEV_ERR << "Cannot create device widget, m_project is null.";
        return;
    }

    std::shared_ptr<vc::device::IDevice> device = m_project->deviceById(id);
    if (!device) {
        LOG_DEV_ERR << "Cannot create device widget, device not found for id: " << id;
        return;
    }
}

void MainWindow::onDeviceDeleted(const QString &id) {
    const auto it = m_deviceEntries.find(id);
    if (it == m_deviceEntries.end()) return;
    if (it->dockWidget)
        m_dockManager->removeDockWidget(it->dockWidget);
    m_deviceEntries.erase(it);
}

void MainWindow::onTaskCreated(const QString &id) {
    if (!m_project) {
        LOG_DEV_ERR << "Cannot create task widget, m_project is null.";
        return;
    }

    std::shared_ptr<vc::model::ITask> task = m_project->taskById(id);
    if (!task) return;

    // Propagate device-assignment changes to the tree
    connect(task.get(), &vc::model::ITask::devicesChanged,
            ui->proj_treeview_wg, &ProjectTreeWidget::refreshTree);

    UITaskEntry entry = createTaskEntry(task, m_anchorDock.get());
    if (!entry.widget) return;

    m_taskEntries.insert(id, entry);
}

void MainWindow::onTaskDeleted(const QString &id) {
    const auto it = m_taskEntries.find(id);
    if (it == m_taskEntries.end()) return;

    for (const UITaskEntry &child : it->children) {
        if (child.dockWidget)
            m_dockManager->removeDockWidget(child.dockWidget);
    }
    if (it->dockWidget)
        m_dockManager->removeDockWidget(it->dockWidget);

    m_taskEntries.erase(it);
}

// ── New tree action slots ─────────────────────────────────────────────────────

void MainWindow::onAddDeviceToTaskRequested(const QString &taskId) {
    if (!m_project) return;

    auto task = m_project->taskById(taskId);
    if (!task) return;

    AddDeviceWizard wizard(m_project->deviceManager(), task->name(), this);
    if (wizard.showWizard() != QDialog::Accepted) return;

    const QString deviceId = wizard.getDeviceId();
    if (m_project->assignDeviceToTask(deviceId, taskId)) {
        LOG_USER_INFO << QString("Device %1 assigned to task %2").arg(deviceId, taskId);
    }
    // task->assignDevice(deviceId);
    // emits devicesChanged → refreshTree
}

void MainWindow::onMoveDeviceRequested(const QString &taskId,
                                       const QString &deviceId)
{
    if (!m_project) return;

    // Build list of candidate tasks (all except current)
    QStringList taskNames;
    QStringList taskIds;
    for (const auto &pair : m_project->getCurrentTasks()) {
        if (pair->id() != taskId) {
            taskNames.append(pair->name());
            taskIds.append(pair->id());
        }
    }

    if (taskIds.isEmpty()) {
        QMessageBox::information(this, tr("Move Device"),
                                 tr("No other tasks available."));
        return;
    }

    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, tr("Move Device"),
        tr("Select destination task:"),
        taskNames, 0, false, &ok);

    if (!ok || chosen.isEmpty()) return;

    const QString targetId = taskIds.value(taskNames.indexOf(chosen));
    if (m_project->moveDeviceToTask(deviceId, taskId, targetId)) {
        LOG_USER_INFO << QString("Device %1 moved from task %2 to %3")
                             .arg(deviceId, taskId, targetId);
    }
}

void MainWindow::onDeleteDeviceRequested(const QString &taskId,
                                         const QString &deviceId)
{
    if (!m_project) return;

    auto task   = m_project->taskById(taskId);
    auto device = m_project->deviceById(deviceId);
    if (!task || !device) return;

    const auto reply = QMessageBox::question(
        this, tr("Remove Device"),
        tr("Remove device '%1' from task '%2'?\nThis cannot be undone.")
            .arg(device->name(), task->name()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // task->unassignDevice(deviceId);                      // emits devicesChanged → refreshTree
    m_project->unassignDeviceFromTask(deviceId, taskId); // emits devicesChanged → refreshTree
    m_project->deviceManager()->releaseDevice(deviceId); // emits deviceDeleted → closes dock
    LOG_USER_INFO << QString("Device %1 removed from task %2").arg(deviceId, taskId);
}

void MainWindow::onDeleteTaskRequested(const QString &taskId)
{
    if (!m_project) return;

    auto task = m_project->taskById(taskId);
    if (!task) return;

    const auto reply = QMessageBox::question(
        this, tr("Delete Task"),
        tr("Delete task '%1' and all its devices?\nThis cannot be undone.")
            .arg(task->name()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Release all devices owned by this task first
    const QStringList devIds = task->assignedDeviceIds();
    for (const QString &devId : devIds) {
        // task->unassignDevice(devId);
        m_project->unassignDeviceFromTask(devId, taskId);
        m_project->deviceManager()->releaseDevice(devId);
    }

    m_project->removeTask(taskId);  // emits taskDeleted → onTaskDeleted + tasksChanged → refreshTree
    LOG_USER_INFO << QString("Task %1 deleted").arg(taskId);
}

static void configureDockWidget(ads::CDockWidget *dock, QWidget *widget) {
    dock->setWidget(widget);
    dock->setFeature(ads::CDockWidget::DockWidgetClosable,  true);
    dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
    dock->setFeature(ads::CDockWidget::DockWidgetMovable,   true);
    dock->setFeature(ads::CDockWidget::DockWidgetPinnable,  true);
}

template <typename T>
static UITaskEntry makeTaskEntry(const QString &entryName,
                                 std::shared_ptr<vc::model::ITask> task) {
    ads::CDockWidget *dock   = new ads::CDockWidget(QString("%1 - %2").arg(task->name(), entryName));
    QWidget          *widget = new T(task, dock);
    UITaskEntry entry;
    entry.name       = entryName;
    entry.dockWidget = dock;
    entry.widget     = widget;
    configureDockWidget(dock, widget);
    return entry;
}

UITaskEntry MainWindow::createTaskEntry(std::shared_ptr<vc::model::ITask> task,
                                        ads::CDockWidget *anchorDock) {
    if (!task) return UITaskEntry();

    switch (task->taskType()) {
    case vc::model::TaskType::LocalizationTask:
        return createLocalizationEntry(task, anchorDock);
    default: break;
    }
    return UITaskEntry();
}

UITaskEntry MainWindow::createLocalizationEntry(std::shared_ptr<vc::model::ITask> task,
                                                ads::CDockWidget *anchorDock) {
    UITaskEntry entry;
    if (!task) return entry;

    ads::CDockWidget *dockWidget = new ads::CDockWidget(task->name());
    QWidget          *taskWidget = new LocalizationTaskWidget(task, dockWidget);
    if (!taskWidget) {
        dockWidget->deleteLater();
        LOG_DEV_ERR << "Cannot create task widget, null pointer error.";
        return entry;
    }

    entry.name       = task->name();
    entry.dockWidget = dockWidget;
    entry.widget     = taskWidget;
    configureDockWidget(dockWidget, taskWidget);

    auto *locWidget = static_cast<LocalizationTaskWidget*>(taskWidget);
    connect(locWidget, &LocalizationTaskWidget::addDeviceRequested,
            this, &MainWindow::onAddDeviceToTaskRequested);

    // UITaskEntry dashEntry  = makeTaskEntry<LocalizationDashboardWidget>  (tr("Dashboard"),   task);
    // UITaskEntry patEntry   = makeTaskEntry<LocalizationPatternsWidget>   (tr("Patterns"),    task);
    // UITaskEntry calibEntry = makeTaskEntry<LocalizationCalibrationWidget>(tr("Calibration"), task);

    // entry.children.insert(dashEntry.name,  dashEntry);
    // entry.children.insert(patEntry.name,   patEntry);
    // entry.children.insert(calibEntry.name, calibEntry);

    m_dockManager->addDockWidget(ads::CenterDockWidgetArea, dockWidget,             anchorDock->dockAreaWidget());
    // m_dockManager->addDockWidget(ads::CenterDockWidgetArea, dashEntry.dockWidget,   anchorDock->dockAreaWidget());
    // m_dockManager->addDockWidget(ads::CenterDockWidgetArea, patEntry.dockWidget,    anchorDock->dockAreaWidget());
    // m_dockManager->addDockWidget(ads::CenterDockWidgetArea, calibEntry.dockWidget,  anchorDock->dockAreaWidget());

    // dashEntry.dockWidget->closeDockWidget();
    // patEntry.dockWidget->closeDockWidget();
    // calibEntry.dockWidget->closeDockWidget();

    dockWidget->setAsCurrentTab();

    return entry;
}
