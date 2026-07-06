#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QMainWindow>
#include <QCloseEvent>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QStringList>
#include "src/utils/theme_manager.h"

#include "DockManager.h"
#include "system_log_form.h"

#include "model/project_repository.h"
#include "model/project.h"

#include "form/project_infor_setting.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct UIDeviceEntry {
    QWidget          *widget{nullptr};
    ads::CDockWidget *dockWidget{nullptr};
};

struct UITaskEntry {
    QString                    name;
    QWidget                   *widget{nullptr};
    ads::CDockWidget          *dockWidget{nullptr};
    QMap<QString, UITaskEntry> children;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // Setup
    void createToolBarActions();
    void createMainContents();

    // UI state
    void handleCloseEvent();
    void updateAccessLevelLabel(bool isAdmin);
    void updateWindowTitle();
    void setProjectEnabled(bool enabled);

    // Project operations
    bool confirmCloseProject();
    bool isProjectOpen() const;
    void saveToFile(const QString &path, bool saveAs = false);
    void addRecentPath(const QString &path);
    bool maybeSave();
    void refreshUIForProject(const QString &path);
    void connectProjectSignals();

    // Dock management
    void createProjectInfoDock();
    void removeProjectInfoDock();
    void closeAllDockTabs();

    // Widget factories
    UITaskEntry createTaskEntry(std::shared_ptr<vc::model::ITask> task, ads::CDockWidget *anchorDock);
    UITaskEntry createLocalizationEntry(std::shared_ptr<vc::model::ITask> task, ads::CDockWidget *anchorDock);

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveAsProject();
    void onCloseProject();
    void onProjectModified();

    void onSystemLogAction();
    void onPrivilegeStandard();
    void onPrivilegeAdmin();

    void onThemeChanged(const QString &styleId, bool isDark);
    void onThemeStyleRegistered(ThemeStyle style);
    void onLanguageAction(QAction *act);

    void onProjectInfoDock();

    void onDeviceDoubleClicked(const QString &id);
    void onTaskDoubleClicked(const QString &id, const QString &widgetName);

    void onDeviceCreated(const QString &id);
    void onDeviceDeleted(const QString &id);
    void onTaskCreated(const QString &id);
    void onTaskDeleted(const QString &id);

    // New tree actions
    void onAddDeviceToTaskRequested(const QString &taskId);
    void onMoveDeviceRequested(const QString &taskId, const QString &deviceId);
    void onDeleteDeviceRequested(const QString &taskId, const QString &deviceId);
    void onDeleteTaskRequested(const QString &taskId);

private:
    Ui::MainWindow *ui;

    // Dock infrastructure
    ads::CDockManager                     *m_dockManager{nullptr};
    ads::CDockContainerWidget             *m_mainDockContainer{nullptr};
    std::shared_ptr<ads::CDockAreaWidget> *m_mainDockArea{nullptr};
    std::shared_ptr<ads::CDockWidget>      m_anchorDock;
    std::shared_ptr<ads::CDockWidget>      m_projectInfoDock;

    // Toolbar actions
    QAction *m_actNewProject{nullptr};
    QAction *m_actOpenProject{nullptr};
    QAction *m_actSaveProject{nullptr};
    QAction *m_actCloseProject{nullptr};
    QAction *m_actCaptureImage{nullptr};

    // View menu
    QMenu        *m_menuTheme{nullptr};
    QActionGroup *m_actGrpTheme{nullptr};
    QMenu        *m_menuLanguage{nullptr};
    QActionGroup *m_actGrpLanguage{nullptr};

    // Core objects
    std::shared_ptr<vc::model::Project> m_project;
    std::shared_ptr<ads::CDockWidget>   m_systemLogDock;
    SystemLogForm                      *m_systemLogForm{nullptr};
    ProjectInforSetting                *m_projectInfoWidget{nullptr};

    // Dock registries
    QMap<QString, UIDeviceEntry> m_deviceEntries;
    QMap<QString, UITaskEntry>   m_taskEntries;

    // State
    QStringList m_recentPaths;
    QString     m_lastFolderDir;
    QString     m_projectFilePath;
    QString     m_currentProjectName;
    bool        m_isDirty{false};
};

#endif // MAINWINDOW_H
