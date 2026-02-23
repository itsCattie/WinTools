#include "modules/disksentinel/src/ui/disk_sentinel_window.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/ui/screen_relative_size.hpp"
#include "logger/logger.hpp"

#include <QApplication>
#include <QDir>
#include <QMouseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStorageInfo>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>
#include <QTreeView>
#include <QVBoxLayout>

// DiskSentinel: disk sentinel window manages UI behavior and presentation.

namespace wintools::disksentinel {

static constexpr const char* kLog = "DiskSentinel/Window";

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

        painter->fillRect(r, opt.palette.mid().color());

        const int barW = static_cast<int>(r.width() * qBound(0.0, fraction, 1.0));
        if (barW > 0) {
            const bool isDir = index.data(StorageModel::IsDirRole).toBool();
            QColor barColor = isDir ? opt.palette.highlight().color()
                                    : QColor(0x27, 0xae, 0x60);

            if (fraction > 0.5)  barColor = barColor.darker(100 + static_cast<int>(40 * fraction));
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

    m_splitter->addWidget(m_treeView);
    m_splitter->addWidget(m_treemap);
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
        swatch->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;")
                                .arg(e.col.name()));
        auto* lbl = new QLabel(e.label, this);
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
    m_scanProgress->setVisible(true);
    setStatus(QStringLiteral("Scanning…"), true);
}

void DiskSentinelWindow::onScanProgress(int files, qint64 bytes,
                                         const QString& currentPath) {
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
    m_model->setRoot(m_scanRoot);
    setDisplayRoot(m_scanRoot.get());
    m_scanProgress->setVisible(false);
    m_reloadBtn->setEnabled(true);
    m_expandCollapseBtn->setEnabled(true);

    m_treeExpanded = false;
    m_expandCollapseBtn->setText(QStringLiteral("⊞ Expand All"));
    m_treeView->expandToDepth(1);
    m_treeView->resizeColumnToContents(StorageModel::ColSize);
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

    if (m_scanRoot) {
        DiskNode* found = nullptr;
        std::function<void(DiskNode*)> search = [&](DiskNode* n) {
            if (!n) return;
            if (n->path == path || QDir::cleanPath(n->path) == QDir::cleanPath(path)) {
                found = n;
                return;
            }
            for (auto& c : n->children) search(c.get());
        };
        search(m_scanRoot.get());
        if (found) { navigateTo(found); return; }
    }

    scanDrive(path);
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

void DiskSentinelWindow::applyTheme() {
    const auto& p = m_palette;

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
    const QString altBg   = blend(p.windowBackground, p.foreground, 0.04f).name();
    const QString hoverBg = blend(p.windowBackground, p.foreground, 0.09f).name();
    const QString selBg   = blend(p.cardBackground,   p.accent,     0.28f).name();

    const QString fluentBase = wintools::themes::FluentStyle::generate(p);

    const QString dsOverlay = QStringLiteral(
        "DiskSentinelWindow { background: %1; color: %4; }"

        "#driveBar  { background: %2; border-bottom: 1px solid %3; }"
        "#driveCard { background: %2; border: 1px solid %3; border-radius: 8px; }"
        "#driveCard:hover { background: %8; border: 1px solid %6; }"

        "#toolBar { background: %2; border-bottom: 1px solid %3; }"

        "QLineEdit { background: %2; border: 1px solid %3; border-radius: 6px;"
        "            color: %4; padding: 2px 6px; }"
        "QLineEdit:focus { border: 1px solid %6; }"

        "QPushButton { background: %2; border: 1px solid %3; border-radius: 6px;"
        "              color: %4; padding: 2px 8px; }"
        "QPushButton:hover   { background: %8; border: 1px solid %6; }"
        "QPushButton:pressed { background: %9; }"
        "QPushButton:disabled { color: %5; }"

        "QTreeView { background: %1; alternate-background-color: %7; color: %4; border: none; }"
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

    setStyleSheet(fluentBase + dsOverlay);

    refreshDrives();
}

}
