#include "app_logger.h"
#include <QDebug>
#include <QMetaType>
#include <QMutexLocker>
#include <QDir>
#include <QCoreApplication>

/// Returns the process-wide AppLogger singleton, constructing it on first call
/// (function-local static, so construction is thread-safe under C++11+ semantics).
/// @return reference to the single AppLogger instance
AppLogger& AppLogger::instance() {
    // Initialize only once time of life cycle
    static AppLogger _instance;
    return _instance;
}

/// Registers LogMessage with the Qt meta-type system (so it can cross the
/// newLogAdded signal/slot boundary) and derives the log directory from the
/// application executable's path, creating "<appDir>/logs" if it does not exist.
AppLogger::AppLogger(QObject *parent) : QObject(parent) {
    qRegisterMetaType<LogMessage>("LogMessage");

    // get executable file path
    QString appPath = QCoreApplication::applicationDirPath();
    m_logDirectory = appPath + "/logs";

    // check log file exist
    QDir dir;
    if (!dir.exists(m_logDirectory)) {
        dir.mkpath(m_logDirectory);
    }
}

/// Closes the log file if it is still open.
AppLogger::~AppLogger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

/// Ensures the current log file matches today's date, closing and reopening
/// (creating "app_log_yyyy-MM-dd.txt" in m_logDirectory) whenever the date has
/// rolled over or no file is currently open; logs a warning via qWarning() if the
/// new file cannot be opened.
void AppLogger::rotateLogFileIfNeeded() {
    QDate today = QDate::currentDate();

    // check date time for log file name
    if (today != m_currentLogDate || !m_logFile.isOpen()) {

        // close previous day's log
        if (m_logFile.isOpen()) {
            m_logFile.close();
        }

        m_currentLogDate = today;

        // create new log file
        QString fileName = QString("%1/app_log_%2.txt")
                               .arg(m_logDirectory)
                               .arg(today.toString("yyyy-MM-dd"));

        m_logFile.setFileName(fileName);

        // open file write only and append
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "CRITICAL ERROR: Cannot create or open file log at:" << fileName;
        }
    }
}

/// Logs a user-facing message (LogCategory::User, no context) at the given level.
void AppLogger::logUser(LogLevel level, const QString &msg) {
    writeLog(LogCategory::User, level, msg, "");
}

/// Logs a developer-facing message (LogCategory::Developer) at the given level,
/// optionally tagged with a context string (e.g. file:line).
void AppLogger::logDev(LogLevel level, const QString &msg, const QString &context) {
    writeLog(LogCategory::Developer, level, msg, context);
}

/// Formats and dispatches a single log entry: builds the LogMessage/text line,
/// writes it to the console (qWarning for Error/Critical, qDebug otherwise) and to
/// the rotated log file, and emits newLogAdded for UI consumers.
/// @note Serialized via m_mutex (QMutexLocker), so safe to call from multiple threads.
void AppLogger::writeLog(LogCategory category, LogLevel level, const QString &msg, const QString &context) {
    QMutexLocker locker(&m_mutex);

    LogMessage logMsg;
    logMsg.timestamp = QDateTime::currentDateTime();
    logMsg.category = category;
    logMsg.level = level;
    logMsg.message = msg;
    logMsg.context = context;

    QString levelStr;
    switch(level) {
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info: levelStr = "INFO"; break;
        case LogLevel::Warning: levelStr = "WARN"; break;
        case LogLevel::Error: levelStr = "ERR"; break;
        case LogLevel::Critical: levelStr = "CRITICAL"; break;
    }

    // log format
    QString catStr = (category == LogCategory::User) ? "[USER]" : "[DEV]";
    QString textOutput = QString("[%1]%2[%3] %4 %5")
                             .arg(logMsg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                             .arg(catStr)
                             .arg(levelStr)
                             .arg(msg)
                             .arg(context.isEmpty() ? "" : "(" + context + ")");

    // output to console
    if (level == LogLevel::Error || level == LogLevel::Critical) {
        qWarning().noquote() << textOutput;
    } else {
        qDebug().noquote() << textOutput;
    }

    // write to log file
    rotateLogFileIfNeeded();

    if (m_logFile.isOpen()) {
        QTextStream out(&m_logFile);
        out << textOutput << "\n";
        // crash-proof
        // force OS to write data immediately
        out.flush();
    }

    // emit signal for UI
    emit newLogAdded(logMsg);
}


