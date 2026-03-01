#include "modules/AdvancedTaskManager/src/ui/task_manager_window.hpp"
#include "modules/AdvancedTaskManager/src/core/process_monitor.hpp"
#include "modules/AdvancedTaskManager/src/model/process_model.hpp"
#include "modules/AdvancedTaskManager/src/ui/perf_graph_widget.hpp"
#include "modules/AdvancedTaskManager/src/ui/profiler_overlay_controller.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "logger/logger.hpp"

#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTreeView>
#include <QTableView>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QGroupBox>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QFrame>
#include <QMessageBox>
#include <QDesktopServices>
#include <QSortFilterProxyModel>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QSignalBlocker>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTableWidgetItem>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <winsvc.h>
#include <shlobj.h>
#endif

namespace wintools::taskmanager {

static constexpr const char* kLog = "TaskManager/Window";

namespace {

struct ServiceEntry {
    QString name;
    QString displayName;
    QString status;
    QString startType;
    quint32 pid{0};
};

#if defined(Q_OS_WIN)
QVector<ServiceEntry> enumerateServices() {
    QVector<ServiceEntry> result;

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return result;

    DWORD bytesNeeded = 0, count = 0, resumeHandle = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                          SERVICE_STATE_ALL, nullptr, 0,
                          &bytesNeeded, &count, &resumeHandle, nullptr);

    QByteArray buf(static_cast<int>(bytesNeeded), '\0');
    auto* services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buf.data());

    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                               SERVICE_STATE_ALL,
                               reinterpret_cast<LPBYTE>(services), bytesNeeded,
                               &bytesNeeded, &count, &resumeHandle, nullptr)) {
        CloseServiceHandle(scm);
        return result;
    }

    result.reserve(static_cast<int>(count));
    for (DWORD i = 0; i < count; ++i) {
        ServiceEntry entry;
        entry.name = QString::fromWCharArray(services[i].lpServiceName);
        entry.displayName = QString::fromWCharArray(services[i].lpDisplayName);
        entry.pid = services[i].ServiceStatusProcess.dwProcessId;

        switch (services[i].ServiceStatusProcess.dwCurrentState) {
        case SERVICE_RUNNING:         entry.status = QStringLiteral("Running"); break;
        case SERVICE_STOPPED:         entry.status = QStringLiteral("Stopped"); break;
        case SERVICE_START_PENDING:   entry.status = QStringLiteral("Starting"); break;
        case SERVICE_STOP_PENDING:    entry.status = QStringLiteral("Stopping"); break;
        case SERVICE_PAUSE_PENDING:   entry.status = QStringLiteral("Pausing"); break;
        case SERVICE_PAUSED:          entry.status = QStringLiteral("Paused"); break;
        case SERVICE_CONTINUE_PENDING:entry.status = QStringLiteral("Resuming"); break;
        default:                      entry.status = QStringLiteral("Unknown"); break;
        }

        SC_HANDLE svc = OpenServiceW(scm, services[i].lpServiceName, SERVICE_QUERY_CONFIG);
        if (svc) {
            DWORD configBytes = 0;
            QueryServiceConfigW(svc, nullptr, 0, &configBytes);
            QByteArray cbuf(static_cast<int>(configBytes), '\0');
            auto* cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(cbuf.data());
            if (QueryServiceConfigW(svc, cfg, configBytes, &configBytes)) {
                switch (cfg->dwStartType) {
                case SERVICE_AUTO_START:   entry.startType = QStringLiteral("Automatic"); break;
                case SERVICE_BOOT_START:   entry.startType = QStringLiteral("Boot"); break;
                case SERVICE_SYSTEM_START: entry.startType = QStringLiteral("System"); break;
                case SERVICE_DEMAND_START: entry.startType = QStringLiteral("Manual"); break;
                case SERVICE_DISABLED:     entry.startType = QStringLiteral("Disabled"); break;
                default:                   entry.startType = QStringLiteral("Unknown"); break;
                }
            }
            CloseServiceHandle(svc);
        }

        result.push_back(std::move(entry));
    }

    CloseServiceHandle(scm);

    std::sort(result.begin(), result.end(), [](const ServiceEntry& a, const ServiceEntry& b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });
    return result;
}

bool controlService(const QString& serviceName, DWORD control) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    DWORD access = (control == 0) ? SERVICE_START : SERVICE_STOP | SERVICE_PAUSE_CONTINUE;
    SC_HANDLE svc = OpenServiceW(scm, serviceName.toStdWString().c_str(), access);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    bool ok = false;
    if (control == 0) {

        ok = StartServiceW(svc, 0, nullptr) != FALSE;
    } else {
        SERVICE_STATUS ss;
        ok = ControlService(svc, control, &ss) != FALSE;
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}
#else
QVector<ServiceEntry> enumerateServices() { return {}; }
bool controlService(const QString&, quint32) { return false; }
#endif

struct StartupEntry {
    QString name;
    QString command;
    QString publisher;
    bool enabled{true};
    bool isHKLM{false};
};

#if defined(Q_OS_WIN)

static QString publisherFromExe(const QString& exePath) {
    if (exePath.isEmpty()) return {};
    const std::wstring wPath = exePath.toStdWString();
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(wPath.c_str(), &dummy);
    if (size == 0) return {};
    QByteArray buf(static_cast<int>(size), '\0');
    if (!GetFileVersionInfoW(wPath.c_str(), 0, size, buf.data())) return {};

    struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; };
    LANGANDCODEPAGE* trans = nullptr;
    UINT transLen = 0;
    if (!VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<LPVOID*>(&trans), &transLen) || transLen == 0)
        return {};

    wchar_t subBlock[128];
    wsprintfW(subBlock, L"\\StringFileInfo\\%04x%04x\\CompanyName",
              trans[0].wLanguage, trans[0].wCodePage);
    wchar_t* company = nullptr;
    UINT companyLen = 0;
    if (VerQueryValueW(buf.data(), subBlock,
                       reinterpret_cast<LPVOID*>(&company), &companyLen) && companyLen > 0)
        return QString::fromWCharArray(company).trimmed();
    return {};
}

static QString extractExePath(const QString& command) {
    QString trimmed = command.trimmed();
    if (trimmed.startsWith(QLatin1Char('"'))) {
        int end = trimmed.indexOf(QLatin1Char('"'), 1);
        if (end > 0) return trimmed.mid(1, end - 1);
    }
    int space = trimmed.indexOf(QLatin1Char(' '));
    return space > 0 ? trimmed.left(space) : trimmed;
}

static QVector<StartupEntry> readRunKey(HKEY root, bool isHKLM) {
    QVector<StartupEntry> result;
    HKEY runKey = nullptr;
    const wchar_t* runPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(root, runPath, 0, KEY_READ, &runKey) != ERROR_SUCCESS)
        return result;

    HKEY approvedKey = nullptr;
    const wchar_t* approvedPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
    RegOpenKeyExW(root, approvedPath, 0, KEY_READ, &approvedKey);

    DWORD index = 0;
    wchar_t valueName[512];
    BYTE data[4096];
    DWORD nameLen, dataLen, type;

    while (true) {
        nameLen = static_cast<DWORD>(std::size(valueName));
        dataLen = static_cast<DWORD>(sizeof(data));
        LONG ret = RegEnumValueW(runKey, index++, valueName, &nameLen,
                                  nullptr, &type, data, &dataLen);
        if (ret != ERROR_SUCCESS) break;
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;

        StartupEntry entry;
        entry.name = QString::fromWCharArray(valueName, static_cast<int>(nameLen));
        entry.command = QString::fromWCharArray(reinterpret_cast<wchar_t*>(data));
        entry.isHKLM = isHKLM;
        entry.enabled = true;

        if (approvedKey) {
            BYTE approvedData[32] = {};
            DWORD approvedLen = sizeof(approvedData);
            DWORD approvedType = 0;
            if (RegQueryValueExW(approvedKey, valueName, nullptr, &approvedType,
                                  approvedData, &approvedLen) == ERROR_SUCCESS
                && approvedLen >= 4) {

                if (approvedData[0] == 3 || approvedData[0] == 7) {
                    entry.enabled = false;
                }
            }
        }

        entry.publisher = publisherFromExe(extractExePath(entry.command));

        result.push_back(std::move(entry));
    }

    if (approvedKey) RegCloseKey(approvedKey);
    RegCloseKey(runKey);
    return result;
}

