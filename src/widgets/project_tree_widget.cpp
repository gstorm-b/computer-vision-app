#include "project_tree_widget.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QFont>
#include <QApplication>
#include <QStyle>
#include <QDebug>

#include "model/task_localization.h"
#include "core/logger/app_logger.h"
#include "core/utils/theme_manager.h"
#include "core/utils/windows_helper.h"

// ──────────────────────────────────────────────────────────────────────────────
//  Chip appearance helpers
// ──────────────────────────────────────────────────────────────────────────────

struct ChipInfo { QString text; QString token; };

static ChipInfo chipForDeviceType(vc::device::DeviceType t) {
    switch (t) {
    case vc::device::DeviceType::Camera:   return { "CAM", QStringLiteral("device.camera") };
    case vc::device::DeviceType::PLC:      return { "PLC", QStringLiteral("device.plc") };
    default:                               return { "DEV", QStringLiteral("device.robot") };
    }
}

static ChipInfo chipForTaskType(vc::model::TaskType t) {
    switch (t) {
    case vc::model::TaskType::LocalizationTask: return { "LOC",  QStringLiteral("device.camera") };
    case vc::model::TaskType::PickPlaceTask:    return { "PICK", QStringLiteral("device.plc") };
    case vc::model::TaskType::InspectTask:      return { "INSP", QStringLiteral("device.robot") };
    default:                                    return { "?",    QStringLiteral("device.default") };
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  ProjectTreeDelegate
// ──────────────────────────────────────────────────────────────────────────────
static constexpr int kChipWidth  = 36;
static constexpr int kChipHeight = 14;
static constexpr int kChipMargin = 4;

void ProjectTreeDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const QString chip      = index.data(TreeItemRole::ChipText).toString();
    const QString chipToken = index.data(TreeItemRole::ChipColor).toString();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    if (chip.isEmpty()) {
        QStyledItemDelegate::paint(painter, opt, index);
        return;
    }

    // Shrink text rect to leave room for the chip on the right
    opt.rect.adjust(0, 0, -(kChipWidth + kChipMargin * 2), 0);
    QStyledItemDelegate::paint(painter, opt, index);

    // Draw chip badge
    const int cy = option.rect.top() + (option.rect.height() - kChipHeight) / 2;
    QRect chipRect(option.rect.right() - kChipWidth - kChipMargin,
                   cy, kChipWidth, kChipHeight);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QString tokenValue = ThemeManager::tokenValue(
        chipToken, ThemeManager::instance()->isDark());
    QColor base(tokenValue);
    if (!base.isValid())
        base = option.palette.color(QPalette::Highlight);
    QColor bg = base;
    bg.setAlpha(30);

    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(chipRect, 3, 3);

    QPen pen(base);
    pen.setWidthF(0.8);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(chipRect, 3, 3);

    painter->setPen(base);
    QFont f = painter->font();
    f.setPointSizeF(6.5);
    f.setWeight(QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    painter->setFont(f);
    painter->drawText(chipRect, Qt::AlignCenter, chip);

    painter->restore();
}

QSize ProjectTreeDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(s.height(), 22));
    return s;
}

// ──────────────────────────────────────────────────────────────────────────────
//  ProjectTreeWidget — constructor
// ──────────────────────────────────────────────────────────────────────────────
ProjectTreeWidget::ProjectTreeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(true);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setItemDelegate(new ProjectTreeDelegate(m_treeView));
    m_treeView->setIndentation(16);
    m_treeView->setAnimated(true);
    layout->addWidget(m_treeView);

    m_model = new QStandardItemModel(this);
    m_treeView->setModel(m_model);

    // Root item (project placeholder)
    m_rootItem = new QStandardItem(svgIcon(":/resrc/icon/new_file.svg"),
                                   tr("No Project"));
    m_rootItem->setEditable(false);
    m_rootItem->setData(0, TreeItemRole::ItemKind);
    m_model->appendRow(m_rootItem);
    m_treeView->expand(m_rootItem->index());

    connect(m_treeView, &QTreeView::doubleClicked,
            this, &ProjectTreeWidget::onItemDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &ProjectTreeWidget::showContextMenu);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) {
        if (m_treeView)
            m_treeView->viewport()->update();
    });

    setEnabled(false);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Public API
