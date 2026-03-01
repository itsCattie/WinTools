#include "modules/AdvancedTaskManager/src/core/process_monitor.hpp"
#include "logger/logger.hpp"

#include <QSysInfo>
#include <algorithm>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winioctl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <sddl.h>
#include <lmcons.h>
#include <iphlpapi.h>
#include <tcpmib.h>
#include <udpmib.h>
#include <powerbase.h>

#include <cstdlib>

static inline quint64 ftToU64(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

namespace wintools::taskmanager {

static constexpr const char* kLog = "TaskManager/Monitor";

static BOOL CALLBACK enumWindowsCb(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid) {
            auto* set = reinterpret_cast<QSet<quint32>*>(lParam);
            set->insert(static_cast<quint32>(pid));
        }
    }
    return TRUE;
}

ProcessMonitor::ProcessMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::refresh);

    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        m_ntSuspend = reinterpret_cast<NtSuspendFn>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "NtSuspendProcess")));
        m_ntResume  = reinterpret_cast<NtResumeFn>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "NtResumeProcess")));
    }

    ensureGpuCounter();
}

ProcessMonitor::~ProcessMonitor() {
    if (m_gpuQuery) {
        PdhCloseQuery(m_gpuQuery);
        m_gpuQuery = nullptr;
        m_gpuCounter = nullptr;
        m_gpuReady = false;
    }
}

void ProcessMonitor::ensureGpuCounter() {
    if (m_gpuReady) {
        return;
    }

    if (PdhOpenQueryW(nullptr, 0, &m_gpuQuery) != ERROR_SUCCESS) {
        m_gpuQuery = nullptr;
        return;
    }

    const PDH_STATUS addStatus = PdhAddEnglishCounterW(
        m_gpuQuery,
        L"\\GPU Engine(*)\\Utilization Percentage",
        0,
        &m_gpuCounter);

    if (addStatus != ERROR_SUCCESS) {
        PdhCloseQuery(m_gpuQuery);
        m_gpuQuery = nullptr;
        m_gpuCounter = nullptr;
        return;
    }

    if (PdhCollectQueryData(m_gpuQuery) != ERROR_SUCCESS) {
        PdhCloseQuery(m_gpuQuery);
        m_gpuQuery = nullptr;
        m_gpuCounter = nullptr;
        return;
    }

    m_gpuReady = true;
}

void ProcessMonitor::setRefreshInterval(int ms) {
    m_timer->setInterval(qMax(250, ms));
}

void ProcessMonitor::start() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "ProcessMonitor started.",
        QStringLiteral("interval=%1ms").arg(m_timer->interval()));
    m_timer->start(); refresh();
}
void ProcessMonitor::stop()  {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "ProcessMonitor stopped.");
    m_timer->stop();
}

QSet<quint32> ProcessMonitor::enumGuiPids() const {
    QSet<quint32> guiPids;
    EnumWindows(enumWindowsCb, reinterpret_cast<LPARAM>(&guiPids));
    return guiPids;
}

QString ProcessMonitor::queryProcessPath(HANDLE hProc) {
    wchar_t buf[MAX_PATH] = {};
    DWORD   len = MAX_PATH;
    if (QueryFullProcessImageNameW(hProc, 0, buf, &len))
        return QString::fromWCharArray(buf, static_cast<int>(len));
    return {};
}

QString ProcessMonitor::queryProcessUser(HANDLE hProc) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProc, TOKEN_QUERY, &hToken))
        return {};

    DWORD needed = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &needed);
    if (!needed) { CloseHandle(hToken); return {}; }

    QByteArray buf(static_cast<int>(needed), '\0');
    auto* tu = reinterpret_cast<TOKEN_USER*>(buf.data());
    if (!GetTokenInformation(hToken, TokenUser, tu, needed, &needed)) {
        CloseHandle(hToken); return {};
    }
    CloseHandle(hToken);

    wchar_t name[UNLEN + 1]  = {};
    wchar_t domain[MAX_PATH] = {};
    DWORD   nameLen   = UNLEN + 1;
    DWORD   domainLen = MAX_PATH;
    SID_NAME_USE use;
    if (LookupAccountSidW(nullptr, tu->User.Sid,
                          name,   &nameLen,
                          domain, &domainLen, &use)) {
        return QString::fromWCharArray(name, static_cast<int>(nameLen));
    }
    return {};
}