QVector<StartupEntry> enumerateStartupEntries() {
    QVector<StartupEntry> result;
    result.append(readRunKey(HKEY_CURRENT_USER, false));
    result.append(readRunKey(HKEY_LOCAL_MACHINE, true));

    wchar_t folderPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, folderPath))) {
        QString startupDir = QString::fromWCharArray(folderPath);
        QDir dir(startupDir);
        const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            StartupEntry entry;
            entry.name = fi.completeBaseName();
            entry.command = fi.absoluteFilePath();
            entry.isHKLM = false;
            entry.enabled = true;

            if (fi.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0) {

                entry.publisher = publisherFromExe(fi.symLinkTarget());
            } else {
                entry.publisher = publisherFromExe(fi.absoluteFilePath());
            }
            result.push_back(std::move(entry));
        }
    }

    std::sort(result.begin(), result.end(), [](const StartupEntry& a, const StartupEntry& b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return result;
}

bool setStartupEnabled(const QString& name, bool isHKLM, bool enabled) {
    HKEY root = isHKLM ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY approvedKey = nullptr;
    const wchar_t* approvedPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

    DWORD access = KEY_READ | KEY_WRITE;
    if (RegOpenKeyExW(root, approvedPath, 0, access, &approvedKey) != ERROR_SUCCESS)
        return false;

    BYTE data[12] = {};
    DWORD dataLen = sizeof(data);
    DWORD type = REG_BINARY;
    const std::wstring wName = name.toStdWString();

    if (RegQueryValueExW(approvedKey, wName.c_str(), nullptr, &type, data, &dataLen) != ERROR_SUCCESS) {

        dataLen = 12;
        memset(data, 0, sizeof(data));
    }

    data[0] = enabled ? 2 : 3;

    bool ok = (RegSetValueExW(approvedKey, wName.c_str(), 0, REG_BINARY, data, dataLen) == ERROR_SUCCESS);
    RegCloseKey(approvedKey);
    return ok;
}

#else
QVector<StartupEntry> enumerateStartupEntries() { return {}; }
bool setStartupEnabled(const QString&, bool, bool) { return false; }
#endif

}

TaskManagerWindow::TaskManagerWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Advanced Task Manager"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/taskmanager.svg")));
    setWindowFlags(Qt::Window |
                   Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint |
                   Qt::WindowCloseButtonHint);
    resize(900, 640);

    m_treeModel   = new ProcessTreeModel(this);
    m_tableModel  = new ProcessTableModel(this);
    m_filterProxy = new ProcessFilterProxy(this);
    m_filterProxy->setSourceModel(m_tableModel);
    m_filterProxy->setSortRole(Qt::DisplayRole);

    m_monitor = new ProcessMonitor(this);
    connect(m_monitor, &ProcessMonitor::processesUpdated,
            this, &TaskManagerWindow::onProcessesUpdated);

    buildUi();
    applyTheme(wintools::themes::ThemeHelper::currentPalette());
    m_themeListener = new wintools::themes::ThemeListener(this);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, [this](bool) {
                applyTheme(wintools::themes::ThemeHelper::currentPalette());
            });

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "TaskManagerWindow opened.");

    auto* overlayController = ProfilerOverlayController::instance();
    connect(overlayController, &ProfilerOverlayController::overlayVisibilityChanged,
            this, [this](bool visible) {
                if (!m_overlayToggle) {
                    return;
                }
                const QSignalBlocker blocker(m_overlayToggle);
                m_overlayToggle->setChecked(visible);
                m_overlayToggle->setText(visible
                    ? QStringLiteral("Disable Overlay")
                    : QStringLiteral("Enable Overlay"));
                if (m_overlaySettings) {
                    m_overlaySettings->setEnabled(visible);
                }
            });

    m_monitor->start();
}

TaskManagerWindow::~TaskManagerWindow() {
    m_monitor->stop();
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "TaskManagerWindow closed.");
}

