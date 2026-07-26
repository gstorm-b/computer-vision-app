#ifndef PROJECT_TREE_WIDGET_H
#define PROJECT_TREE_WIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QMap>
#include <QString>
#include <memory>

#include "device/device_manager.h"
#include "model/itask.h"
#include "model/project.h"

/// Custom Qt::ItemDataRole values used to stash tree-row metadata (id, kind,
/// parent link, and chip badge appearance) on each QStandardItem.
namespace TreeItemRole {
    constexpr int ItemId    = Qt::UserRole;      ///< QString — task or device ID
    constexpr int ItemKind  = Qt::UserRole + 1;  ///< int — 0:root  1:task  2:device
    constexpr int ParentId  = Qt::UserRole + 2;  ///< QString — parent task ID (devices only)
    constexpr int ChipText  = Qt::UserRole + 3;  ///< QString — "LOC" / "CAM" / "PLC"
    constexpr int ChipColor = Qt::UserRole + 4;  ///< QString — token name, e.g. "device.camera"
}

/// Kind of tree row stored in TreeItemRole::ItemKind: the project root, a task
/// node, or a device node.
enum class TreeItemKind { Root = 0, Task = 1, Device = 2 };

// ──────────────────────────────────────────────────────────────────────────────
//  ProjectTreeDelegate — draws colored chip badges on the right of each row
// ──────────────────────────────────────────────────────────────────────────────
/// Item delegate that paints the default row content shrunk to leave room on
/// the right, then draws a rounded, translucent-filled chip badge sourced from
/// TreeItemRole::ChipText / TreeItemRole::ChipColor over that space; rows with
/// no chip text fall back to plain QStyledItemDelegate painting.
class ProjectTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    /// Paints the row via QStyledItemDelegate with its rect shrunk to leave
    /// room for the chip, then draws the chip badge (rounded rect + label) in
    /// that reserved space when the index has chip text.
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    /// Returns the default size hint with the height clamped to a 22 px
    /// minimum, so rows are tall enough to fit the chip badge.
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

/// Displays the current project as a tree — project root, then each task with
/// its assigned devices nested underneath, using colored chip badges (via
/// ProjectTreeDelegate) to mark task/device type:
///
///  Tree structure:
///    ▾ Project Name                   [root]
///        ▾ Task_Loc_01         [LOC]  [task]
///            Basler_cam_01     [CAM]  [device]
///            PLC_Mitsu_01      [PLC]  [device]
///        ▾ Task_Loc_02         [LOC]  [task]
///            ...
///
///  Signals emitted (MainWindow connects these):
///    taskDoubleClicked(taskId, widgetName) — open task dock
///    deviceDoubleClicked(deviceId)         — open device config dock
///    addDeviceToTaskRequested(taskId)      — show AddDeviceWizard
///    moveDeviceRequested(taskId, deviceId) — show move-to-task dialog
///    deleteDeviceRequested(taskId, deviceId)
///    deleteTaskRequested(taskId)
class ProjectTreeWidget : public QWidget {
    Q_OBJECT

public:
    /// Builds the tree view + model, installs ProjectTreeDelegate, creates the
    /// "No Project" root placeholder item, wires double-click/context-menu/
    /// theme-change handling, and disables the widget until a project is set
    /// via setProject().
    explicit ProjectTreeWidget(QWidget *parent = nullptr);

    /// Adopts `proj` as the active project (or clears the tree if `proj` is
    /// null), caches its device manager, re-enables the widget, sets the root
    /// item's label to the project name (or "Project" if empty), and rebuilds
    /// the tree.
    /// @param proj the project to display; a null pointer clears the current project
    void setProject(std::shared_ptr<vc::model::Project> proj);
    /// Releases the current project and device manager, blocks context-menu/
    /// double-click actions, removes all task/device rows, resets the root
    /// item's label to "No Project", and disables the widget.
    void clearProject();
    void clearManager() { clearProject(); }   // backward-compat alias