ProcessCategory ProcessMonitor::categorize(const ProcessInfo&    pi,
                                            const QSet<quint32>& guiPids) {
    if (guiPids.contains(pi.pid))
        return ProcessCategory::App;

    const QString luser = pi.username.toLower();
    if (luser == "system" || luser == "local service" ||
        luser == "network service" || luser.isEmpty()) {
        return ProcessCategory::System;
    }

    const QString lpath = pi.exePath.toLower().replace('\\', '/');
    if (lpath.contains("/windows/") || lpath.startsWith("c:/windows"))
        return ProcessCategory::System;

    return ProcessCategory::Background;
}

void ProcessMonitor::refresh() {

    double gpuUsagePercent = 0.0;
    QHash<quint32, double> gpuByPid;
    if (!m_gpuReady) {
        ensureGpuCounter();
    }
    if (m_gpuReady && m_gpuQuery && m_gpuCounter) {
        if (PdhCollectQueryData(m_gpuQuery) == ERROR_SUCCESS) {
            DWORD bufferSize = 0;
            DWORD itemCount = 0;
            DWORD status = static_cast<DWORD>(PdhGetFormattedCounterArrayW(
                m_gpuCounter,
                PDH_FMT_DOUBLE,
                &bufferSize,
                &itemCount,
                nullptr));

            if (status == static_cast<DWORD>(PDH_MORE_DATA)
                && bufferSize > 0 && itemCount > 0) {
                QByteArray buffer(static_cast<int>(bufferSize), 0);
                auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());

                status = static_cast<DWORD>(PdhGetFormattedCounterArrayW(
                    m_gpuCounter,
                    PDH_FMT_DOUBLE,
                    &bufferSize,
                    &itemCount,
                    items));

                if (status == ERROR_SUCCESS) {
                    double total = 0.0;
                    bool hadTotal = false;
                    for (DWORD i = 0; i < itemCount; ++i) {
                        const auto& item = items[i];
                        if (item.FmtValue.CStatus != ERROR_SUCCESS) {
                            continue;
                        }

                        const QString instanceName = QString::fromWCharArray(item.szName ? item.szName : L"");
                        const double value = qMax(0.0, item.FmtValue.doubleValue);

                        if (instanceName.compare(QStringLiteral("_Total"), Qt::CaseInsensitive) == 0) {
                            total = value;
                            hadTotal = true;
                            continue;
                        }

                        const int pidMarker = instanceName.indexOf(QStringLiteral("pid_"), 0, Qt::CaseInsensitive);
                        if (pidMarker >= 0) {
                            const int begin = pidMarker + 4;
                            int end = begin;
                            while (end < instanceName.size() && instanceName.at(end).isDigit()) {
                                ++end;
                            }
                            if (end > begin) {
                                bool ok = false;
                                const quint32 pid = instanceName.mid(begin, end - begin).toUInt(&ok);
                                if (ok && pid > 0) {
                                    gpuByPid[pid] = gpuByPid.value(pid, 0.0) + value;
                                }
                            }
                        }

                        total += value;
                    }

                    if (!hadTotal) {
                        total = qMin(100.0, total);
                    }
                    gpuUsagePercent = qBound(0.0, total, 100.0);

                    for (auto it = gpuByPid.begin(); it != gpuByPid.end(); ++it) {
                        it.value() = qBound(0.0, it.value(), 100.0);
                    }
                }
            }
        }
    }

    FILETIME ftIdle = {}, ftKernel = {}, ftUser = {};
    GetSystemTimes(&ftIdle, &ftKernel, &ftUser);
    const quint64 sysIdle   = ftToU64(ftIdle);
    const quint64 sysKernel = ftToU64(ftKernel);
    const quint64 sysUser   = ftToU64(ftUser);

    const quint64 dIdle   = sysIdle   - m_prevSysIdle;
    const quint64 dKernel = sysKernel - m_prevSysKernel;
    const quint64 dUser   = sysUser   - m_prevSysUser;
    const quint64 dTotal  = dKernel + dUser;

    double sysCpu = 0.0;
    if (dTotal > 0)
        sysCpu = 100.0 * static_cast<double>(dTotal - dIdle) / static_cast<double>(dTotal);

    m_prevSysIdle   = sysIdle;
    m_prevSysKernel = sysKernel;
    m_prevSysUser   = sysUser;

    MEMORYSTATUSEX msx = {};
    msx.dwLength = sizeof(msx);
    GlobalMemoryStatusEx(&msx);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {

        SystemPerf sp;
        sp.cpuUsagePercent  = sysCpu;
        sp.gpuUsagePercent  = gpuUsagePercent;
        sp.totalMemoryBytes = msx.ullTotalPhys;
        sp.availMemoryBytes = msx.ullAvailPhys;
        sp.usedMemoryBytes  = msx.ullTotalPhys - msx.ullAvailPhys;
        sp.commitedBytes    = msx.ullTotalPageFile - msx.ullAvailPageFile;
        sp.commitLimitBytes = msx.ullTotalPageFile;
        emit processesUpdated({}, sp);
        return;
    }

    const QSet<quint32> guiPids = enumGuiPids();

    QVector<ProcessInfo> result;
    result.reserve(256);

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    QHash<quint32, quint32> parentMap;
    if (Process32FirstW(snap, &pe)) {
        do {
            parentMap.insert(static_cast<quint32>(pe.th32ProcessID),
                             static_cast<quint32>(pe.th32ParentProcessID));
        } while (Process32NextW(snap, &pe));
    }

    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            const quint32 pid = static_cast<quint32>(pe.th32ProcessID);
            if (pid == 0) {
                do {} while (false);
            } else {
                ProcessInfo pi;
                pi.pid       = pid;
                pi.parentPid = static_cast<quint32>(pe.th32ParentProcessID);
                pi.name      = QString::fromWCharArray(pe.szExeFile);
                pi.threadCount = static_cast<quint32>(pe.cntThreads);
                pi.gpuPercent = gpuByPid.value(pid, 0.0);

                HANDLE hProc = OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION |
                    PROCESS_VM_READ,
                    FALSE, pid);

                if (hProc) {

                    pi.exePath = queryProcessPath(hProc);

                    pi.username = queryProcessUser(hProc);

                    PROCESS_MEMORY_COUNTERS_EX pmc = {};
                    pmc.cb = sizeof(pmc);
                    if (GetProcessMemoryInfo(hProc,
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                            sizeof(pmc))) {
                        pi.workingSetBytes = static_cast<quint64>(pmc.WorkingSetSize);
                        pi.privateBytes    = static_cast<quint64>(pmc.PrivateUsage);
                        pi.peakWorkingSet  = static_cast<quint64>(pmc.PeakWorkingSetSize);
                    }

                    DWORD handleCount = 0;
                    if (GetProcessHandleCount(hProc, &handleCount))
                        pi.handleCount = static_cast<quint32>(handleCount);

                    FILETIME ftCreate = {}, ftExit = {}, ftKern = {}, ftUserP = {};
                    if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKern, &ftUserP)) {
                        pi.kernelTime = ftToU64(ftKern);
                        pi.userTime   = ftToU64(ftUserP);
                    }

                    IO_COUNTERS ioc = {};
                    if (GetProcessIoCounters(hProc, &ioc)) {
                        pi.rawReadBytes  = ioc.ReadTransferCount;
                        pi.rawWriteBytes = ioc.WriteTransferCount;
                    }

                    pi.priority = static_cast<int>(GetPriorityClass(hProc));

                    if (guiPids.contains(pid)) {

                        HWND hwnd = nullptr;

                        struct FindHwnd { HWND result; quint32 pid; };
                        FindHwnd fh{ nullptr, pid };
                        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                            auto* s = reinterpret_cast<FindHwnd*>(lp);
                            DWORD wpid = 0;
                            GetWindowThreadProcessId(h, &wpid);
                            if (wpid == s->pid && IsWindowVisible(h)) {
                                s->result = h;
                                return FALSE;
                            }
                            return TRUE;
                        }, reinterpret_cast<LPARAM>(&fh));
                        hwnd = fh.result;
                        if (hwnd && IsHungAppWindow(hwnd))
                            pi.status = ProcessStatus::NotResponding;
                        else
                            pi.status = ProcessStatus::Running;
                    } else {
                        pi.status = ProcessStatus::Running;
                    }

                    CloseHandle(hProc);
                } else {

                    pi.status = ProcessStatus::Running;
                }

                auto it = m_prevTimes.find(pid);
                if (it != m_prevTimes.end()) {
                    ProcTimes& prev = it.value();
                    quint64 dK = pi.kernelTime - prev.kernelTime;
                    quint64 dU = pi.userTime   - prev.userTime;
                    if (dTotal > 0)
                        pi.cpuPercent = 100.0 * static_cast<double>(dK + dU)
                                                / static_cast<double>(dTotal);

                    quint64 dR = pi.rawReadBytes  - prev.readBytes;
                    quint64 dW = pi.rawWriteBytes - prev.writeBytes;
                    double  dt = static_cast<double>(m_timer->interval()) / 1000.0;
                    if (dt > 0.0) {
                        pi.diskReadBytesPerSec  = static_cast<quint64>(dR / dt);
                        pi.diskWriteBytesPerSec = static_cast<quint64>(dW / dt);
                    }
                }

                m_prevTimes[pid] = { pi.kernelTime, pi.userTime,
                                     pi.rawReadBytes, pi.rawWriteBytes };

                pi.category = categorize(pi, guiPids);

                result.push_back(std::move(pi));
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    {
        QSet<quint32> livePids;
        livePids.reserve(result.size());
        for (auto& p : result) livePids.insert(p.pid);
        auto it = m_prevTimes.begin();
        while (it != m_prevTimes.end()) {
            if (!livePids.contains(it.key()))
                it = m_prevTimes.erase(it);
            else
                ++it;
        }
    }

    QHash<quint32, int> pidToIndex;
    pidToIndex.reserve(result.size());
    for (int i = 0; i < result.size(); ++i)
        pidToIndex.insert(result[i].pid, i);

    for (auto& pi : result) {
        auto parentIt = pidToIndex.find(pi.parentPid);
        if (parentIt != pidToIndex.end())
            result[parentIt.value()].childPids.append(pi.pid);
    }

    std::stable_sort(result.begin(), result.end(),
        [](const ProcessInfo& a, const ProcessInfo& b) {
            if (a.category != b.category)
                return static_cast<int>(a.category) < static_cast<int>(b.category);
            return a.cpuPercent > b.cpuPercent;
        });

    {
        QHash<quint32, int> tcpByPid;
        QHash<quint32, int> udpByPid;

        DWORD tcpSize = 0;
        GetExtendedTcpTable(nullptr, &tcpSize, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL, 0);
        if (tcpSize > 0) {
            QByteArray tcpBuf(static_cast<int>(tcpSize), 0);
            if (GetExtendedTcpTable(tcpBuf.data(), &tcpSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const quint32 pid = table->table[i].dwOwningPid;
                    if (pid > 0) tcpByPid[pid]++;
                }
            }
        }

        DWORD udpSize = 0;
        GetExtendedUdpTable(nullptr, &udpSize, FALSE, AF_INET,
                            UDP_TABLE_OWNER_PID, 0);
        if (udpSize > 0) {
            QByteArray udpBuf(static_cast<int>(udpSize), 0);
            if (GetExtendedUdpTable(udpBuf.data(), &udpSize, FALSE, AF_INET,
                                    UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(udpBuf.data());
                for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                    const quint32 pid = table->table[i].dwOwningPid;
                    if (pid > 0) udpByPid[pid]++;
                }
            }
        }

        for (auto& pi : result) {
            pi.tcpConnections = tcpByPid.value(pi.pid, 0);
            pi.udpEndpoints   = udpByPid.value(pi.pid, 0);
        }
    }

    SystemPerf sysPerf;
    sysPerf.cpuUsagePercent  = sysCpu;
    sysPerf.gpuUsagePercent  = gpuUsagePercent;
    sysPerf.totalMemoryBytes = static_cast<quint64>(msx.ullTotalPhys);
    sysPerf.availMemoryBytes = static_cast<quint64>(msx.ullAvailPhys);
    sysPerf.usedMemoryBytes  = sysPerf.totalMemoryBytes - sysPerf.availMemoryBytes;
    sysPerf.commitedBytes    = static_cast<quint64>(msx.ullTotalPageFile
                               - msx.ullAvailPageFile);
    sysPerf.commitLimitBytes = static_cast<quint64>(msx.ullTotalPageFile);
    sysPerf.processCount     = result.size();
    for (auto& p : result) sysPerf.threadCount  += static_cast<int>(p.threadCount);
    for (auto& p : result) sysPerf.handleCount  += static_cast<int>(p.handleCount);
    sysPerf.uptimeSeconds    = static_cast<double>(GetTickCount64()) / 1000.0;

    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        int nCpus = static_cast<int>(si.dwNumberOfProcessors);
        struct PROCESSOR_POWER_INFORMATION {
            ULONG Number;
            ULONG MaxMhz;
            ULONG CurrentMhz;
            ULONG MhzLimit;
            ULONG MaxIdleState;
            ULONG CurrentIdleState;
        };
        QVector<PROCESSOR_POWER_INFORMATION> ppis(nCpus);
        NTSTATUS status = CallNtPowerInformation(
            ProcessorInformation, nullptr, 0,
            ppis.data(),
            static_cast<ULONG>(nCpus * sizeof(PROCESSOR_POWER_INFORMATION)));
        if (status == 0 && nCpus > 0) {
            double totalMhz = 0.0;
            for (int i = 0; i < nCpus; ++i)
                totalMhz += ppis[i].CurrentMhz;
            sysPerf.cpuFreqMHz = totalMhz / nCpus;
        }
    }

    {
        const double intervalSec =
            static_cast<double>(m_timer->interval()) / 1000.0;

        wchar_t driveBuf[512] = {};
        if (GetLogicalDriveStringsW(511, driveBuf)) {
            for (const wchar_t* p = driveBuf; *p; p += wcslen(p) + 1) {
                UINT dtype = GetDriveTypeW(p);
                if (dtype != DRIVE_FIXED && dtype != DRIVE_REMOVABLE)
                    continue;

                wchar_t driveLetter[3] = { p[0], p[1], 0 };
                wchar_t ioctlPath[8];
                swprintf(ioctlPath, 8, L"\\\\.\\%c:", p[0]);

                HANDLE hVol = CreateFileW(
                    ioctlPath, 0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, 0, nullptr);

                QString dl = QString::fromWCharArray(driveLetter);
                DiskPerf dp;
                dp.driveLetter = dl;

                wchar_t volName[MAX_PATH + 1] = {};
                if (GetVolumeInformationW(p, volName, MAX_PATH + 1,
                                          nullptr, nullptr, nullptr,
                                          nullptr, 0)) {
                    dp.volumeLabel = QString::fromWCharArray(volName);
                }

                ULARGE_INTEGER freeForCaller, totalSize, totalFree;
                if (GetDiskFreeSpaceExW(p, &freeForCaller, &totalSize, &totalFree)) {
                    dp.totalBytes = static_cast<quint64>(totalSize.QuadPart);
                    dp.freeBytes  = static_cast<quint64>(freeForCaller.QuadPart);
                }

                if (hVol != INVALID_HANDLE_VALUE) {
                    DISK_PERFORMANCE diskPerf = {};
                    DWORD returned = 0;
                    if (DeviceIoControl(hVol, IOCTL_DISK_PERFORMANCE,
                                        nullptr, 0,
                                        &diskPerf, sizeof(diskPerf),
                                        &returned, nullptr)) {
                        quint64 curRead  = static_cast<quint64>(diskPerf.BytesRead.QuadPart);
                        quint64 curWrite = static_cast<quint64>(diskPerf.BytesWritten.QuadPart);

                        auto it = m_prevDiskCounters.find(dl);
                        if (it != m_prevDiskCounters.end() && intervalSec > 0.0) {
                            quint64 dR = curRead  - it->bytesRead;
                            quint64 dW = curWrite - it->bytesWritten;
                            dp.readBytesPerSec  = static_cast<quint64>(dR / intervalSec);
                            dp.writeBytesPerSec = static_cast<quint64>(dW / intervalSec);
                        }
                        m_prevDiskCounters[dl] = { curRead, curWrite };
                    }
                    CloseHandle(hVol);
                }

                sysPerf.disks.append(dp);
            }
        }
    }

    {
        const double intervalSec =
            static_cast<double>(m_timer->interval()) / 1000.0;

        DWORD dwSize = 0;
        GetIfTable(nullptr, &dwSize, FALSE);
        auto* ifTable = static_cast<PMIB_IFTABLE>(malloc(dwSize));
        if (ifTable && GetIfTable(ifTable, &dwSize, FALSE) == NO_ERROR) {
            quint64 totalRecv = 0, totalSent = 0;
            for (DWORD i = 0; i < ifTable->dwNumEntries; ++i) {
                const MIB_IFROW& r = ifTable->table[i];
                if (r.dwType == MIB_IF_TYPE_LOOPBACK) continue;
                totalRecv += r.dwInOctets;
                totalSent += r.dwOutOctets;
            }

            if (intervalSec > 0.0) {
                if (m_prevNetRecvBytes > 0 || m_prevNetSentBytes > 0) {
                    if (totalRecv >= m_prevNetRecvBytes)
                        sysPerf.network.recvBytesPerSec = static_cast<quint64>(
                            (totalRecv - m_prevNetRecvBytes) / intervalSec);
                    if (totalSent >= m_prevNetSentBytes)
                        sysPerf.network.sentBytesPerSec = static_cast<quint64>(
                            (totalSent - m_prevNetSentBytes) / intervalSec);
                }
                m_prevNetRecvBytes = totalRecv;
                m_prevNetSentBytes = totalSent;
            }
        }
        free(ifTable);
    }

    emit processesUpdated(result, sysPerf);
}

