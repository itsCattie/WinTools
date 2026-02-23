#pragma once

// MediaBar: debug logger manages core logic and state.

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include "logger/logger.hpp"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace debuglog {

enum class Level {
    Trace,
    Info,
    Warn,
    Error
};

inline QMutex& lockObject() {
    static QMutex mutex;
    return mutex;
}

inline QString levelName(Level level) {
    switch (level) {
    case Level::Trace:
        return "TRACE";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    }

    return "INFO";
}

inline QString levelColor(Level level) {
    switch (level) {
    case Level::Trace:
        return "\x1b[90m";
    case Level::Info:
        return "\x1b[36m";
    case Level::Warn:
        return "\x1b[33m";
    case Level::Error:
        return "\x1b[31m";
    }

    return "\x1b[0m";
}

#ifdef Q_OS_WIN
inline void enableAnsiIfAvailable() {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    initialized = true;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE || out == nullptr) {
        return;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(out, &mode)) {
        return;
    }

    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

inline QString liveLogPath() {
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.trimmed().isEmpty()) {
        localAppData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (localAppData.trimmed().isEmpty()) {
        localAppData = QDir::homePath();
    }

    const QString directory = QDir(localAppData).filePath("WinTools/logs");
    QDir().mkpath(directory);
    return QDir(directory).filePath("wintools-live.log");
}

inline QHash<QString, qint64>& traceLastSeen() {
    static QHash<QString, qint64> states;
    return states;
}

inline wintools::logger::Severity mapToSeverity(Level level) {
    switch (level) {
    case Level::Error: return wintools::logger::Severity::Error;
    case Level::Warn:  return wintools::logger::Severity::Warning;
    default:           return wintools::logger::Severity::Pass;
    }
}

inline void emitLine(Level level, const QString& source, const QString& message) {
    const QString line = QString("[%1] [MediaBar.%2] [%3] %4")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"), source, levelName(level), message);

    QTextStream(stdout) << levelColor(level) << line << "\x1b[0m" << Qt::endl;

    QFile file(liveLogPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << line << "\n";
        file.close();
    }

    wintools::logger::Logger::log(
        QStringLiteral("MediaBar/%1").arg(source),
        mapToSeverity(level),
        message);
}

inline void writeLine(Level level, const QString& source, const QString& message) {
    QMutexLocker locker(&lockObject());

#ifdef Q_OS_WIN
    enableAnsiIfAvailable();
#endif

    if (level == Level::Trace) {
        constexpr qint64 kTraceWindowMs = 5000;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QString key = source + "|" + message;
        const qint64 lastSeen = traceLastSeen().value(key, 0);

        if (lastSeen > 0 && (now - lastSeen) < kTraceWindowMs) {
            return;
        }

        traceLastSeen()[key] = now;
    }

    emitLine(level, source, message);
}

inline void trace(const QString& source, const QString& message) {
    writeLine(Level::Trace, source, message);
}

inline void info(const QString& source, const QString& message) {
    writeLine(Level::Info, source, message);
}

inline void warn(const QString& source, const QString& message) {
    writeLine(Level::Warn, source, message);
}

inline void error(const QString& source, const QString& message) {
    writeLine(Level::Error, source, message);
}

}