    /// Updates the root item's displayed label to `name` (or "Project" if
    /// empty), without touching the rest of the tree.
    void changeProjectName(const QString &name);
    /// Enables or disables user interaction with the tree; when disabling,
    /// also sets m_accessBlock so double-click/context-menu handlers no-op.
    /// @param ena true to enable the tree, false to block interaction and disable it
    void enableProjectTree(bool ena);

public slots:
    /// Removes all task/device rows and rebuilds them from the current
    /// project's tasks (and each task's assigned devices), then expands the
    /// whole tree. No-op if there is no root item or no current project.
    void refreshTree();

signals:
    /// Emitted on double-click of a task row so the host can open its task dock.
    void taskDoubleClicked(const QString &taskId, const QString &widgetName);
    /// Emitted on double-click of a device row (or "Open Configuration" in its
    /// context menu) so the host can open its device config dock.
    void deviceDoubleClicked(const QString &deviceId);
    /// Emitted from the task context menu's "Add Device..." action so the host
    /// can show the AddDeviceWizard for `taskId`.
    void addDeviceToTaskRequested(const QString &taskId);
    /// Emitted from the device context menu's "Move to Task..." action so the
    /// host can show a dialog to move `deviceId` off `taskId`.
    void moveDeviceRequested(const QString &taskId, const QString &deviceId);
    /// Emitted from the device context menu's "Remove Device" action so the
    /// host can delete `deviceId` from `taskId`.
    void deleteDeviceRequested(const QString &taskId, const QString &deviceId);
    /// Emitted from the task context menu's "Delete Task" action so the host
    /// can delete `taskId`.
    void deleteTaskRequested(const QString &taskId);

    // kept for any legacy callers
    /// Legacy no-argument signal kept for backward compatibility; not emitted
    /// anywhere in this class (refreshTree() is now the single update path).
    void refreshDeviceBranch();
    /// Legacy no-argument signal kept for backward compatibility; not emitted
    /// anywhere in this class (refreshTree() is now the single update path).
    void refreshTaskBranch();

private slots:
    /// Handles a tree row double-click: emits taskDoubleClicked() for a task
    /// row or deviceDoubleClicked() for a device row. No-op if access is
    /// blocked (see m_accessBlock), the index is invalid, or no item resolves
    /// from it.
    void onItemDoubleClicked(const QModelIndex &index);
    /// Builds and shows a context menu appropriate to the row kind at `pos`:
    /// "New Localization Task..." for the root, "Add Device.../Delete Task"
    /// for a task row, or "Open Configuration/Move to Task.../Remove Device"
    /// for a device row. No-op if access is blocked, the position doesn't hit
    /// a valid row, or no item resolves from it.
    /// @param pos position in the tree view's viewport coordinates
    void showContextMenu(const QPoint &pos);

    /// Prompts for a new task name (re-prompting on empty or duplicate names
    /// until valid or cancelled), then creates a vc::model::TaskLocalization
    /// and adds it to the project. No-op if there is no current project.
    void onContextNewTask();
    /// Requests that the host add a device to `taskId` (shows AddDeviceWizard).
    void onContextAddDevice(const QString &taskId);
    /// Requests that the host delete the task `taskId`.
    void onContextDeleteTask(const QString &taskId);
    /// Requests that the host remove `deviceId` from `taskId`.
    void onContextDeleteDevice(const QString &taskId, const QString &deviceId);
    /// Requests that the host show a dialog to move `deviceId` (currently
    /// under `taskId`) to a different task.
    void onContextMoveDevice(const QString &taskId, const QString &deviceId);

private:
    /// Creates a task row (icon, name, id, kind, chip data) under the root
    /// item, records it in m_taskItems, then adds a child row for each of the
    /// task's assigned devices that still exists in the device manager. No-op
    /// if `task` is null or no device manager is set.
    void buildTaskItem(vc::model::ITask *task);
    /// Creates a device row (type-specific icon, name, id, kind, parent task
    /// id, chip data) and appends it as a child of `taskItem`. No-op if
    /// `device` or `taskItem` is null.
    void buildDeviceItem(QStandardItem *taskItem,
                         vc::device::IDevice *device,
                         const QString &taskId);

    /// Looks up the task's row item by id.
    /// @return the QStandardItem for `taskId`, or nullptr if not found
    QStandardItem *findTaskItem(const QString &taskId) const;

    bool m_accessBlock{true};   ///< When true, double-click/context-menu handlers no-op (no project loaded, or interaction disabled).

    QTreeView          *m_treeView{nullptr};   ///< Tree view showing the project/task/device hierarchy.
    QStandardItemModel *m_model{nullptr};      ///< Backing model for m_treeView.
    QStandardItem      *m_rootItem{nullptr};   ///< Always-present root row (project name, or "No Project").

    std::shared_ptr<vc::model::Project>        m_project;    ///< Currently displayed project, or null if none is set.
    std::shared_ptr<vc::device::DeviceManager> m_dvManager;  ///< Device manager cached from m_project, used to resolve task-assigned devices.

    QMap<QString, QStandardItem *> m_taskItems;   // taskId → item
};

#endif // PROJECT_TREE_WIDGET_H
