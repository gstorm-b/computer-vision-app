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

/// Read-only operator dashboard for a TaskLocalization task: status lamps, device connection
/// lamps, the live signal monitor, per-cycle KPI/result table with a vision result viewer
/// overlay, and the task event log. Refreshes purely from TaskLocalization signals
/// (devicesChanged, taskStateChanged, cycleResultUpdated, taskLogAppended, signalChanged) and
/// never writes configuration or signal values back to the task.
class LocalizationDashboardWidget : public ITaskWidget
{
    Q_OBJECT

public:
    /// Builds the dashboard UI for `task` and wires it up via initWidget(). `task` is expected to
    /// be a TaskLocalization; if it is not, initWidget() logs an error and the widget stays
    /// otherwise inert.
    /// @param task the task model this dashboard displays/monitors
    /// @param dock the owning dock widget (forwarded to ITaskWidget for theme/lifecycle handling)
    /// @param parent optional parent widget
    explicit LocalizationDashboardWidget(std::shared_ptr<vc::model::ITask> task,
                                         ads::CDockWidget *dock = nullptr,
                                         QWidget *parent = nullptr);

    /// Destroys the generated UI object; the widget itself owns no other resources requiring
    /// explicit cleanup (connections are torn down by QObject).
    ~LocalizationDashboardWidget();

    /// No-op: the dashboard is read-only and never pushes edits back to the task.
    void loadConfigToTask() override;
    /// Re-reads the task's TaskLocalizeConfig and refreshes the signal tags, task context labels,
    /// and task-state lamp from it.
    void loadConfigToWidget() override;

private:
    /// Resolves m_localizeTask from the base-class task pointer and, on success, wires up the
    /// lamps, connection lamps, vision viewer, result table, and all task/signal connections; the
    /// single entry point called once from the constructor.
    void initWidget();
    /// Sets the display names and idle (Off) status/text for the task/camera/pattern/cycle lamps.
    void setupLamps();

    // ── Device connection lamps (hl_state_connection) ──────────────────────
    // The PLC / Camera / Vision-Output lamps reflect the live link state of the
    // devices currently bound to the task. Each lamp subscribes to its device's
    // runner (IDeviceRunner::connectStatusChanged, forwarded onto the GUI
    // thread); the subscriptions are rebuilt whenever the device bindings or the
    // active camera change.
    /// Sets the display names and idle (Off) status/text for the three connection lamps, then
    /// calls rebuildConnectionWiring() to wire them to the currently bound devices.
    void setupConnectionLamps();
    /// Disconnects all current connection-lamp subscriptions (m_connectionConns) and rewires the
    /// PLC, camera, and vision-output lamps from the current device bindings/active camera.
    void rebuildConnectionWiring();
    /// Wires a single connection lamp to `deviceId`'s runner: seeds the lamp from the runner's
    /// current connect status and subscribes to further connectStatusChanged updates, appending
    /// the connection to m_connectionConns. Sets the lamp to "Not set"/"—" when the id is empty or
    /// no runner is registered yet for it.
    /// @param lamp the connection lamp to update (no-op if null)
    /// @param deviceId the bound device id to resolve a runner for
    void wireConnectionLamp(StatusLamp *lamp, const QString &deviceId);
    /// Maps a vc::device::ConnectStatus to the lamp's Status/text pair (e.g. Connected → Ok,
    /// LostConnected/ConnectFailed → Error, Disconnected/NoConnection → Off) and applies it.
    /// @param lamp the connection lamp to update (no-op if null)
    /// @param status the device's current connect status
    void applyConnectStatusToLamp(StatusLamp *lamp, vc::device::ConnectStatus status);
    /// Active camera device for the camera lamp: the device mapped to the live
    /// nActiveCamera number, falling back to the first bound camera.
    /// @return the resolved device id, or an empty string if no camera is bound.
    QString resolveActiveCameraDeviceId() const;

