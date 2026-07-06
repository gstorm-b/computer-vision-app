#ifndef SYSTEM_LOG_FORM_H
#define SYSTEM_LOG_FORM_H

#include "logger/app_logger.h"

#include <QVector>
#include <QWidget>

namespace Ui {
class SystemLogForm;
}

class SystemLogForm : public QWidget {
    Q_OBJECT
public:
    explicit SystemLogForm(QWidget *parent = nullptr);
    ~SystemLogForm();

private slots:
    void onNewLogReceived(const LogMessage &msg);
    void onViewModeChanged();
    void onClearAll();

private:
    void reloadStyleSheet();
    void rerenderLogs();
    bool shouldShow(const LogMessage &msg) const;
    QString renderLogEntry(const LogMessage &msg, bool dark) const;

    Ui::SystemLogForm *ui;
    QVector<LogMessage> m_entries;
};

#endif // SYSTEM_LOG_FORM_H
