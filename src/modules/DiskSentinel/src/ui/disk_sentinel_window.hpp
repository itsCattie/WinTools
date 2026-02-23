#pragma once

// DiskSentinel: disk sentinel window manages UI behavior and presentation.

#include "common/themes/window_colour.hpp"
#include "modules/disksentinel/src/core/disk_node.hpp"
#include "modules/disksentinel/src/scanner/disk_scanner.hpp"
#include "modules/disksentinel/src/model/storage_model.hpp"
#include "modules/disksentinel/src/ui/treemap_widget.hpp"

#include <QDialog>
#include <memory>

namespace wintools::themes { class ThemeListener; }

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeView;
class QSplitter;
class QScrollArea;
class QProgressBar;
class QFrame;
class QStyledItemDelegate;
class QTimer;

namespace wintools::disksentinel {

class DiskSentinelWindow : public QDialog {
    Q_OBJECT

public:
    explicit DiskSentinelWindow(QWidget* parent = nullptr);
    ~DiskSentinelWindow() override;

private slots:

    void onScanStarted(const QString& path);
    void onScanProgress(int files, qint64 bytes, const QString& currentPath);
    void onScanFinished(std::shared_ptr<wintools::disksentinel::DiskNode> root);
    void onScanCancelled();

    void navigateTo(DiskNode* node);
    void navigateUp();
    void navigatePath(const QString& path);
    void onTreeSelectionChanged();
    void onTreemapNodeClicked(DiskNode* node);

    void scanDrive(const QString& rootPath);
    void refreshDrives();

    void rescan();
    void reload();
    void toggleExpandCollapse();

private:
    void buildUi();
    void buildDrivePanel();
    void buildToolBar();
    void buildMainSplitter();
    void buildLegend();

    QFrame*  makeDriveCard(const QString& root,
                           const QString& label,
                           qint64 used, qint64 total);

    void setDisplayRoot(DiskNode* node);
    void updatePathBar(DiskNode* node);
    void setStatus(const QString& msg, bool busy = false);
    void applyTheme();

    QScrollArea*   m_driveScroll  = nullptr;
    QFrame*        m_driveBar     = nullptr;

    QPushButton*   m_upBtn        = nullptr;
    QLineEdit*     m_pathBar      = nullptr;
    QPushButton*   m_rescanBtn         = nullptr;
    QPushButton*   m_reloadBtn          = nullptr;
    QPushButton*   m_expandCollapseBtn  = nullptr;
    bool           m_treeExpanded       = false;
    QLabel*        m_statusLabel        = nullptr;
    QProgressBar*  m_scanProgress       = nullptr;

    QSplitter*     m_splitter     = nullptr;
    QTreeView*     m_treeView     = nullptr;
    TreemapWidget* m_treemap      = nullptr;

    QFrame*        m_legend       = nullptr;

    DiskScanner*                     m_scanner    = nullptr;
    StorageModel*                    m_model      = nullptr;

    std::shared_ptr<DiskNode>        m_scanRoot;

    DiskNode*                        m_displayRoot = nullptr;

    QString                          m_scanPath;

    wintools::themes::ThemePalette   m_palette;
    wintools::themes::ThemeListener* m_themeListener = nullptr;
};

}