void TaskManagerWindow::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    root->addWidget(tabs, 1);

    buildProcessesTab();
    buildPerformanceTab();
    buildDetailsTab();
    buildServicesTab();
    buildStartupTab();
    buildStatusBar();

    auto* procPage = new QWidget;
    auto* procLay  = new QVBoxLayout(procPage);
    procLay->setContentsMargins(6, 6, 6, 4);
    procLay->setSpacing(4);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    m_treeSearch = new QLineEdit;
    m_treeSearch->setPlaceholderText(QStringLiteral("Search processes…"));
    m_treeSearch->setClearButtonEnabled(true);
    toolbar->addWidget(m_treeSearch, 1);
    m_procUsageSummary = new QLabel(QStringLiteral("CPU: —  |  GPU: —"));
    toolbar->addWidget(m_procUsageSummary);

    m_btnEndTask = new QPushButton(QStringLiteral("End Task"));
    m_btnEndTask->setEnabled(false);
    m_btnEndTree = new QPushButton(QStringLiteral("End Task Tree"));
    m_btnEndTree->setEnabled(false);
    m_btnSuspend = new QPushButton(QStringLiteral("Suspend"));
    m_btnSuspend->setEnabled(false);
    m_btnResume  = new QPushButton(QStringLiteral("Resume"));
    m_btnResume->setEnabled(false);

    toolbar->addWidget(m_btnEndTask);
    toolbar->addWidget(m_btnEndTree);
    toolbar->addWidget(m_btnSuspend);
    toolbar->addWidget(m_btnResume);
    procLay->addLayout(toolbar);
    procLay->addWidget(m_processTree, 1);

    auto* perfPage    = new QWidget;
    auto* perfPageLay = new QVBoxLayout(perfPage);
    perfPageLay->setContentsMargins(0, 0, 0, 0);
    perfPageLay->setSpacing(0);

    auto* perfHeader = new QHBoxLayout;
    perfHeader->setContentsMargins(8, 4, 8, 4);
    m_overlayToggle = new QPushButton(QStringLiteral("Enable Overlay"));
    m_overlayToggle->setCheckable(true);
    m_overlaySettings = new QPushButton(QStringLiteral("Overlay Settings"));
    m_overlaySettings->setEnabled(false);
    m_overlaySettings->setToolTip(QStringLiteral("Configure overlay hotkey from WinTools Hotkeys table"));
    m_perfViewToggle = new QPushButton(QStringLiteral("Tile View"));
    m_perfViewToggle->setFixedWidth(110);
    m_perfViewToggle->setToolTip(QStringLiteral("Switch to tile view to see all graphs at once"));
    perfHeader->addStretch();
    perfHeader->addWidget(m_overlayToggle);
    perfHeader->addWidget(m_overlaySettings);
    perfHeader->addWidget(m_perfViewToggle);
    perfPageLay->addLayout(perfHeader);

    m_perfStack = new QStackedWidget;
    perfPageLay->addWidget(m_perfStack, 1);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    auto* perfInner = new QWidget;
    m_perfScrollLayout = new QVBoxLayout(perfInner);
    m_perfScrollLayout->setContentsMargins(12, 12, 12, 12);
    m_perfScrollLayout->setSpacing(12);

    m_cpuGroup = new QGroupBox(QStringLiteral("CPU Utilization"));
    auto* cpuLay   = new QVBoxLayout(m_cpuGroup);
    cpuLay->addWidget(m_cpuGraph);
    auto* cpuStats = new QHBoxLayout;
    m_lblCpuUsage = new QLabel(QStringLiteral("CPU: —"));
    m_lblGpuUsage = new QLabel(QStringLiteral("GPU: —"));
    m_lblHandles  = new QLabel(QStringLiteral("Handles: —"));
    m_lblProcessCount = new QLabel(QStringLiteral("Processes: —"));
    m_lblThreadCount  = new QLabel(QStringLiteral("Threads: —"));
    m_lblUptime   = new QLabel(QStringLiteral("Uptime: —"));
    cpuStats->addWidget(m_lblCpuUsage);
    cpuStats->addWidget(m_lblGpuUsage);
    cpuStats->addStretch();
    cpuStats->addWidget(m_lblHandles);
    cpuStats->addWidget(m_lblProcessCount);
    cpuStats->addWidget(m_lblThreadCount);
    cpuStats->addWidget(m_lblUptime);
    cpuLay->addLayout(cpuStats);
    m_perfScrollLayout->addWidget(m_cpuGroup);

    m_gpuGroup = new QGroupBox(QStringLiteral("GPU Utilization"));
    auto* gpuLay   = new QVBoxLayout(m_gpuGroup);
    gpuLay->addWidget(m_gpuGraph);
    auto* gpuStats = new QHBoxLayout;
    m_lblGpuGraphUsage = new QLabel(QStringLiteral("Usage: —"));
    gpuStats->addWidget(m_lblGpuGraphUsage);
    gpuStats->addStretch();
    gpuLay->addLayout(gpuStats);

    m_gpuProcessTable = new QTableWidget(0, 3, m_gpuGroup);
    m_gpuProcessTable->setHorizontalHeaderLabels({
        QStringLiteral("Process"),
        QStringLiteral("PID"),
        QStringLiteral("GPU %")
    });
    m_gpuProcessTable->horizontalHeader()->setStretchLastSection(true);
    m_gpuProcessTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_gpuProcessTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_gpuProcessTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_gpuProcessTable->verticalHeader()->setVisible(false);
    m_gpuProcessTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gpuProcessTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_gpuProcessTable->setFocusPolicy(Qt::NoFocus);
    m_gpuProcessTable->setMaximumHeight(220);
    gpuLay->addWidget(m_gpuProcessTable);

    m_perfScrollLayout->addWidget(m_gpuGroup);

    m_memGroup = new QGroupBox(QStringLiteral("Memory Usage"));
    auto* memLay   = new QVBoxLayout(m_memGroup);
    memLay->addWidget(m_memGraph);
    auto* memStats = new QHBoxLayout;
    m_lblMemUsage   = new QLabel(QStringLiteral("In Use: —"));
    m_lblMemDetails = new QLabel(QStringLiteral("Available: — / Total: —"));
    memStats->addWidget(m_lblMemUsage);
    memStats->addStretch();
    memStats->addWidget(m_lblMemDetails);
    memLay->addLayout(memStats);
    m_perfScrollLayout->addWidget(m_memGroup);

    m_diskContainerWidget = new QWidget;
    m_diskContainerLayout = new QVBoxLayout(m_diskContainerWidget);
    m_diskContainerLayout->setContentsMargins(0, 0, 0, 0);
    m_diskContainerLayout->setSpacing(8);
    m_perfScrollLayout->addWidget(m_diskContainerWidget);

    m_netGroup  = new QGroupBox(QStringLiteral("Network"));
    auto* netLay    = new QVBoxLayout(m_netGroup);
    auto* netGraphs = new QHBoxLayout;
    netGraphs->addWidget(m_netRecvGraph, 1);
    netGraphs->addWidget(m_netSentGraph, 1);
    netLay->addLayout(netGraphs);
    auto* netStats = new QHBoxLayout;
    m_lblNetRecv = new QLabel(QStringLiteral("Recv: —"));
    m_lblNetSent = new QLabel(QStringLiteral("Sent: —"));
    netStats->addWidget(m_lblNetRecv);
    netStats->addStretch();
    netStats->addWidget(m_lblNetSent);
    netLay->addLayout(netStats);
    m_perfScrollLayout->addWidget(m_netGroup);
    m_perfScrollLayout->addStretch();

    scrollArea->setWidget(perfInner);
    m_perfStack->addWidget(scrollArea);

    auto* tileScrollArea = new QScrollArea;
    tileScrollArea->setWidgetResizable(true);
    auto* tileInner = new QWidget;
    m_perfTileGrid = new QGridLayout(tileInner);
    m_perfTileGrid->setContentsMargins(8, 8, 8, 8);
    m_perfTileGrid->setSpacing(8);
    m_perfTileGrid->setColumnStretch(0, 1);
    m_perfTileGrid->setColumnStretch(1, 1);
    tileScrollArea->setWidget(tileInner);
    m_perfStack->addWidget(tileScrollArea);

    connect(m_perfViewToggle, &QPushButton::clicked,
            this, &TaskManagerWindow::togglePerfView);
    connect(m_overlayToggle, &QPushButton::toggled,
            this, &TaskManagerWindow::toggleProfilerOverlay);
    connect(m_overlaySettings, &QPushButton::clicked,
            this, &TaskManagerWindow::showProfilerOverlayMenu);

    m_overlayMenu = new QMenu(this);

    auto* cornerMenu = m_overlayMenu->addMenu(QStringLiteral("Position"));
    auto* cornerGroup = new QActionGroup(this);
    cornerGroup->setExclusive(true);

    auto* overlayController = ProfilerOverlayController::instance();

    auto addCornerAction = [&](const QString& label, int corner, bool checked) {
        QAction* action = cornerMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(checked);
        cornerGroup->addAction(action);
        connect(action, &QAction::triggered, this, [corner]() {
            ProfilerOverlayController::instance()->setCorner(corner);
        });
    };
    addCornerAction(QStringLiteral("Top Left"), 0, false);
    addCornerAction(QStringLiteral("Top Right"), 1, true);
    addCornerAction(QStringLiteral("Bottom Left"), 2, false);
    addCornerAction(QStringLiteral("Bottom Right"), 3, false);

    auto* scaleMenu = m_overlayMenu->addMenu(QStringLiteral("Size"));
    auto* scaleGroup = new QActionGroup(this);
    scaleGroup->setExclusive(true);
    auto addScaleAction = [&](const QString& label, double scale, bool checked) {
        QAction* action = scaleMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(checked);
        scaleGroup->addAction(action);
        connect(action, &QAction::triggered, this, [scale]() {
            ProfilerOverlayController::instance()->setScaleFactor(scale);
        });
    };
    addScaleAction(QStringLiteral("Compact"), 0.85, false);
    addScaleAction(QStringLiteral("Normal"), 1.0, true);
    addScaleAction(QStringLiteral("Large"), 1.25, false);

    auto* opacityMenu = m_overlayMenu->addMenu(QStringLiteral("Opacity"));
    auto* opacityGroup = new QActionGroup(this);
    opacityGroup->setExclusive(true);
    auto addOpacityAction = [&](const QString& label, double opacity, bool checked) {
        QAction* action = opacityMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(checked);
        opacityGroup->addAction(action);
        connect(action, &QAction::triggered, this, [opacity]() {
            ProfilerOverlayController::instance()->setOverlayOpacity(opacity);
        });
    };
    addOpacityAction(QStringLiteral("60%"), 0.60, false);
    addOpacityAction(QStringLiteral("75%"), 0.75, false);
    addOpacityAction(QStringLiteral("85%"), 0.85, true);
    addOpacityAction(QStringLiteral("100%"), 1.0, false);

    m_overlayMenu->addSeparator();
    m_overlayShowNetAction = m_overlayMenu->addAction(QStringLiteral("Show Network"));
    m_overlayShowNetAction->setCheckable(true);
    m_overlayShowNetAction->setChecked(overlayController->showNetwork());
    connect(m_overlayShowNetAction, &QAction::toggled, this, [](bool checked) {
        ProfilerOverlayController::instance()->setShowNetwork(checked);
    });

    m_overlayShowDiskAction = m_overlayMenu->addAction(QStringLiteral("Show Disk"));
    m_overlayShowDiskAction->setCheckable(true);
    m_overlayShowDiskAction->setChecked(overlayController->showDisk());
    connect(m_overlayShowDiskAction, &QAction::toggled, this, [](bool checked) {
        ProfilerOverlayController::instance()->setShowDisk(checked);
    });

    {
        const bool overlayVisible = overlayController->isOverlayVisible();
        const QSignalBlocker blocker(m_overlayToggle);
        m_overlayToggle->setChecked(overlayVisible);
        m_overlayToggle->setText(overlayVisible
            ? QStringLiteral("Disable Overlay")
            : QStringLiteral("Enable Overlay"));
        m_overlaySettings->setEnabled(overlayVisible);
    }

    auto* detailPage = new QWidget;
    auto* detailLay  = new QVBoxLayout(detailPage);
    detailLay->setContentsMargins(6, 6, 6, 4);
    detailLay->setSpacing(4);
    m_detailsSearch = new QLineEdit;
    m_detailsSearch->setPlaceholderText(QStringLiteral("Filter by name, PID, user…"));
    m_detailsSearch->setClearButtonEnabled(true);
    detailLay->addWidget(m_detailsSearch);
    detailLay->addWidget(m_detailsTable, 1);

    auto* servicesPage = new QWidget;
    auto* servicesLay  = new QVBoxLayout(servicesPage);
    servicesLay->setContentsMargins(6, 6, 6, 4);
    servicesLay->setSpacing(4);
    m_servicesSearch = new QLineEdit;
    m_servicesSearch->setPlaceholderText(QStringLiteral("Filter services…"));
    m_servicesSearch->setClearButtonEnabled(true);
    auto* svcBtnRow = new QHBoxLayout;
    svcBtnRow->setSpacing(6);
    svcBtnRow->addWidget(m_servicesSearch, 1);
    auto* svcRefreshBtn = new QPushButton(QStringLiteral("Refresh"));
    svcBtnRow->addWidget(svcRefreshBtn);
    servicesLay->addLayout(svcBtnRow);
    servicesLay->addWidget(m_servicesTable, 1);

    connect(svcRefreshBtn, &QPushButton::clicked, this, [this]() {
        buildServicesTab();
    });

    auto* startupPage = new QWidget;
    auto* startupLay  = new QVBoxLayout(startupPage);
    startupLay->setContentsMargins(6, 6, 6, 4);
    startupLay->setSpacing(4);
    m_startupSearch = new QLineEdit;
    m_startupSearch->setPlaceholderText(QStringLiteral("Filter startup apps…"));
    m_startupSearch->setClearButtonEnabled(true);
    auto* startupBtnRow = new QHBoxLayout;
    startupBtnRow->setSpacing(6);
    startupBtnRow->addWidget(m_startupSearch, 1);
    auto* startupRefreshBtn = new QPushButton(QStringLiteral("Refresh"));
    startupBtnRow->addWidget(startupRefreshBtn);
    startupLay->addLayout(startupBtnRow);
    startupLay->addWidget(m_startupTable, 1);

    connect(startupRefreshBtn, &QPushButton::clicked, this, [this]() {
        buildStartupTab();
    });

    tabs->addTab(procPage,     QStringLiteral("Processes"));
    tabs->addTab(perfPage,     QStringLiteral("Performance"));
    tabs->addTab(detailPage,   QStringLiteral("Details"));
    tabs->addTab(servicesPage, QStringLiteral("Services"));
    tabs->addTab(startupPage,  QStringLiteral("Startup"));

    auto* statusBar = new QFrame;
    statusBar->setFrameShape(QFrame::StyledPanel);
    statusBar->setMaximumHeight(24);
    auto* statusLay = new QHBoxLayout(statusBar);
    statusLay->setContentsMargins(8, 2, 8, 2);
    statusLay->setSpacing(16);
    statusLay->addWidget(m_statusProcesses);
    statusLay->addWidget(m_statusThreads);
    statusLay->addWidget(m_statusCpu);
    statusLay->addWidget(m_statusGpu);
    statusLay->addWidget(m_statusMemory);
    statusLay->addStretch();
    root->addWidget(statusBar);

    connect(m_treeSearch, &QLineEdit::textChanged,
            this, &TaskManagerWindow::onSearchTextChanged);
    connect(m_detailsSearch, &QLineEdit::textChanged,
            this, [this](const QString& t){ m_filterProxy->setFilterText(t); });
    connect(m_servicesSearch, &QLineEdit::textChanged,
            this, [this](const QString& t){
                m_servicesProxy->setFilterFixedString(t);
            });
    connect(m_startupSearch, &QLineEdit::textChanged,
            this, [this](const QString& t){
                m_startupProxy->setFilterFixedString(t);
            });
    connect(m_btnEndTask,  &QPushButton::clicked, this, &TaskManagerWindow::endSelectedTask);
    connect(m_btnEndTree,  &QPushButton::clicked, this, &TaskManagerWindow::endSelectedTaskTree);
    connect(m_btnSuspend,  &QPushButton::clicked, this, &TaskManagerWindow::suspendSelectedTask);
    connect(m_btnResume,   &QPushButton::clicked, this, &TaskManagerWindow::resumeSelectedTask);
    connect(tabs, &QTabWidget::currentChanged, this, &TaskManagerWindow::onTabChanged);

    connect(m_processTree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection&, const QItemSelection&) {
                quint32 pid = treeSelectedPid();
                m_btnEndTask->setEnabled(pid != 0);
                m_btnEndTree->setEnabled(pid != 0);
                m_btnSuspend->setEnabled(pid != 0);
                m_btnResume->setEnabled(pid != 0);
            });
}