static bool enableDebugPrivilege() {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }
    bool ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr) != 0;
    CloseHandle(hToken);
    return ok;
}

QString ProcessMonitor::endProcess(quint32 pid) {

    enableDebugPrivilege();

    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) {
        DWORD err = GetLastError();
        QString msg;
        if (err == ERROR_ACCESS_DENIED)
            msg = QStringLiteral("Access denied. The process may be running as Administrator.");
        else
            msg = QStringLiteral("Failed to open process (error %1).").arg(err);
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            QStringLiteral("endProcess: OpenProcess failed."),
            QStringLiteral("PID=%1 err=%2").arg(pid).arg(err));
        return msg;
    }
    if (!TerminateProcess(h, 1)) {
        DWORD err = GetLastError();
        CloseHandle(h);
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            QStringLiteral("TerminateProcess failed."),
            QStringLiteral("PID=%1 err=%2").arg(pid).arg(err));
        return QStringLiteral("Could not terminate the process (error %1).").arg(err);
    }
    CloseHandle(h);
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
        QStringLiteral("Process terminated."),
        QStringLiteral("PID=%1").arg(pid));
    refresh();
    return {};
}

QString ProcessMonitor::endProcessTree(quint32 pid) {
    enableDebugPrivilege();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return endProcess(pid);

    QHash<quint32, QVector<quint32>> childrenOf;
    PROCESSENTRY32W pe = {}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            quint32 cpid = static_cast<quint32>(pe.th32ProcessID);
            quint32 ppid = static_cast<quint32>(pe.th32ParentProcessID);
            childrenOf[ppid].push_back(cpid);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    QVector<quint32> toKill = { pid };
    QSet<quint32>    visited = { pid };
    for (int i = 0; i < toKill.size(); ++i) {
        for (quint32 child : childrenOf.value(toKill[i])) {
            if (!visited.contains(child)) {
                toKill.push_back(child);
                visited.insert(child);
            }
        }
    }

    int failCount = 0;
    QString lastError;
    for (int i = toKill.size() - 1; i >= 0; --i) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, toKill[i]);
        if (h) {
            if (!TerminateProcess(h, 1)) {
                ++failCount;
                lastError = QStringLiteral("Failed to terminate PID %1.").arg(toKill[i]);
            }
            CloseHandle(h);
        } else {
            ++failCount;
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED)
                lastError = QStringLiteral("Access denied for PID %1.").arg(toKill[i]);
        }
    }

    refresh();

    if (failCount == 0)
        return {};
    if (failCount == toKill.size())
        return QStringLiteral("Could not terminate any process in the tree. %1").arg(lastError);
    return QStringLiteral("%1 of %2 processes could not be terminated. %3")
        .arg(failCount).arg(toKill.size()).arg(lastError);
}

