#include "modules/disksentinel/src/ui/disk_sentinel_window.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/color_utils.hpp"
#include "common/ui/screen_relative_size.hpp"
#include "logger/logger.hpp"

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QMouseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStorageInfo>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QTimer>
#include <QToolTip>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace wintools::disksentinel {

static constexpr const char* kLog = "DiskSentinel/Window";

QString normalizedPathKey(const QString& path) {
    return QDir::cleanPath(path).toLower();
}

QColor categoryBaseColor(const QString& category) {
    if (category == "image") return QColor(0x5b, 0x9b, 0xd5);
    if (category == "video") return QColor(0xc0, 0x39, 0x2b);
    if (category == "audio") return QColor(0x27, 0xae, 0x60);
    if (category == "document") return QColor(0xe6, 0xac, 0x00);
    if (category == "archive") return QColor(0x8e, 0x44, 0xad);
    if (category == "code") return QColor(0x16, 0xa0, 0x85);
    if (category == "executable") return QColor(0xe6, 0x7e, 0x22);
    return QColor(0x7f, 0x8c, 0x8d);
}

QColor categoryThemeColor(const QString& category,
                          const wintools::themes::ThemePalette& palette,
                          bool darkTheme) {
    const QColor base = categoryBaseColor(category);
    if (darkTheme) {
        return wintools::themes::blendColor(base, palette.windowBackground, 0.08f);
    }
    return wintools::themes::blendColor(base, palette.windowBackground, 0.32f);
}

class ShareBarDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        if (opt.state & QStyle::State_Selected) {
            painter->fillRect(opt.rect, opt.palette.highlight());
        } else {
            painter->fillRect(opt.rect, opt.backgroundBrush);
        }

        const double fraction = index.data(StorageModel::FractionRole).toDouble();
        const QRect  r        = opt.rect.adjusted(4, 3, -4, -3);

        QColor trackColor = opt.palette.base().color();
        if (trackColor.lightness() < 128) {
            trackColor = trackColor.lighter(150);
        } else {
            trackColor = trackColor.darker(106);
        }
        painter->fillRect(r, trackColor);

        const int barW = static_cast<int>(r.width() * qBound(0.0, fraction, 1.0));
        if (barW > 0) {
            const bool isDir = index.data(StorageModel::IsDirRole).toBool();
            QColor barColor = isDir ? opt.palette.highlight().color()
                                    : QColor(0x27, 0xae, 0x60);

            if (fraction > 0.5)  barColor = barColor.darker(100 + static_cast<int>(22 * fraction));
            if (fraction > 0.8)  barColor = QColor(0xc0, 0x39, 0x2b);
            painter->fillRect(QRect(r.x(), r.y(), barW, r.height()), barColor);
        }

        const QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(opt.state & QStyle::State_Selected
                        ? opt.palette.highlightedText().color()
                        : opt.palette.text().color());
        painter->drawText(r, Qt::AlignCenter, text);
    }
};

DiskSentinelWindow::DiskSentinelWindow(QWidget* parent)
    : QDialog(parent, Qt::Window)
    , m_scanner(new DiskScanner(this))
    , m_model(new StorageModel(this))
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setWindowTitle(QStringLiteral("DiskSentinel"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/disksentinel.svg")));
    setMinimumSize(1100, 720);
    resize(1280, 800);
    wintools::ui::enableRelativeSizeAcrossScreens(this);

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                   "DiskSentinelWindow created.");

    m_progressUiTimer.start();

    buildUi();
    applyTheme();

    connect(m_scanner, &DiskScanner::scanStarted,
            this, &DiskSentinelWindow::onScanStarted);
    connect(m_scanner, &DiskScanner::progressUpdate,
            this, &DiskSentinelWindow::onScanProgress);
    connect(m_scanner, &DiskScanner::scanFinished,
            this, &DiskSentinelWindow::onScanFinished);
    connect(m_scanner, &DiskScanner::scanCancelled,
            this, &DiskSentinelWindow::onScanCancelled);
    connect(m_scanner, &DiskScanner::scanStats,
            this, [](const ScanStats& s) {
                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                    QStringLiteral("Scan stats: %1 files, %2 dirs, %3 bytes, %4 ms")
                        .arg(s.files).arg(s.dirs)
                        .arg(DiskNode::prettySize(s.bytes))
                        .arg(s.elapsedMs));
            });

    m_themeListener = new wintools::themes::ThemeListener(this);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, [this](bool) {
                m_palette = wintools::themes::ThemeHelper::currentPalette();
                applyTheme();
            });
}

DiskSentinelWindow::~DiskSentinelWindow() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                   "DiskSentinelWindow closing – scan thread will self-terminate.");

}

void DiskSentinelWindow::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    buildDrivePanel();
    root->addWidget(m_driveScroll);

    buildToolBar();

    buildMainSplitter();
    root->addWidget(m_splitter, 1);

    buildLegend();
    root->addWidget(m_legend);
}