void TaskManagerWindow::buildProcessesTab() {
    m_processTree = new QTreeView;
    m_processTree->setModel(m_treeModel);
    m_processTree->setUniformRowHeights(false);
    m_processTree->setAlternatingRowColors(true);
    m_processTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_processTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_processTree->setSortingEnabled(false);
    m_processTree->setExpandsOnDoubleClick(true);
    m_processTree->setItemsExpandable(true);
    m_processTree->setAnimated(true);
    m_processTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_processTree->header()->setDefaultSectionSize(80);

    connect(m_processTree, &QTreeView::customContextMenuRequested,
            this, &TaskManagerWindow::onTreeContextMenu);
}

void TaskManagerWindow::buildPerformanceTab() {
    m_cpuGraph = new PerfGraphWidget;
    m_cpuGraph->setTitle(QStringLiteral("CPU"));
    m_cpuGraph->setLineColor(QColor(0x17, 0xB3, 0x78));
    m_cpuGraph->setMaxValue(100.0);
    m_cpuGraph->setMinimumHeight(120);

    m_gpuGraph = new PerfGraphWidget;
    m_gpuGraph->setTitle(QStringLiteral("GPU"));
    m_gpuGraph->setLineColor(QColor(0x00, 0xB4, 0xD8));
    m_gpuGraph->setMaxValue(100.0);
    m_gpuGraph->setMinimumHeight(120);

    m_memGraph = new PerfGraphWidget;
    m_memGraph->setTitle(QStringLiteral("Memory"));
    m_memGraph->setLineColor(QColor(0x74, 0x7F, 0xE8));
    m_memGraph->setMaxValue(100.0);
    m_memGraph->setMinimumHeight(120);

    m_netRecvGraph = new PerfGraphWidget;
    m_netRecvGraph->setTitle(QStringLiteral("Receive"));
    m_netRecvGraph->setLineColor(QColor(0x00, 0xB4, 0xD8));
    m_netRecvGraph->setValueFormat(PerfGraphWidget::ValueFormat::BytesPerSec);
    m_netRecvGraph->setAutoScale(true);
    m_netRecvGraph->setMaxValue(100.0 * 1024.0);
    m_netRecvGraph->setMinimumHeight(100);

    m_netSentGraph = new PerfGraphWidget;
    m_netSentGraph->setTitle(QStringLiteral("Send"));
    m_netSentGraph->setLineColor(QColor(0xE0, 0x40, 0xFF));
    m_netSentGraph->setValueFormat(PerfGraphWidget::ValueFormat::BytesPerSec);
    m_netSentGraph->setAutoScale(true);
    m_netSentGraph->setMaxValue(100.0 * 1024.0);
    m_netSentGraph->setMinimumHeight(100);
}

void TaskManagerWindow::buildDetailsTab() {
    m_detailsTable = new QTableView;
    m_detailsTable->setModel(m_filterProxy);
    m_detailsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_detailsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_detailsTable->setAlternatingRowColors(true);
    m_detailsTable->setSortingEnabled(true);
    m_detailsTable->verticalHeader()->hide();
    m_detailsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_detailsTable->horizontalHeader()->setDefaultSectionSize(78);
    m_detailsTable->horizontalHeader()->setSortIndicatorShown(true);
    m_detailsTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_detailsTable, &QTableView::customContextMenuRequested,
            this, &TaskManagerWindow::onDetailsContextMenu);
}

