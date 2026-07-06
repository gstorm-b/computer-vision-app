#ifndef D_DEVICES_TABLE_H
#define D_DEVICES_TABLE_H

#include <QObject>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QWidget>
#include <QHeaderView>
#include <QStringList>
#include <QHBoxLayout>
#include <QToolTip>
#include <QHelpEvent>

class DDevicesTable : public QTableWidget {
    Q_OBJECT
public:
    explicit DDevicesTable(QWidget *parent = nullptr);
    ~DDevicesTable();

    void setDeviceMap(QMap<QString, QString> *dv_map);
    void updateFromDeviceMap();
    void updateToDeviceMap();
    void setCommentEditableMode(bool enable);
    void removeAllRow();
    void addNewRow(QString name, qint16 value, QString comment);
    bool editDeviceValue(QString name, qint16 value);
    bool editDeviceComment(QString name, QString comment);
    QString getDeviceComment(QString name, bool *ok = nullptr);
    void clearStatusAllDevices();

private:
    void initTable();
    void setNewRowData(int index, QString name, qint16 value, QString comment);
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
    QStringList m_table_headers = {tr("Name"), tr("Value"), tr("Comment")};
    QMap<QString, QString> *m_device_map{nullptr};
};

#endif // D_DEVICES_TABLE_H