void DiskSentinelWindow::buildDrivePanel() {
    m_driveBar    = new QFrame(this);
    m_driveBar->setObjectName(QStringLiteral("driveBar"));
    m_driveBar->setMinimumHeight(96);
    m_driveBar->setMaximumHeight(96);

    auto* hbox = new QHBoxLayout(m_driveBar);
    hbox->setSpacing(8);
    hbox->setContentsMargins(8, 6, 8, 6);
    hbox->addStretch(1);

    m_driveScroll = new QScrollArea(this);
    m_driveScroll->setWidget(m_driveBar);
    m_driveScroll->setWidgetResizable(true);
    m_driveScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_driveScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_driveScroll->setFixedHeight(104);
    m_driveScroll->setFrameShape(QFrame::NoFrame);

    refreshDrives();
}

void DiskSentinelWindow::buildToolBar() {
    auto* bar   = new QFrame(this);
    bar->setObjectName(QStringLiteral("toolBar"));
    bar->setFixedHeight(42);
    auto* hbox  = new QHBoxLayout(bar);
    hbox->setContentsMargins(8, 4, 8, 4);
    hbox->setSpacing(6);

    m_upBtn = new QPushButton(QStringLiteral("⬆ Up"), this);
    m_upBtn->setFixedSize(64, 28);
    m_upBtn->setToolTip(QStringLiteral("Go up one directory"));
    m_upBtn->setEnabled(false);
    connect(m_upBtn, &QPushButton::clicked, this, &DiskSentinelWindow::navigateUp);

    m_pathBar = new QLineEdit(this);
    m_pathBar->setPlaceholderText(QStringLiteral("Select a drive or enter a path..."));
    m_pathBar->setReadOnly(false);
    connect(m_pathBar, &QLineEdit::returnPressed, this, [this]() {
        navigatePath(m_pathBar->text().trimmed());
    });

    m_rescanBtn = new QPushButton(QStringLiteral("⟳ Scan"), this);
    m_rescanBtn->setFixedSize(72, 28);
    m_rescanBtn->setToolTip(QStringLiteral("Scan the path entered above"));
    connect(m_rescanBtn, &QPushButton::clicked, this, &DiskSentinelWindow::rescan);

    m_reloadBtn = new QPushButton(QStringLiteral("↺ Reload"), this);
    m_reloadBtn->setFixedSize(80, 28);
    m_reloadBtn->setToolTip(QStringLiteral("Re-scan the current path to refresh all file sizes"));
    m_reloadBtn->setEnabled(false);
    connect(m_reloadBtn, &QPushButton::clicked, this, &DiskSentinelWindow::reload);

    m_expandCollapseBtn = new QPushButton(QStringLiteral("⊞ Expand All"), this);
    m_expandCollapseBtn->setFixedSize(100, 28);
    m_expandCollapseBtn->setToolTip(QStringLiteral("Expand or collapse all folders in the tree"));
    m_expandCollapseBtn->setEnabled(false);
    connect(m_expandCollapseBtn, &QPushButton::clicked,
            this, &DiskSentinelWindow::toggleExpandCollapse);

    m_exportBtn = new QPushButton(QStringLiteral("⬇ Export CSV"), this);
    m_exportBtn->setFixedSize(100, 28);
    m_exportBtn->setToolTip(QStringLiteral("Export the current scan results to a CSV file"));
    m_exportBtn->setEnabled(false);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &DiskSentinelWindow::exportCsv);

    m_findDupesBtn = new QPushButton(QStringLiteral("🔍 Duplicates"), this);
    m_findDupesBtn->setFixedSize(110, 28);
    m_findDupesBtn->setToolTip(QStringLiteral("Find duplicate files by content hash"));
    m_findDupesBtn->setEnabled(false);
    connect(m_findDupesBtn, &QPushButton::clicked,
            this, &DiskSentinelWindow::findDuplicates);

    m_statusLabel = new QLabel(QStringLiteral("Ready"), this);
    m_statusLabel->setMinimumWidth(200);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scanProgress = new QProgressBar(this);
    m_scanProgress->setRange(0, 0);
    m_scanProgress->setFixedWidth(140);
    m_scanProgress->setFixedHeight(18);
    m_scanProgress->setVisible(false);
    m_scanProgress->setTextVisible(false);

    hbox->addWidget(m_upBtn);
    hbox->addWidget(m_pathBar, 1);
    hbox->addWidget(m_rescanBtn);
    hbox->addWidget(m_reloadBtn);
    hbox->addWidget(m_expandCollapseBtn);
    hbox->addWidget(m_exportBtn);
    hbox->addWidget(m_findDupesBtn);
    hbox->addWidget(m_scanProgress);
    hbox->addWidget(m_statusLabel);

    auto* vbox = qobject_cast<QVBoxLayout*>(layout());
    if (vbox) vbox->addWidget(bar);
}

