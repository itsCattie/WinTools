#include "modules/AdvancedTaskManager/src/ui/task_manager_window.hpp"
#include "modules/AdvancedTaskManager/src/core/process_monitor.hpp"
#include "modules/AdvancedTaskManager/src/model/process_model.hpp"
#include "modules/AdvancedTaskManager/src/ui/perf_graph_widget.hpp"
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
#include <QApplication>
#include <QClipboard>
#include <QUrl>
#include <QFileInfo>

// AdvancedTaskManager: task manager window manages UI behavior and presentation.

namespace wintools::taskmanager {

static constexpr const char* kLog = "TaskManager/Window";

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
    applyDarkTheme();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "TaskManagerWindow opened.");
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
    m_perfViewToggle = new QPushButton(QStringLiteral("Tile View"));
    m_perfViewToggle->setFixedWidth(110);
    m_perfViewToggle->setToolTip(QStringLiteral("Switch to tile view to see all graphs at once"));
    perfHeader->addStretch();
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
    m_lblHandles  = new QLabel(QStringLiteral("Handles: —"));
    m_lblProcessCount = new QLabel(QStringLiteral("Processes: —"));
    m_lblThreadCount  = new QLabel(QStringLiteral("Threads: —"));
    m_lblUptime   = new QLabel(QStringLiteral("Uptime: —"));
    cpuStats->addWidget(m_lblCpuUsage);
    cpuStats->addStretch();
    cpuStats->addWidget(m_lblHandles);
    cpuStats->addWidget(m_lblProcessCount);
    cpuStats->addWidget(m_lblThreadCount);
    cpuStats->addWidget(m_lblUptime);
    cpuLay->addLayout(cpuStats);
    m_perfScrollLayout->addWidget(m_cpuGroup);

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

    auto* detailPage = new QWidget;
    auto* detailLay  = new QVBoxLayout(detailPage);
    detailLay->setContentsMargins(6, 6, 6, 4);
    detailLay->setSpacing(4);
    m_detailsSearch = new QLineEdit;
    m_detailsSearch->setPlaceholderText(QStringLiteral("Filter by name, PID, user…"));
    m_detailsSearch->setClearButtonEnabled(true);
    detailLay->addWidget(m_detailsSearch);
    detailLay->addWidget(m_detailsTable, 1);

    tabs->addTab(procPage,  QStringLiteral("Processes"));
    tabs->addTab(perfPage,  QStringLiteral("Performance"));
    tabs->addTab(detailPage, QStringLiteral("Details"));

    auto* statusBar = new QFrame;
    statusBar->setFrameShape(QFrame::StyledPanel);
    statusBar->setMaximumHeight(24);
    auto* statusLay = new QHBoxLayout(statusBar);
    statusLay->setContentsMargins(8, 2, 8, 2);
    statusLay->setSpacing(16);
    statusLay->addWidget(m_statusProcesses);
    statusLay->addWidget(m_statusThreads);
    statusLay->addWidget(m_statusCpu);
    statusLay->addWidget(m_statusMemory);
    statusLay->addStretch();
    root->addWidget(statusBar);

    connect(m_treeSearch, &QLineEdit::textChanged,
            this, &TaskManagerWindow::onSearchTextChanged);
    connect(m_detailsSearch, &QLineEdit::textChanged,
            this, [this](const QString& t){ m_filterProxy->setFilterText(t); });
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

void TaskManagerWindow::buildStatusBar() {
    m_statusProcesses = new QLabel(QStringLiteral("Processes: —"));
    m_statusThreads   = new QLabel(QStringLiteral("Threads: —"));
    m_statusCpu       = new QLabel(QStringLiteral("CPU: —"));
    m_statusMemory    = new QLabel(QStringLiteral("Memory: —"));
}