void TaskManagerWindow::buildServicesTab() {
    enum Col { ColName, ColDisplay, ColStatus, ColStartType, ColPID, ColCount };

    const bool firstBuild = (m_servicesModel == nullptr);

    if (firstBuild) {
        m_servicesModel = new QStandardItemModel(this);
        m_servicesModel->setHorizontalHeaderLabels({
            QStringLiteral("Service Name"),
            QStringLiteral("Display Name"),
            QStringLiteral("Status"),
            QStringLiteral("Start Type"),
            QStringLiteral("PID")
        });

        m_servicesProxy = new QSortFilterProxyModel(this);
        m_servicesProxy->setSourceModel(m_servicesModel);
        m_servicesProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_servicesProxy->setFilterKeyColumn(-1);

        m_servicesTable = new QTableView;
        m_servicesTable->setModel(m_servicesProxy);
        m_servicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_servicesTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_servicesTable->setAlternatingRowColors(true);
        m_servicesTable->setSortingEnabled(true);
        m_servicesTable->verticalHeader()->hide();
        m_servicesTable->horizontalHeader()->setSectionResizeMode(ColDisplay, QHeaderView::Stretch);
        m_servicesTable->horizontalHeader()->setDefaultSectionSize(110);
        m_servicesTable->horizontalHeader()->setSortIndicatorShown(true);
        m_servicesTable->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(m_servicesTable, &QTableView::customContextMenuRequested, this,
                [this](const QPoint& pos) {
            const QModelIndex idx = m_servicesTable->indexAt(pos);
            if (!idx.isValid()) return;

            const QModelIndex srcIdx = m_servicesProxy->mapToSource(idx);
            const QString svcName = m_servicesModel->item(srcIdx.row(), ColName)->text();
            const QString status  = m_servicesModel->item(srcIdx.row(), ColStatus)->text();

            QMenu menu;
            auto* startAct   = menu.addAction(QStringLiteral("▶ Start"));
            auto* stopAct    = menu.addAction(QStringLiteral("■ Stop"));
            auto* restartAct = menu.addAction(QStringLiteral("↻ Restart"));
            menu.addSeparator();
            menu.addAction(QStringLiteral("Copy Service Name"), [svcName]() {
                QApplication::clipboard()->setText(svcName);
            });

            startAct->setEnabled(status != QStringLiteral("Running"));
            stopAct->setEnabled(status == QStringLiteral("Running"));
            restartAct->setEnabled(status == QStringLiteral("Running"));

            connect(startAct, &QAction::triggered, this, [this, svcName]() {
#if defined(Q_OS_WIN)
                if (controlService(svcName, 0)) {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                        QStringLiteral("Started service: %1").arg(svcName));
                } else {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                        QStringLiteral("Failed to start service: %1 (may require elevation)").arg(svcName));
                    QMessageBox::warning(this, QStringLiteral("Service Control"),
                        QStringLiteral("Failed to start '%1'. Administrator privileges may be required.").arg(svcName));
                }
                buildServicesTab();
#endif
            });

            connect(stopAct, &QAction::triggered, this, [this, svcName]() {
#if defined(Q_OS_WIN)
                if (controlService(svcName, SERVICE_CONTROL_STOP)) {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                        QStringLiteral("Stopped service: %1").arg(svcName));
                } else {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                        QStringLiteral("Failed to stop service: %1 (may require elevation)").arg(svcName));
                    QMessageBox::warning(this, QStringLiteral("Service Control"),
                        QStringLiteral("Failed to stop '%1'. Administrator privileges may be required.").arg(svcName));
                }
                buildServicesTab();
#endif
            });

            connect(restartAct, &QAction::triggered, this, [this, svcName]() {
#if defined(Q_OS_WIN)
                controlService(svcName, SERVICE_CONTROL_STOP);

                QTimer::singleShot(1500, this, [this, svcName]() {
                    controlService(svcName, 0);
                    buildServicesTab();
                });
#endif
            });

            menu.exec(m_servicesTable->viewport()->mapToGlobal(pos));
        });
    }

    m_servicesModel->removeRows(0, m_servicesModel->rowCount());

    const auto services = enumerateServices();
    for (const auto& svc : services) {
        QList<QStandardItem*> row;
        row.reserve(ColCount);

        auto* nameItem = new QStandardItem(svc.name);
        nameItem->setEditable(false);
        row.append(nameItem);

        auto* displayItem = new QStandardItem(svc.displayName);
        displayItem->setEditable(false);
        row.append(displayItem);

        auto* statusItem = new QStandardItem(svc.status);
        statusItem->setEditable(false);
        if (svc.status == QStringLiteral("Running"))
            statusItem->setForeground(QColor(0x4c, 0xaf, 0x50));
        else if (svc.status == QStringLiteral("Stopped"))
            statusItem->setForeground(QColor(0xf4, 0x43, 0x36));
        row.append(statusItem);

        auto* startItem = new QStandardItem(svc.startType);
        startItem->setEditable(false);
        row.append(startItem);

        auto* pidItem = new QStandardItem(svc.pid > 0 ? QString::number(svc.pid) : QString{});
        pidItem->setEditable(false);
        pidItem->setData(static_cast<qlonglong>(svc.pid), Qt::EditRole + 1);
        row.append(pidItem);

        m_servicesModel->appendRow(row);
    }

    if (firstBuild) {
        m_servicesTable->sortByColumn(ColDisplay, Qt::AscendingOrder);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Enumerated %1 Windows services.").arg(services.size()));
}

