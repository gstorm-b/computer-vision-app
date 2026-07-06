#include "system_log_form.h"
#include "ui_system_log_form.h"

#include "utils/theme_manager.h"

#include <QComboBox>
#include <QFile>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>

namespace {

QString tokenValue(const char *name, bool dark) {
    return ThemeManager::tokenValue(QString::fromLatin1(name), dark);
}

QString severityColor(LogLevel level, bool dark) {
    switch (level) {
    case LogLevel::Warning:
        return tokenValue("state.warning", dark);
    case LogLevel::Error:
    case LogLevel::Critical:
        return tokenValue("state.error", dark);
    case LogLevel::Debug:
        return tokenValue("state.info", dark);
    case LogLevel::Info:
    default:
        return tokenValue("text.primary", dark);
    }
}

QString levelLabel(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:    return QStringLiteral("DEBUG");
    case LogLevel::Info:     return QStringLiteral("INFO");
    case LogLevel::Warning:  return QStringLiteral("WARN");
    case LogLevel::Error:    return QStringLiteral("ERROR");
    case LogLevel::Critical: return QStringLiteral("CRITICAL");
    }
    return QStringLiteral("INFO");
}

} // namespace

SystemLogForm::SystemLogForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SystemLogForm) {
    ui->setupUi(this);

    QStringList viewModeList = {tr("User"), tr("Developer")};
    ui->cbx_view_mode->addItems(viewModeList);

    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) {
                reloadStyleSheet();
                rerenderLogs();
            });
    connect(&AppLogger::instance(), &AppLogger::newLogAdded,
            this, &SystemLogForm::onNewLogReceived);
    connect(ui->cbx_view_mode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { onViewModeChanged(); });
    connect(ui->btn_clearAll, &QPushButton::clicked,
            this, &SystemLogForm::onClearAll);
}

SystemLogForm::~SystemLogForm() {
    delete ui;
}

void SystemLogForm::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/system_log_form_dark.qss")
        : QStringLiteral(":/styles/system_log_form_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(ThemeManager::instance()->resolveTokens(
            QString::fromUtf8(f.readAll())));
    }
}

bool SystemLogForm::shouldShow(const LogMessage &msg) const {
    return ui->cbx_view_mode->currentIndex() == 1
           || msg.category == LogCategory::User;
}

QString SystemLogForm::renderLogEntry(const LogMessage &msg, bool dark) const {
    const QString timeColor = tokenValue("text.muted", dark);
    const QString textColor = tokenValue("text.primary", dark);
    const QString ctxColor  = tokenValue("text.disabled", dark);
    const QString levelColor = severityColor(msg.level, dark);

    const QString timeStr = msg.timestamp.toString(QStringLiteral("hh:mm:ss"));
    const QString catStr = msg.category == LogCategory::User
        ? QStringLiteral("[USER]")
        : QStringLiteral("[DEV]");
    const QString levelStr = levelLabel(msg.level);
    const QString message = msg.message.toHtmlEscaped();
    const QString context = msg.context.trimmed().toHtmlEscaped();
    const QString ctxHtml = context.isEmpty()
        ? QString()
        : QStringLiteral(" <span style='color:%1;'>(%2)</span>")
              .arg(ctxColor, context);

    return QStringLiteral(
        "<div style='margin:0 0 6px 0;'>"
        "<span style='color:%1;'>[%2]</span> "
        "<span style='color:%3;'>%4</span> "
        "<span style='color:%5;'>[%6]</span> "
        "<span style='color:%7;'>%8</span>%9"
        "</div>")
        .arg(timeColor,
             timeStr,
             textColor,
             catStr,
             levelColor,
             levelStr,
             textColor,
             message,
             ctxHtml);
}

void SystemLogForm::rerenderLogs() {
    auto *scrollBar = ui->log_view->verticalScrollBar();
    const int previousValue = scrollBar ? scrollBar->value() : 0;
    const bool wasAtBottom = scrollBar
        ? previousValue >= scrollBar->maximum() - 4
        : true;

    const bool dark = ThemeManager::instance()->isDark();
    QStringList rows;
    rows.reserve(m_entries.size());
    for (const LogMessage &msg : m_entries) {
        if (shouldShow(msg))
            rows << renderLogEntry(msg, dark);
    }

    const QString html = QStringLiteral(
        "<html><body style='margin:0; color:%1; font-family:\"JetBrains Mono\",\"Consolas\",monospace; "
        "font-size:10pt;'>%2</body></html>")
        .arg(tokenValue("text.primary", dark), rows.join(QString()));
    ui->log_view->setHtml(html);

    if (!scrollBar)
        return;
    if (wasAtBottom) {
        scrollBar->setValue(scrollBar->maximum());
    } else {
        scrollBar->setValue(qMin(previousValue, scrollBar->maximum()));
    }
}

void SystemLogForm::onNewLogReceived(const LogMessage &msg) {
    m_entries.push_back(msg);
    if (shouldShow(msg))
        rerenderLogs();
}

void SystemLogForm::onViewModeChanged() {
    rerenderLogs();
}

void SystemLogForm::onClearAll() {
    m_entries.clear();
    ui->log_view->clear();
}
