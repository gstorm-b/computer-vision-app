#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QFile>
#include <QTextStream>

/// Audience a log message is destined for: user-facing (shown to the operator) or
/// developer-facing (diagnostic, tagged with file:line context).
enum class LogCategory {
    User,
    Developer
};

/// Severity of a log message, ordered from least to most severe.
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

/// A single recorded log entry, as emitted by AppLogger::newLogAdded.
struct LogMessage {
    QDateTime timestamp;   ///< Time the message was recorded.
    LogCategory category;  ///< Whether this is a user- or developer-facing message.
    LogLevel level;        ///< Severity of the message.
    QString message;       ///< The formatted message text.
    QString context;       ///< Developer context (e.g. file:line); empty for user messages.
};

/// Registers LogMessage with Qt's meta-type system so it can be carried across
/// queued signal/slot connections (e.g. to a UI log view on another thread).
Q_DECLARE_METATYPE(LogMessage)

/// Application-wide logging facade (singleton): routes user- and developer-facing
/// messages to a rotating daily log file and emits newLogAdded for live UI display.
class AppLogger : public QObject {
    Q_OBJECT

public:
    /// Temporary stream object returned by AppLogger's info()/warning()/error()/...
    /// helpers: buffers `operator<<` chained values and, on destruction, flushes the
    /// accumulated text to the owning AppLogger as one log message (dev-tagged when
    /// constructed with a non-empty context, user-facing otherwise).
    class LogStream {
    public:
        /// Starts a new buffered log entry at `level` for `logger`.
        /// @param ctx developer context (e.g. file:line); empty means a user-facing entry
        LogStream(LogLevel level, AppLogger* logger, QString ctx = QString())
            : m_context(ctx), m_level(level), m_logger(logger) {
            m_stream.setString(&m_buffer);
        }

        /// Copies the level, logger pointer, and accumulated buffer text from `other`
        /// (the copy gets its own QTextStream bound to its own buffer).
        LogStream(const LogStream& other)
            : m_level(other.m_level), m_logger(other.m_logger), m_buffer(other.m_buffer) {
            m_stream.setString(&m_buffer);
        }

        /// Flushes the buffered text (if any) to the owning logger via logDev/logUser,
        /// choosing the path based on whether a context was supplied.
        ~LogStream() {
            if (!m_buffer.isEmpty()) {
                (!m_context.isEmpty()) ?
                    m_logger->logDev(m_level, m_buffer, m_context) :
                    m_logger->logUser(m_level, m_buffer);
            }
        }

        /// Appends `value` (converted via QTextStream) followed by a space to the
        /// buffered message; enables chained `stream << a << b << c` usage.
        template<typename T>
        LogStream& operator<<(const T& value) {
            m_stream << value << " ";
            return *this;
        }

    private:
        QString m_context;      ///< Developer context tag; empty for user-facing entries.
        LogLevel m_level;       ///< Severity this entry will be logged at.
        AppLogger* m_logger;    ///< Logger the buffered text is flushed to on destruction.
        QString m_buffer;       ///< Accumulated message text built up by operator<<.
        QTextStream m_stream;   ///< Stream bound to m_buffer used to format appended values.
    };

    /// Returns the process-wide AppLogger singleton, lazily constructing it (parented
    /// to qApp) on first call.
    static AppLogger& instance();

    /// Starts a buffered user-facing log entry at Info level.
    LogStream info()     { return LogStream(LogLevel::Info, this); }
    /// Starts a buffered user-facing log entry at Warning level.
    LogStream warning()     { return LogStream(LogLevel::Warning, this); }
    /// Starts a buffered user-facing log entry at Error level.
    LogStream error()    { return LogStream(LogLevel::Error, this); }
    /// Starts a buffered user-facing log entry at Critical level.
    LogStream critical() { return LogStream(LogLevel::Critical, this); }

