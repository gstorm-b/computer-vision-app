#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QMainWindow>
#include <QCloseEvent>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QStringList>
#include "core/utils/theme_manager.h"

#include "DockManager.h"
#include "system_log_form.h"

#include "model/project_repository.h"
#include "model/project.h"

#include "ui/forms/project_infor_setting.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/// Bookkeeping entry for a device dock: the device widget plus the ADS dock widget that
/// hosts it, tracked in MainWindow::m_deviceEntries so the dock can be found/toggled by id.
struct UIDeviceEntry {
    QWidget          *widget{nullptr};
    ads::CDockWidget *dockWidget{nullptr};
};

/// Bookkeeping entry for a task dock: the task's display name, its widget/dock, and any
/// child docks (e.g. dashboard/patterns sub-widgets) keyed by child name.
struct UITaskEntry {
    QString                    name;
    QWidget                   *widget{nullptr};
    ads::CDockWidget          *dockWidget{nullptr};
    QMap<QString, UITaskEntry> children;
};

/// Main application window: hosts the ADS dock layout, the project tree, toolbar/menu
/// actions, and owns the current vc::model::Project plus the docks for its devices/tasks.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Builds the UI, toolbar actions, and dock layout, and wires the project tree signals.
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    /// Prompts to save (Save/Close/Cancel) before the window closes; ignores the event on Cancel.
    void closeEvent(QCloseEvent *event) override;

private:
    // Setup
    /// Creates the file/task toolbar actions plus the Theme and Language submenus under View.
    void createToolBarActions();
    /// Creates the ADS dock manager, the system-log dock, and the fixed central anchor dock.
    void createMainContents();

    // UI state
    /// Persists m_lastFolderDir and releases the current project on window close.
    void handleCloseEvent();
    /// Updates the Access Level menu title to "Admin" or "Standard".
    void updateAccessLevelLabel(bool isAdmin);
    /// Rebuilds the window title from the current project name and dirty/unsaved state.
    void updateWindowTitle();
    /// Enables or disables the task toolbar depending on whether a project is open.
    void setProjectEnabled(bool enabled);

    // Project operations
    /// Asks the user whether to save before closing the current project.
    /// @return true if the project was closed (Save or Close chosen), false if the user cancelled
    bool confirmCloseProject();
    /// Returns whether a project is currently loaded (m_project is non-null).
    bool isProjectOpen() const;
    /// Writes m_project to `path`, creating a new .vproj file first when `saveAs` or the file
    /// doesn't exist yet; shows a critical QMessageBox and returns early on any failure.
    void saveToFile(const QString &path, bool saveAs = false);
    /// Prepends `path` to m_recentPaths if not already present.
    void addRecentPath(const QString &path);
    /// If the project has unsaved changes, asks Save/Discard/Cancel and saves when requested.
    /// @return true if it is safe to proceed (saved, discarded, or already clean), false on Cancel
    bool maybeSave();
    /// Updates path/dirty state, window title, and recent paths, then refreshes the project tree
    /// widget's name and contents for the newly opened/created project.
    void refreshUIForProject(const QString &path);
    /// Connects the current m_project's modification/device/task signals to the corresponding
    /// MainWindow slots and to ProjectTreeWidget::refreshTree.
    void connectProjectSignals();

    // Dock management
    /// Creates the ProjectInforSetting widget/dock docked alongside the anchor dock.
    void createProjectInfoDock();
    /// Removes and resets the project-info dock, if one exists.
    void removeProjectInfoDock();
    /// Removes and clears all device and task (and task-child) docks from the dock manager.
    void closeAllDockTabs();

    // Widget factories
    /// Dispatches by task->taskType() to the matching create*Entry helper.
    /// @return the created UITaskEntry, or a default-constructed one for unhandled task types
    UITaskEntry createTaskEntry(std::shared_ptr<vc::model::ITask> task, ads::CDockWidget *anchorDock);
    /// Builds the dock widget and LocalizationTaskWidget for a localization task, wires its
    /// addDeviceRequested signal, and docks it in the center area next to `anchorDock`.
    UITaskEntry createLocalizationEntry(std::shared_ptr<vc::model::ITask> task, ads::CDockWidget *anchorDock);