void TaskManagerWindow::applyDarkTheme() {
    setStyleSheet(QStringLiteral(R"(
QDialog, QWidget {
    background-color: #1e1e1e;
    color: #e0e0e0;
}
QTabWidget::pane {
    border: 1px solid #333;
    background: #1e1e1e;
}
QTabBar::tab {
    background: #2a2a2a;
    color: #aaa;
    padding: 6px 18px;
    margin-right: 2px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}
QTabBar::tab:selected {
    background: #1e1e1e;
    color: #ffffff;
    border-bottom: 2px solid #17B378;
}
QTreeView, QTableView {
    background: #1e1e1e;
    alternate-background-color: #252525;
    color: #e0e0e0;
    border: 1px solid #333;
    gridline-color: #2a2a2a;
    selection-background-color: #264f78;
    selection-color: #ffffff;
    outline: 0;
}
QHeaderView::section {
    background-color: #2d2d2d;
    color: #ccc;
    padding: 4px 8px;
    border: none;
    border-right: 1px solid #444;
    border-bottom: 1px solid #444;
}
QHeaderView::section:hover {
    background-color: #383838;
}
QLineEdit {
    background: #2a2a2a;
    color: #e0e0e0;
    border: 1px solid #444;
    border-radius: 4px;
    padding: 4px 8px;
}
QLineEdit:focus {
    border-color: #17B378;
}
QPushButton {
    background-color: #333;
    color: #e0e0e0;
    border: 1px solid #555;
    border-radius: 4px;
    padding: 4px 12px;
    min-width: 80px;
}
QPushButton:hover  { background-color: #3d3d3d; }
QPushButton:pressed { background-color: #264f78; }
QPushButton:disabled  { color: #555; border-color: #333; }
QGroupBox {
    border: 1px solid #3a3a3a;
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 4px;
    color: #ccc;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: #17B378;
}
QMenu {
    background: #252525;
    color: #e0e0e0;
    border: 1px solid #444;
}
QMenu::item:selected { background: #264f78; }
QScrollBar:vertical { background: #1e1e1e; width: 10px; }
QScrollBar::handle:vertical { background: #444; border-radius: 5px; min-height: 20px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QFrame[frameShape="4"] { color: #333; }
QLabel { color: #ccc; }
    )"));
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
    applyDarkTheme();

    DiskGraphEntry entry;
    entry.group    = group;
    entry.graph    = graph;
    entry.lblRead  = lblRead;
    entry.lblWrite = lblWrite;
    entry.lblSpace = lblSpace;
    m_diskGraphs.insert(driveLetter, entry);
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
    if (sysPerf.totalMemoryBytes > 0) {
        double memPct = 100.0 * static_cast<double>(sysPerf.usedMemoryBytes)
                              / static_cast<double>(sysPerf.totalMemoryBytes);
        m_statusMemory->setText(QStringLiteral("Memory: %1%")
            .arg(memPct, 0, 'f', 0));
    }

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
        m_perfScrollLayout->removeWidget(m_memGroup);
        m_perfScrollLayout->removeWidget(m_diskContainerWidget);
        m_perfScrollLayout->removeWidget(m_netGroup);

        m_perfTileGrid->addWidget(m_cpuGroup,            0, 0);
        m_perfTileGrid->addWidget(m_memGroup,            0, 1);
        m_perfTileGrid->addWidget(m_diskContainerWidget, 1, 0);
        m_perfTileGrid->addWidget(m_netGroup,            1, 1);

        m_cpuGraph->setMinimumHeight(80);
        m_memGraph->setMinimumHeight(80);
        m_netRecvGraph->setMinimumHeight(60);
        m_netSentGraph->setMinimumHeight(60);
        for (auto& e : m_diskGraphs)
            if (e.graph) e.graph->setMinimumHeight(60);

        m_perfStack->setCurrentIndex(1);
        m_perfViewToggle->setText(QStringLiteral("Scroll View"));
    } else {

        m_perfTileGrid->removeWidget(m_cpuGroup);
        m_perfTileGrid->removeWidget(m_memGroup);
        m_perfTileGrid->removeWidget(m_diskContainerWidget);
        m_perfTileGrid->removeWidget(m_netGroup);

        m_perfScrollLayout->insertWidget(0, m_cpuGroup);
        m_perfScrollLayout->insertWidget(1, m_memGroup);
        m_perfScrollLayout->insertWidget(2, m_diskContainerWidget);
        m_perfScrollLayout->insertWidget(3, m_netGroup);

        m_cpuGraph->setMinimumHeight(120);
        m_memGraph->setMinimumHeight(120);
        m_netRecvGraph->setMinimumHeight(100);
        m_netSentGraph->setMinimumHeight(100);
        for (auto& e : m_diskGraphs)
            if (e.graph) e.graph->setMinimumHeight(100);

        m_perfStack->setCurrentIndex(0);
        m_perfViewToggle->setText(QStringLiteral("Tile View"));
    }
}

}