    /// Refreshes each signal-monitor row's tag from the current config (readConfigField), so the
    /// monitor can show "(not mapped)" for unbound signals; live values themselves still arrive
    /// via TaskLocalization::signalChanged.
    void pushSignalTagsFromConfig();
    /// Refreshes the vision-output/PLC device labels and resets the camera/pattern-group value
    /// labels to "—" from the current device bindings in m_config.
    void updateTaskContext();
    /// Maps the task's current TaskState to the "Cycle" lamp's status (Faulted → Error,
    /// RunningCycle/Recovering → Warning, Ready → Ok, otherwise Off) and applies it with the
    /// state's display name as the lamp text.
    void updateTaskStateLabel();
    /// Configures tbl_result's 11 result columns/headers, clears its rows, and wires selection
    /// sync (itemSelectionChanged) and an Escape shortcut to clear the selection.
    void setupResultTable();
    /// Updates the KPI labels, the vision result viewer/legacy image view, and repopulates
    /// tbl_result from a completed cycle's result.
    /// @param result the cycle's detected/sent counts, timing, image(s), and per-object rows
    void updateCycleResult(const vc::model::LocalizationRuntimeController::CycleResult &result);
    /// Converts a runtime task-log entry to a TaskEvent (mapping its severity string via
    /// severityToLevel) and appends it to the operator log view.
    /// @param entry the runtime log entry (timestamp, severity, message) to append
    void appendTaskLog(const vc::model::LocalizationRuntimeController::TaskLogEntry &entry);
    /// Lazily creates the VisionResultViewerWidget, swaps it in place of the legacy gv_match_view
    /// in the layout, and connects its resultObjectSelectionChanged signal to the table sync slot.
    void installVisionViewer();
    /// Selects the vision-viewer result object matching the currently selected table row (or
    /// clears the viewer selection if no row, or no matching overlay index, is selected).
    void syncResultSelectionFromTable();
    /// Selects the table row whose overlay index matches `objectIndex` (or clears the table
    /// selection if `objectIndex` is non-positive or has no matching row); blocks the table's
    /// signals while doing so to avoid re-triggering syncResultSelectionFromTable().
    /// @param objectIndex the vision-viewer result object index selected by the user
    void syncResultSelectionFromViewer(int objectIndex);
    /// Clears both the result table's selection (signal-blocked) and the vision viewer's selected
    /// result object.
    void clearResultSelection();
    /// Looks up the overlay object index stashed in column 0's Qt::UserRole data for `row`.
    /// @param row the result-table row to query
    /// @return the row's overlay object index, or -1 if `row` is out of range or has no item
    int resultOverlayIndexForRow(int row) const;

    /// Route a single live signal value to its dashboard visual (state lamps,
    /// fault panel, or context value label). Monitor-list refresh is handled
    /// separately at the call site.
    /// @param name the signal's internalName (e.g. "bCameraValid", "nFaultCode")
    /// @param value the signal's current value
    void applySignalToDashboard(const QString &name, const QVariant &value);
    /// Drive the fault panel (frame_fault[active] + value labels + task lamp).
    /// @param active whether the task fault is currently active
    /// @param faultCode the current fault code to display
    void setFaultState(bool active, int faultCode);

private:
    Ui::LocalizationDashboardWidget *ui;  ///< Generated UI form; owned by this widget.

    vc::model::TaskLocalization *m_localizeTask{nullptr};  ///< Task model this dashboard monitors; not owned.
    vc::model::TaskLocalizeConfig m_config;  ///< Cached copy of the task's current localize config.

    int  m_lastFaultCode{0};  ///< Most recently reported nFaultCode value.
    bool m_taskFaultActive{false};  ///< Whether bTaskFault is currently active (drives the fault panel/lamp).
    QMap<QString, QVariant> m_liveSignalValues;  ///< Latest value received per signal internalName, keyed for overlay lookups.
    VisionResultViewerWidget *m_resultViewer{nullptr};  ///< Lazily-created vision result viewer that replaces gv_match_view; owned via Qt parent-child.

    /// Live nActiveCamera number; -1 = unknown (use first bound camera).
    int m_activeCameraNumber{-1};
    /// Active runner subscriptions for the connection lamps; dropped/rebuilt on
    /// binding or active-camera changes.
    QList<QMetaObject::Connection> m_connectionConns;
};

#endif // LOCALIZATION_DASHBOARD_WIDGET_H
