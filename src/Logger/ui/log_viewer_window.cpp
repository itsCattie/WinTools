#include "logger/ui/log_viewer_window.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/ui/screen_relative_size.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

// WinTools: log viewer window manages UI behavior and presentation.

namespace wintools::logviewer {

static constexpr QColor kColorError   { 0xff, 0x44, 0x44 };
static constexpr QColor kColorWarning { 0xff, 0x99, 0x00 };
static constexpr QColor kColorPass    { 0x27, 0xae, 0x60 };
static constexpr QColor kColorBgError  { 0x3a, 0x10, 0x10 };
static constexpr QColor kColorBgWarn   { 0x35, 0x28, 0x00 };

static QString sevLabel(wintools::logger::Severity s) {
    switch (s) {
    case wintools::logger::Severity::Error:   return QStringLiteral("ERR");
    case wintools::logger::Severity::Warning: return QStringLiteral("WARN");
    case wintools::logger::Severity::Pass:    return QStringLiteral("PASS");
    }
    return {};
}

LogTableModel::LogTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void LogTableModel::setEntries(const QVector<wintools::logger::LogEntry>& entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void LogTableModel::appendEntry(const wintools::logger::LogEntry& entry) {
    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append(entry);
    endInsertRows();
}

void LogTableModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

int LogTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_entries.size();
}

int LogTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LogTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const auto& e = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:
            return e.timestamp.toString(QStringLiteral("HH:mm:ss.zzz"));
        case ColSeverity:
            return sevLabel(e.severity);
        case ColSource:
            return e.source;
        case ColMessage: {
            QString msg = e.reason;
            if (!e.data.trimmed().isEmpty())
                msg += QStringLiteral(" | ") + e.data;
            return msg;
        }
        }
        return {};
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == ColSeverity) {
            switch (e.severity) {
            case wintools::logger::Severity::Error:   return QBrush(kColorError);
            case wintools::logger::Severity::Warning: return QBrush(kColorWarning);
            case wintools::logger::Severity::Pass:    return QBrush(kColorPass);
            }
        }
        return {};
    }

    if (role == Qt::BackgroundRole) {
        switch (e.severity) {
        case wintools::logger::Severity::Error:   return QBrush(kColorBgError);
        case wintools::logger::Severity::Warning: return QBrush(kColorBgWarn);
        default: return {};
        }
    }

    if (role == Qt::UserRole) {

        return static_cast<int>(e.severity);
    }

    if (role == Qt::UserRole + 1) {

        return e.source;
    }

    return {};
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColTime:     return QStringLiteral("Time");
    case ColSeverity: return QStringLiteral("Lv");
    case ColSource:   return QStringLiteral("Source");
    case ColMessage:  return QStringLiteral("Message");
    }
    return {};
}

LogFilterProxy::LogFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void LogFilterProxy::setSourcePrefix(const QString& prefix) {
    m_sourcePrefix = prefix;
    invalidateFilter();
}

void LogFilterProxy::setSeverityFilter(int sev) {
    m_severity = sev;
    invalidateFilter();
}

void LogFilterProxy::setMessageFilter(const QString& text) {
    m_msgFilter = text;
    invalidateFilter();
}