private slots:
    /// Creates a new project via NewProjectDialog after confirming/closing any open project.
    void onNewProject();
    /// Opens a .vproj file chosen via QFileDialog, closing any currently open project first.
    void onOpenProject();
    /// Saves the current project; if no file path is set yet, delegates to onSaveAsProject().
    void onSaveProject();
    /// Saves the current project to a new .vproj path chosen via QFileDialog.
    void onSaveAsProject();
    /// Clears the project tree, releases m_project, and tears down its docks.
    void onCloseProject();
    /// Marks the project dirty, refreshes the window title, and updates the tree's project name.
    void onProjectModified();

    /// Shows/raises the system-log dock, creating it in the dock manager if not yet added.
    void onSystemLogAction();
    /// Sets the access level label to "Standard".
    void onPrivilegeStandard();
    /// Sets the access level label to "Admin".
    void onPrivilegeAdmin();

    /// Syncs the Theme menu's checked action to `styleId` and refreshes the tree if a project
    /// is open (theme-dependent icons/colors need repainting).
    void onThemeChanged(const QString &styleId, bool isDark);
    /// Adds a menu entry/action for a newly registered theme style, if not already present.
    void onThemeStyleRegistered(ThemeStyle style);
    /// Persists the chosen language and informs the user the change applies on next launch.
    void onLanguageAction(QAction *act);

    /// Currently unused; reserved slot for a project-info dock toggle action.
    void onProjectInfoDock();

    /// Shows/raises the dock for device `id`, creating it via onDeviceCreated() if needed.
    void onDeviceDoubleClicked(const QString &id);
    /// Shows/raises the dock for task `id` (only the task's own dock; `widgetName` selects a
    /// child sub-widget dock when non-empty, currently unimplemented beyond the empty case).
    void onTaskDoubleClicked(const QString &id, const QString &widgetName);

    /// Looks up the newly created device by `id`; logs and returns if the project or device is
    /// missing. (Dock/widget creation for the device is not performed here.)
    void onDeviceCreated(const QString &id);
    /// Removes the dock (if any) for device `id` and erases its UIDeviceEntry.
    void onDeviceDeleted(const QString &id);
    /// Creates the task's dock/widget via createTaskEntry(), wires its devicesChanged signal to
    /// refresh the tree, and registers the resulting UITaskEntry.
    void onTaskCreated(const QString &id);
    /// Removes the docks for task `id` and all of its child entries, then erases the entry.
    void onTaskDeleted(const QString &id);

    // New tree actions
    /// Runs AddDeviceWizard and, on acceptance, assigns the chosen device to task `taskId`.
    void onAddDeviceToTaskRequested(const QString &taskId);
    /// Prompts to pick a destination task and moves `deviceId` from `taskId` to it.
    void onMoveDeviceRequested(const QString &taskId, const QString &deviceId);
    /// Confirms with the user, then unassigns and releases `deviceId` from task `taskId`.
    void onDeleteDeviceRequested(const QString &taskId, const QString &deviceId);
    /// Confirms with the user, then releases all of the task's devices and removes the task.
    void onDeleteTaskRequested(const QString &taskId);

private:
    Ui::MainWindow *ui;  ///< Generated UI form; owns the widgets declared in mainwindow.ui.

    // Dock infrastructure
    ads::CDockManager                     *m_dockManager{nullptr};        ///< ADS dock manager hosting all docks in ui->wg_dock.
    ads::CDockContainerWidget             *m_mainDockContainer{nullptr};  ///< Unused/reserved reference to the main dock container.
    std::shared_ptr<ads::CDockAreaWidget> *m_mainDockArea{nullptr};       ///< Unused/reserved reference to the main dock area.
    std::shared_ptr<ads::CDockWidget>      m_anchorDock;                 ///< Fixed, non-closable central dock that task/device docks are tabbed against.
    std::shared_ptr<ads::CDockWidget>      m_projectInfoDock;             ///< Dock hosting the project-info widget; created/removed per open project.

    // Toolbar actions
    QAction *m_actNewProject{nullptr};    ///< File toolbar action: create a new project.
    QAction *m_actOpenProject{nullptr};   ///< File toolbar action: open an existing project.
    QAction *m_actSaveProject{nullptr};   ///< File toolbar action: save the current project.
    QAction *m_actCloseProject{nullptr};  ///< File toolbar action: close the current project.
    QAction *m_actCaptureImage{nullptr};  ///< Task toolbar action: capture image (icon-only).

    // View menu
    QMenu        *m_menuTheme{nullptr};       ///< "Theme" submenu under View, populated from ThemeManager styles.
    QActionGroup *m_actGrpTheme{nullptr};     ///< Exclusive action group backing the theme radio-selection in m_menuTheme.
    QMenu        *m_menuLanguage{nullptr};    ///< "Language" submenu under View.
    QActionGroup *m_actGrpLanguage{nullptr};  ///< Exclusive action group backing the language radio-selection in m_menuLanguage.

    // Core objects
    std::shared_ptr<vc::model::Project> m_project;         ///< The currently open project, or null when none is open.
    std::shared_ptr<ads::CDockWidget>   m_systemLogDock;    ///< Dock hosting the system-log widget; created once in createMainContents().
    SystemLogForm                      *m_systemLogForm{nullptr};    ///< Widget shown in m_systemLogDock.
    ProjectInforSetting                *m_projectInfoWidget{nullptr};  ///< Widget shown in m_projectInfoDock for the current project.

    // Dock registries
    QMap<QString, UIDeviceEntry> m_deviceEntries;  ///< Live device docks/widgets keyed by device id.
    QMap<QString, UITaskEntry>   m_taskEntries;    ///< Live task docks/widgets (and their children) keyed by task id.

    // State
    QStringList m_recentPaths;         ///< Recently opened/saved project file paths, most-recent first.
    QString     m_lastFolderDir;       ///< Last folder used in open/save dialogs; persisted via AppSettings.
    QString     m_projectFilePath;     ///< Absolute path of the current project's .vproj file, empty if unsaved.
    QString     m_currentProjectName;  ///< Cached name of the current project, used by updateWindowTitle().
    bool        m_isDirty{false};      ///< True when the current project has unsaved changes.
};

#endif // MAINWINDOW_H
