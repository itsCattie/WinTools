#pragma once

// AdvancedTaskManager: process monitor manages core logic and state.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

#include "modules/AdvancedTaskManager/src/core/process_info.hpp"

namespace wintools::taskmanager {

class ProcessMonitor : public QObject {
    Q_OBJECT

public:
    explicit ProcessMonitor(QObject* parent = nullptr);
    ~ProcessMonitor() override;

    void setRefreshInterval(int ms);
    int  refreshInterval() const { return m_timer->interval(); }

    void start();
    void stop();

    QString endProcess(quint32 pid);

    QString endProcessTree(quint32 pid);
    bool suspendProcess(quint32 pid);
    bool resumeProcess(quint32 pid);
    bool setPriority(quint32 pid, int priorityClass);

signals:
    void processesUpdated(QVector<wintools::taskmanager::ProcessInfo> processes,
                          wintools::taskmanager::SystemPerf            sysPerf);

private slots:
    void refresh();

private:

    QSet<quint32>   enumGuiPids() const;
    static QString  queryProcessPath(HANDLE hProc);
    static QString  queryProcessUser(HANDLE hProc);
    static ProcessCategory categorize(const ProcessInfo& pi, const QSet<quint32>& guiPids);

    struct ProcTimes {
        quint64 kernelTime = 0;
        quint64 userTime   = 0;
        quint64 readBytes  = 0;
        quint64 writeBytes = 0;
    };

    QHash<quint32, ProcTimes>  m_prevTimes;

    quint64 m_prevSysIdle   = 0;
    quint64 m_prevSysKernel = 0;
    quint64 m_prevSysUser   = 0;

    struct DiskCounters {
        quint64 bytesRead    = 0;
        quint64 bytesWritten = 0;
    };
    QHash<QString, DiskCounters> m_prevDiskCounters;

    quint64 m_prevNetRecvBytes = 0;
    quint64 m_prevNetSentBytes = 0;
    QTimer* m_timer = nullptr;

    using NtSuspendFn = LONG (WINAPI*)(HANDLE);
    using NtResumeFn  = LONG (WINAPI*)(HANDLE);
    NtSuspendFn m_ntSuspend = nullptr;
    NtResumeFn  m_ntResume  = nullptr;
};

}