bool LogFilterProxy::filterAcceptsRow(int row, const QModelIndex& parent) const {
    const QModelIndex srcIdx = sourceModel()->index(row, 0, parent);
    const QString source = srcIdx.data(Qt::UserRole + 1).toString();
    const int     sev    = srcIdx.data(Qt::UserRole).toInt();

    if (!m_sourcePrefix.isEmpty() && !source.startsWith(m_sourcePrefix, Qt::CaseInsensitive))
        return false;

    if (m_severity >= 0 && sev != m_severity)
        return false;

    if (!m_msgFilter.isEmpty()) {
        const QModelIndex msgIdx = sourceModel()->index(row, LogTableModel::ColMessage, parent);
        const QString msg = msgIdx.data(Qt::DisplayRole).toString();
        if (!msg.contains(m_msgFilter, Qt::CaseInsensitive) &&
            !source.contains(m_msgFilter, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

static QString buildLogViewerQss(const wintools::themes::ThemePalette& p) {
    using wintools::themes::FluentStyle;

    const QString bg      = p.windowBackground.name();
    const QString cardBg  = p.cardBackground.name();
    const QString border  = p.cardBorder.name();
    const QString fg      = p.foreground.name();
    const QString muted   = p.mutedForeground.name();
    const QString accent  = p.accent.name();
    const QString hover   = p.hoverBackground.name();

    const QString supplement = QStringLiteral(

        "QListWidget#sourceList { background-color: %1; border: none; outline: none;"
        "  border-right: 1px solid %2; font-size: 12px; }"
        "QListWidget#sourceList::item { color: %3; padding: 6px 14px; border-radius: 6px; margin: 1px 4px; }"
        "QListWidget#sourceList::item:selected { background-color: %4; color: #ffffff; }"
        "QListWidget#sourceList::item:hover:!selected { background-color: %5; color: %6; }"

        "QTableView { background: %7; alternate-background-color: %1; color: %6; border: none;"
        "  gridline-color: %2; selection-background-color: %4; selection-color: #fff;"
        "  font-family: 'Cascadia Mono', 'Consolas', monospace; font-size: 12px; outline: 0; }"
        "QHeaderView::section { background: %1; color: %3; padding: 3px 8px; border: none;"
        "  border-right: 1px solid %2; border-bottom: 1px solid %2; font-size: 11px; }"

        "#legendBar { background: %1; border-top: 1px solid %2; }"
    ).arg(cardBg, border, muted, accent, hover, fg, bg);

    return FluentStyle::generate(p) + supplement;
}

LogViewerWindow::LogViewerWindow(QWidget* parent)
    : QDialog(parent, Qt::Window)
    , m_model(new LogTableModel(this))
    , m_proxy(new LogFilterProxy(this))
{
    setWindowTitle(QStringLiteral("WinTools – Log Viewer"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/logviewer.svg")));
    setMinimumSize(1000, 640);
    resize(1200, 720);
    wintools::ui::enableRelativeSizeAcrossScreens(this);

    m_proxy->setSourceModel(m_model);

    buildUi();
    applyTheme();

    m_model->setEntries(wintools::logger::LogSink::instance()->entries());
    updateLegendCounts();

    connect(wintools::logger::LogSink::instance(),
            &wintools::logger::LogSink::entryAdded,
            this, &LogViewerWindow::onNewEntry);

    if (!m_model->rowCount())
        return;
    QTimer::singleShot(0, this, [this]{ scrollToBottom(); });
}

LogViewerWindow::~LogViewerWindow() = default;

void LogViewerWindow::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbarFrame = new QFrame(this);
    toolbarFrame->setObjectName(QStringLiteral("toolbar"));
    toolbarFrame->setFixedHeight(44);
    auto* tbLay = new QHBoxLayout(toolbarFrame);
    tbLay->setContentsMargins(8, 0, 8, 0);
    tbLay->setSpacing(8);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setObjectName(QStringLiteral("filterEdit"));
    m_filterEdit->setPlaceholderText(QStringLiteral("Filter messages\u2026"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMinimumWidth(260);

    m_severityCombo = new QComboBox(this);
    m_severityCombo->addItem(QStringLiteral("All Levels"),  -1);
    m_severityCombo->addItem(QStringLiteral("  ● Errors"),    0);
    m_severityCombo->addItem(QStringLiteral("  ● Warnings"),  1);
    m_severityCombo->addItem(QStringLiteral("  ● Info/Pass"), 2);

    m_autoScrollChk = new QCheckBox(QStringLiteral("Auto-scroll"), this);
    m_autoScrollChk->setChecked(true);

    m_clearBtn  = new QPushButton(QStringLiteral("Clear"), this);
    m_exportBtn = new QPushButton(QStringLiteral("Export…"), this);

    tbLay->addWidget(m_filterEdit, 1);
    tbLay->addWidget(m_severityCombo);
    tbLay->addStretch();
    tbLay->addWidget(m_autoScrollChk);
    tbLay->addWidget(m_clearBtn);
    tbLay->addWidget(m_exportBtn);

    root->addWidget(toolbarFrame);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->setChildrenCollapsible(false);

    buildSidebar();
    m_splitter->addWidget(m_sidebar);

    auto* rightFrame = new QFrame(this);
    auto* rightLay   = new QVBoxLayout(rightFrame);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);
    buildTable(rightFrame);
    rightLay->addWidget(m_tableView, 1);
    m_splitter->addWidget(rightFrame);

    m_splitter->setSizes({190, 1010});
    root->addWidget(m_splitter, 1);

    buildLegend();
    root->addWidget(m_legend);

    connect(m_sourceList, &QListWidget::currentRowChanged,
            this, &LogViewerWindow::onSourceSelected);
    connect(m_severityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogViewerWindow::onSeverityFilterChanged);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &LogViewerWindow::onMessageFilterChanged);
    connect(m_clearBtn,  &QPushButton::clicked, this, &LogViewerWindow::clearLog);
    connect(m_exportBtn, &QPushButton::clicked, this, &LogViewerWindow::exportLog);
    connect(m_autoScrollChk, &QCheckBox::toggled, this, [this](bool on){
        m_autoScroll = on;
    });
}

void LogViewerWindow::buildSidebar() {
    m_sidebar = new QFrame(this);
    m_sidebar->setFixedWidth(190);
    auto* lay = new QVBoxLayout(m_sidebar);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* hdr = new QLabel(QStringLiteral("  SOURCES"), m_sidebar);
    hdr->setObjectName(QStringLiteral("sidebarHeader"));
    hdr->setFixedHeight(32);
    lay->addWidget(hdr);

    m_sourceList = new QListWidget(m_sidebar);
    m_sourceList->setObjectName(QStringLiteral("sourceList"));

    auto addSep = [&](const QString& text) {
        auto* item = new QListWidgetItem(text, m_sourceList);
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(0x55, 0x55, 0x55));
        QFont f = item->font();
        f.setPointSize(9);
        f.setBold(true);
        item->setFont(f);
        item->setData(Qt::UserRole + 10, true);
    };

    auto addItem = [&](const QString& label, const QString& prefix) {
        auto* item = new QListWidgetItem(label, m_sourceList);
        item->setData(Qt::UserRole, prefix);
    };

    addItem(QStringLiteral("  All"),        QStringLiteral(""));
    addSep(QStringLiteral("  ─────────────────"));
    addItem(QStringLiteral("  AudioMaster"),     QStringLiteral("AudioMaster"));
    addItem(QStringLiteral("  DiskSentinel"),    QStringLiteral("DiskSentinel"));
    addItem(QStringLiteral("  GameVault"),        QStringLiteral("GameVault"));
    addItem(QStringLiteral("  MediaBar"),         QStringLiteral("MediaBar"));
    addItem(QStringLiteral("  StreamVault"),      QStringLiteral("StreamVault"));
    addItem(QStringLiteral("  TaskManager"),     QStringLiteral("TaskManager"));
    addSep(QStringLiteral("  ─────────────────"));
    addItem(QStringLiteral("  System / Other"),  QStringLiteral("System"));

    m_sourceList->setCurrentRow(0);
    lay->addWidget(m_sourceList, 1);
}

void LogViewerWindow::buildTable(QWidget* parent) {
    m_tableView = new QTableView(parent);
    m_tableView->setModel(m_proxy);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSortingEnabled(false);
    m_tableView->verticalHeader()->hide();
    m_tableView->verticalHeader()->setDefaultSectionSize(20);

    auto* hdr = m_tableView->horizontalHeader();
    hdr->setSectionResizeMode(LogTableModel::ColTime,     QHeaderView::Fixed);
    hdr->setSectionResizeMode(LogTableModel::ColSeverity, QHeaderView::Fixed);
    hdr->setSectionResizeMode(LogTableModel::ColSource,   QHeaderView::Interactive);
    hdr->setSectionResizeMode(LogTableModel::ColMessage,  QHeaderView::Stretch);
    m_tableView->setColumnWidth(LogTableModel::ColTime,     100);
    m_tableView->setColumnWidth(LogTableModel::ColSeverity, 44);
    m_tableView->setColumnWidth(LogTableModel::ColSource,   190);
}

void LogViewerWindow::buildLegend() {
    m_legend = new QFrame(this);
    m_legend->setObjectName(QStringLiteral("legendBar"));
    m_legend->setFixedHeight(30);

    auto* hbox = new QHBoxLayout(m_legend);
    hbox->setContentsMargins(10, 0, 10, 0);
    hbox->setSpacing(14);

    auto makeSwatch = [&](QColor col, const QString& label) -> QLabel* {
        auto* swatch = new QFrame(this);
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;").arg(col.name()));
        auto* lbl = new QLabel(label, this);
        lbl->setStyleSheet(QStringLiteral("font-size:11px;"));
        hbox->addWidget(swatch);
        hbox->addWidget(lbl);
        return lbl;
    };

    m_lblErrors   = makeSwatch(kColorError,   QStringLiteral("Error"));
    m_lblWarnings = makeSwatch(kColorWarning, QStringLiteral("Warning"));
    m_lblPasses   = makeSwatch(kColorPass,    QStringLiteral("Pass"));

    hbox->addStretch(1);

    m_lblShowing = new QLabel(this);
    m_lblShowing->setStyleSheet(QStringLiteral("font-size:11px;"));
    hbox->addWidget(m_lblShowing);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    hbox->addWidget(sep);

    m_lblTotal = new QLabel(this);
    m_lblTotal->setStyleSheet(QStringLiteral("font-size:11px;"));
    hbox->addWidget(m_lblTotal);
}

void LogViewerWindow::applyTheme() {
    const auto palette = wintools::themes::ThemeHelper::currentPalette();
    setStyleSheet(buildLogViewerQss(palette));
}

void LogViewerWindow::onNewEntry(const wintools::logger::LogEntry& entry) {
    m_model->appendEntry(entry);
    updateLegendCounts();
    if (m_autoScroll)
        QTimer::singleShot(0, this, [this]{ scrollToBottom(); });
}

void LogViewerWindow::onSourceSelected(int row) {
    if (row < 0) return;
    const auto* item = m_sourceList->item(row);
    if (!item) return;

    if (item->data(Qt::UserRole + 10).toBool()) return;

    const QString prefix = item->data(Qt::UserRole).toString();
    m_proxy->setSourcePrefix(prefix);
    updateLegendCounts();
}

void LogViewerWindow::onSeverityFilterChanged(int index) {
    const int sev = m_severityCombo->itemData(index).toInt();
    m_proxy->setSeverityFilter(sev);
    updateLegendCounts();
}

void LogViewerWindow::onMessageFilterChanged(const QString& text) {
    m_proxy->setMessageFilter(text);
    updateLegendCounts();
}

void LogViewerWindow::clearLog() {
    m_model->clear();
    updateLegendCounts();
}

void LogViewerWindow::exportLog() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Log"),
        QStringLiteral("wintools-export.log"),
        QStringLiteral("Log files (*.log);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);

    const int rows = m_proxy->rowCount();
    for (int r = 0; r < rows; ++r) {
        const QString time = m_proxy->index(r, LogTableModel::ColTime).data().toString();
        const QString sev  = m_proxy->index(r, LogTableModel::ColSeverity).data().toString();
        const QString src  = m_proxy->index(r, LogTableModel::ColSource).data().toString();
        const QString msg  = m_proxy->index(r, LogTableModel::ColMessage).data().toString();
        out << time << "  " << sev << "  [" << src << "]  " << msg << "\n";
    }
}

void LogViewerWindow::updateLegendCounts() {
    const int total   = m_model->rowCount();
    const int showing = m_proxy->rowCount();

    int errs = 0, warns = 0, passes = 0;
    for (int r = 0; r < total; ++r) {
        const int sev = m_model->index(r, 0).data(Qt::UserRole).toInt();
        if (sev == 0) ++errs;
        else if (sev == 1) ++warns;
        else ++passes;
    }

    m_lblErrors->setText(QStringLiteral("Error (%1)").arg(errs));
    m_lblWarnings->setText(QStringLiteral("Warning (%1)").arg(warns));
    m_lblPasses->setText(QStringLiteral("Pass (%1)").arg(passes));
    m_lblShowing->setText(QStringLiteral("Showing %1").arg(showing));
    m_lblTotal->setText(QStringLiteral("Total %1").arg(total));
}

void LogViewerWindow::scrollToBottom() {
    if (!m_tableView) return;
    const int rows = m_proxy->rowCount();
    if (rows > 0)
        m_tableView->scrollTo(m_proxy->index(rows - 1, 0));
}

}