void TaskManagerWindow::buildStartupTab() {
    enum Col { ColName, ColPublisher, ColStatus, ColCommand, ColCount };

    const bool firstBuild = (m_startupModel == nullptr);

    if (firstBuild) {
        m_startupModel = new QStandardItemModel(this);
        m_startupModel->setHorizontalHeaderLabels({
            QStringLiteral("Name"),
            QStringLiteral("Publisher"),
            QStringLiteral("Status"),
            QStringLiteral("Command")
        });

        m_startupProxy = new QSortFilterProxyModel(this);
        m_startupProxy->setSourceModel(m_startupModel);
        m_startupProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_startupProxy->setFilterKeyColumn(-1);

        m_startupTable = new QTableView;
        m_startupTable->setModel(m_startupProxy);
        m_startupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_startupTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_startupTable->setAlternatingRowColors(true);
        m_startupTable->setSortingEnabled(true);
        m_startupTable->verticalHeader()->hide();
        m_startupTable->horizontalHeader()->setSectionResizeMode(ColCommand, QHeaderView::Stretch);
        m_startupTable->horizontalHeader()->setDefaultSectionSize(130);
        m_startupTable->horizontalHeader()->setSortIndicatorShown(true);
        m_startupTable->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(m_startupTable, &QTableView::customContextMenuRequested, this,
                [this](const QPoint& pos) {
            const QModelIndex idx = m_startupTable->indexAt(pos);
            if (!idx.isValid()) return;

            const QModelIndex srcIdx = m_startupProxy->mapToSource(idx);
            const int row = srcIdx.row();
            const QString name = m_startupModel->item(row, ColName)->text();
            const QString status = m_startupModel->item(row, ColStatus)->text();
            const QString command = m_startupModel->item(row, ColCommand)->text();
            const bool isHKLM = m_startupModel->item(row, ColName)->data(Qt::UserRole + 1).toBool();
            const bool currentlyEnabled = (status == QStringLiteral("Enabled"));

            QMenu menu;
            auto* toggleAct = menu.addAction(currentlyEnabled
                ? QStringLiteral("Disable")
                : QStringLiteral("Enable"));
            menu.addSeparator();
            auto* openLocAct = menu.addAction(QStringLiteral("Open File Location"));
            menu.addSeparator();
            menu.addAction(QStringLiteral("Copy Name"), [name]() {
                QApplication::clipboard()->setText(name);
            });
            menu.addAction(QStringLiteral("Copy Command"), [command]() {
                QApplication::clipboard()->setText(command);
            });

            connect(toggleAct, &QAction::triggered, this, [this, name, isHKLM, currentlyEnabled]() {
#if defined(Q_OS_WIN)
                const bool newState = !currentlyEnabled;
                if (setStartupEnabled(name, isHKLM, newState)) {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                        QStringLiteral("%1 startup item: %2").arg(newState ? QStringLiteral("Enabled") : QStringLiteral("Disabled"), name));
                } else {
                    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                        QStringLiteral("Failed to toggle startup item: %1 (may require elevation)").arg(name));
                    QMessageBox::warning(this, QStringLiteral("Startup Control"),
                        QStringLiteral("Failed to change '%1'. Administrator privileges may be required.").arg(name));
                }
                buildStartupTab();
#endif
            });

            connect(openLocAct, &QAction::triggered, this, [command]() {
                const QString exePath = extractExePath(command);
                const QFileInfo fi(exePath);
                if (fi.exists()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
                }
            });

            menu.exec(m_startupTable->viewport()->mapToGlobal(pos));
        });
    }

    m_startupModel->removeRows(0, m_startupModel->rowCount());

    const auto entries = enumerateStartupEntries();
    for (const auto& entry : entries) {
        QList<QStandardItem*> row;
        row.reserve(ColCount);

        auto* nameItem = new QStandardItem(entry.name);
        nameItem->setEditable(false);
        nameItem->setData(entry.isHKLM, Qt::UserRole + 1);
        row.append(nameItem);

        auto* pubItem = new QStandardItem(entry.publisher.isEmpty() ? QStringLiteral("Unknown") : entry.publisher);
        pubItem->setEditable(false);
        row.append(pubItem);

        auto* statusItem = new QStandardItem(entry.enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled"));
        statusItem->setEditable(false);
        if (entry.enabled)
            statusItem->setForeground(QColor(0x4c, 0xaf, 0x50));
        else
            statusItem->setForeground(QColor(0xf4, 0x43, 0x36));
        row.append(statusItem);

        auto* cmdItem = new QStandardItem(entry.command);
        cmdItem->setEditable(false);
        row.append(cmdItem);

        m_startupModel->appendRow(row);
    }

    if (firstBuild) {
        m_startupTable->sortByColumn(ColName, Qt::AscendingOrder);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Enumerated %1 startup entries.").arg(entries.size()));
}

void TaskManagerWindow::buildStatusBar() {
    m_statusProcesses = new QLabel(QStringLiteral("Processes: —"));
    m_statusThreads   = new QLabel(QStringLiteral("Threads: —"));
    m_statusCpu       = new QLabel(QStringLiteral("CPU: —"));
    m_statusGpu       = new QLabel(QStringLiteral("GPU: —"));
    m_statusMemory    = new QLabel(QStringLiteral("Memory: —"));
}

void TaskManagerWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    using wintools::themes::FluentStyle;

    const QString supplement = QStringLiteral(
        "QDialog, QWidget { background-color: %1; color: %2; }"
        "QTabWidget::pane { border: 1px solid %3; background: %1; }"
        "QTabBar::tab { background: %4; color: %5; padding: 6px 18px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: %1; color: %2; border-bottom: 2px solid %6; }"
        "QGroupBox::title { color: %6; }"
        "QMenu { background: %4; color: %2; border: 1px solid %3; }"
        "QMenu::item:selected { background: %7; }"
        "QFrame[frameShape=\"4\"] { color: %3; }"
    ).arg(
        palette.windowBackground.name(),
        palette.foreground.name(),
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.mutedForeground.name(),
        palette.accent.name(),
        palette.hoverBackground.name());

    wintools::themes::ThemeHelper::applyThemeTo(this, supplement);

    const QColor chartBackground = palette.cardBackground;
    QColor chartGrid = palette.cardBorder;
    chartGrid.setAlpha(160);
    QColor chartText = palette.foreground;
    QColor chartSubtle = palette.mutedForeground;
    QColor chartCrosshair = palette.foreground;
    chartCrosshair.setAlpha(90);

    auto applyGraphPalette = [&](PerfGraphWidget* graph) {
        if (!graph) return;
        graph->setBackground(chartBackground);
        graph->setGridColor(chartGrid);
        graph->setTextColor(chartText);
        graph->setSubtleTextColor(chartSubtle);
        graph->setCrosshairColor(chartCrosshair);
    };

    applyGraphPalette(m_cpuGraph);
    applyGraphPalette(m_gpuGraph);
    applyGraphPalette(m_memGraph);
    applyGraphPalette(m_netRecvGraph);
    applyGraphPalette(m_netSentGraph);
    for (auto it = m_diskGraphs.begin(); it != m_diskGraphs.end(); ++it) {
        applyGraphPalette(it.value().graph);
    }

    ProfilerOverlayController::instance()->applyTheme(palette);
}

void TaskManagerWindow::createDiskGraph(const QString& driveLetter,
                                         const QString& volumeLabel) {
    if (m_diskGraphs.contains(driveLetter) || !m_diskContainerLayout) return;

    QString title = volumeLabel.isEmpty()
        ? QStringLiteral("Disk %1:").arg(driveLetter)
        : QStringLiteral("Disk %1: (%2)").arg(driveLetter, volumeLabel);
    auto* group = new QGroupBox(title);
    auto* lay   = new QVBoxLayout(group);

    auto* graph = new PerfGraphWidget;
    graph->setTitle(QStringLiteral("I/O"));
    graph->setLineColor(QColor(0xF3, 0x9C, 0x12));
    graph->setValueFormat(PerfGraphWidget::ValueFormat::BytesPerSec);
    graph->setAutoScale(true);
    graph->setMaxValue(1024.0 * 1024.0);
    graph->setMinimumHeight(100);
    lay->addWidget(graph);

    auto* statsRow = new QHBoxLayout;
    auto* lblRead  = new QLabel(QStringLiteral("Read: \u2014"));
    auto* lblWrite = new QLabel(QStringLiteral("Write: \u2014"));
    auto* lblSpace = new QLabel(QStringLiteral("Free: \u2014"));
    statsRow->addWidget(lblRead);
    statsRow->addWidget(lblWrite);
    statsRow->addStretch();
    statsRow->addWidget(lblSpace);
    lay->addLayout(statsRow);

    m_diskContainerLayout->addWidget(group);

    DiskGraphEntry entry;
    entry.group    = group;
    entry.graph    = graph;
    entry.lblRead  = lblRead;
    entry.lblWrite = lblWrite;
    entry.lblSpace = lblSpace;
    m_diskGraphs.insert(driveLetter, entry);

    applyTheme(wintools::themes::ThemeHelper::currentPalette());
}

