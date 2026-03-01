#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace wintools::taskmanager {

enum class ProcessStatus {
    Running,
    Suspended,
    NotResponding,
    Unknown
};

enum class ProcessCategory {
    App,
    Background,
    System
};

struct ProcessInfo {

    quint32  pid         = 0;
    quint32  parentPid   = 0;
    QString  name;
    QString  exePath;
    QString  username;

    ProcessCategory category = ProcessCategory::Background;
    ProcessStatus   status   = ProcessStatus::Unknown;
    int             priority = 8;

    double   cpuPercent = 0.0;
    double   gpuPercent = 0.0;

    quint64  workingSetBytes  = 0;
    quint64  privateBytes     = 0;
    quint64  peakWorkingSet   = 0;

    quint64  diskReadBytesPerSec  = 0;
    quint64  diskWriteBytesPerSec = 0;

    quint32  handleCount  = 0;
    quint32  threadCount  = 0;

    quint64  kernelTime = 0;
    quint64  userTime   = 0;

    quint64  rawReadBytes  = 0;
    quint64  rawWriteBytes = 0;

    int      tcpConnections  = 0;
    int      udpEndpoints    = 0;

    QVector<quint32> childPids;
};

struct DiskPerf {
    QString driveLetter;
    QString volumeLabel;
    quint64 readBytesPerSec  = 0;
    quint64 writeBytesPerSec = 0;
    quint64 totalBytes       = 0;
    quint64 freeBytes        = 0;
};

struct NetPerf {
    quint64 recvBytesPerSec = 0;
    quint64 sentBytesPerSec = 0;
};

struct SystemPerf {
    double  cpuUsagePercent    = 0.0;
    double  gpuUsagePercent    = 0.0;
    double  cpuFreqMHz         = 0.0;
    quint64 totalMemoryBytes   = 0;
    quint64 availMemoryBytes   = 0;
    quint64 usedMemoryBytes    = 0;
    quint64 commitedBytes      = 0;
    quint64 commitLimitBytes   = 0;
    int     processCount       = 0;
    int     threadCount        = 0;
    int     handleCount        = 0;
    double  uptimeSeconds      = 0.0;
    QVector<DiskPerf> disks;
    NetPerf           network;
};

}
