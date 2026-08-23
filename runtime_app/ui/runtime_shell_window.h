#ifndef RUNTIME_SHELL_WINDOW_H
#define RUNTIME_SHELL_WINDOW_H

#include <memory>

#include <QList>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "DockManager.h"
#include "DockWidget.h"

#include "model/project.h"

class QAction;
class QActionGroup;
class SystemLogForm;

QT_BEGIN_NAMESPACE
namespace Ui { class RuntimeShellWindow; }
QT_END_NAMESPACE

/**
 * @file runtime_shell_window.h
 * @brief RuntimeShellWindow — main window of the operator runtime executable
 *        (`ncr_runtime.exe`).
 */

/**
 * @class RuntimeShellWindow
 * @brief Operator runtime shell: pick a project (or reload the last one), then run every
 *        localization task with nothing on screen but their dashboards.
 *
 * Two pages in a QStackedWidget, both authored in `runtime_shell_window.ui`:
 * @code
 *   [0] Project select — last-used path, Browse, and why the last attempt failed
 *   [1] Runtime view   — ADS dock manager, one LocalizationDashboardWidget per task
 * @endcode
 *
 * Startup goes straight to page 1 when the remembered project path exists and loads;
 * otherwise page 0, with a message that distinguishes "never ran one" from "the file is
 * gone" from "the file would not load". Those look identical to an operator at 6 a.m.
 * unless they are spelled out.
 *
 * @note This shell is read-only with respect to the project file. It loads, it runs, it
 *       never saves — commissioning edits belong to `ncr_picking.exe`.
 * @note It deliberately reuses LocalizationDashboardWidget and NOT LocalizationTaskWidget:
 *       the latter starts Commission on the task in its constructor, which is the opposite
 *       of what this executable is for.
 * @note Reaching commissioning goes through Project → Open Editor, which needs the `Admin`
 *       role and stops every running task first. The two shells never run at once.
 */
class RuntimeShellWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Builds the menus and docks. Does NOT load a project — see beginStartup().
    explicit RuntimeShellWindow(QWidget *parent = nullptr);
    /// Ends runtime on every running task before the window and its docks are destroyed.
    ~RuntimeShellWindow() override;

    /**
     * @brief Loads the remembered project and enters runtime on its tasks.
     *
     * Deliberately **not** called from the constructor. This work takes seconds — it reads
     * the project file, decodes every stored training image, and opens the camera, the PLC
     * socket and the vision-output port — and while it runs there is no window on screen at
     * all if it happens before show().
     *
     * That gap is what made a hand-off from the commissioning shell come up behind other
     * windows: `AllowSetForegroundWindow()` does not expire with time, but the operator
     * watching a bare desktop clicks something, and any user input not directed at this
     * process revokes the grant before presentShellWindow() ever asks for the foreground.
     * The commissioning shell's window costs milliseconds to build, which is why only this
     * direction showed the fault, and why it was intermittent rather than constant.
     *
     * Call it from `main()` **after** vc::ui::presentShellWindow(), on a queued invocation —
     * posting it from the constructor would queue it ahead of that function's raise and
     * reinstate the very ordering this exists to avoid.
     */
    void beginStartup();

protected:
    /// Ends runtime on every running task so device threads stop before the app exits.
    void closeEvent(QCloseEvent *event) override;

private slots:
    // ── Project menu ──────────────────────────────────────────────────────
    /// Opens a file dialog and starts the chosen project.
    void onLoadProject();
    /// Stops every running task and returns to the project-select page.
    void onCloseProject();
    /// Hands off to the commissioning shell: authorise, confirm, stop every task, exit.
    void onOpenEditor();

    // ── View menu ─────────────────────────────────────────────────────────
    /// Shows and raises the system-log dock, creating it on first use.
    void onSystemLogAction();
    /// Re-applies the tile layout for the newly chosen tile count.
    void onLayoutActionTriggered(QAction *action);
    /// Persists the chosen language and says it applies on the next start.
    void onLanguageActionTriggered(QAction *action);

private:
    // ── Construction ──────────────────────────────────────────────────────
    /// Wires the menu actions and installs the shared Theme/Language submenus.
    void buildMenus();
    /// Creates the ADS dock manager inside the form's dock host.
    void buildDockHost();
    /// Loads and applies this window's stylesheet for the active theme.
    void reloadStyleSheet();
    /// Checks the Theme action matching `styleId`, without re-triggering the group.
    void syncThemeAction(const QString &styleId);

    // ── Project lifecycle ─────────────────────────────────────────────────
    /// Loads the remembered project path if there is one and it still loads, otherwise
    /// shows the select page with the specific reason.
    void tryStartRememberedProject();
    /**
     * @brief Loads `path`, enters runtime on its tasks, and switches to the runtime page.
     *
     * The remembered path is written only on success, so a corrupt or missing file never
     * becomes the one the next launch tries first.
     *
     * @param[in] path absolute path of the .vproj file to open
     * @return true if the project loaded and the runtime page is now showing
     */
    bool startProject(const QString &path);
    /// Shows page 0 with `reason` displayed beneath the remembered path.
    void showProjectSelect(const QString &reason);

    // ── Runtime ───────────────────────────────────────────────────────────
    /// Creates one dock + dashboard per localization task (capped at kMaxTasks), calls
    /// beginRuntime() on each, and applies the default layout.
    void startTaskRuntimes();
    /// Calls endRuntime() on every task this shell started; safe to call more than once.
    void stopTaskRuntimes();
    /// Re-applies the tile layout for the currently checked Layout action.
    void applyCurrentLayout();
    /// Checks the Layout action matching `tileCount`, without re-triggering the group.
    void selectLayoutAction(int tileCount);
    /// Enables or disables the actions that only make sense with a project running.
    void setRuntimeActionsEnabled(bool enabled);
    /// Enables or disables the controls that must not be re-entered while beginStartup() runs.
    void setStartupControlsEnabled(bool enabled);

    /// Maximum tasks shown at once; matches RuntimeLayoutController::kMaxTiles.
    static const int kMaxTasks;

    Ui::RuntimeShellWindow *ui;  ///< Generated UI form; owns both pages, the menus and the actions.

    // ── UI ────────────────────────────────────────────────────────────────
    // The menus and actions are declared in runtime_shell_window.ui; these groups add the
    // exclusivity Designer cannot express.
    QActionGroup *m_layoutGroup{nullptr};        ///< Exclusive group over the Layout tile-count actions.
    QActionGroup *m_themeGroup{nullptr};         ///< Exclusive group over the Theme actions.
    QActionGroup *m_languageGroup{nullptr};      ///< Exclusive group over the Language actions.
    ads::CDockManager *m_dockManager{nullptr};   ///< Hosts one dock per task dashboard, plus the system-log dock.
    ads::CDockWidget  *m_systemLogDock{nullptr}; ///< Dock hosting the shared SystemLogForm; created on first use.

    // ── Model ─────────────────────────────────────────────────────────────
    std::shared_ptr<vc::model::Project> m_project;  ///< Currently running project; null until one loads.
    QString m_projectPath;                          ///< Path the current project was loaded from.
    QList<ads::CDockWidget *> m_taskDocks;          ///< Task docks in display order; owned by the dock manager.
    QStringList m_runningTaskIds;                   ///< Ids of tasks this shell called beginRuntime() on.
};

#endif // RUNTIME_SHELL_WINDOW_H