void TaskManagerWindow::onProcessesUpdated(QVector<ProcessInfo> processes,
                                            SystemPerf           sysPerf) {
    m_lastSysPerf = sysPerf;

    m_treeModel->update(processes);

    for (int g = 0; g < 3; ++g) {
        QModelIndex grpIdx = m_treeModel->index(g, 0);
        if (!m_processTree->isExpanded(grpIdx))
            m_processTree->expand(grpIdx);
    }

    m_tableModel->update(processes);

    m_cpuGraph->addValue(sysPerf.cpuUsagePercent);
    m_gpuGraph->addValue(sysPerf.gpuUsagePercent);
    if (sysPerf.totalMemoryBytes > 0) {
        double memPct = 100.0 * static_cast<double>(sysPerf.usedMemoryBytes)
                              / static_cast<double>(sysPerf.totalMemoryBytes);
        m_memGraph->addValue(memPct);
    }

    if (sysPerf.cpuFreqMHz > 0.0) {
        double ghz = sysPerf.cpuFreqMHz / 1000.0;
        m_lblCpuUsage->setText(QStringLiteral("CPU: %1%  (%2 GHz)")
            .arg(sysPerf.cpuUsagePercent, 0, 'f', 1)
            .arg(ghz, 0, 'f', 2));
    } else {
        m_lblCpuUsage->setText(QStringLiteral("CPU: %1%")
            .arg(sysPerf.cpuUsagePercent, 0, 'f', 1));
    }
    m_lblHandles->setText(QStringLiteral("Handles: %1")
        .arg(sysPerf.handleCount));
    m_lblProcessCount->setText(QStringLiteral("Processes: %1")
        .arg(sysPerf.processCount));
    m_lblThreadCount->setText(QStringLiteral("Threads: %1")
        .arg(sysPerf.threadCount));

    {
        quint64 up = static_cast<quint64>(sysPerf.uptimeSeconds);
        quint64 d  = up / 86400;
        quint64 h  = (up % 86400) / 3600;
        quint64 m  = (up % 3600) / 60;
        quint64 s  = up % 60;
        QString uptimeStr;
        if (d > 0)
            uptimeStr = QStringLiteral("%1d %2:%3:%4")
                .arg(d).arg(h, 2, 10, QLatin1Char('0'))
                .arg(m, 2, 10, QLatin1Char('0'))
                .arg(s, 2, 10, QLatin1Char('0'));
        else
            uptimeStr = QStringLiteral("%1:%2:%3")
                .arg(h, 2, 10, QLatin1Char('0'))
                .arg(m, 2, 10, QLatin1Char('0'))
                .arg(s, 2, 10, QLatin1Char('0'));
        m_lblUptime->setText(QStringLiteral("Up: %1").arg(uptimeStr));
    }

    auto fmtGB = [](quint64 b) -> QString {
        return QStringLiteral("%1 GB").arg(
            static_cast<double>(b) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    };
    m_lblMemUsage->setText(QStringLiteral("In Use: %1")
        .arg(fmtGB(sysPerf.usedMemoryBytes)));
    m_lblMemDetails->setText(QStringLiteral("Available: %1  /  Total: %2")
        .arg(fmtGB(sysPerf.availMemoryBytes)).arg(fmtGB(sysPerf.totalMemoryBytes)));

    m_statusProcesses->setText(QStringLiteral("Processes: %1")
        .arg(sysPerf.processCount));
    m_statusThreads->setText(QStringLiteral("Threads: %1")
        .arg(sysPerf.threadCount));
    m_statusCpu->setText(QStringLiteral("CPU: %1%")
        .arg(sysPerf.cpuUsagePercent, 0, 'f', 1));
    m_statusGpu->setText(QStringLiteral("GPU: %1%")
        .arg(sysPerf.gpuUsagePercent, 0, 'f', 1));
    if (sysPerf.totalMemoryBytes > 0) {
        double memPct = 100.0 * static_cast<double>(sysPerf.usedMemoryBytes)
                              / static_cast<double>(sysPerf.totalMemoryBytes);
        m_statusMemory->setText(QStringLiteral("Memory: %1%")
            .arg(memPct, 0, 'f', 0));
    }

    m_lblGpuUsage->setText(QStringLiteral("GPU: %1%")
        .arg(sysPerf.gpuUsagePercent, 0, 'f', 1));
    m_lblGpuGraphUsage->setText(QStringLiteral("Usage: %1%")
        .arg(sysPerf.gpuUsagePercent, 0, 'f', 1));

    {
        struct GpuEntry { QString name; quint32 pid; double gpu; };
        QVector<GpuEntry> gpuEntries;
        gpuEntries.reserve(processes.size());
        for (const auto& p : processes) {
            if (p.gpuPercent > 0.01)
                gpuEntries.append({p.name, p.pid, p.gpuPercent});
        }
        std::sort(gpuEntries.begin(), gpuEntries.end(),
                  [](const GpuEntry& a, const GpuEntry& b) { return a.gpu > b.gpu; });
        constexpr int kMaxRows = 8;
        int rowCount = qMin(static_cast<int>(gpuEntries.size()), kMaxRows);
        m_gpuProcessTable->setRowCount(rowCount);
        for (int i = 0; i < rowCount; ++i) {
            const auto& e = gpuEntries[i];
            auto* nameItem = new QTableWidgetItem(e.name);
            auto* pidItem  = new QTableWidgetItem(QString::number(e.pid));
            pidItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* gpuItem  = new QTableWidgetItem(
                QStringLiteral("%1%").arg(e.gpu, 0, 'f', 1));
            gpuItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_gpuProcessTable->setItem(i, 0, nameItem);
            m_gpuProcessTable->setItem(i, 1, pidItem);
            m_gpuProcessTable->setItem(i, 2, gpuItem);
        }
    }

    m_procUsageSummary->setText(QStringLiteral("CPU: %1%  |  GPU: %2%")
        .arg(sysPerf.cpuUsagePercent, 0, 'f', 1)
        .arg(sysPerf.gpuUsagePercent, 0, 'f', 1));

    auto fmtRate = [](quint64 bps) -> QString {
        if (bps >= 1024ULL * 1024 * 1024)
            return QStringLiteral("%1 GB/s").arg(bps / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
        if (bps >= 1024ULL * 1024)
            return QStringLiteral("%1 MB/s").arg(bps / (1024.0 * 1024.0), 0, 'f', 1);
        if (bps >= 1024ULL)
            return QStringLiteral("%1 KB/s").arg(bps / 1024.0, 0, 'f', 0);
        return QStringLiteral("%1 B/s").arg(bps);
    };

    for (const auto& dp : sysPerf.disks) {
        if (!m_diskGraphs.contains(dp.driveLetter))
            createDiskGraph(dp.driveLetter, dp.volumeLabel);

        auto it = m_diskGraphs.find(dp.driveLetter);
        if (it == m_diskGraphs.end() || !it->graph) continue;

        auto& entry = *it;
        entry.graph->addValue(static_cast<double>(dp.readBytesPerSec
                                                  + dp.writeBytesPerSec));
        entry.lblRead->setText(QStringLiteral("Read: %1").arg(fmtRate(dp.readBytesPerSec)));
        entry.lblWrite->setText(QStringLiteral("Write: %1").arg(fmtRate(dp.writeBytesPerSec)));
        if (dp.totalBytes > 0) {
            entry.lblSpace->setText(
                QStringLiteral("Free: %1 / %2")
                    .arg(fmtGB(dp.freeBytes)).arg(fmtGB(dp.totalBytes)));
        }
    }

    m_netRecvGraph->addValue(static_cast<double>(sysPerf.network.recvBytesPerSec));
    m_netSentGraph->addValue(static_cast<double>(sysPerf.network.sentBytesPerSec));
    m_lblNetRecv->setText(QStringLiteral("Recv: %1").arg(fmtRate(sysPerf.network.recvBytesPerSec)));
    m_lblNetSent->setText(QStringLiteral("Sent: %1").arg(fmtRate(sysPerf.network.sentBytesPerSec)));

}

quint32 TaskManagerWindow::treeSelectedPid() const {
    const auto indexes = m_processTree->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return 0;
    const QModelIndex& idx = indexes.first();
    if (idx.data(IsGroupRole).toBool()) return 0;
    return idx.data(PidRole).toUInt();
}

quint32 TaskManagerWindow::detailsSelectedPid() const {
    const auto rows = m_detailsTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) return 0;
    return rows.first().data(PidRole).toUInt();
}

void TaskManagerWindow::onTreeContextMenu(const QPoint& pos) {
    quint32 pid = treeSelectedPid();
    if (!pid) return;

    const QModelIndex idx = m_processTree->indexAt(pos);
    const ProcessInfo* pi = m_treeModel->infoAt(idx);

    QMenu menu(this);
    menu.addAction(QStringLiteral("End Task"),           this, &TaskManagerWindow::endSelectedTask);
    menu.addAction(QStringLiteral("End Task Tree"),      this, &TaskManagerWindow::endSelectedTaskTree);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Suspend"),            this, &TaskManagerWindow::suspendSelectedTask);
    menu.addAction(QStringLiteral("Resume"),             this, &TaskManagerWindow::resumeSelectedTask);
    menu.addSeparator();

    QMenu* priMenu = menu.addMenu(QStringLiteral("Set Priority"));
    struct { const char* label; int cls; } priorities[] = {
        { "Idle",         0x00000040 },
        { "Below Normal", 0x00004000 },
        { "Normal",       0x00000020 },
        { "Above Normal", 0x00008000 },
        { "High",         0x00000080 },
        { "Realtime",     0x00000100 },
    };
    for (auto& pr : priorities) {
        QAction* a = priMenu->addAction(QLatin1String(pr.label));
        int cls = pr.cls;
        connect(a, &QAction::triggered, this, [this, pid, cls]() {
            m_monitor->setPriority(pid, cls);
        });
    }

    menu.addSeparator();

    if (pi && !pi->exePath.isEmpty()) {
        menu.addAction(QStringLiteral("Open File Location"),
                       this, &TaskManagerWindow::openFileLocation);
    }
    menu.addAction(QStringLiteral("Copy PID"), this, [pid]() {
        QApplication::clipboard()->setText(QString::number(pid));
    });
    if (pi) {
        menu.addAction(QStringLiteral("Copy Name"), this, [pi]() {
            QApplication::clipboard()->setText(pi->name);
        });
    }

    menu.exec(m_processTree->viewport()->mapToGlobal(pos));
}

void TaskManagerWindow::onDetailsContextMenu(const QPoint& pos) {
    quint32 pid = detailsSelectedPid();
    if (!pid) return;

    const QModelIndex proxyIdx = m_detailsTable->indexAt(pos);
    const QModelIndex srcIdx   = m_filterProxy->mapToSource(proxyIdx);
    const ProcessInfo* pi = m_tableModel->infoAt(srcIdx.row());

    QMenu menu(this);
    menu.addAction(QStringLiteral("End Process"), this, [this, pid]() {
        if (QMessageBox::question(this, QStringLiteral("End Process"),
            QStringLiteral("Forcibly end process %1?").arg(pid),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_monitor->endProcess(pid);
        }
    });
    menu.addAction(QStringLiteral("End Process Tree"), this, [this, pid]() {
        if (QMessageBox::question(this, QStringLiteral("End Process Tree"),
            QStringLiteral("Terminate process %1 and all its children?").arg(pid),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_monitor->endProcessTree(pid);
        }
    });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Suspend"),  this, [this, pid]() { m_monitor->suspendProcess(pid); });
    menu.addAction(QStringLiteral("Resume"),   this, [this, pid]() { m_monitor->resumeProcess(pid); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Copy PID"), this, [pid]() {
        QApplication::clipboard()->setText(QString::number(pid));
    });
    if (pi) {
        menu.addAction(QStringLiteral("Copy Name"), this, [pi]() {
            QApplication::clipboard()->setText(pi->name);
        });
        if (!pi->exePath.isEmpty()) {
            const QString exePath = pi->exePath;
            menu.addAction(QStringLiteral("Open File Location"), this, [exePath]() {
                const QFileInfo fi(exePath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
            });
        }
    }

    menu.exec(m_detailsTable->viewport()->mapToGlobal(pos));
}

void TaskManagerWindow::endSelectedTask() {
    quint32 pid = treeSelectedPid();
    if (!pid) return;
    if (QMessageBox::question(this, QStringLiteral("End Task"),
            QStringLiteral("Forcibly end PID %1?").arg(pid),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("User requested End Task."),
            QStringLiteral("PID=%1").arg(pid));
        const QString err = m_monitor->endProcess(pid);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("End Task Failed"), err);
        }
    }
}

void TaskManagerWindow::endSelectedTaskTree() {
    quint32 pid = treeSelectedPid();
    if (!pid) return;
    if (QMessageBox::question(this, QStringLiteral("End Task Tree"),
            QStringLiteral("Terminate PID %1 and all its descendants?").arg(pid),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("User requested End Task Tree."),
            QStringLiteral("PID=%1").arg(pid));
        const QString err = m_monitor->endProcessTree(pid);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("End Task Tree"), err);
        }
    }
}