// ──────────────────────────────────────────────────────────────────────────────
void ProjectTreeWidget::setProject(std::shared_ptr<vc::model::Project> proj)
{
    if (!proj) { clearProject(); return; }

    m_project   = proj;
    m_dvManager = proj->deviceManager();
    m_accessBlock = false;
    setEnabled(true);

    m_rootItem->setText(proj->name().isEmpty() ? tr("Project") : proj->name());
    refreshTree();
}

void ProjectTreeWidget::clearProject()
{
    m_accessBlock = true;
    m_project.reset();
    m_dvManager.reset();
    m_taskItems.clear();

    if (m_rootItem->hasChildren())
        m_rootItem->removeRows(0, m_rootItem->rowCount());
    m_rootItem->setText(tr("No Project"));

    setEnabled(false);
}

void ProjectTreeWidget::changeProjectName(const QString &name)
{
    if (m_rootItem)
        m_rootItem->setText(name.isEmpty() ? tr("Project") : name);
}

void ProjectTreeWidget::enableProjectTree(bool ena)
{
    m_accessBlock = !ena;
    setEnabled(ena);
}

void ProjectTreeWidget::refreshTree()
{
    if (!m_rootItem) return;

    // Clear all task children
    if (m_rootItem->hasChildren())
        m_rootItem->removeRows(0, m_rootItem->rowCount());
    m_taskItems.clear();

    if (!m_project) return;

    // Rebuild from model
    const auto &tasks = m_project->getCurrentTasks();
    for (auto it = tasks.cbegin(); it != tasks.cend(); ++it) {
        buildTaskItem(it->get());
    }

    m_treeView->expandAll();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Tree builders
// ──────────────────────────────────────────────────────────────────────────────
void ProjectTreeWidget::buildTaskItem(vc::model::ITask *task)
{
    if (!task) return;

    ChipInfo chip = chipForTaskType(task->taskType());

    auto *item = new QStandardItem(svgIcon(":/resrc/icon/task_add.svg"),
                                   task->name());
    item->setEditable(false);
    item->setData(task->id(),                       TreeItemRole::ItemId);
    item->setData(static_cast<int>(TreeItemKind::Task), TreeItemRole::ItemKind);
    item->setData(chip.text,                        TreeItemRole::ChipText);
    item->setData(chip.token,                       TreeItemRole::ChipColor);

    m_rootItem->appendRow(item);
    m_taskItems.insert(task->id(), item);

    // Add owned devices
    if (!m_dvManager) return;
    const auto &devices = m_dvManager->getCurrentDevices();
    for (const QString &devId : task->assignedDeviceIds()) {
        if (devices.contains(devId))
            buildDeviceItem(item, devices.value(devId).get(), task->id());
    }
}

void ProjectTreeWidget::buildDeviceItem(QStandardItem *taskItem,
                                        vc::device::IDevice *device,
                                        const QString &taskId)
{
    if (!device || !taskItem) return;

    ChipInfo chip = chipForDeviceType(device->deviceType());

    QIcon icon;
    switch (device->deviceType()) {
    case vc::device::DeviceType::Camera:
        icon = svgIcon(":/resrc/icon/camera.svg");
        break;
    case vc::device::DeviceType::PLC:
        icon = svgIcon(":/resrc/icon/plc_devices.svg");
        break;
    case vc::device::DeviceType::VisionOutput:
        icon = svgIcon(":/resrc/icon/vision_target.svg");
        break;
    default:
        icon = svgIcon(":/resrc/icon/filter.svg");
        break;
    }

    auto *item = new QStandardItem(icon, device->name());
    item->setEditable(false);
    item->setData(device->id(),                         TreeItemRole::ItemId);
    item->setData(static_cast<int>(TreeItemKind::Device), TreeItemRole::ItemKind);
    item->setData(taskId,                               TreeItemRole::ParentId);
    item->setData(chip.text,                            TreeItemRole::ChipText);
    item->setData(chip.token,                           TreeItemRole::ChipColor);

    taskItem->appendRow(item);
}

QStandardItem *ProjectTreeWidget::findTaskItem(const QString &taskId) const
{
    return m_taskItems.value(taskId, nullptr);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Double-click navigation
// ──────────────────────────────────────────────────────────────────────────────
void ProjectTreeWidget::onItemDoubleClicked(const QModelIndex &index)
{
    if (m_accessBlock || !index.isValid()) return;

    auto *item = m_model->itemFromIndex(index);
    if (!item) return;

    const int kind = item->data(TreeItemRole::ItemKind).toInt();

    if (kind == static_cast<int>(TreeItemKind::Task)) {
        const QString taskId = item->data(TreeItemRole::ItemId).toString();
        emit taskDoubleClicked(taskId, QString());

    } else if (kind == static_cast<int>(TreeItemKind::Device)) {
        const QString deviceId = item->data(TreeItemRole::ItemId).toString();
        emit deviceDoubleClicked(deviceId);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  Context menu
// ──────────────────────────────────────────────────────────────────────────────
void ProjectTreeWidget::showContextMenu(const QPoint &pos)
{
    if (m_accessBlock) return;

    const QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid()) return;

    auto *item = m_model->itemFromIndex(index);
    if (!item) return;

    const int kind = item->data(TreeItemRole::ItemKind).toInt();
    QMenu menu(this);

    if (kind == static_cast<int>(TreeItemKind::Root)) {
        // Project root — add task
        QAction *actNew = menu.addAction(
            QApplication::style()->standardIcon(QStyle::SP_FileIcon),
            tr("New Localization Task..."));
        connect(actNew, &QAction::triggered, this, &ProjectTreeWidget::onContextNewTask);

    } else if (kind == static_cast<int>(TreeItemKind::Task)) {
        const QString taskId = item->data(TreeItemRole::ItemId).toString();

        QAction *actAdd = menu.addAction(
            QApplication::style()->standardIcon(QStyle::SP_ComputerIcon),
            tr("Add Device..."));
        connect(actAdd, &QAction::triggered, this, [this, taskId]() {
            onContextAddDevice(taskId);
        });

        menu.addSeparator();

        QAction *actDel = menu.addAction(
            QApplication::style()->standardIcon(QStyle::SP_TrashIcon),
            tr("Delete Task"));
        connect(actDel, &QAction::triggered, this, [this, taskId]() {
            onContextDeleteTask(taskId);
        });

    } else if (kind == static_cast<int>(TreeItemKind::Device)) {
        const QString deviceId = item->data(TreeItemRole::ItemId).toString();
        const QString taskId   = item->data(TreeItemRole::ParentId).toString();

        QAction *actOpen = menu.addAction(
            svgIcon(":/resrc/icon/setting.svg"),
            tr("Open Configuration"));
        connect(actOpen, &QAction::triggered, this, [this, deviceId]() {
            emit deviceDoubleClicked(deviceId);
        });

        QAction *actMove = menu.addAction(
            QApplication::style()->standardIcon(QStyle::SP_ArrowRight),
            tr("Move to Task..."));
        connect(actMove, &QAction::triggered, this, [this, taskId, deviceId]() {
            onContextMoveDevice(taskId, deviceId);
        });

        menu.addSeparator();

        QAction *actDel = menu.addAction(
            QApplication::style()->standardIcon(QStyle::SP_TrashIcon),
            tr("Remove Device"));
        connect(actDel, &QAction::triggered, this, [this, taskId, deviceId]() {
            onContextDeleteDevice(taskId, deviceId);
        });
    }

    if (!menu.isEmpty())
        menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

// ──────────────────────────────────────────────────────────────────────────────
//  Context actions
// ──────────────────────────────────────────────────────────────────────────────
void ProjectTreeWidget::onContextNewTask()
{
    if (!m_project) return;

    bool ok = false;
    QString name;
    while (true) {
        name = QInputDialog::getText(this, tr("New Task"),
                                     tr("Task name:"), QLineEdit::Normal,
                                     QString(), &ok);
        if (!ok) return;

        name = name.trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("Task name cannot be empty."));
            continue;
        }
        if (m_project->isTaskNameOccupied(name)) {
            QMessageBox::warning(this, tr("Error"), tr("Task name already exists."));
            continue;
        }
        break;
    }

    auto *task = new vc::model::TaskLocalization(name);
    m_project->addTask(task);          // triggers tasksChanged → refreshTree via MainWindow
    LOG_USER_INFO << QString("New task created: %1 (%2)").arg(name, task->id());
}

void ProjectTreeWidget::onContextAddDevice(const QString &taskId)
{
    emit addDeviceToTaskRequested(taskId);
}

void ProjectTreeWidget::onContextDeleteTask(const QString &taskId)
{
    emit deleteTaskRequested(taskId);
}

void ProjectTreeWidget::onContextDeleteDevice(const QString &taskId,
                                              const QString &deviceId)
{
    emit deleteDeviceRequested(taskId, deviceId);
}

void ProjectTreeWidget::onContextMoveDevice(const QString &taskId,
                                            const QString &deviceId)
{
    emit moveDeviceRequested(taskId, deviceId);
}
