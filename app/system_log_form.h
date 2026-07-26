#ifndef SYSTEM_LOG_FORM_H
#define SYSTEM_LOG_FORM_H

#include "core/logger/app_logger.h"

#include <QVector>
#include <QWidget>

namespace Ui {
class SystemLogForm;
}

/// Dockable log viewer widget: renders AppLogger messages as color-coded HTML, filtered by a
/// User/Developer view-mode combo box, and re-themes itself on ThemeManager changes.
class SystemLogForm : public QWidget {
    Q_OBJECT
public:
    /// Populates the view-mode combo, applies the initial stylesheet, and connects to
    /// ThemeManager::themeChanged and AppLogger::newLogAdded.
    explicit SystemLogForm(QWidget *parent = nullptr);
    ~SystemLogForm();

private slots:
    /// Appends `msg` to m_entries and re-renders the log view if it passes shouldShow().
    void onNewLogReceived(const LogMessage &msg);
    /// Re-renders the log view when the User/Developer view-mode selection changes.
    void onViewModeChanged();
    /// Clears m_entries and the rendered log view.
    void onClearAll();

private:
    /// Loads the light or dark QSS resource matching ThemeManager::isDark() and applies it
    /// after resolving theme tokens.
    void reloadStyleSheet();
    /// Rebuilds the log view's HTML from m_entries (filtered by shouldShow()), preserving the
    /// scroll position or pinning to the bottom if the view was already scrolled to bottom.
    void rerenderLogs();
    /// True in Developer view mode, or in User mode when `msg` is a user-category message.
    bool shouldShow(const LogMessage &msg) const;
    /// Formats one log entry as an HTML line (timestamp, category, level, message, context)
    /// colored via theme tokens for the given `dark` mode.
    QString renderLogEntry(const LogMessage &msg, bool dark) const;

    Ui::SystemLogForm *ui;         ///< Generated UI form; owns the log view, combo box, and clear button.
    QVector<LogMessage> m_entries;  ///< All received log messages, in arrival order (unfiltered).
};

#endif // SYSTEM_LOG_FORM_H
