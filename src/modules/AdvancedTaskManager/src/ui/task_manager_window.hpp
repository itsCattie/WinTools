#pragma once

// AdvancedTaskManager: task manager window manages UI behavior and presentation.

#include <QDialog>
#include <QMap>
#include <QVector>

#include "modules/AdvancedTaskManager/src/core/process_info.hpp"

class QTabWidget;
class QTreeView;
class QTableView;
class QLineEdit;
class QPushButton;
class QLabel;
class QTimer;
class QMenu;
class QHeaderView;
class QSortFilterProxyModel;
class QGroupBox;
class QVBoxLayout;
class QGridLayout;
class QStackedWidget;

namespace wintools::taskmanager {

class ProcessMonitor;
class ProcessTreeModel;
class ProcessTableModel;
class ProcessFilterProxy;
class PerfGraphWidget;

class TaskManagerWindow : public QDialog {
    Q_OBJECT

public:
    explicit TaskManagerWindow(QWidget* parent = nullptr);
    ~TaskManagerWindow() override;

private slots:

    void onProcessesUpdated(QVector<wintools::taskmanager::ProcessInfo> processes,
                             wintools::taskmanager::SystemPerf           sysPerf);

    void onTreeContextMenu(const QPoint& pos);
    void onDetailsContextMenu(const QPoint& pos);
    void endSelectedTask();
    void endSelectedTaskTree();
    void suspendSelectedTask();
    void resumeSelectedTask();
    void openFileLocation();

    void onSearchTextChanged(const QString& text);

    void onTabChanged(int index);

    void togglePerfView();

private:
    void buildUi();
    void buildProcessesTab();
    void buildPerformanceTab();
    void buildDetailsTab();
    void buildStatusBar();
    void applyDarkTheme();

    void createDiskGraph(const QString& driveLetter,
                          const QString& volumeLabel = {});

    QString selectedPidInTree()    const;
    QString selectedPidInDetails() const;
    quint32 treeSelectedPid()    const;
    quint32 detailsSelectedPid() const;

    ProcessMonitor*    m_monitor      = nullptr;

    ProcessTreeModel*  m_treeModel    = nullptr;
    ProcessTableModel* m_tableModel   = nullptr;
    ProcessFilterProxy* m_filterProxy = nullptr;

    QTreeView*   m_processTree   = nullptr;
    QLineEdit*   m_treeSearch    = nullptr;
    QPushButton* m_btnEndTask    = nullptr;
    QPushButton* m_btnEndTree    = nullptr;
    QPushButton* m_btnSuspend    = nullptr;
    QPushButton* m_btnResume     = nullptr;

    PerfGraphWidget* m_cpuGraph  = nullptr;
    PerfGraphWidget* m_memGraph  = nullptr;
    QLabel* m_lblCpuUsage        = nullptr;
    QLabel* m_lblMemUsage        = nullptr;
    QLabel* m_lblMemDetails      = nullptr;
    QLabel* m_lblUptime          = nullptr;
    QLabel* m_lblHandles         = nullptr;
    QLabel* m_lblProcessCount    = nullptr;
    QLabel* m_lblThreadCount     = nullptr;

    struct DiskGraphEntry {
        QGroupBox*       group    = nullptr;
        PerfGraphWidget* graph    = nullptr;
        QLabel*          lblRead  = nullptr;
        QLabel*          lblWrite = nullptr;
        QLabel*          lblSpace = nullptr;
    };
    QMap<QString, DiskGraphEntry> m_diskGraphs;
    QVBoxLayout* m_diskContainerLayout   = nullptr;
    QWidget*     m_diskContainerWidget   = nullptr;

    PerfGraphWidget* m_netRecvGraph = nullptr;
    PerfGraphWidget* m_netSentGraph = nullptr;
    QLabel*          m_lblNetRecv   = nullptr;
    QLabel*          m_lblNetSent   = nullptr;

    QGroupBox*       m_cpuGroup   = nullptr;
    QGroupBox*       m_memGroup   = nullptr;
    QGroupBox*       m_netGroup   = nullptr;

    QStackedWidget*  m_perfStack        = nullptr;
    QVBoxLayout*     m_perfScrollLayout = nullptr;
    QGridLayout*     m_perfTileGrid     = nullptr;
    QPushButton*     m_perfViewToggle   = nullptr;
    bool             m_tileView         = false;

    QTableView* m_detailsTable   = nullptr;
    QLineEdit*  m_detailsSearch  = nullptr;

    QLabel* m_statusProcesses    = nullptr;
    QLabel* m_statusThreads      = nullptr;
    QLabel* m_statusCpu          = nullptr;
    QLabel* m_statusMemory       = nullptr;

    int m_currentTab = 0;

    SystemPerf m_lastSysPerf;
};

}
