#pragma once

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QString>
#include <QWidget>

/// Severity of one TaskEvent log entry; drives both the "LEVEL" badge text
/// and the QSS `severity` property used to color-code the row.
enum class TaskEventLevel {
    Info,       ///< Normal operational event
    Warning,    ///< Recoverable / attention-needed condition
    Error,      ///< Fault / failed operation
    Success     ///< Positive confirmation (cycle OK, recovery OK, …)
};

/// Data record for one row of TaskEventLogWidget.
struct TaskEvent {
    QDateTime      timestamp = QDateTime::currentDateTime();  ///< Defaults to the construction time.
    TaskEventLevel level     = TaskEventLevel::Info;  ///< Severity shown as the LEVEL badge.
    QString        message;  ///< Event text shown in the message column.
    /// Optional component tag shown between level and message,
    /// e.g. "Camera", "PLC", "Runtime".  Empty = hidden.
    QString        source;
};

/// Visual row inside TaskEventLogWidget for a single TaskEvent.
///
/// Layout (left to right):
///   [3 px severity bar] | [hh:mm:ss] | [LEVEL] | [[SOURCE]] | [message ...]
///
/// Styling is owned entirely by dark.qss / light.qss via:
///   TaskEventItemWidget[severity="info"|"warning"|"error"|"success"]
///   TaskEventItemWidget[...] QFrame[eventPart="bar"]
///   TaskEventItemWidget[...] QLabel[eventPart="level"]
///   QLabel[eventPart="time"|"message"|"source"]
class TaskEventItemWidget : public QWidget
{
    Q_OBJECT
public:
    /// Builds the row's child widgets from `event` and sets the `severity`
    /// dynamic property (before child creation, so the initial QSS polish
    /// already matches) for style-sheet-driven color coding.
    explicit TaskEventItemWidget(const TaskEvent &event,
                                 QWidget *parent = nullptr);

    /// Returns the severity level this row was constructed with.
    TaskEventLevel level() const { return m_level; }

private:
    TaskEventLevel m_level;  ///< Severity captured at construction time.

    /// Builds the severity bar, timestamp, level badge, optional source tag,
    /// and message labels and lays them out in a single horizontal row.
    void        setupUi(const TaskEvent &event);
    /// Maps a severity level to the QSS `severity` property value
    /// ("info"/"warning"/"error"/"success").
    static const char *severityProperty(TaskEventLevel level);
    /// Maps a severity level to its short badge text (INFO/WARN/ERROR/OK).
    static QString     levelText       (TaskEventLevel level);
};

/// Operator-facing task event log (read-only).
///
/// Designed for operator dashboards: shows discrete task events (ready, cycle
/// start, fault, recovery ...) as a timestamped, severity-coded list.
/// NOT a replacement for the global SystemLog -- scope is the active task only.
///
/// Usage:
///   TaskEvent ev;
///   ev.level   = TaskEventLevel::Error;
///   ev.message = "Camera grab timeout - fault code 102.";
///   ev.source  = "Camera";
///   m_eventLog->appendEvent(ev);
class TaskEventLogWidget : public QListWidget
{
    Q_OBJECT
public:
    /// Constructs an empty log and applies its list-view style (no
    /// selection highlight beyond single-selection, no focus rectangle,
    /// uniform row heights for fast painting).
    explicit TaskEventLogWidget(QWidget *parent = nullptr);

    /// Append one event row and auto-scroll to the bottom.
    void appendEvent(const TaskEvent &event);

    /// Remove all rows.
    void clearEvents();

    /// Maximum rows retained before oldest is discarded. 0 = unlimited.
    void setMaxEvents(int max);
    /// Returns the configured maximum row count (0 = unlimited).
    int  maxEvents() const { return m_maxEvents; }

private:
    int  m_maxEvents = 500;  ///< Oldest row is discarded once count() reaches this (0 = unlimited).
    /// Configures selection/scroll/focus behavior for the list view; visual
    /// styling itself comes from dark.qss / light.qss selectors.
    void setupStyle();
};