bool ProcessMonitor::suspendProcess(quint32 pid) {
    if (!m_ntSuspend) return false;
    HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return false;
    bool ok = (m_ntSuspend(h) >= 0);
    CloseHandle(h);
    if (ok)
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Process suspended."),
            QStringLiteral("PID=%1").arg(pid));
    return ok;
}

bool ProcessMonitor::resumeProcess(quint32 pid) {
    if (!m_ntResume) return false;
    HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return false;
    bool ok = (m_ntResume(h) >= 0);
    CloseHandle(h);
    if (ok)
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Process resumed."),
            QStringLiteral("PID=%1").arg(pid));
    return ok;
}

bool ProcessMonitor::setPriority(quint32 pid, int priorityClass) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return false;
    bool ok = SetPriorityClass(h, static_cast<DWORD>(priorityClass));
    CloseHandle(h);
    return ok;
}

}

#else

namespace wintools::taskmanager {

ProcessMonitor::ProcessMonitor(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::refresh);
    m_timer->setInterval(2000);
}

ProcessMonitor::~ProcessMonitor() {
    stop();
}

void ProcessMonitor::setRefreshInterval(int ms) { m_timer->setInterval(ms); }
void ProcessMonitor::start() { m_timer->start(); }
void ProcessMonitor::stop()  { m_timer->stop(); }
void ProcessMonitor::refresh() {

    emit processesUpdated({}, {});
}

QString ProcessMonitor::endProcess(quint32) { return "Not supported on this platform"; }
QString ProcessMonitor::endProcessTree(quint32) { return "Not supported on this platform"; }
bool ProcessMonitor::suspendProcess(quint32) { return false; }
bool ProcessMonitor::resumeProcess(quint32) { return false; }
bool ProcessMonitor::setPriority(quint32, int) { return false; }

}

#endif
