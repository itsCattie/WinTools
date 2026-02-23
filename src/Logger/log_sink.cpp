#include "logger/log_sink.hpp"

// WinTools: log sink manages feature behavior.

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
    {
        std::lock_guard<std::mutex> g(m_mutex);
        if (m_buffer.size() >= kMaxEntries)
            m_buffer.removeFirst();
        m_buffer.append(entry);
        ++m_totalReceived;
    }
    emit entryAdded(entry);
}

}
