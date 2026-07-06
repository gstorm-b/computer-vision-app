#pragma once

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QString>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventLevel  —  severity of one log entry
// ─────────────────────────────────────────────────────────────────────────────
enum class TaskEventLevel {
    Info,       ///< Normal operational event
    Warning,    ///< Recoverable / attention-needed condition
    Error,      ///< Fault / failed operation
    Success     ///< Positive confirmation (cycle OK, recovery OK, …)
};

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEvent  —  data record for one row
// ─────────────────────────────────────────────────────────────────────────────
struct TaskEvent {
    QDateTime      timestamp = QDateTime::currentDateTime();
    TaskEventLevel level     = TaskEventLevel::Info;
    QString        message;
    /// Optional component tag shown between level and message,
    /// e.g. "Camera", "PLC", "Runtime".  Empty = hidden.
    QString        source;
};

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventItemWidget  —  visual row inside TaskEventLogWidget
//
//  Layout (left → right):
//    [3 px severity bar] | [hh:mm:ss] | [LEVEL] | [[SOURCE]] | [message …]
//
//  Styling is owned entirely by dark.qss / light.qss via:
//    TaskEventItemWidget[severity="info"|"warning"|"error"|"success"]
//    TaskEventItemWidget[...] QFrame[eventPart="bar"]
//    TaskEventItemWidget[...] QLabel[eventPart="level"]
//    QLabel[eventPart="time"|"message"|"source"]
// ─────────────────────────────────────────────────────────────────────────────
class TaskEventItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskEventItemWidget(const TaskEvent &event,
                                 QWidget *parent = nullptr);

    TaskEventLevel level() const { return m_level; }

private:
    TaskEventLevel m_level;

    void        setupUi(const TaskEvent &event);
    static const char *severityProperty(TaskEventLevel level);
    static QString     levelText       (TaskEventLevel level);
};

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventLogWidget  —  operator-facing task event log (read-only)
//
//  Designed for operator dashboards: shows discrete task events (ready, cycle
//  start, fault, recovery …) as a timestamped, severity-coded list.
//  NOT a replacement for the global SystemLog — scope is the active task only.
//
//  Usage:
//    TaskEvent ev;
//    ev.level   = TaskEventLevel::Error;
//    ev.message = "Camera grab timeout — fault code 102.";
//    ev.source  = "Camera";
//    m_eventLog->appendEvent(ev);
// ─────────────────────────────────────────────────────────────────────────────
class TaskEventLogWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit TaskEventLogWidget(QWidget *parent = nullptr);

    /// Append one event row and auto-scroll to the bottom.
    void appendEvent(const TaskEvent &event);

    /// Remove all rows.
    void clearEvents();

    /// Maximum rows retained before oldest is discarded. 0 = unlimited.
    void setMaxEvents(int max);
    int  maxEvents() const { return m_maxEvents; }

private:
    int  m_maxEvents = 500;
    void setupStyle();
};
