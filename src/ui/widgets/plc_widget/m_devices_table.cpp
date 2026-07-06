#include "m_devices_table.h"
#include "ui/widgets/clamp.h"

MDevicesTable::MDevicesTable(QWidget *parent) :
    QTableWidget(parent) {

    m_comment_editable = true;
    initTable();
}

MDevicesTable::~MDevicesTable() {

}

void MDevicesTable::setDeviceMap(QMap<QString, QString> *dv_map) {
    if (dv_map != nullptr) {
        m_device_map = dv_map;
    }
    updateFromDeviceMap();
}

void MDevicesTable::updateFromDeviceMap() {
    if (m_device_map == nullptr) {
        return;
    }

    QMap<QString, QString>::iterator it_dv = m_device_map->begin();
    QMap<QString, QString>::iterator it_dv_end = m_device_map->end();
    for (;it_dv != it_dv_end;++it_dv) {
        // editDeviceComment(it_dv.key(), it_dv.value());
        QList<QTableWidgetItem*> match_result = this->findItems(it_dv.key(), Qt::MatchFlag::MatchExactly);
        if (match_result.isEmpty()) {
            continue;
        }

        QModelIndex edit_index = indexFromItem(match_result.first());
        int edit_row = edit_index.row();
        QTableWidgetItem *item_comment = this->item(edit_row, 2);
        item_comment->setText(it_dv.value());
    }
}

void MDevicesTable::updateToDeviceMap() {
    if (m_device_map == nullptr) {
        return;
    }

    int row_count = this->rowCount();
    for (int idx=0;idx<row_count;idx++) {
        QString name = this->item(idx, 0)->text();
        QString comment = this->item(idx, 2)->text();
        (*m_device_map)[name] = comment;
    }
}

void MDevicesTable::setCommentEditableMode(bool enable) {
    if (m_comment_editable != enable) {
        for (int index=0;index<this->rowCount();index++) {
            QTableWidgetItem *set_item = this->item(index, 2);
            if (enable) {
                set_item->setFlags(set_item->flags() & ~Qt::ItemIsSelectable | Qt::ItemIsEditable);
                continue;
            }
            set_item->setFlags(set_item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);
        }
    }

    m_comment_editable = enable;
    setEditableBackgroundColor();
}

void MDevicesTable::removeAllRow() {
    int total = 1;
    while (total > 0) {
        this->removeRow(0);
        total = this->rowCount();
    }
}

void MDevicesTable::addNewRow(QString name, bool status, QString comment) {
    QModelIndexList selected_row_items = this->selectedIndexes();
    if (selected_row_items.count() > 0) {
        int insert_index = selected_row_items.at(0).row() + 1;
        insertRow(insert_index);
        setNewRowData(insert_index, name, status, comment);
    } else {
        int new_index = this->rowCount();
        this->setRowCount(this->rowCount() + 1);
        setNewRowData(new_index, name, status, comment);
    }
    if (m_device_map != nullptr) {
        (*m_device_map)[name] = comment;
    }
}

bool MDevicesTable::editDeviceStatus(QString name, bool status) {
    QList<QTableWidgetItem*> match_result = this->findItems(name, Qt::MatchFlag::MatchExactly);
    if (match_result.isEmpty()) {
        // qDebug() << "Not found item:" << name;
        return false;
    }

    QModelIndex edit_index = indexFromItem(match_result.first());
    int edit_row = edit_index.row();
    CLamp *edit_lamp = (CLamp *)(this->cellWidget(edit_row, 1));
    edit_lamp->setEnableState(status);
    return true;
}

bool MDevicesTable::editDeviceComment(QString name, QString comment) {
    QList<QTableWidgetItem*> match_result = this->findItems(name, Qt::MatchFlag::MatchExactly);
    if (match_result.isEmpty()) {
        // qDebug() << "Not found item:" << name;
        return false;
    }

    QModelIndex edit_index = indexFromItem(match_result.first());
    int edit_row = edit_index.row();
    QTableWidgetItem *item_comment = this->item(edit_row, 2);
    item_comment->setText(comment);
    if (m_device_map != nullptr) {
        (*m_device_map)[name] = comment;
    }
    return true;
}

QString MDevicesTable::getDeviceComment(QString name, bool *ok) {
    QList<QTableWidgetItem*> match_result = this->findItems(name, Qt::MatchFlag::MatchExactly);
    if (match_result.isEmpty()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return "";
    }

    QModelIndex index = indexFromItem(match_result.first());
    return this->item(index.row(), 2)->text();
}

void MDevicesTable::clearStatusAllDevices() {
    for (int index=0;index<this->rowCount();index++) {
        CLamp *edit_lamp = (CLamp *)(this->cellWidget(index, 0));
        edit_lamp->setEnableState(false);
    }
}

void MDevicesTable::initTable() {
    this->setColumnCount(3);
    this->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    this->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    this->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    // this->setWordWrap(true);
    this->setColumnWidth(0, 100);
    this->setColumnWidth(1, 100);
    // this->setColumnWidth(2, 2000);
    // this->horizontalHeader()->setVisible(false);
    this->verticalHeader()->setVisible(false);
    this->setHorizontalHeaderLabels(m_table_headers);
    this->setHorizontalScrollMode(ScrollMode::ScrollPerPixel);
}

void MDevicesTable::setNewRowData(int index, QString name, bool status, QString comment) {
    CLamp *item_status = new CLamp(this);
    item_status->setEnableState(status);
    item_status->setLampMaxSize(20);
    setCellWidget(index, 1, item_status);
    // item(index, 0)->setFlags(item(index, 0)->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);

    QTableWidgetItem *item_name = new QTableWidgetItem(name);
    item_name->setFlags(item_name->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
    item_name->setTextAlignment(Qt::AlignmentFlag::AlignCenter);
    setItem(index, 0, item_name);

    QTableWidgetItem *item_comment = new QTableWidgetItem(comment);
    if (!m_comment_editable) {
        item_comment->setFlags(item_comment->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);
    } else {
        item_comment->setFlags(item_comment->flags() & ~Qt::ItemIsSelectable | Qt::ItemIsEditable);
    }
    item_comment->setToolTip(item_comment->text());
    setItem(index, 2, item_comment);
}

void MDevicesTable::setEditableBackgroundColor() {
    // if (m_comment_editable) {
    //     this->setStyleSheet("QFrame { background-color: white;}");
    //     return;
    // }
    // this->setStyleSheet("QFrame { background-color: lightGray;}");
}