    /// Starts a buffered developer-facing log entry at Info level tagged with `ctx`.
    LogStream dev_info(QString ctx)     { return LogStream(LogLevel::Info, this, ctx); }
    /// Starts a buffered developer-facing log entry at Debug level tagged with `ctx`.
    LogStream dev_debug(QString ctx)     { return LogStream(LogLevel::Debug, this, ctx); }
    /// Starts a buffered developer-facing log entry at Error level tagged with `ctx`.
    LogStream dev_error(QString ctx)    { return LogStream(LogLevel::Error, this, ctx); }

    /// Records a user-facing message: writes it to the log file and emits newLogAdded.
    void logUser(LogLevel level, const QString &msg);
    /// Records a developer-facing message tagged with `context` (e.g. file:line): writes
    /// it to the log file and emits newLogAdded.
    void logDev(LogLevel level, const QString &msg, const QString &context = "");

    /// Deleted: AppLogger is a non-copyable singleton.
    AppLogger(const AppLogger&) = delete;
    /// Deleted: AppLogger is a non-copyable singleton.
    AppLogger& operator=(const AppLogger&) = delete;

signals:
    /// Emitted whenever a message has been logged (user or developer), after it has
    /// been written to the log file.
    void newLogAdded(const LogMessage &logMsg);

private:
    /// Constructs the logger; private, use instance() instead.
    explicit AppLogger(QObject *parent = nullptr);
    /// Default destructor override (no additional cleanup beyond QObject's).
    ~AppLogger() override;

    QMutex m_mutex;   ///< Guards access to the log file during writeLog/rotateLogFileIfNeeded.

    QFile m_logFile;             ///< Currently open log file for the active log date.
    QDate m_currentLogDate;      ///< Date the currently open m_logFile was created for.
    QString m_logDirectory;      ///< Directory log files are written into.

    /// Builds the full log line for (category, level, msg, context), writes it to the
    /// current log file (rotating first if the day has changed), and emits newLogAdded.
    void writeLog(LogCategory category, LogLevel level, const QString &msg, const QString &context);

    /// Closes and reopens m_logFile under a new date-stamped name when the current date
    /// no longer matches m_currentLogDate (auto rolling check for daily log file).
    void rotateLogFileIfNeeded();
};

// #define LOG_DEV_DEBUG(msg) AppLogger::instance().logDev(LogLevel::Debug, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))
// #define LOG_DEV_INFO(msg)  AppLogger::instance().logDev(LogLevel::Info, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))
// #define LOG_DEV_ERR(msg)   AppLogger::instance().logDev(LogLevel::Error, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))

// #define LOG_USER_INFO(msg) AppLogger::instance().logUser(LogLevel::Info, msg)
// #define LOG_USER_WARN(msg)  AppLogger::instance().logUser(LogLevel::Warning, msg)
// #define LOG_USER_ERR(msg)  AppLogger::instance().logUser(LogLevel::Error, msg)
// #define LOG_USER_CRIT(msg)  AppLogger::instance().logUser(LogLevel::Critical, msg)

/// Logs a developer-facing message at Info level, tagged with the calling file:line.
#define LOG_DEV_INFO        AppLogger::instance().dev_info(QString("%1:%2").arg(__FILE__).arg(__LINE__))
/// Logs a developer-facing message at Debug level, tagged with the calling file:line.
#define LOG_DEV_DEBUG       AppLogger::instance().dev_debug(QString("%1:%2").arg(__FILE__).arg(__LINE__))
/// Logs a developer-facing message at Error level, tagged with the calling file:line.
#define LOG_DEV_ERR         AppLogger::instance().dev_error(QString("%1:%2").arg(__FILE__).arg(__LINE__))

/// Logs a user-facing message at Info level (no file/line context attached).
#define LOG_USER_INFO       AppLogger::instance().info()
/// Logs a user-facing message at Warning level (no file/line context attached).
#define LOG_USER_WARN       AppLogger::instance().warning()
/// Logs a user-facing message at Error level (no file/line context attached).
#define LOG_USER_ERR        AppLogger::instance().error()
/// Logs a user-facing message at Critical level (no file/line context attached).
#define LOG_USER_CRIT       AppLogger::instance().critical()

#endif // APP_LOGGER_H