void DiskSentinelWindow::buildMainSplitter() {
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(StorageModel::ColSize, Qt::DescendingOrder);
    m_treeView->header()->setSectionResizeMode(StorageModel::ColName,
                                               QHeaderView::Stretch);
    m_treeView->header()->setSectionResizeMode(StorageModel::ColSize,
                                               QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(StorageModel::ColShare,
                                               QHeaderView::Fixed);
    m_treeView->header()->setSectionResizeMode(StorageModel::ColItems,
                                               QHeaderView::ResizeToContents);
    m_treeView->header()->resizeSection(StorageModel::ColShare, 120);
    m_treeView->setMinimumWidth(280);

    m_treeView->setItemDelegateForColumn(StorageModel::ColShare,
                                         new ShareBarDelegate(m_treeView));

    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &DiskSentinelWindow::onTreeContextMenu);

    connect(m_treeView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex&, const QModelIndex&) {
                onTreeSelectionChanged();
            });

    m_treemap = new TreemapWidget(this);
    m_treemap->setMinimumWidth(400);
    connect(m_treemap, &TreemapWidget::nodeClicked,
            this, &DiskSentinelWindow::onTreemapNodeClicked);
    connect(m_treemap, &TreemapWidget::nodeHovered, this, [this](DiskNode* node) {
        if (node)
            setStatus(QStringLiteral("%1  (%2)").arg(node->name,
                                                      DiskNode::prettySize(node->size)));
    });

    m_pieChart = new PieChartWidget(this);
    m_pieChart->setThemePalette(m_palette);

    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_treemap);
    m_rightSplitter->addWidget(m_pieChart);
    m_rightSplitter->setSizes({600, 200});
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 1);

    m_splitter->addWidget(m_treeView);
    m_splitter->addWidget(m_rightSplitter);
    m_splitter->setSizes({350, 900});
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
}

void DiskSentinelWindow::buildLegend() {
    m_legend = new QFrame(this);
    m_legend->setObjectName(QStringLiteral("legendBar"));
    m_legend->setFixedHeight(28);

    auto* hbox = new QHBoxLayout(m_legend);
    hbox->setContentsMargins(8, 4, 8, 4);
    hbox->setSpacing(12);

    struct Entry { QString cat; QString label; QColor col; };
    const QVector<Entry> entries = {
        { "image",      "Images",      {0x5b,0x9b,0xd5} },
        { "video",      "Video",       {0xc0,0x39,0x2b} },
        { "audio",      "Audio",       {0x27,0xae,0x60} },
        { "document",   "Documents",   {0xe6,0xac,0x00} },
        { "archive",    "Archives",    {0x8e,0x44,0xad} },
        { "code",       "Code",        {0x16,0xa0,0x85} },
        { "executable", "Executables", {0xe6,0x7e,0x22} },
        { "other",      "Other",       {0x7f,0x8c,0x8d} },
    };

    for (const auto& e : entries) {
        auto* swatch = new QFrame(this);
        swatch->setFixedSize(14, 14);
        swatch->setObjectName(QStringLiteral("legendSwatch"));
        swatch->setProperty("category", e.cat);
        swatch->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;")
                                .arg(e.col.name()));
        auto* lbl = new QLabel(e.label, this);
        lbl->setObjectName(QStringLiteral("legendLabel"));
        lbl->setProperty("category", e.cat);
        lbl->setStyleSheet(QStringLiteral("font-size:11px;"));
        hbox->addWidget(swatch);
        hbox->addWidget(lbl);
    }
    hbox->addStretch(1);
}

QFrame* DiskSentinelWindow::makeDriveCard(const QString& root,
                                           const QString& label,
                                           qint64 used, qint64 total) {
    auto* card = new QFrame(m_driveBar);
    card->setObjectName(QStringLiteral("driveCard"));
    card->setFixedSize(160, 82);
    card->setCursor(Qt::PointingHandCursor);
    card->setToolTip(root);

    auto* vbox = new QVBoxLayout(card);
    vbox->setContentsMargins(8, 6, 8, 6);
    vbox->setSpacing(3);

    auto* rootLbl = new QLabel(root.left(3), card);
    rootLbl->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:%1;")
                            .arg(m_palette.foreground.name()));

    auto* nameLbl = new QLabel(label.isEmpty()
                               ? QStringLiteral("Local Disk") : label, card);
    nameLbl->setStyleSheet(QStringLiteral("font-size:10px;color:%1;")
                            .arg(m_palette.mutedForeground.name()));
    nameLbl->setWordWrap(false);

    auto* bar = new QProgressBar(card);
    bar->setRange(0, 1000);
    bar->setValue(total > 0 ? static_cast<int>(used * 1000 / total) : 0);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);

    const double pct = total > 0 ? double(used) / double(total) : 0;
    QString barStyle = QStringLiteral("QProgressBar{background:%1;border-radius:4px;}"
                                       "QProgressBar::chunk{border-radius:4px;background:")
                                       .arg(m_palette.cardBorder.name());
    if (pct > 0.90)      barStyle += QStringLiteral("#c0392b");
    else if (pct > 0.75) barStyle += QStringLiteral("#e67e22");
    else                 barStyle += QStringLiteral("#27ae60");
    barStyle += QStringLiteral(";}");
    bar->setStyleSheet(barStyle);

    auto* sizeLbl = new QLabel(
        DiskNode::prettySize(used) + QStringLiteral(" / ") + DiskNode::prettySize(total), card);
    sizeLbl->setStyleSheet(QStringLiteral("font-size:10px;color:%1;")
                            .arg(m_palette.mutedForeground.name()));
    sizeLbl->setAlignment(Qt::AlignCenter);

    vbox->addWidget(rootLbl);
    vbox->addWidget(nameLbl);
    vbox->addWidget(bar);
    vbox->addWidget(sizeLbl);

    card->installEventFilter(this);
    card->setProperty("scanPath", root);

    auto handler = [this, root](QMouseEvent* e) {
        if (e->button() == Qt::LeftButton) scanDrive(root);
    };

    class ClickFilter : public QObject {
    public:
        std::function<void(QMouseEvent*)> fn;
        ClickFilter(QObject* p, std::function<void(QMouseEvent*)> f)
            : QObject(p), fn(std::move(f)) {}
        bool eventFilter(QObject*, QEvent* ev) override {
            if (ev->type() == QEvent::MouseButtonPress)
                fn(static_cast<QMouseEvent*>(ev));
            return false;
        }
    };
    card->installEventFilter(new ClickFilter(card, handler));

    return card;
}

