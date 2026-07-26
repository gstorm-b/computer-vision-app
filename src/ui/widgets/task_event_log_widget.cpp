#include "task_event_log_widget.h"

#include <QHBoxLayout>
#include <QSizePolicy>

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventItemWidget
// ─────────────────────────────────────────────────────────────────────────────

/// Constructs the row widget for `event`: records its severity, tags the
/// widget with the matching `severity` dynamic property, and builds the
/// child layout via setupUi().
TaskEventItemWidget::TaskEventItemWidget(const TaskEvent &event,
                                         QWidget *parent)
    : QWidget(parent)
    , m_level(event.level)
{
    // Set the severity property BEFORE setupUi() so that when child widgets
    // are first shown, the initial QSS polish already evaluates the correct
    // parent[severity="…"] selector without requiring an explicit repolish.
    setProperty("severity", severityProperty(event.level));
    setupUi(event);
}

/// Builds the row's child layout: a fixed-width severity bar, followed by a
/// horizontal content row of fixed-width timestamp and level labels, an
/// optional source tag (only when `event.source` is non-empty), and a
/// stretching, mouse-selectable message label.
void TaskEventItemWidget::setupUi(const TaskEvent &event)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Left severity bar (3 px, full row height) ─────────────────────────────
    auto *bar = new QFrame(this);
    bar->setProperty("eventPart", QStringLiteral("bar"));
    bar->setFixedWidth(3);
    bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    root->addWidget(bar);

    // ── Horizontal content row ────────────────────────────────────────────────
    auto *content = new QHBoxLayout();
    content->setContentsMargins(8, 4, 8, 4);
    content->setSpacing(10);

    // Timestamp — monospace, fixed width, text.muted
    auto *time = new QLabel(event.timestamp.toString(QStringLiteral("hh:mm:ss")), this);
    time->setProperty("eventPart", QStringLiteral("time"));
    time->setFixedWidth(56);
    time->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    content->addWidget(time);

    // Level badge — fixed width so columns stay aligned across severities
    auto *lv = new QLabel(levelText(event.level), this);
    lv->setProperty("eventPart", QStringLiteral("level"));
    lv->setFixedWidth(44);
    lv->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    content->addWidget(lv);

    // Source tag — optional component identifier, e.g. "[Camera]"
    if (!event.source.isEmpty()) {
        auto *src = new QLabel(QStringLiteral("[%1]").arg(event.source), this);
        src->setProperty("eventPart", QStringLiteral("source"));
        src->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        content->addWidget(src);
    }

    // Message — stretches to fill remaining width; single-line for scan speed
    auto *msg = new QLabel(event.message, this);
    msg->setProperty("eventPart", QStringLiteral("message"));
    msg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    msg->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    msg->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content->addWidget(msg, 1);

    root->addLayout(content);
}

/// Maps a severity level to the QSS `severity` property value; unmatched/
/// default levels map to "info".
/// @return "warning", "error", "success", or "info"
const char *TaskEventItemWidget::severityProperty(TaskEventLevel level)
{
    switch (level) {
    case TaskEventLevel::Warning: return "warning";
    case TaskEventLevel::Error:   return "error";
    case TaskEventLevel::Success: return "success";
    default:                      return "info";
    }
}

/// Maps a severity level to its short badge text; unmatched/default levels
/// map to "INFO".
/// @return "WARN", "ERROR", "OK", or "INFO"
QString TaskEventItemWidget::levelText(TaskEventLevel level)
{
    switch (level) {
    case TaskEventLevel::Warning: return QStringLiteral("WARN");
    case TaskEventLevel::Error:   return QStringLiteral("ERROR");
    case TaskEventLevel::Success: return QStringLiteral("OK");
    default:                      return QStringLiteral("INFO");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventLogWidget
// ─────────────────────────────────────────────────────────────────────────────

/// Constructs an empty log and applies its list-view style via setupStyle().
TaskEventLogWidget::TaskEventLogWidget(QWidget *parent)
    : QListWidget(parent)
{
    setupStyle();
}

/// Configures selection, scrolling, and focus behavior for the list view:
/// single selection, per-pixel vertical scrolling, no horizontal scrollbar,
/// uniform item sizes (faster painting), no spacing, and no focus rectangle.
/// Visual styling itself comes from dark.qss / light.qss selectors.
void TaskEventLogWidget::setupStyle()
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setResizeMode(QListView::Adjust);
    setUniformItemSizes(true); // all rows share a fixed height → faster painting
    setSpacing(0);
    setFocusPolicy(Qt::NoFocus);
    // Visual styling is owned by dark.qss / light.qss via TaskEventLogWidget selectors
}

/// Appends one event row, discarding the oldest row first if the log is at
/// its maxEvents() capacity, and scrolls the view to the newly added row.
void TaskEventLogWidget::appendEvent(const TaskEvent &event)
{
    // Discard oldest row when at capacity
    if (m_maxEvents > 0 && count() >= m_maxEvents)
        delete takeItem(0);

    auto *item = new QListWidgetItem(this);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    // Row height: 30 px — comfortable for operator reading at a glance
    item->setSizeHint(QSize(0, 30));

    auto *w = new TaskEventItemWidget(event, this);
    setItemWidget(item, w);
    scrollToBottom();
}

/// Removes all rows from the log.
void TaskEventLogWidget::clearEvents()
{
    clear();
}

/// Sets the maximum retained row count, clamped to a non-negative value
/// (0 = unlimited). Does not trim any rows already present; enforcement
/// happens on the next appendEvent() call.
void TaskEventLogWidget::setMaxEvents(int max)
{
    m_maxEvents = qMax(0, max);
}