void TaskManagerWindow::suspendSelectedTask() {
    quint32 pid = treeSelectedPid();
    if (pid) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Suspending process."),
            QStringLiteral("PID=%1").arg(pid));
        m_monitor->suspendProcess(pid);
    }
}

void TaskManagerWindow::resumeSelectedTask() {
    quint32 pid = treeSelectedPid();
    if (pid) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Resuming process."),
            QStringLiteral("PID=%1").arg(pid));
        m_monitor->resumeProcess(pid);
    }
}

void TaskManagerWindow::openFileLocation() {
    const QModelIndex idx = m_processTree->currentIndex();
    const ProcessInfo* pi = m_treeModel->infoAt(idx);
    if (!pi || pi->exePath.isEmpty()) return;
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(pi->exePath).absolutePath()));
}

void TaskManagerWindow::onSearchTextChanged(const QString& text) {

    if (text.isEmpty()) return;

    for (int g = 0; g < 3; ++g) {
        QModelIndex grpIdx = m_treeModel->index(g, 0);
        int childCount = m_treeModel->rowCount(grpIdx);
        for (int r = 0; r < childCount; ++r) {
            QModelIndex child = m_treeModel->index(r, 0, grpIdx);
            QString name = child.data(Qt::DisplayRole).toString();
            if (name.contains(text, Qt::CaseInsensitive)) {
                m_processTree->scrollTo(child, QAbstractItemView::PositionAtCenter);
                m_processTree->setCurrentIndex(child);
                return;
            }
        }
    }
}

void TaskManagerWindow::onTabChanged(int index) {
    m_currentTab = index;
}

void TaskManagerWindow::togglePerfView() {
    m_tileView = !m_tileView;
    if (m_tileView) {

        m_perfScrollLayout->removeWidget(m_cpuGroup);
        m_perfScrollLayout->removeWidget(m_gpuGroup);
        m_perfScrollLayout->removeWidget(m_memGroup);
        m_perfScrollLayout->removeWidget(m_diskContainerWidget);
        m_perfScrollLayout->removeWidget(m_netGroup);

        m_perfTileGrid->addWidget(m_cpuGroup,            0, 0);
        m_perfTileGrid->addWidget(m_gpuGroup,            0, 1);
        m_perfTileGrid->addWidget(m_memGroup,            1, 0);
        m_perfTileGrid->addWidget(m_diskContainerWidget, 1, 1);
        m_perfTileGrid->addWidget(m_netGroup,            2, 0, 1, 2);

        m_cpuGraph->setMinimumHeight(80);
        m_gpuGraph->setMinimumHeight(80);
        m_memGraph->setMinimumHeight(80);
        m_netRecvGraph->setMinimumHeight(60);
        m_netSentGraph->setMinimumHeight(60);
        for (auto& e : m_diskGraphs)
            if (e.graph) e.graph->setMinimumHeight(60);

        m_perfStack->setCurrentIndex(1);
        m_perfViewToggle->setText(QStringLiteral("Scroll View"));
    } else {

        m_perfTileGrid->removeWidget(m_cpuGroup);
        m_perfTileGrid->removeWidget(m_gpuGroup);
        m_perfTileGrid->removeWidget(m_memGroup);
        m_perfTileGrid->removeWidget(m_diskContainerWidget);
        m_perfTileGrid->removeWidget(m_netGroup);

        m_perfScrollLayout->insertWidget(0, m_cpuGroup);
        m_perfScrollLayout->insertWidget(1, m_gpuGroup);
        m_perfScrollLayout->insertWidget(2, m_memGroup);
        m_perfScrollLayout->insertWidget(3, m_diskContainerWidget);
        m_perfScrollLayout->insertWidget(4, m_netGroup);

        m_cpuGraph->setMinimumHeight(120);
        m_gpuGraph->setMinimumHeight(120);
        m_memGraph->setMinimumHeight(120);
        m_netRecvGraph->setMinimumHeight(100);
        m_netSentGraph->setMinimumHeight(100);
        for (auto& e : m_diskGraphs)
            if (e.graph) e.graph->setMinimumHeight(100);

        m_perfStack->setCurrentIndex(0);
        m_perfViewToggle->setText(QStringLiteral("Tile View"));
    }
}

void TaskManagerWindow::toggleProfilerOverlay(bool enabled) {
    auto* overlayController = ProfilerOverlayController::instance();
    if (enabled) {
        overlayController->showOverlay();
        if (m_overlayToggle) {
            m_overlayToggle->setText(QStringLiteral("Disable Overlay"));
        }
        if (m_overlaySettings) {
            m_overlaySettings->setEnabled(true);
        }
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            "Profiler overlay enabled.");
    } else {
        overlayController->hideOverlay();
        if (m_overlayToggle) {
            m_overlayToggle->setText(QStringLiteral("Enable Overlay"));
        }
        if (m_overlaySettings) {
            m_overlaySettings->setEnabled(false);
        }
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            "Profiler overlay disabled.");
    }
}

void TaskManagerWindow::showProfilerOverlayMenu() {
    if (!m_overlayMenu || !m_overlaySettings) {
        return;
    }

    const QPoint popupPos = m_overlaySettings->mapToGlobal(
        QPoint(0, m_overlaySettings->height()));
    m_overlayMenu->popup(popupPos);
}

}
