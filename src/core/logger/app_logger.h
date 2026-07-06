#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QFile>
#include <QTextStream>

enum class LogCategory {
    User,
    Developer
};

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

struct LogMessage {
    QDateTime timestamp;
    LogCategory category;
    LogLevel level;
    QString message;
    QString context;
};

Q_DECLARE_METATYPE(LogMessage)

class AppLogger : public QObject {
    Q_OBJECT

public:
    class LogStream {
    public:
        LogStream(LogLevel level, AppLogger* logger, QString ctx = QString())
            : m_context(ctx), m_level(level), m_logger(logger) {
            m_stream.setString(&m_buffer);
        }

        LogStream(const LogStream& other)
            : m_level(other.m_level), m_logger(other.m_logger), m_buffer(other.m_buffer) {
            m_stream.setString(&m_buffer);
        }

        ~LogStream() {
            if (!m_buffer.isEmpty()) {
                (!m_context.isEmpty()) ?
                    m_logger->logDev(m_level, m_buffer, m_context) :
                    m_logger->logUser(m_level, m_buffer);
            }
        }

        template<typename T>
        LogStream& operator<<(const T& value) {
            m_stream << value << " ";
            return *this;
        }

    private:
        QString m_context;
        LogLevel m_level;
        AppLogger* m_logger;
        QString m_buffer;
        QTextStream m_stream;
    };

    static AppLogger& instance();

    LogStream info()     { return LogStream(LogLevel::Info, this); }
    LogStream warning()     { return LogStream(LogLevel::Warning, this); }
    LogStream error()    { return LogStream(LogLevel::Error, this); }
    LogStream critical() { return LogStream(LogLevel::Critical, this); }

    LogStream dev_info(QString ctx)     { return LogStream(LogLevel::Info, this, ctx); }
    LogStream dev_debug(QString ctx)     { return LogStream(LogLevel::Debug, this, ctx); }
    LogStream dev_error(QString ctx)    { return LogStream(LogLevel::Error, this, ctx); }

    void logUser(LogLevel level, const QString &msg);
    void logDev(LogLevel level, const QString &msg, const QString &context = "");

    AppLogger(const AppLogger&) = delete;
    AppLogger& operator=(const AppLogger&) = delete;

signals:
    void newLogAdded(const LogMessage &logMsg);

private:
    explicit AppLogger(QObject *parent = nullptr);
    ~AppLogger() override;

    QMutex m_mutex;

    QFile m_logFile;
    QDate m_currentLogDate;
    QString m_logDirectory;

    void writeLog(LogCategory category, LogLevel level, const QString &msg, const QString &context);

    // auto rolling check for daily log file
    void rotateLogFileIfNeeded();
};

// #define LOG_DEV_DEBUG(msg) AppLogger::instance().logDev(LogLevel::Debug, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))
// #define LOG_DEV_INFO(msg)  AppLogger::instance().logDev(LogLevel::Info, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))
// #define LOG_DEV_ERR(msg)   AppLogger::instance().logDev(LogLevel::Error, msg, QString("%1:%2").arg(__FILE__).arg(__LINE__))

// #define LOG_USER_INFO(msg) AppLogger::instance().logUser(LogLevel::Info, msg)
// #define LOG_USER_WARN(msg)  AppLogger::instance().logUser(LogLevel::Warning, msg)
// #define LOG_USER_ERR(msg)  AppLogger::instance().logUser(LogLevel::Error, msg)
// #define LOG_USER_CRIT(msg)  AppLogger::instance().logUser(LogLevel::Critical, msg)

#define LOG_DEV_INFO        AppLogger::instance().dev_info(QString("%1:%2").arg(__FILE__).arg(__LINE__))
#define LOG_DEV_DEBUG       AppLogger::instance().dev_debug(QString("%1:%2").arg(__FILE__).arg(__LINE__))
#define LOG_DEV_ERR         AppLogger::instance().dev_error(QString("%1:%2").arg(__FILE__).arg(__LINE__))

#define LOG_USER_INFO       AppLogger::instance().info()
#define LOG_USER_WARN       AppLogger::instance().warning()
#define LOG_USER_ERR        AppLogger::instance().error()
#define LOG_USER_CRIT       AppLogger::instance().critical()

#endif // APP_LOGGER_H
