#pragma once

// WinTools: logger manages feature behavior.

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace wintools::logger {

enum class Severity {
    Error,
    Warning,
    Pass
};

struct LogEntry {
    QDateTime timestamp;
    QString   source;
    Severity  severity{Severity::Pass};
    QString   reason;
    QString   data;
};

class Logger {
public:
    static QString getLiveLogPath();
    static void log(const QString& source, Severity severity,
                    const QString& reason, const QString& data = QString());
};

}

Q_DECLARE_METATYPE(wintools::logger::LogEntry)
