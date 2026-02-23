#pragma once

// WinTools: log sink manages feature behavior.

#include "logger/logger.hpp"

#include <QObject>
#include <QVector>
#include <mutex>

namespace wintools::logger {

class LogSink : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxEntries = 10'000;

    static LogSink* instance();
    QVector<LogEntry> entries() const;
    quint64 totalReceived() const;

public slots:
    void appendEntry(const wintools::logger::LogEntry& entry);

signals:
    void entryAdded(const wintools::logger::LogEntry& entry);

private:
    explicit LogSink(QObject* parent = nullptr);

    mutable std::mutex      m_mutex;
    QVector<LogEntry>       m_buffer;
    quint64                 m_totalReceived{0};
};

}
