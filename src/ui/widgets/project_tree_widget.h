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

// ──────────────────────────────────────────────────────────────────────────────
//  Role constants for tree items
// ──────────────────────────────────────────────────────────────────────────────
namespace TreeItemRole {
    constexpr int ItemId    = Qt::UserRole;      // QString — task or device ID
    constexpr int ItemKind  = Qt::UserRole + 1;  // int — 0:root  1:task  2:device
    constexpr int ParentId  = Qt::UserRole + 2;  // QString — parent task ID (devices only)
    constexpr int ChipText  = Qt::UserRole + 3;  // QString — "LOC" / "CAM" / "PLC"
    constexpr int ChipColor = Qt::UserRole + 4;  // QString — token name, e.g. "device.camera"
}

enum class TreeItemKind { Root = 0, Task = 1, Device = 2 };

// ──────────────────────────────────────────────────────────────────────────────
//  ProjectTreeDelegate — draws colored chip badges on the right of each row
// ──────────────────────────────────────────────────────────────────────────────
class ProjectTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

// ──────────────────────────────────────────────────────────────────────────────
//  ProjectTreeWidget
//
//  Tree structure:
//    ▾ Project Name                   [root]
//        ▾ Task_Loc_01         [LOC]  [task]
//            Basler_cam_01     [CAM]  [device]
//            PLC_Mitsu_01      [PLC]  [device]
//        ▾ Task_Loc_02         [LOC]  [task]
//            ...
//
//  Signals emitted (MainWindow connects these):
//    taskDoubleClicked(taskId, widgetName) — open task dock
//    deviceDoubleClicked(deviceId)         — open device config dock
//    addDeviceToTaskRequested(taskId)      — show AddDeviceWizard
//    moveDeviceRequested(taskId, deviceId) — show move-to-task dialog
//    deleteDeviceRequested(taskId, deviceId)
//    deleteTaskRequested(taskId)
// ──────────────────────────────────────────────────────────────────────────────
class ProjectTreeWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProjectTreeWidget(QWidget *parent = nullptr);

    void setProject(std::shared_ptr<vc::model::Project> proj);
    void clearProject();
    void clearManager() { clearProject(); }   // backward-compat alias

    void changeProjectName(const QString &name);
    void enableProjectTree(bool ena);

public slots:
    void refreshTree();

signals:
    void taskDoubleClicked(const QString &taskId, const QString &widgetName);
    void deviceDoubleClicked(const QString &deviceId);
    void addDeviceToTaskRequested(const QString &taskId);
    void moveDeviceRequested(const QString &taskId, const QString &deviceId);
    void deleteDeviceRequested(const QString &taskId, const QString &deviceId);
    void deleteTaskRequested(const QString &taskId);

    // kept for any legacy callers
    void refreshDeviceBranch();
    void refreshTaskBranch();

private slots:
    void onItemDoubleClicked(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);

    void onContextNewTask();
    void onContextAddDevice(const QString &taskId);
    void onContextDeleteTask(const QString &taskId);
    void onContextDeleteDevice(const QString &taskId, const QString &deviceId);
    void onContextMoveDevice(const QString &taskId, const QString &deviceId);

private:
    void buildTaskItem(vc::model::ITask *task);
    void buildDeviceItem(QStandardItem *taskItem,
                         vc::device::IDevice *device,
                         const QString &taskId);

    QStandardItem *findTaskItem(const QString &taskId) const;

    bool m_accessBlock{true};

    QTreeView          *m_treeView{nullptr};
    QStandardItemModel *m_model{nullptr};
    QStandardItem      *m_rootItem{nullptr};

    std::shared_ptr<vc::model::Project>        m_project;
    std::shared_ptr<vc::device::DeviceManager> m_dvManager;

    QMap<QString, QStandardItem *> m_taskItems;   // taskId → item
};

#endif // PROJECT_TREE_WIDGET_H