void DiskSentinelWindow::refreshDrives() {

    auto* hbox = qobject_cast<QHBoxLayout*>(m_driveBar->layout());
    while (hbox->count() > 1)
        delete hbox->takeAt(0)->widget();

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    int inserted = 0;
    for (const QStorageInfo& vol : volumes) {
        if (!vol.isValid() || !vol.isReady()) continue;

        if (vol.rootPath().isEmpty()) continue;

        auto* card = makeDriveCard(
            vol.rootPath(),
            vol.displayName(),
            vol.bytesTotal() - vol.bytesFree(),
            vol.bytesTotal());
        hbox->insertWidget(inserted++, card);
    }

    if (inserted == 0) {
        auto* lbl = new QLabel(QStringLiteral("No drives found"), m_driveBar);
        hbox->insertWidget(0, lbl);
    }
}

void DiskSentinelWindow::scanDrive(const QString& rootPath) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Drive card clicked: '%1'").arg(rootPath));
    m_scanPath = rootPath;
    m_scanner->startScan(rootPath);
}

void DiskSentinelWindow::rescan() {
    const QString path = m_pathBar->text().trimmed();
    if (!path.isEmpty()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Rescan requested for path '%1'.").arg(path));
        m_scanPath = path;
        m_scanner->startScan(path);
    } else {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                                       "Rescan ignored: path bar is empty.");
    }
}

void DiskSentinelWindow::reload() {
    if (!m_scanPath.isEmpty()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Reload requested for '%1'.").arg(m_scanPath));
        m_scanner->clearCache();
        m_scanner->startScan(m_scanPath);
    } else {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                                       "Reload ignored: no previous scan path.");
    }
}

void DiskSentinelWindow::toggleExpandCollapse() {
    if (!m_model->root()) return;
    m_expandCollapseBtn->setEnabled(false);
    m_treeView->setUpdatesEnabled(false);

    if (m_treeExpanded) {

        const int topRows = m_model->rowCount(QModelIndex());
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Collapsing %1 top-level rows.").arg(topRows));
        for (int i = 0; i < topRows; ++i)
            m_treeView->collapse(m_model->index(i, 0));
        m_treeExpanded = false;
        m_expandCollapseBtn->setText(QStringLiteral("⊞ Expand All"));
        m_expandCollapseBtn->setToolTip(QStringLiteral("Expand all folders in the tree"));
    } else {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                       "Expanding all tree nodes.");

        m_treeView->expandAll();
        m_treeExpanded = true;
        m_expandCollapseBtn->setText(QStringLiteral("⊟ Collapse All"));
        m_expandCollapseBtn->setToolTip(QStringLiteral("Collapse all folders in the tree"));
    }

    m_treeView->setUpdatesEnabled(true);
    m_expandCollapseBtn->setEnabled(true);
}

void DiskSentinelWindow::onScanStarted(const QString& path) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Scan started: '%1'").arg(path));
    m_model->setRoot(nullptr);
    m_treemap->setRoot(nullptr);
    m_displayRoot = nullptr;
    m_pathBar->setText(path);
    m_upBtn->setEnabled(false);
    m_reloadBtn->setEnabled(false);
    m_expandCollapseBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);
    m_findDupesBtn->setEnabled(false);
    m_scanProgress->setVisible(true);
    m_lastProgressUiMs = 0;
    m_progressUiTimer.restart();
    setStatus(QStringLiteral("Scanning…"), true);
}

void DiskSentinelWindow::onScanProgress(int files, qint64 bytes,
                                         const QString& currentPath) {
    const qint64 nowMs = m_progressUiTimer.elapsed();
    if (nowMs - m_lastProgressUiMs < 120) {
        return;
    }
    m_lastProgressUiMs = nowMs;

    setStatus(QStringLiteral("Scanning: %1 files, %2 — %3")
              .arg(files).arg(DiskNode::prettySize(bytes))
              .arg(QDir(currentPath).dirName()));
}

void DiskSentinelWindow::onScanFinished(std::shared_ptr<DiskNode> root) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Scan finished: '%1', %2 total.")
            .arg(root ? root->path : QStringLiteral("(null)"),
                 DiskNode::prettySize(root ? root->size : 0)));
    m_scanRoot = std::move(root);
    rebuildPathIndex();
    m_model->setRoot(m_scanRoot);
    setDisplayRoot(m_scanRoot.get());
    m_scanProgress->setVisible(false);
    m_reloadBtn->setEnabled(true);
    m_expandCollapseBtn->setEnabled(true);
    m_exportBtn->setEnabled(true);
    m_findDupesBtn->setEnabled(true);

    m_treeExpanded = false;
    m_expandCollapseBtn->setText(QStringLiteral("⊞ Expand All"));
    m_treeView->expandToDepth(1);
    m_treeView->header()->setSectionResizeMode(StorageModel::ColSize,
                                               QHeaderView::ResizeToContents);
    setStatus(QStringLiteral("Scan complete — %1 total in %2")
              .arg(DiskNode::prettySize(m_scanRoot ? m_scanRoot->size : 0),
                   m_scanRoot ? m_scanRoot->path : QString{}));
}

