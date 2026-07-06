#include "app_logger.h"
#include <QDebug>
#include <QMetaType>
#include <QMutexLocker>
#include <QDir>
#include <QCoreApplication>

AppLogger& AppLogger::instance() {
    // Initialize only once time of life cycle
    static AppLogger _instance;
    return _instance;
}

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

AppLogger::~AppLogger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

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

void AppLogger::logUser(LogLevel level, const QString &msg) {
    writeLog(LogCategory::User, level, msg, "");
}

void AppLogger::logDev(LogLevel level, const QString &msg, const QString &context) {
    writeLog(LogCategory::Developer, level, msg, context);
}

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


