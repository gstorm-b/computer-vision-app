#include "task_event_log_widget.h"

#include <QHBoxLayout>
#include <QSizePolicy>

// ─────────────────────────────────────────────────────────────────────────────
//  TaskEventItemWidget
// ─────────────────────────────────────────────────────────────────────────────

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

const char *TaskEventItemWidget::severityProperty(TaskEventLevel level)
{
    switch (level) {
    case TaskEventLevel::Warning: return "warning";
    case TaskEventLevel::Error:   return "error";
    case TaskEventLevel::Success: return "success";
    default:                      return "info";
    }
}

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

TaskEventLogWidget::TaskEventLogWidget(QWidget *parent)
    : QListWidget(parent)
{
    setupStyle();
}

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

void TaskEventLogWidget::clearEvents()
{
    clear();
}

void TaskEventLogWidget::setMaxEvents(int max)
{
    m_maxEvents = qMax(0, max);
}