void DiskSentinelWindow::onScanCancelled() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                                   "Scan cancelled — UI updated.");
    m_scanProgress->setVisible(false);
    setStatus(QStringLiteral("Scan cancelled."));
}

void DiskSentinelWindow::setDisplayRoot(DiskNode* node) {
    m_displayRoot = node;
    m_treemap->setRoot(node);
    m_pieChart->setRoot(node);
    updatePathBar(node);

    m_upBtn->setEnabled(node && node->parent != nullptr);
}

void DiskSentinelWindow::updatePathBar(DiskNode* node) {
    if (!node) return;
    m_pathBar->setText(node->path);
}

void DiskSentinelWindow::navigateTo(DiskNode* node) {
    if (!node) return;
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Navigate to: '%1'").arg(node->path));
    setDisplayRoot(node);

    const QModelIndex idx = m_model->indexForNode(node);
    if (idx.isValid()) {
        m_treeView->setCurrentIndex(idx);
        m_treeView->scrollTo(idx);
    }
}

void DiskSentinelWindow::navigateUp() {
    if (!m_displayRoot) return;
    DiskNode* par = m_displayRoot->parent;
    if (par) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Navigate up: '%1' → '%2'")
                .arg(m_displayRoot->path, par->path));
        setDisplayRoot(par);
    }
}

void DiskSentinelWindow::navigatePath(const QString& path) {
    if (path.isEmpty()) return;

    if (!m_pathIndex.isEmpty()) {
        auto it = m_pathIndex.constFind(normalizedPathKey(path));
        if (it != m_pathIndex.constEnd() && it.value()) {
            navigateTo(it.value());
            return;
        }
    }

    scanDrive(path);
}

void DiskSentinelWindow::rebuildPathIndex() {
    m_pathIndex.clear();
    if (!m_scanRoot) return;

    QVector<DiskNode*> stack;
    stack.reserve(4096);
    stack.push_back(m_scanRoot.get());

    while (!stack.isEmpty()) {
        DiskNode* node = stack.back();
        stack.pop_back();
        if (!node) continue;

        m_pathIndex.insert(normalizedPathKey(node->path), node);
        for (const auto& child : node->children) {
            if (child) {
                stack.push_back(child.get());
            }
        }
    }
}

void DiskSentinelWindow::onTreeSelectionChanged() {
    const QModelIndex idx = m_treeView->currentIndex().siblingAtColumn(0);
    if (!idx.isValid()) return;
    DiskNode* node = StorageModel::nodeForIndex(idx);
    if (node) {
        m_treemap->setRoot(node);
        updatePathBar(node);
        m_upBtn->setEnabled(node->parent != nullptr);
    }
}

void DiskSentinelWindow::onTreemapNodeClicked(DiskNode* node) {
    if (!node) return;
    if (node->isDir) {
        setDisplayRoot(node);

        const QModelIndex idx = m_model->indexForNode(node);
        if (idx.isValid()) {
            m_treeView->setCurrentIndex(idx);
            m_treeView->scrollTo(idx);
            m_treeView->expand(idx);
        }
    }
}

void DiskSentinelWindow::setStatus(const QString& msg, bool busy) {
    m_statusLabel->setText(msg);
    Q_UNUSED(busy);
}

void DiskSentinelWindow::onTreeContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_treeView->indexAt(pos);
    if (!idx.isValid()) return;

    DiskNode* node = StorageModel::nodeForIndex(idx.siblingAtColumn(0));
    if (!node) return;

    QMenu menu(this);

    menu.addAction(QStringLiteral("Open in Explorer"), this, [node]() {
        const QString target = node->isDir ? node->path
                                           : QFileInfo(node->path).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    });

    menu.addAction(QStringLiteral("Copy Path"), this, [node]() {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(node->path));
    });

    menu.addAction(QStringLiteral("Copy Size"), this, [node]() {
        QApplication::clipboard()->setText(DiskNode::prettySize(node->size));
    });

    if (node->isDir) {
        menu.addSeparator();
        menu.addAction(QStringLiteral("Navigate Into"), this, [this, node]() {
            navigateTo(node);
        });
    }

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

