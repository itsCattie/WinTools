#include "logger/logger.hpp"
#include "logger/log_sink.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTextStream>

#include <iostream>
#include <mutex>

// WinTools: logger manages feature behavior.

namespace wintools::logger {

namespace {
std::mutex g_sync;

QString severityToString(Severity severity) {
    switch (severity) {
    case Severity::Error:
        return "Error";
    case Severity::Warning:
        return "Warning";
    case Severity::Pass:
        return "Pass";
    }
    return "Unknown";
}

QString resolveLogDirectory() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(base);
    dir.mkpath("logs");
    return dir.filePath("logs");
}

QString resolveLogFilePath() {
    QDir dir(resolveLogDirectory());
    return dir.filePath("wintools-live.log");
}
}

QString Logger::getLiveLogPath() {
    std::lock_guard<std::mutex> guard(g_sync);
    const QString filePath = resolveLogFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
    }
    return filePath;
}

void Logger::log(const QString& source, Severity severity, const QString& reason, const QString& data) {
    const QDateTime now = QDateTime::currentDateTime();
    const QString ts = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString line = QString("[%1] [%2] [%3] %4").arg(ts, source, severityToString(severity), reason);
    if (!data.trimmed().isEmpty()) {
        line += QString(" | Data: %1").arg(data);
    }

    {
        std::lock_guard<std::mutex> guard(g_sync);
        QFile file(resolveLogFilePath());
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << line << Qt::endl;
        }
    }

    std::cout << line.toStdString() << std::endl;

    LogEntry entry;
    entry.timestamp = now;
    entry.source    = source;
    entry.severity  = severity;
    entry.reason    = reason;
    entry.data      = data;

    QMetaObject::invokeMethod(
        LogSink::instance(),
        "appendEntry",
        Qt::QueuedConnection,
        Q_ARG(wintools::logger::LogEntry, entry));
}

}
