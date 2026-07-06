#ifndef M_DEVICES_TABLE_H
#define M_DEVICES_TABLE_H

#include <QObject>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QWidget>
#include <QHeaderView>
#include <QStringList>
#include <QHBoxLayout>
#include <QToolTip>
#include <QHelpEvent>

class MDevicesTable : public QTableWidget {
        Q_OBJECT
public:
    explicit MDevicesTable(QWidget *parent = nullptr);
    ~MDevicesTable();

    void setDeviceMap(QMap<QString, QString> *dv_map);
    void updateFromDeviceMap();
    void updateToDeviceMap();
    void setCommentEditableMode(bool enable);
    void removeAllRow();
    void addNewRow(QString name, bool status, QString comment);
    bool editDeviceStatus(QString name, bool status);
    bool editDeviceComment(QString name, QString comment);
    QString getDeviceComment(QString name, bool *ok = nullptr);
    void clearStatusAllDevices();

private:
    void initTable();
    void setNewRowData(int index, QString name, bool status, QString comment);
    void setEditableBackgroundColor();

protected:
    bool viewportEvent(QEvent* e) override {
        if (e->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(e);
            QTableWidgetItem* item = itemAt(he->pos());
            if (item) {
                QToolTip::showText(he->globalPos(), item->text(), viewport());
                return true;
            }
        }
        return QTableWidget::viewportEvent(e);
    }

private:
    bool m_comment_editable;
    QStringList m_table_headers = {tr("Name"), tr("State"), tr("Comment")};
    QMap<QString, QString> *m_device_map{nullptr};
};


#endif // M_DEVICES_TABLE_H