void DiskSentinelWindow::exportCsv() {
    if (!m_scanRoot) return;

    const QString defaultName = QStringLiteral("DiskSentinel_%1.csv")
        .arg(QDir(m_scanRoot->path).dirName().replace(QRegularExpression("[^a-zA-Z0-9_]"), "_"));

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Scan Results"),
        defaultName,
        QStringLiteral("CSV Files (*.csv);;All Files (*)"));

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            QStringLiteral("CSV export failed: could not open '%1'").arg(filePath));
        setStatus(QStringLiteral("Export failed: could not write file."));
        return;
    }

    QTextStream out(&file);
    out << "Name,Path,Size (bytes),Size,Items,Type,Category\n";

    QVector<DiskNode*> stack;
    stack.reserve(4096);
    stack.push_back(m_scanRoot.get());

    int rows = 0;
    while (!stack.isEmpty()) {
        DiskNode* node = stack.back();
        stack.pop_back();
        if (!node) continue;

        auto escape = [](const QString& s) -> QString {
            if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
                QString escaped = s;
                escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                return QLatin1Char('"') + escaped + QLatin1Char('"');
            }
            return s;
        };

        const QString type = node->isDir ? QStringLiteral("Directory") : QStringLiteral("File");
        const QString cat  = node->isDir ? QString{} : DiskNode::category(node->name);

        out << escape(node->name) << ','
            << escape(QDir::toNativeSeparators(node->path)) << ','
            << node->size << ','
            << escape(DiskNode::prettySize(node->size)) << ','
            << node->itemCount << ','
            << type << ','
            << cat << '\n';

        ++rows;

        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
            if (*it) stack.push_back(it->get());
        }
    }

    file.close();
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("CSV exported: %1 rows → '%2'").arg(rows).arg(filePath));
    setStatus(QStringLiteral("Exported %1 entries to CSV.").arg(rows));
}

