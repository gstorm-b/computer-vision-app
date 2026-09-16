#ifndef LOCALIZATION_SETTING_WIDGET_H
#define LOCALIZATION_SETTING_WIDGET_H

#include <QWidget>

#include "DockWidget.h"

#include "model/task_localization.h"
#include "ui/forms/task_widget.h"

namespace Ui {
class LocalizationSettingWidget;
}

/**
 * @file localization_setting_widget.h
 * @brief LocalizationSettingWidget — task settings widget for a localization task.
 */

/**
 * @class LocalizationSettingWidget
 * @brief Task settings widget for a localization task: binds the vision-output and PLC comm
 *        devices, the camera-number map, per-camera ROI workspaces, and the PLC signal-tag
 *        map to the underlying TaskLocalization's TaskLocalizeConfig, keeping the UI and the
 *        task's config in sync in both directions.
 */
class LocalizationSettingWidget : public ITaskWidget {
    Q_OBJECT

public:
    /**
     * @brief Builds the widget for @p task, sets up the generated UI, and wires up all
     *        internal signal/slot connections via initWidget().
     * @param[in] task the owning task (expected to be a TaskLocalization; a
     *        dynamic_cast failure is logged and leaves the widget inert)
     * @param[in] dock optional dock widget host
     * @param[in] parent optional parent widget
     */
    explicit LocalizationSettingWidget(std::shared_ptr<vc::model::ITask> task,
                                       ads::CDockWidget *dock = nullptr,
                                       QWidget *parent = nullptr);
    /// Destroys the generated UI object.
    ~LocalizationSettingWidget();

    /// Pushes the widget's current config (m_config) into the bound task.
    void loadConfigToTask() override;
    /// Reloads m_config from the task and repopulates every UI element
    /// (device combos, camera map, workspace rows, signal-tag values) to
    /// reflect it.
    void loadConfigToWidget() override;

    /**
     * @brief Offers to purge the signal map's orphaned rows, before the project is saved.
     *
     * An orphan is a tag that is no longer in the bound PLC's list — most often because the
     * device's configured range shrank, or the device was swapped for one that speaks a
     * different address space. Since Phase 9 / C4 the runtime refuses to start on one, so a
     * project saved with orphans is a project that will not run.
     *
     * **Required rows are never purged without the operator being told what that costs.** The
     * rows are classified against LocalizationRuntimeController::requiredSignalNames(), and
     * clearing a required one converts "the tag is wrong" into "the signal is unmapped" —
     * which is still a refusal, just a less informative one.
     *
     * @return false only when the operator chose to abandon the save; true when there was
     *         nothing to ask about, or the question was answered.
     * @note Purging writes "" back into the config through the existing signalMappingChanged
     *       wiring, for the confirmed rows only.
     */
    bool confirmOrphanedSignalsBeforeSave();

private:
    // ── Build / wire-up ─────────────────────────────────────────────────
    /// One-time setup: resolves m_localizeTask, loads the initial config,
    /// appends the fixed signal-row schema to the signals map widget, wires
    /// all combo/list/dialog signals to their handlers, subscribes to the
    /// task's devicesChanged and the project's deviceManager modification
    /// signal, then calls loadConfigToWidget().
    void initWidget();
    /// Rebuilds the vision-output-device and comm-device (PLC) comboboxes
    /// from the task's currently assigned devices, keeping each combo's
    /// previously selected device id selected if it is still present.
    void rebuildDeviceCombos();
    /// Rebuilds the camera mapping widget's available camera options from
    /// the task's currently assigned Camera-type devices.
    void rebuildCameraList();
    /// Rebuilds the camera workspace list widget with the task's assigned
    /// cameras and refreshes each row's ROI/condition-ROI/reference-image
    /// state via refreshWorkspaceRow().
    void rebuildCameraWorkspaceList();
    /// Refreshes the bool/number IO tag lists shown in the signals map
    /// widget from the digital/word IO providers exposed by the device
    /// identified by `deviceId`; clears both tag lists if the device is
    /// missing or does not implement the required IO provider interfaces.
    void refreshCommTags(const QString &deviceId);
    /// Reloads the whole widget (loadConfigToWidget()) when `deviceId`
    /// refers to a device assigned to this task, in response to that
    /// device being modified elsewhere.
    void refreshDevicePresentation(const QString &deviceId);
    /// Writes m_config back into m_localizeTask via setTaskLocalizeConfig().
    void pushConfigToTask();

    // ── Camera workspace (ROI) ──────────────────────────────────────────
    /// Updates the "use workspace" (ROI) flag for `cameraId`'s workspace,
    /// persists it via pushConfigToTask(), and refreshes its row UI.
    void onWorkspaceUseToggled(const QString &cameraId, bool enabled);
    /// Updates the "use condition workspace" flag for `cameraId`'s
    /// workspace, persists it via pushConfigToTask(), and refreshes its
    /// row UI.
    void onWorkspaceConditionToggled(const QString &cameraId, bool enabled);
    /// Opens a WorkspaceSettingDialog for `cameraId` so the user can define
    /// the working/condition ROIs and optionally grab a reference image
    /// through the live CameraRunner; on acceptance, writes the resulting
    /// ROIs, flags, and reference image back into the camera's workspace
    /// config and persists it via pushConfigToTask().
    void onWorkspaceSetRequested(const QString &cameraId);

    /// Push the current workspace state of one camera to its row widget.
    void refreshWorkspaceRow(const QString &cameraId);

    // ── Robot pick check ────────────────────────────────────────────────
    /**
     * @brief Opens the RobotKinematicCheckWidget on the TASK's pick-check settings.
     *
     * Hosted in a dialog rather than inline, following the camera-workspace precedent
     * (onWorkspaceSetRequested()): the editor carries a pick-path table and an FK/IK tester
     * and is far taller than the other frames on this page, which has no scroll area of its
     * own.
     *
     * The vision-output device widgets keep their own copies of this editor, bound to their
     * own device configs. Those drive the device-side advisory check only; since Phase 9 / F1
     * the localization runtime reads the task's setting — this one.
     */
    void onRobotCheckSetRequested();
    /// Updates the one-line summary next to the "Set…" button from m_config.
    void refreshRobotCheckSummary();

private:
    Ui::LocalizationSettingWidget *ui;  ///< Generated UI accessor; owned by this widget.

    vc::model::TaskLocalization     *m_localizeTask{nullptr};  ///< Non-owning cast of ITaskWidget's m_task; null if the task is not a TaskLocalization.
    vc::model::TaskLocalizeConfig    m_config;  ///< Local working copy of the task's localization config, synced with the task via loadConfigToWidget()/pushConfigToTask().
};

#endif // LOCALIZATION_SETTING_WIDGET_H
