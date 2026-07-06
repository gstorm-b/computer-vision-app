#pragma once
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
//  StatusLamp  —  read-only state indicator for operator dashboards
//
//  Thin QFrame subclass (Rule 3.8). The stable class name allows QSS selectors
//  like  StatusLamp[status="ok"] { border-left-color: #40c870; }
//  to apply uniformly to all lamp instances without per-objectName rules.
//
//  Layout:
//    ┌──────────────┐
//    │  TASK        │  <- lampPart="name"  (small, muted, uppercase)
//    │  Ready       │  <- lampPart="state" (larger, bold, severity-coloured)
//    └──────────────┘
//
//  Usage (from host cpp):
//    ui->lamp_task->setLampName("Task");
//    ui->lamp_task->setStatus(StatusLamp::Status::Ok, tr("Ready"));
// ─────────────────────────────────────────────────────────────────────────────
class StatusLamp : public QFrame
{
    Q_OBJECT
public:
    enum class Status { Ok, Warning, Error, Off };
    Q_ENUM(Status)

    explicit StatusLamp(QWidget *parent = nullptr);

    /// Set the fixed name displayed above the state text (shown in UPPER CASE).
    void setLampName(const QString &name);

    /// Update the status property (triggers QSS repolish) and state text.
    void setStatus(Status status, const QString &stateText = QString());

    Status  status()    const { return m_status; }
    QString lampName()  const { return m_nameLabel->text(); }
    QString stateText() const { return m_stateLabel->text(); }

private:
    Status  m_status    = Status::Off;
    QLabel *m_nameLabel  = nullptr;
    QLabel *m_stateLabel = nullptr;

    static const char *statusProperty(Status s);
};