void DiskSentinelWindow::findDuplicates() {
    if (!m_scanRoot) return;

    setStatus(QStringLiteral("Finding duplicates…"), true);
    QApplication::processEvents();

    QHash<qint64, QVector<DiskNode*>> bySize;
    {
        QVector<DiskNode*> stack;
        stack.reserve(4096);
        stack.push_back(m_scanRoot.get());
        while (!stack.isEmpty()) {
            DiskNode* node = stack.back();
            stack.pop_back();
            if (!node) continue;
            if (!node->isDir && node->size > 0) {
                bySize[node->size].push_back(node);
            }
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
                if (*it) stack.push_back(it->get());
            }
        }
    }

    QVector<QPair<qint64, QVector<DiskNode*>>> candidates;
    int totalToHash = 0;
    for (auto it = bySize.begin(); it != bySize.end(); ++it) {
        if (it.value().size() > 1) {
            candidates.push_back({it.key(), it.value()});
            totalToHash += it.value().size();
        }
    }

    if (totalToHash == 0) {
        setStatus(QStringLiteral("No potential duplicates found (all files have unique sizes)."));
        return;
    }

    constexpr qint64 kPartialSize = 8192;

    QProgressDialog progress(QStringLiteral("Hashing files for duplicate detection…"),
                             QStringLiteral("Cancel"), 0, totalToHash, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    int hashed = 0;

    QHash<QByteArray, QVector<DiskNode*>> duplicateGroups;

    for (const auto& [fileSize, nodes] : candidates) {
        if (progress.wasCanceled()) break;

        QHash<QByteArray, QVector<DiskNode*>> partialGroups;
        for (DiskNode* node : nodes) {
            if (progress.wasCanceled()) break;
            progress.setValue(hashed++);
            QApplication::processEvents();

            QFile file(node->path);
            if (!file.open(QIODevice::ReadOnly)) continue;

            QCryptographicHash hasher(QCryptographicHash::Sha256);
            const QByteArray chunk = file.read(kPartialSize);
            hasher.addData(chunk);
            const QByteArray partialHash = hasher.result();
            partialGroups[partialHash].push_back(node);
        }

        for (auto pit = partialGroups.begin(); pit != partialGroups.end(); ++pit) {
            if (pit.value().size() < 2) continue;

            if (fileSize <= kPartialSize) {

                duplicateGroups[pit.key()].append(pit.value());
            } else {

                QHash<QByteArray, QVector<DiskNode*>> fullGroups;
                for (DiskNode* node : pit.value()) {
                    if (progress.wasCanceled()) break;

                    QFile file(node->path);
                    if (!file.open(QIODevice::ReadOnly)) continue;

                    QCryptographicHash hasher(QCryptographicHash::Sha256);
                    while (!file.atEnd()) {
                        hasher.addData(file.read(65536));
                    }
                    fullGroups[hasher.result()].push_back(node);
                }
                for (auto fit = fullGroups.begin(); fit != fullGroups.end(); ++fit) {
                    if (fit.value().size() > 1) {
                        duplicateGroups[fit.key()].append(fit.value());
                    }
                }
            }
        }
    }

    progress.setValue(totalToHash);

    if (progress.wasCanceled()) {
        setStatus(QStringLiteral("Duplicate search cancelled."));
        return;
    }

    int groupCount = duplicateGroups.size();
    int totalDupeFiles = 0;
    qint64 wastedBytes = 0;
    for (auto it = duplicateGroups.begin(); it != duplicateGroups.end(); ++it) {
        totalDupeFiles += it.value().size();

        if (!it.value().isEmpty()) {
            wastedBytes += static_cast<qint64>(it.value().size() - 1) * it.value().first()->size;
        }
    }

    if (groupCount == 0) {
        setStatus(QStringLiteral("No duplicate files found."));
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Duplicate scan: no duplicates found."));
        return;
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("Duplicate Files — %1 groups, %2 wasted")
                            .arg(groupCount).arg(DiskNode::prettySize(wastedBytes)));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(900, 600);

    auto* dlgLayout = new QVBoxLayout(dlg);
    dlgLayout->setContentsMargins(12, 12, 12, 12);
    dlgLayout->setSpacing(8);

    auto* summaryLabel = new QLabel(
        QStringLiteral("Found <b>%1</b> duplicate groups (%2 files). "
                       "Potential space savings: <b>%3</b>.")
            .arg(groupCount).arg(totalDupeFiles).arg(DiskNode::prettySize(wastedBytes)),
        dlg);
    summaryLabel->setWordWrap(true);

    auto* tree = new QTreeWidget(dlg);
    tree->setHeaderLabels({QStringLiteral("Name / Path"), QStringLiteral("Size"),
                           QStringLiteral("SHA-256 (partial)")});
    tree->setColumnWidth(0, 500);
    tree->setColumnWidth(1, 100);
    tree->setAlternatingRowColors(true);
    tree->setRootIsDecorated(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);

    QVector<QPair<QByteArray, QVector<DiskNode*>>> sortedGroups;
    sortedGroups.reserve(groupCount);
    for (auto it = duplicateGroups.begin(); it != duplicateGroups.end(); ++it) {
        sortedGroups.push_back({it.key(), it.value()});
    }
    std::sort(sortedGroups.begin(), sortedGroups.end(),
              [](const auto& a, const auto& b) {
                  qint64 wa = a.second.isEmpty() ? 0 : static_cast<qint64>(a.second.size() - 1) * a.second.first()->size;
                  qint64 wb = b.second.isEmpty() ? 0 : static_cast<qint64>(b.second.size() - 1) * b.second.first()->size;
                  return wa > wb;
              });

    for (const auto& [hash, nodes] : sortedGroups) {
        if (nodes.isEmpty()) continue;
        const qint64 wasted = static_cast<qint64>(nodes.size() - 1) * nodes.first()->size;

        auto* groupItem = new QTreeWidgetItem(tree);
        groupItem->setText(0, QStringLiteral("%1 copies — %2 wasted")
                                  .arg(nodes.size()).arg(DiskNode::prettySize(wasted)));
        groupItem->setText(1, DiskNode::prettySize(nodes.first()->size));
        groupItem->setText(2, QString::fromLatin1(hash.toHex().left(16)));
        groupItem->setExpanded(false);

        auto groupFont = groupItem->font(0);
        groupFont.setBold(true);
        groupItem->setFont(0, groupFont);

        for (DiskNode* node : nodes) {
            auto* fileItem = new QTreeWidgetItem(groupItem);
            fileItem->setText(0, QDir::toNativeSeparators(node->path));
            fileItem->setText(1, DiskNode::prettySize(node->size));
            fileItem->setData(0, Qt::UserRole, node->path);
        }
    }

    connect(tree, &QTreeWidget::customContextMenuRequested, dlg, [tree](const QPoint& pos) {
        auto* item = tree->itemAt(pos);
        if (!item) return;
        const QString path = item->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;

        QMenu menu;
        menu.addAction(QStringLiteral("Open in Explorer"), [path]() {
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                                    {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
        });
        menu.addAction(QStringLiteral("Copy Path"), [path]() {
            QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
        });
        menu.exec(tree->viewport()->mapToGlobal(pos));
    });

    auto* btnRow = new QHBoxLayout();
    auto* exportDupesBtn = new QPushButton(QStringLiteral("⬇ Export Duplicates CSV"), dlg);
    btnRow->addStretch();
    btnRow->addWidget(exportDupesBtn);

    connect(exportDupesBtn, &QPushButton::clicked, dlg, [this, sortedGroups, dlg]() {
        const QString filePath = QFileDialog::getSaveFileName(
            dlg,
            QStringLiteral("Export Duplicate Files"),
            QStringLiteral("DiskSentinel_Duplicates.csv"),
            QStringLiteral("CSV Files (*.csv);;All Files (*)"));
        if (filePath.isEmpty()) return;

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

        QTextStream out(&file);
        out << "Group,Name,Path,Size (bytes),Size,Hash\n";
        int group = 0;
        for (const auto& [hash, nodes] : sortedGroups) {
            ++group;
            for (DiskNode* node : nodes) {
                auto escape = [](const QString& s) -> QString {
                    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
                        QString escaped = s;
                        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                        return QLatin1Char('"') + escaped + QLatin1Char('"');
                    }
                    return s;
                };
                out << group << ','
                    << escape(node->name) << ','
                    << escape(QDir::toNativeSeparators(node->path)) << ','
                    << node->size << ','
                    << escape(DiskNode::prettySize(node->size)) << ','
                    << QString::fromLatin1(hash.toHex()) << '\n';
            }
        }
        file.close();
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Duplicates CSV exported to '%1'").arg(filePath));
    });

    dlgLayout->addWidget(summaryLabel);
    dlgLayout->addWidget(tree, 1);
    dlgLayout->addLayout(btnRow);

    const QString dlgStyle = QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QTreeWidget { background: %3; color: %2; border: 1px solid %4; alternate-background-color: %5; }"
        "QTreeWidget::item:hover { background: %6; }"
        "QHeaderView::section { background: %3; color: %7; border: none; border-right: 1px solid %4; padding: 4px 6px; font-weight: bold; }"
        "QPushButton { background: %3; border: 1px solid %4; color: %2; padding: 6px 12px; border-radius: 6px; }"
        "QPushButton:hover { background: %6; }"
        "QLabel { color: %2; }"
    ).arg(
        m_palette.windowBackground.name(),
        m_palette.foreground.name(),
        m_palette.cardBackground.name(),
        m_palette.cardBorder.name(),
        m_palette.hoverBackground.name(),
        m_palette.hoverBackground.name(),
        m_palette.mutedForeground.name());
    dlg->setStyleSheet(dlgStyle);

    dlg->show();

    setStatus(QStringLiteral("Found %1 duplicate groups (%2 wasted).")
                  .arg(groupCount).arg(DiskNode::prettySize(wastedBytes)));
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Duplicate scan: %1 groups, %2 files, %3 wasted.")
            .arg(groupCount).arg(totalDupeFiles).arg(DiskNode::prettySize(wastedBytes)));
}

