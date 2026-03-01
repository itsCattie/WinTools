#include "logger/log_sink.hpp"

namespace wintools::logger {

LogSink::LogSink(QObject* parent) : QObject(parent) {
    qRegisterMetaType<wintools::logger::LogEntry>("wintools::logger::LogEntry");
    m_buffer.reserve(kMaxEntries);
}

LogSink* LogSink::instance() {

    static LogSink* s_inst = new LogSink(nullptr);
    return s_inst;
}

QVector<LogEntry> LogSink::entries() const {
    std::lock_guard<std::mutex> g(m_mutex);
    return m_buffer;
}

quint64 LogSink::totalReceived() const {
    std::lock_guard<std::mutex> g(m_mutex);
    return m_totalReceived;
}

void LogSink::appendEntry(const wintools::logger::LogEntry& entry) {
    const QString key = QString("%1|%2|%3").arg(entry.source, entry.reason, entry.data);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    {
        std::lock_guard<std::mutex> g(m_mutex);

        const qint64 last = m_lastSeenMs.value(key, 0);
        if (last > 0 && (nowMs - last) < m_dedupWindowMs) {

            ++m_totalReceived;
            return;
        }

        m_lastSeenMs.insert(key, nowMs);

        if (m_buffer.size() >= kMaxEntries)
            m_buffer.removeFirst();
        m_buffer.append(entry);
        ++m_totalReceived;
    }

    emit entryAdded(entry);
}

}