void DiskSentinelWindow::applyTheme() {
    const auto& p = m_palette;
    const bool dark = wintools::themes::ThemeHelper::isDarkTheme();

    auto blend = [](const QColor& base, const QColor& tint, float a) -> QColor {
        return QColor(
            int(base.red()   * (1.f - a) + tint.red()   * a),
            int(base.green() * (1.f - a) + tint.green() * a),
            int(base.blue()  * (1.f - a) + tint.blue()  * a));
    };

    const QString bg      = p.windowBackground.name();
    const QString cardBg  = p.cardBackground.name();
    const QString border  = p.cardBorder.name();
    const QString fg      = p.foreground.name();
    const QString muted   = p.mutedForeground.name();
    const QString accent  = p.accent.name();
    const QString altBg   = blend(p.windowBackground, p.foreground, dark ? 0.045f : 0.025f).name();
    const QString hoverBg = blend(p.windowBackground, p.foreground, dark ? 0.10f : 0.06f).name();
    const QString selBg   = blend(p.cardBackground,   p.accent, dark ? 0.30f : 0.20f).name();

    const QString fluentBase = wintools::themes::FluentStyle::generate(p);

    const QString dsOverlay = QStringLiteral(
        "DiskSentinelWindow { background: %1; color: %4; }"

        "#driveBar  { background: %2; border-bottom: 1px solid %3; }"
        "#driveCard { background: %2; border: 1px solid %3; border-radius: 8px; }"
        "#driveCard:hover { background: %8; border: 1px solid %6; }"

        "#toolBar { background: %2; border-bottom: 1px solid %3; }"

        "QLineEdit { background: %1; border: 1px solid %3; border-radius: 6px;"
        "            color: %4; padding: 2px 6px; }"
        "QLineEdit:focus { border: 1px solid %6; }"

        "QPushButton { background: %1; border: 1px solid %3; border-radius: 6px;"
        "              color: %4; padding: 2px 8px; }"
        "QPushButton:hover   { background: %8; border: 1px solid %6; }"
        "QPushButton:pressed { background: %9; }"
        "QPushButton:disabled { color: %5; }"

        "QTreeView { background: %1; alternate-background-color: %7; color: %4; border: 1px solid %3; }"
        "QTreeView::item:hover:!selected { background: %8; }"
        "QTreeView::item:selected { background: %9; color: %4; }"

        "QHeaderView::section { background: %2; color: %5; border: none;"
        "                       border-right: 1px solid %3; padding: 4px 6px;"
        "                       font-weight: bold; font-size: 11px; }"

        "QScrollArea { background: %1; border: none; }"

        "QSplitter::handle { background: %3; }"

        "#legendBar { background: %2; border-top: 1px solid %3; }"

        "QLabel { color: %4; }"

        "QProgressBar { background: %3; border: none; border-radius: 4px; }"
        "QProgressBar::chunk { background: %6; border-radius: 4px; }"
    ).arg(bg, cardBg, border, fg, muted, accent, altBg, hoverBg, selBg);

    wintools::themes::ThemeHelper::applyThemeTo(this, dsOverlay);

    if (m_treemap) {
        QPalette pal = m_treemap->palette();
        pal.setColor(QPalette::Window, p.windowBackground);
        pal.setColor(QPalette::WindowText, p.mutedForeground);
        m_treemap->setPalette(pal);
        m_treemap->setAutoFillBackground(true);
    }

    if (m_pieChart)
        m_pieChart->setThemePalette(m_palette);

    const auto swatches = m_legend->findChildren<QFrame*>(QStringLiteral("legendSwatch"));
    for (auto* swatch : swatches) {
        const QString category = swatch->property("category").toString();
        const QColor c = categoryThemeColor(category, p, dark);
        swatch->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;border:1px solid %2;")
            .arg(c.name(), p.cardBorder.name()));
    }
    const auto labels = m_legend->findChildren<QLabel*>(QStringLiteral("legendLabel"));
    for (auto* label : labels) {
        label->setStyleSheet(QStringLiteral("font-size:11px;color:%1;").arg(p.mutedForeground.name()));
    }

    refreshDrives();
}

}
