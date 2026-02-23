// GameVault: gamevault window manages UI behavior and presentation.

#include "gamevault_window.hpp"
#include "game_card_delegate.hpp"

#include "logger/logger.hpp"
#include "modules/GameVault/src/core/game_entry.hpp"
#include "modules/GameVault/src/core/game_library.hpp"
#include "modules/GameVault/src/core/gamevault_settings.hpp"
#include "modules/GameVault/src/core/playtime_tracker.hpp"
#include "modules/GameVault/src/model/game_model.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/ui/screen_relative_size.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QThreadPool>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace wintools::gamevault {

static constexpr const char* kLog = "GameVault/Window";

[[maybe_unused, gnu::unused]] static const char* kSteamQss = R"(
QDialog {
    background-color: #1b2838;
    color: #c6d4df;
}
QListWidget#sidebar {
    background-color: #171D25;
    border: none;
    outline: none;
    font-size: 13px;
}
QListWidget#sidebar::item {
    color: #8f98a0;
    padding: 6px 16px;
    border-radius: 2px;
}
QListWidget#sidebar::item:selected {
    background-color: #2a475e;
    color: #ffffff;
}
QListWidget#sidebar::item:hover:!selected {
    background-color: #1e3448;
    color: #c6d4df;
}
QLineEdit {
    background-color: #32404e;
    color: #c6d4df;
    border: 1px solid #4a6174;
    border-radius: 3px;
    padding: 5px 10px;
    font-size: 13px;
    selection-background-color: #66c0f4;
}
QLineEdit:focus { border-color: #66c0f4; }
QPushButton {
    background-color: #32404e;
    color: #c6d4df;
    border: 1px solid #4a6174;
    border-radius: 3px;
    padding: 5px 14px;
    font-size: 12px;
}
QPushButton:hover  { background-color: #3d536b; color: #ffffff; }
QPushButton:pressed{ background-color: #4a6174; }
QPushButton#playBtn {
    background-color: #4d7a1e;
    color: #eaf2d7;
    border: none;
    font-size: 14px;
    font-weight: bold;
    padding: 8px 28px;
    border-radius: 3px;
}
QPushButton#playBtn:hover  { background-color: #5c921f; }
QPushButton#playBtn:pressed{ background-color: #3d6016; }
QPushButton#backBtn {
    background-color: transparent;
    border: none;
    color: #8f98a0;
    font-size: 13px;
    padding: 4px 8px;
}
QPushButton#backBtn:hover { color: #66c0f4; }
QPushButton#installedToggle {
    background-color: transparent;
    border: 1px solid #4a6174;
    border-radius: 3px;
    color: #8f98a0;
    font-size: 12px;
    text-align: left;
    padding: 5px 10px;
}
QPushButton#installedToggle:checked {
    background-color: #2a475e;
    color: #c6d4df;
    border-color: #66c0f4;
}
QPushButton#settingsBtn {
    background-color: transparent;
    border: none;
    color: #8f98a0;
    font-size: 12px;
    text-align: left;
    padding: 5px 10px;
}
QPushButton#settingsBtn:hover { color: #c6d4df; }
QListView#gridView {
    background-color: #1b2838;
    border: none;
    outline: none;
}
QListView#gridView::item:selected { background: transparent; }
QListView#gridView::item:hover    { background: transparent; }
QWidget#detailPage {
    background-color: #1b2838;
}
QWidget#heroArea {
    background-color: #16202d;
}
QLabel#heroTitle {
    color: #ffffff;
    font-size: 28px;
    font-weight: bold;
    background: transparent;
}
QLabel#detailPlatform {
    color: #8f98a0;
    font-size: 13px;
}
QLabel#sectionHeader {
    color: #8f98a0;
    font-size: 11px;
    letter-spacing: 1px;
}
QLabel#playtimeBig {
    color: #c6d4df;
    font-size: 22px;
    font-weight: bold;
}
QLabel#playtimeSub {
    color: #8f98a0;
    font-size: 13px;
}
QLabel#achLabel {
    color: #8f98a0;
    font-size: 13px;
}
QProgressBar#achBar {
    border: none;
    background-color: #2a3f52;
    border-radius: 3px;
    max-height: 6px;
}
QProgressBar#achBar::chunk {
    background-color: #4d8bbd;
    border-radius: 3px;
}
QLabel#statusLabel {
    color: #8f98a0;
    font-size: 11px;
    padding: 0 4px 4px 4px;
}
QScrollBar:vertical {
    background: #1b2838;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #4a6174;
    border-radius: 4px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #66c0f4; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { height: 0; }
QDialog#settingsDlg {
    background-color: #1b2838;
    color: #c6d4df;
}
QListWidget {
    background-color: #32404e;
    color: #c6d4df;
    border: 1px solid #4a6174;
    border-radius: 3px;
}
QListWidget::item:selected { background-color: #2a475e; }
)";

static constexpr int kSidebarPlatformRole = Qt::UserRole + 100;
static constexpr int kSidebarIsSeparator  = Qt::UserRole + 101;

QString trackingIdForEntry(const GameEntry& e) {
    if (!e.platformId.trimmed().isEmpty()) return e.platformId.trimmed();

    // Local/custom titles often do not have a platform-native id.
    // Build a deterministic id from stable fields so playtime and overrides survive rescans.
    const QString stable = QString("%1|%2|%3|%4")
        .arg(static_cast<int>(e.platform))
        .arg(e.title.trimmed().toLower())
        .arg(e.executablePath.trimmed().toLower())
        .arg(e.installPath.trimmed().toLower());

    const QByteArray hash = QCryptographicHash::hash(stable.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QString("local_%1").arg(QString::fromLatin1(hash.left(16)));
}

QString settingsKeyForEntry(const GameEntry& e) {
    return QString("%1|%2").arg(static_cast<int>(e.platform)).arg(trackingIdForEntry(e));
}

QString overrideLocatorForEntry(const GameEntry& e) {
    return QString("%1|%2|%3")
        .arg(static_cast<int>(e.platform))
        .arg(e.title.trimmed().toLower())
        .arg(e.installPath.trimmed().toLower());
}

void applyCustomArtOverride(GameEntry& e) {
    const QString key = settingsKeyForEntry(e);
    const QString path = GameVaultSettings::instance().customArtPath(key).trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    const QString artUrl = QUrl::fromLocalFile(path).toString();
    e.artBannerUrl = artUrl;
    e.artCapsuleUrl = artUrl;
}

QStringList newsTokensForTitle(const QString& title) {
    QString normalized = title.toLower();
    normalized.replace(QRegularExpression("[^a-z0-9]+"), " ");
    const QStringList raw = normalized.split(' ', Qt::SkipEmptyParts);

    QStringList tokens;
    tokens.reserve(raw.size());
    for (const QString& token : raw) {
        if (token.size() < 3) continue;
        if (token == "the" || token == "and" || token == "for" || token == "edition"
            || token == "game" || token == "classic") {
            continue;
        }
        tokens << token;
    }
    tokens.removeDuplicates();
    return tokens;
}

bool epicNewsItemMatchesGame(const QString& title, const QString& link, const QStringList& tokens) {
    if (tokens.isEmpty()) return false;

    const QString hay = (title + " " + link).toLower();
    int matched = 0;
    for (const QString& token : tokens) {
        if (hay.contains(token)) ++matched;
    }

    if (tokens.size() <= 2) return matched >= 1;
    return matched >= 2;
}

void applyExecutableIconArtFallback(GameEntry& e, QHash<QString, QPixmap>& cache) {
    if (!e.artBannerUrl.isEmpty() || !e.artCapsuleUrl.isEmpty()) return;

    QString iconSource = e.iconPath;
    if (iconSource.isEmpty()) iconSource = e.executablePath;
    if (iconSource.isEmpty() || !QFileInfo::exists(iconSource)) return;

    QIcon icon(iconSource);
    QPixmap iconPx = icon.pixmap(128, 128);
    if (iconPx.isNull()) {
        QFileIconProvider provider;
        icon = provider.icon(QFileInfo(iconSource));
        iconPx = icon.pixmap(128, 128);
    }
    if (iconPx.isNull()) return;

    const QString key = QString("local-art://%1").arg(trackingIdForEntry(e));
    if (!cache.contains(key)) {
        QPixmap banner(460, 215);
        banner.fill(QColor(0x14, 0x1C, 0x26));

        QPainter painter(&banner);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QLinearGradient grad(0, 0, 460, 215);
        grad.setColorAt(0.0, QColor(0x18, 0x24, 0x31));
        grad.setColorAt(1.0, QColor(0x10, 0x16, 0x20));
        painter.fillRect(banner.rect(), grad);

        const QSize iconSize(96, 96);
        QPixmap scaled = iconPx.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint pt((banner.width() - scaled.width()) / 2, (banner.height() - scaled.height()) / 2 - 6);
        painter.drawPixmap(pt, scaled);

        painter.setPen(QColor(0x8F, 0x98, 0xA0));
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        painter.drawText(QRect(12, banner.height() - 30, banner.width() - 24, 20), Qt::AlignCenter,
                         e.title.left(40));
        painter.end();

        cache.insert(key, banner);
    }

    e.artBannerUrl = key;
    e.artCapsuleUrl = key;
}

GameVaultWindow::GameVaultWindow(QWidget* parent)
    : QDialog(parent)
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setWindowTitle("Game Vault");
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/gamevault.svg")));
    setMinimumSize(1100, 680);
    resize(1360, 820);
    wintools::ui::enableRelativeSizeAcrossScreens(this);

    m_model = new GameListModel(this);
    m_proxy = new GameFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::DisplayRole);
    m_proxy->sort(GameCol::Title, Qt::AscendingOrder);

    m_nam = new QNetworkAccessManager(this);

    buildUi();
    applyTheme(m_palette);

    m_themeListener = new wintools::themes::ThemeListener(this);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, &GameVaultWindow::onThemeChanged);
    startScan();
}

GameVaultWindow::~GameVaultWindow() {
    if (m_cardReply)   m_cardReply->abort();
    if (m_bannerReply) m_bannerReply->abort();
    if (m_newsReply)   m_newsReply->abort();
}

void GameVaultWindow::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildSidebar());

    auto* div = new QFrame(this);
    div->setObjectName("divider");
    div->setFixedWidth(1);
    root->addWidget(div);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildLibraryPage());
    m_stack->addWidget(buildDetailPage());
    m_stack->setCurrentIndex(kLibraryPage);
    root->addWidget(m_stack, 1);
}

QWidget* GameVaultWindow::buildSidebar() {
    auto* panel = new QWidget(this);
    panel->setObjectName("sidebarPanel");
    panel->setFixedWidth(220);

    auto* vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(0, 0, 0, 12);
    vbox->setSpacing(0);

    auto* headerRow = new QWidget(panel);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(16, 18, 16, 14);
    headerLayout->setSpacing(8);

    auto* headerIcon = new QLabel(headerRow);
    headerIcon->setPixmap(QIcon(QStringLiteral(":/icons/modules/gamevault.svg")).pixmap(18, 18));
    headerIcon->setFixedSize(18, 18);
    headerIcon->setAlignment(Qt::AlignCenter);

    auto* header = new QLabel("GAME VAULT", headerRow);
    header->setObjectName("sidebarHeader");
    header->setStyleSheet("font-size: 15px; font-weight: bold; letter-spacing: 1px;");

    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(header);
    headerLayout->addStretch();
    vbox->addWidget(headerRow);

    m_sidebar = new QListWidget(panel);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setFocusPolicy(Qt::NoFocus);

    auto addItem = [&](const QString& text, int platformVal, bool separator = false) {
        auto* item = new QListWidgetItem(text, m_sidebar);
        item->setData(kSidebarPlatformRole, platformVal);
        item->setData(kSidebarIsSeparator,  separator);
        if (separator) {
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(m_palette.mutedForeground);
            QFont f = item->font();
            f.setPixelSize(10);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            item->setFont(f);
        }
        return item;
    };

    m_allGamesItem = addItem("ALL GAMES", -1);
    m_allGamesItem->setSelected(true);

    addItem("  PLATFORMS", -999, true);

    for (int p = static_cast<int>(GamePlatform::Steam);
             p <= static_cast<int>(GamePlatform::Unknown); ++p)
    {
        if (p == static_cast<int>(GamePlatform::Unknown)) continue;
        m_platformItems[p] = addItem("  " + platformName(static_cast<GamePlatform>(p)), p);
        m_platformItems[p]->setHidden(true);
    }

    m_platformItems[static_cast<int>(GamePlatform::Unknown)] =
        addItem("  Custom / Other", static_cast<int>(GamePlatform::Unknown));
    m_platformItems[static_cast<int>(GamePlatform::Unknown)]->setHidden(true);

    connect(m_sidebar, &QListWidget::currentRowChanged,
            this, &GameVaultWindow::onSidebarItemClicked);

    vbox->addWidget(m_sidebar, 1);

    auto* hr = new QFrame(panel);
    hr->setObjectName("divider");
    hr->setFrameShape(QFrame::HLine);
    hr->setStyleSheet(QString("color: %1; margin: 6px 12px;").arg(m_palette.divider.name()));
    vbox->addWidget(hr);

    m_installedToggle = new QPushButton("☐  Installed only", panel);
    m_installedToggle->setObjectName("installedToggle");
    m_installedToggle->setCheckable(true);
    m_installedToggle->setFlat(true);
    connect(m_installedToggle, &QPushButton::toggled, this, &GameVaultWindow::onInstalledOnlyToggled);
    vbox->addWidget(m_installedToggle);

    m_settingsBtn = new QPushButton("⚙  Scan Paths…", panel);
    m_settingsBtn->setObjectName("settingsBtn");
    m_settingsBtn->setFlat(true);
    connect(m_settingsBtn, &QPushButton::clicked, this, &GameVaultWindow::openSettings);
    vbox->addWidget(m_settingsBtn);

    return panel;
}

QWidget* GameVaultWindow::buildLibraryPage() {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 12, 16, 0);
    vbox->setSpacing(8);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);

    m_search = new QLineEdit(page);
    m_search->setPlaceholderText("🔍  Search your library…");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(320);
    connect(m_search, &QLineEdit::textChanged, this, &GameVaultWindow::onSearchChanged);

    m_rescanBtn = new QPushButton("↺  Rescan", page);
    m_rescanBtn->setFixedWidth(96);
    connect(m_rescanBtn, &QPushButton::clicked, this, &GameVaultWindow::rescan);

    auto* addGameBtn = new QPushButton("+  Add Game", page);
    addGameBtn->setFixedWidth(110);
    connect(addGameBtn, &QPushButton::clicked, this, &GameVaultWindow::addCustomGame);

    toolbar->addWidget(m_search);
    toolbar->addStretch(1);
    toolbar->addWidget(addGameBtn);
    toolbar->addWidget(m_rescanBtn);
    vbox->addLayout(toolbar);

    m_cardDelegate = new GameCardDelegate(this);
    m_cardDelegate->setArtCache(&m_artCache);

    m_gridView = new QListView(page);
    m_gridView->setObjectName("gridView");
    m_gridView->setModel(m_proxy);
    m_gridView->setItemDelegate(m_cardDelegate);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setWrapping(true);
    m_gridView->setSpacing(0);
    m_gridView->setUniformItemSizes(true);
    m_gridView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_gridView->setMouseTracking(true);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gridView, &QListView::activated,
            this, &GameVaultWindow::onGridActivated);
    connect(m_gridView, &QListView::doubleClicked,
            this, &GameVaultWindow::onGridActivated);
    connect(m_gridView, &QListView::customContextMenuRequested,
            this, &GameVaultWindow::onGridContextMenu);
    vbox->addWidget(m_gridView, 1);

    m_statusLabel = new QLabel("Scanning library…", page);
    m_statusLabel->setObjectName("statusLabel");
    vbox->addWidget(m_statusLabel);

    return page;
}

QWidget* GameVaultWindow::buildDetailPage() {
    auto* page = new QWidget(this);
    page->setObjectName("detailPage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background-color: #1b2838; border: none; }");

    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(16, 10, 16, 10);
    topBar->setSpacing(12);

    auto* backBtn = new QPushButton("◀  Back to Library", page);
    backBtn->setObjectName("backBtn");
    connect(backBtn, &QPushButton::clicked, this, &GameVaultWindow::goToLibrary);
    topBar->addWidget(backBtn);
    topBar->addStretch(1);

    m_folderBtn = new QPushButton("📂  Browse", page);
    connect(m_folderBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentEntry.installPath.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentEntry.installPath));
    });
    topBar->addWidget(m_folderBtn);

    m_customArtBtn = new QPushButton("🖼  Custom Art", page);
    connect(m_customArtBtn, &QPushButton::clicked, this, [this]() {
        const QString art = QFileDialog::getOpenFileName(
            this,
            QString("Select custom art for %1").arg(m_currentEntry.title),
            {},
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (art.isEmpty()) return;

        const QString key = settingsKeyForEntry(m_currentEntry);
        GameVaultSettings::instance().setCustomArtPath(key, QDir::fromNativeSeparators(art));
        startScan();
    });
    topBar->addWidget(m_customArtBtn);

    m_playBtn = new QPushButton("▶  PLAY", page);
    m_playBtn->setObjectName("playBtn");
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        launchEntry(m_currentEntry);
    });
    topBar->addWidget(m_playBtn);

    outer->addLayout(topBar);

    auto* heroArea = new QWidget(page);
    heroArea->setObjectName("heroArea");
    heroArea->setFixedHeight(310);
    auto* heroLayout = new QVBoxLayout(heroArea);
    heroLayout->setContentsMargins(0, 0, 0, 0);

    m_heroLabel = new QLabel(heroArea);
    m_heroLabel->setAlignment(Qt::AlignCenter);
    m_heroLabel->setMinimumHeight(216);
    m_heroLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_heroLabel->setStyleSheet("background-color: #16202d;");
    heroLayout->addWidget(m_heroLabel, 1);

    m_heroTitle = new QLabel(heroArea);
    m_heroTitle->setObjectName("heroTitle");
    m_heroTitle->setWordWrap(true);
    m_heroTitle->setContentsMargins(20, 8, 20, 8);
    m_heroTitle->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 transparent, stop:1 rgba(0,0,0,180));"
        "color: #ffffff; font-size: 28px; font-weight: bold;");
    heroLayout->addWidget(m_heroTitle);

    outer->addWidget(heroArea);

    auto* body = new QWidget(page);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(30, 20, 30, 30);
    bodyLayout->setSpacing(0);

    m_detailPlatform = new QLabel(body);
    m_detailPlatform->setObjectName("detailPlatform");
    bodyLayout->addWidget(m_detailPlatform);
    bodyLayout->addSpacing(20);

    auto mkSection = [&](const QString& title) {
        auto* lbl = new QLabel(title.toUpper(), body);
        lbl->setObjectName("sectionHeader");
        bodyLayout->addWidget(lbl);
        auto* hr2 = new QFrame(body);
        hr2->setFrameShape(QFrame::HLine);
        hr2->setStyleSheet("color: #2a3f52; margin: 4px 0 12px 0;");
        bodyLayout->addWidget(hr2);
    };

    mkSection("Playtime");

    m_detailPlaytime = new QLabel(body);
    m_detailPlaytime->setObjectName("playtimeBig");
    bodyLayout->addWidget(m_detailPlaytime);

    m_detailLastPlay = new QLabel(body);
    m_detailLastPlay->setObjectName("playtimeSub");
    bodyLayout->addWidget(m_detailLastPlay);

    bodyLayout->addSpacing(24);

    m_achSection = new QFrame(body);
    m_achSection->setFrameShape(QFrame::NoFrame);
    auto* achLayout = new QVBoxLayout(m_achSection);
    achLayout->setContentsMargins(0, 0, 0, 0);
    achLayout->setSpacing(6);

    auto* achHeader = new QLabel("ACHIEVEMENTS", m_achSection);
    achHeader->setObjectName("sectionHeader");
    achLayout->addWidget(achHeader);

    auto* achHr = new QFrame(m_achSection);
    achHr->setFrameShape(QFrame::HLine);
    achHr->setStyleSheet("color: #2a3f52; margin: 4px 0 12px 0;");
    achLayout->addWidget(achHr);

    m_achBar = new QProgressBar(m_achSection);
    m_achBar->setObjectName("achBar");
    m_achBar->setTextVisible(false);
    m_achBar->setFixedHeight(6);
    m_achBar->setRange(0, 100);
    achLayout->addWidget(m_achBar);

    m_achLabel = new QLabel(m_achSection);
    m_achLabel->setObjectName("achLabel");
    achLayout->addWidget(m_achLabel);

    bodyLayout->addWidget(m_achSection);
    bodyLayout->addSpacing(20);

    mkSection("News");
    m_newsBrowser = new QTextBrowser(body);
    m_newsBrowser->setOpenExternalLinks(true);
    m_newsBrowser->setMaximumHeight(180);
    m_newsBrowser->setStyleSheet(
        "background-color:#16202d; color:#c6d4df; border:1px solid #2a3f52; border-radius:4px; padding:6px;");
    m_newsBrowser->setHtml("<span style='color:#8f98a0'>No news loaded.</span>");
    bodyLayout->addWidget(m_newsBrowser);

    bodyLayout->addStretch(1);

    outer->addWidget(body, 1);

    return scroll;
}

void GameVaultWindow::startScan() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Library scan started.");
    m_rescanBtn->setEnabled(false);
    setStatusText("Scanning…");
    m_model->clear();
    m_artCache.clear();
    m_artQueue.clear();
    if (m_cardReply) { m_cardReply->abort(); m_cardReply = nullptr; }

    auto* worker = new GameLibraryWorker(nullptr);
    connect(worker, &GameLibraryWorker::scanComplete,
            this, &GameVaultWindow::onScanComplete, Qt::QueuedConnection);
    connect(worker, &GameLibraryWorker::scanError,
            this, &GameVaultWindow::onScanError,    Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(worker);
}

void GameVaultWindow::rescan() { startScan(); }

void GameVaultWindow::onScanComplete(QVector<wintools::gamevault::GameEntry> games) {
    // Manual entries are persisted separately from scanners; merge them here so the UI is one unified list.
    const QVector<GameEntry> manualGames = GameVaultSettings::instance().manualGames();
    for (GameEntry mg : manualGames) {
        games.push_back(std::move(mg));
    }

    for (GameEntry& g : games) {
        // Apply executable/tracking overrides before merging stats so keys resolve to the same logical game.
        const QString locatorKey = overrideLocatorForEntry(g);
        const QString overrideExe = GameVaultSettings::instance().gameExecutableOverridePath(locatorKey).trimmed();
        if (!overrideExe.isEmpty()) {
            g.executablePath = overrideExe;
            if (g.installPath.trimmed().isEmpty()) {
                g.installPath = QFileInfo(overrideExe).absolutePath();
            }
            g.installed = QFileInfo::exists(overrideExe);
        }

        const QString trackingOverride = GameVaultSettings::instance().gameTrackingIdOverride(locatorKey).trimmed();
        if (!trackingOverride.isEmpty() && g.platformId.trimmed().isEmpty()) {
            g.platformId = trackingOverride;
        }

        const QString plat = platformName(g.platform);
        const QString trackId = trackingIdForEntry(g);
        const quint64 trackedPlaytime = PlaytimeTracker::instance().playtime(plat, trackId);
        const qint64 trackedLast = PlaytimeTracker::instance().lastPlayed(plat, trackId);

        if (trackedPlaytime > g.playtimeSeconds) g.playtimeSeconds = trackedPlaytime;
        if (trackedLast > g.lastPlayedEpoch) g.lastPlayedEpoch = trackedLast;

        applyCustomArtOverride(g);
        applyExecutableIconArtFallback(g, m_artCache);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Library scan complete."),
        QStringLiteral("%1 games found").arg(games.size()));
    m_model->setGames(games);
    m_proxy->sort(GameCol::Title, Qt::AscendingOrder);
    setStatusText(QString("%1 games in library").arg(games.size()));
    m_rescanBtn->setEnabled(true);
    updateSidebarCounts();
    rebuildArtQueue();
    fetchNextCardArt();
}

void GameVaultWindow::onScanError(QString message) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
        QStringLiteral("Library scan error."), message);
    setStatusText("Scan error: " + message);
    m_rescanBtn->setEnabled(true);
}

void GameVaultWindow::onSearchChanged(const QString& text) {
    m_proxy->setSearchText(text);
    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onSidebarItemClicked(int row) {
    if (row < 0) return;
    const QListWidgetItem* item = m_sidebar->item(row);
    if (!item) return;
    if (item->data(kSidebarIsSeparator).toBool()) {

        m_sidebar->clearSelection();
        return;
    }

    const int platVal = item->data(kSidebarPlatformRole).toInt();
    if (platVal == -1)
        m_proxy->clearPlatformFilter();
    else
        m_proxy->setPlatformFilter(static_cast<GamePlatform>(platVal));

    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onInstalledOnlyToggled(bool checked) {
    m_installedOnly = checked;
    m_installedToggle->setText(checked ? "☑  Installed only" : "☐  Installed only");
    m_proxy->setInstalledOnly(checked);
    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onGridActivated(const QModelIndex& idx) {
    if (!idx.isValid()) return;
    const QModelIndex src = m_proxy->mapToSource(idx);
    const GameEntry*  e   = m_model->entryAt(src);
    if (e) goToDetail(*e);
}

void GameVaultWindow::onGridContextMenu(const QPoint& pos) {
    const QModelIndex proxyIdx = m_gridView->indexAt(pos);
    if (!proxyIdx.isValid()) return;
    const GameEntry* e = m_model->entryAt(m_proxy->mapToSource(proxyIdx));
    if (!e) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #32404e; color: #c6d4df; border: 1px solid #4a6174; }"
        "QMenu::item:selected { background-color: #2a475e; }");

    menu.addAction("▶  Play",        this, [this, e]() { launchEntry(*e); });
    menu.addAction("ℹ  View details", this, [this, e]() { goToDetail(*e); });
    menu.addAction("Copy title",      this, [e]() {
        QGuiApplication::clipboard()->setText(e->title);
    });
    if (e->platform == GamePlatform::Unknown) {
        menu.addAction("🛠  Change executable…", this, [this, e]() {
            const QString newExe = QFileDialog::getOpenFileName(
                this,
                QString("Select executable for %1").arg(e->title),
                e->installPath,
                "Executables (*.exe *.bat *.cmd *.lnk);;All files (*)");
            if (newExe.trimmed().isEmpty()) return;

            const QString oldTrackId = trackingIdForEntry(*e);
            auto& settings = GameVaultSettings::instance();

            bool updatedManual = false;
            QVector<GameEntry> manualGames = settings.manualGames();
            for (GameEntry& mg : manualGames) {
                if (settingsKeyForEntry(mg) != settingsKeyForEntry(*e)) {
                    continue;
                }

                mg.executablePath = QDir::fromNativeSeparators(newExe);
                mg.installPath = QFileInfo(newExe).absolutePath();
                mg.installed = QFileInfo::exists(newExe);
                if (mg.platformId.trimmed().isEmpty()) {
                    mg.platformId = oldTrackId;
                }
                updatedManual = true;
                break;
            }

            if (updatedManual) {
                settings.setManualGames(manualGames);
            } else {
                const QString locatorKey = overrideLocatorForEntry(*e);
                settings.setGameExecutableOverride(
                    locatorKey,
                    QDir::fromNativeSeparators(newExe),
                    oldTrackId);
            }

            wintools::logger::Logger::log(
                kLog,
                wintools::logger::Severity::Pass,
                QStringLiteral("Game executable updated."),
                QStringLiteral("title=%1 newExe=%2").arg(e->title, newExe));
            startScan();
        });
    }
    menu.addSeparator();
    menu.addAction("🖼  Set custom art…", this, [this, e]() {
        const QString art = QFileDialog::getOpenFileName(
            this,
            QString("Select custom art for %1").arg(e->title),
            {},
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (art.isEmpty()) return;

        const QString key = settingsKeyForEntry(*e);
        GameVaultSettings::instance().setCustomArtPath(key, QDir::fromNativeSeparators(art));
        startScan();
    });
    menu.addAction("🧹  Clear custom art", this, [this, e]() {
        const QString key = settingsKeyForEntry(*e);
        GameVaultSettings::instance().clearCustomArtPath(key);
        startScan();
    });
    if (!e->installPath.isEmpty()) {
        menu.addAction("📂  Open folder", this, [e]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(e->installPath));
        });
    }
    menu.exec(m_gridView->viewport()->mapToGlobal(pos));
}

void GameVaultWindow::goToDetail(const GameEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Viewing game detail."),
        QStringLiteral("title=%1 platform=%2").arg(e.title).arg(static_cast<int>(e.platform)));
    m_currentEntry = e;

    m_heroTitle->setText(e.title);
    m_heroLabel->setPixmap({});
    m_heroLabel->setText("Loading…");

    QString meta = "<b>" + platformName(e.platform) + "</b>";
    if (!e.systemTag.isEmpty() && e.systemTag != platformName(e.platform))
        meta += "  ·  " + e.systemTag;
    m_detailPlatform->setText(meta);

    const QString platStr = platformName(e.platform);
    quint64 totalPlaytime = e.playtimeSeconds;
    qint64  lastPlayEpoch = e.lastPlayedEpoch;

    const QString trackId = trackingIdForEntry(e);
    quint64 tracked = PlaytimeTracker::instance().playtime(platStr, trackId);
    totalPlaytime = qMax(totalPlaytime, tracked);
    qint64 trackedLast = PlaytimeTracker::instance().lastPlayed(platStr, trackId);
    if (trackedLast > lastPlayEpoch)
        lastPlayEpoch = trackedLast;

    if (totalPlaytime > 0) {
        const quint64 h = totalPlaytime / 3600;
        const quint64 m = (totalPlaytime % 3600) / 60;
        const quint64 s = totalPlaytime % 60;
        if (h > 0) {
            m_detailPlaytime->setText(
                QString("%1.%2 hours on record").arg(h).arg(m / 6));
        } else if (m > 0) {
            m_detailPlaytime->setText(QString("%1 minutes on record").arg(m));
        } else {
            m_detailPlaytime->setText(QString("%1 seconds on record").arg(s));
        }
    } else {
        if (e.platform == GamePlatform::EpicGames) {
            m_detailPlaytime->setText("Epic playtime unavailable");
        } else {
            m_detailPlaytime->setText("No playtime recorded");
        }
    }

    if (lastPlayEpoch > 0)
        m_detailLastPlay->setText(
            "Last played  " +
            QDateTime::fromSecsSinceEpoch(lastPlayEpoch).toString("d MMMM yyyy"));
    else
        m_detailLastPlay->setText("Never played");

    if (e.achievementsTotal > 0) {
        m_achBar->setValue(e.achievementsPercent());
        m_achLabel->setText(
            QString("%1 of %2 achievements  (%3%)")
                .arg(e.achievementsUnlocked)
                .arg(e.achievementsTotal)
                .arg(e.achievementsPercent()));
        m_achSection->setVisible(true);
    } else if (e.platform == GamePlatform::EpicGames) {
        m_achBar->setValue(0);
        m_achLabel->setText("Epic achievements not detected locally for this title.");
        m_achSection->setVisible(true);
    } else {
        m_achSection->setVisible(false);
    }

    m_folderBtn->setVisible(!e.installPath.isEmpty());

    if (m_newsReply) {
        m_newsReply->abort();
        m_newsReply = nullptr;
    }

    if (e.platform == GamePlatform::Steam && !e.platformId.isEmpty()) {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml("<span style='color:#8f98a0'>Loading Steam news…</span>");

        QUrl api("https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/");
        QUrlQuery query;
        query.addQueryItem("appid", e.platformId);
        query.addQueryItem("count", "5");
        query.addQueryItem("maxlength", "180");
        query.addQueryItem("format", "json");
        api.setQuery(query);

        QNetworkRequest req(api);
        req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools-GameVault/1.0");
        m_newsReply = m_nam->get(req);
        connect(m_newsReply, &QNetworkReply::finished, this, [this]() {
            auto* reply = qobject_cast<QNetworkReply*>(sender());
            if (!reply) return;
            reply->deleteLater();
            if (reply != m_newsReply) return;
            m_newsReply = nullptr;

            if (reply->error() != QNetworkReply::NoError) {
                m_newsBrowser->setHtml("<span style='color:#8f98a0'>Unable to load Steam news.</span>");
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray items = root.value("appnews").toObject().value("newsitems").toArray();
            if (items.isEmpty()) {
                m_newsBrowser->setHtml("<span style='color:#8f98a0'>No recent Steam news for this game.</span>");
                return;
            }

            QString html;
            for (const QJsonValue& value : items) {
                const QJsonObject n = value.toObject();
                const QString title = n.value("title").toString().toHtmlEscaped();
                const QString url = n.value("url").toString();
                const qint64 epoch = n.value("date").toInteger();
                const QString date = epoch > 0
                    ? QDateTime::fromSecsSinceEpoch(epoch).toString("d MMM yyyy")
                    : QString();

                html += QString("<div style='margin-bottom:8px'><a href='%1' style='color:#66c0f4;text-decoration:none'><b>%2</b></a><br><span style='color:#8f98a0;font-size:11px'>%3</span></div>")
                    .arg(url, title, date);
            }
            m_newsBrowser->setHtml(html);
        });
    } else if (e.platform == GamePlatform::EpicGames) {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml("<span style='color:#8f98a0'>Loading Epic Games Store news…</span>");

        QNetworkRequest req(QUrl("https://store.epicgames.com/en-US/news/rss"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools-GameVault/1.0");
        m_newsReply = m_nam->get(req);
        connect(m_newsReply, &QNetworkReply::finished, this, [this, title = e.title]() {
            auto* reply = qobject_cast<QNetworkReply*>(sender());
            if (!reply) return;
            reply->deleteLater();
            if (reply != m_newsReply) return;
            m_newsReply = nullptr;

            if (reply->error() != QNetworkReply::NoError) {
                m_newsBrowser->setHtml(
                    QString("<span style='color:#8f98a0'>Unable to load Epic news feed. <a href='https://store.epicgames.com/en-US/news' style='color:#66c0f4'>Open Epic News</a></span>"));
                return;
            }

            QXmlStreamReader xml(reply->readAll());
            struct Item { QString t; QString l; QString d; };
            QVector<Item> items;
            Item current;
            bool inItem = false;

            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == "item") {
                    inItem = true;
                    current = Item{};
                } else if (xml.isEndElement() && xml.name() == "item") {
                    inItem = false;
                    if (!current.t.isEmpty() && !current.l.isEmpty()) {
                        items.push_back(current);
                    }
                } else if (inItem && xml.isStartElement()) {
                    if (xml.name() == "title") current.t = xml.readElementText();
                    else if (xml.name() == "link") current.l = xml.readElementText();
                    else if (xml.name() == "pubDate") current.d = xml.readElementText();
                }
            }

            if (items.isEmpty()) {
                m_newsBrowser->setHtml(
                    "<span style='color:#8f98a0'>No Epic news items found. <a href='https://store.epicgames.com/en-US/news' style='color:#66c0f4'>Open Epic News</a></span>");
                return;
            }

            const QStringList tokens = newsTokensForTitle(title);
            QString html;
            int count = 0;
            for (const Item& item : items) {
                const QString tEsc = item.t.toHtmlEscaped();
                if (!epicNewsItemMatchesGame(item.t, item.l, tokens)) {
                    continue;
                }
                html += QString("<div style='margin-bottom:8px'><a href='%1' style='color:#66c0f4;text-decoration:none'><b>%2</b></a><br><span style='color:#8f98a0;font-size:11px'>%3</span></div>")
                    .arg(item.l, tEsc, item.d.toHtmlEscaped());
                ++count;
                if (count >= 5) break;
            }

            if (html.isEmpty()) {
                m_newsBrowser->setHtml(
                    "<span style='color:#8f98a0'>No game-specific Epic news found for this title. <a href='https://store.epicgames.com/en-US/news' style='color:#66c0f4'>Open Epic News</a></span>");
                return;
            }

            m_newsBrowser->setHtml(html);
        });
    } else {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml("<span style='color:#8f98a0'>News feed is available for Steam titles.</span>");
    }

    m_stack->setCurrentIndex(kDetailPage);

    const QString artUrl = e.artBannerUrl.isEmpty() ? e.artCapsuleUrl : e.artBannerUrl;
    if (!artUrl.isEmpty()) {
        if (m_artCache.contains(artUrl)) {

            const QPixmap& px = m_artCache[artUrl];
            m_heroLabel->setPixmap(
                px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation));
        } else {
            fetchDetailBanner(artUrl);
        }
    } else {
        QString iconSource = e.iconPath;
        if (iconSource.isEmpty()) iconSource = e.executablePath;

        if (!iconSource.isEmpty()) {
            QFileIconProvider provider;
            const QIcon icon = provider.icon(QFileInfo(iconSource));
            const QPixmap iconPx = icon.pixmap(160, 160);
            if (!iconPx.isNull()) {
                m_heroLabel->setPixmap(iconPx);
            } else {
                m_heroLabel->setText(platformName(e.platform));
            }
        } else {
            m_heroLabel->setText(platformName(e.platform));
        }
    }
}

void GameVaultWindow::goToLibrary() {
    if (m_bannerReply) { m_bannerReply->abort(); m_bannerReply = nullptr; }
    m_stack->setCurrentIndex(kLibraryPage);
}

void GameVaultWindow::updateSidebarCounts() {

    QHash<int, int> counts;
    int total = 0;
    for (int r = 0; r < m_model->rowCount(); ++r) {
        const GameEntry* e = m_model->entryAt(r);
        if (!e) continue;
        ++counts[static_cast<int>(e->platform)];
        ++total;
    }

    if (m_allGamesItem)
        m_allGamesItem->setText(QString("ALL GAMES  (%1)").arg(total));

    for (auto it = m_platformItems.begin(); it != m_platformItems.end(); ++it) {
        const int platKey = it.key();
        const int cnt     = counts.value(platKey, 0);
        QListWidgetItem* item = it.value();
        if (!item) continue;
        if (cnt == 0) {
            item->setHidden(true);
        } else {
            const QString name = platformName(static_cast<GamePlatform>(platKey));
            item->setText(QString("  %1  (%2)").arg(name).arg(cnt));
            item->setHidden(false);
        }
    }
}

void GameVaultWindow::rebuildArtQueue() {
    m_artQueue.clear();
    for (int r = 0; r < m_model->rowCount(); ++r) {
        const GameEntry* e = m_model->entryAt(r);
        if (!e) continue;
        const QString url = e->artBannerUrl.isEmpty() ? e->artCapsuleUrl : e->artBannerUrl;
        if (!url.isEmpty() && !m_artCache.contains(url))
            m_artQueue << url;
    }

    // Multiple games can reference the same CDN asset; dedup prevents redundant network requests.
    m_artQueue.removeDuplicates();
}

void GameVaultWindow::fetchNextCardArt() {
    if (m_cardReply || m_artQueue.isEmpty()) return;

    const QString url = m_artQueue.takeFirst();
    m_cardReplyUrl = url;

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_cardReply = m_nam->get(req);
    connect(m_cardReply, &QNetworkReply::finished,
            this, &GameVaultWindow::onCardArtLoaded);
}

void GameVaultWindow::onCardArtLoaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_cardReply) return;
    m_cardReply = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap px;
        if (px.loadFromData(reply->readAll()) && !px.isNull())
            m_artCache[m_cardReplyUrl] = px;
    }

    // Repaint only cards that use the downloaded URL to keep large grid updates cheap.
    QRect dirty;
    if (!m_cardReplyUrl.isEmpty()) {
        for (int r = 0; r < m_proxy->rowCount(); ++r) {
            const QModelIndex proxyIdx = m_proxy->index(r, 0);
            if (!proxyIdx.isValid()) continue;
            const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
            const GameEntry* e = m_model->entryAt(srcIdx);
            if (!e) continue;
            const QString cardUrl = e->artBannerUrl.isEmpty() ? e->artCapsuleUrl : e->artBannerUrl;
            if (cardUrl != m_cardReplyUrl) continue;

            const QRect rect = m_gridView->visualRect(proxyIdx);
            if (!rect.isValid() || rect.isEmpty()) continue;
            dirty = dirty.isNull() ? rect : dirty.united(rect);
        }
    }
    if (!dirty.isNull()) {
        m_gridView->viewport()->update(dirty.adjusted(-2, -2, 2, 2));
    } else {
        m_gridView->viewport()->update();
    }

    // If detail view is open for the same title, refresh hero art immediately from cache.
    const QString curUrl = m_currentEntry.artBannerUrl.isEmpty()
                           ? m_currentEntry.artCapsuleUrl
                           : m_currentEntry.artBannerUrl;
    if (m_stack->currentIndex() == kDetailPage && curUrl == m_cardReplyUrl
        && m_artCache.contains(curUrl)) {
        const QPixmap& px = m_artCache[curUrl];
        m_heroLabel->setPixmap(
            px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    }

    fetchNextCardArt();
}

void GameVaultWindow::fetchDetailBanner(const QString& url) {
    if (m_bannerReply) { m_bannerReply->abort(); m_bannerReply = nullptr; }

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_bannerReply = m_nam->get(req);
    connect(m_bannerReply, &QNetworkReply::finished,
            this, &GameVaultWindow::onBannerLoaded);
}

void GameVaultWindow::onBannerLoaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_bannerReply) return;
    m_bannerReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        m_heroLabel->setText("(no art)");
        return;
    }

    QPixmap px;
    if (px.loadFromData(reply->readAll()) && !px.isNull()) {
        const QString url = m_currentEntry.artBannerUrl.isEmpty()
                            ? m_currentEntry.artCapsuleUrl
                            : m_currentEntry.artBannerUrl;
        m_artCache[url] = px;
        m_heroLabel->setPixmap(
            px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    } else {
        m_heroLabel->setText("(no art)");
    }
}

void GameVaultWindow::openSettings() {
    auto* dlg = new QDialog(this);
    dlg->setObjectName("settingsDlg");
    dlg->setWindowTitle("Game Vault – Scan Paths");
    dlg->setMinimumSize(540, 420);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(styleSheet());

    auto& settings = GameVaultSettings::instance();
    auto* layout   = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* folderLbl = new QLabel(
        QStringLiteral("<b style='color:%1'>Custom game folders</b><br>"
                       "<span style='color:%2;font-size:11px'>"
                       "Each folder is scanned for game sub-directories and executables.</span>")
            .arg(m_palette.foreground.name(), m_palette.mutedForeground.name()),
        dlg);
    folderLbl->setTextFormat(Qt::RichText);
    layout->addWidget(folderLbl);

    auto* folderList = new QListWidget(dlg);
    folderList->addItems(settings.customGameFolders());
    layout->addWidget(folderList, 1);

    auto* folderBtns = new QHBoxLayout;
    auto* addFolderBtn = new QPushButton("+ Add folder…", dlg);
    auto* remFolderBtn = new QPushButton("− Remove",       dlg);
    folderBtns->addWidget(addFolderBtn);
    folderBtns->addWidget(remFolderBtn);
    folderBtns->addStretch();
    layout->addLayout(folderBtns);

    connect(addFolderBtn, &QPushButton::clicked, dlg, [=, &settings]() {
        const QString dir = QFileDialog::getExistingDirectory(dlg, "Select game folder");
        if (!dir.isEmpty()) {
            settings.addCustomGameFolder(dir);
            folderList->addItem(dir);
        }
    });
    connect(remFolderBtn, &QPushButton::clicked, dlg, [=, &settings]() {
        for (auto* item : folderList->selectedItems()) {
            settings.removeCustomGameFolder(item->text());
            delete item;
        }
    });

    layout->addWidget(new QLabel(
        "<b style='color:#c6d4df'>Emulator executable overrides</b><br>"
        "<span style='color:#8f98a0;font-size:11px'>"
        "Leave blank to auto-detect.</span>", dlg));

    const QStringList emulatorNames = {"RPCS3", "Yuzu", "Ryujinx", "Dolphin", "DeSmuME"};
    QHash<QString, QLineEdit*> emuEdits;

    auto* emuWidget = new QWidget(dlg);
    auto* emuGrid   = new QVBoxLayout(emuWidget);
    emuGrid->setSpacing(4);
    for (const QString& name : emulatorNames) {
        auto* row    = new QHBoxLayout;
        auto* lbl    = new QLabel(name + ":", emuWidget);
        lbl->setFixedWidth(80);
        lbl->setStyleSheet("color:#8f98a0");
        auto* edit   = new QLineEdit(settings.emulatorPath(name), emuWidget);
        edit->setPlaceholderText("Auto-detect");
        auto* browse = new QPushButton("…", emuWidget);
        browse->setFixedWidth(30);
        connect(browse, &QPushButton::clicked, dlg, [edit, dlg, name]() {
            const QString p = QFileDialog::getOpenFileName(
                dlg, "Select " + name, {}, "Executables (*.exe)");
            if (!p.isEmpty()) edit->setText(p);
        });
        row->addWidget(lbl);
        row->addWidget(edit, 1);
        row->addWidget(browse);
        emuGrid->addLayout(row);
        emuEdits[name] = edit;
    }
    layout->addWidget(emuWidget);

    auto* bbx = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(bbx);

    connect(bbx, &QDialogButtonBox::accepted, dlg, [=, &settings]() {
        for (const QString& name : emulatorNames) {
            const QString p = emuEdits[name]->text().trimmed();
            if (p.isEmpty()) settings.clearEmulatorPath(name);
            else             settings.setEmulatorPath(name, p);
        }
        dlg->accept();
    });
    connect(bbx, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    dlg->exec();
}

void GameVaultWindow::setStatusText(const QString& text) {
    if (m_statusLabel) m_statusLabel->setText(text);
}

void GameVaultWindow::onThemeChanged(bool) {
    applyTheme(wintools::themes::ThemeHelper::currentPalette());
}

void GameVaultWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    using wintools::themes::FluentStyle;
    (void)kSteamQss;
    m_palette = palette;

    const QString supplement = QStringLiteral(
        "QDialog { background-color: %1; color: %2; }"
        "QWidget#sidebarPanel { background-color: %3; }"
        "QFrame#divider { background-color: %8; }"
        "QLabel#sidebarHeader { color: %2; }"
        "QListWidget#sidebar { background-color: %3; border: none; outline: none; font-size: 13px; }"
        "QListWidget#sidebar::item { color: %4; padding: 6px 16px; border-radius: 6px; }"
        "QListWidget#sidebar::item:selected { background-color: %5; color: %2; }"
        "QListWidget#sidebar::item:hover:!selected { background-color: %6; color: %2; }"
        "QListView#gridView { background-color: %1; border: none; outline: none; }"
        "QWidget#detailPage { background-color: %1; }"
        "QWidget#heroArea { background-color: %7; }"
        "QLabel#heroTitle { color: %2; font-size: 28px; font-weight: bold; }"
        "QLabel#detailPlatform, QLabel#playtimeSub, QLabel#achLabel, QLabel#sectionHeader, QLabel#statusLabel { color: %4; }"
        "QLabel#playtimeBig { color: %2; font-size: 22px; font-weight: bold; }"
        "QProgressBar#achBar { border: none; background-color: %8; border-radius: 3px; max-height: 6px; }"
        "QProgressBar#achBar::chunk { background-color: %5; border-radius: 3px; }"
    ).arg(
        palette.windowBackground.name(),
        palette.foreground.name(),
        palette.cardBackground.name(),
        palette.mutedForeground.name(),
        palette.accent.name(),
        palette.hoverBackground.name(),
        palette.cardBackground.name(),
        palette.cardBorder.name());

    setStyleSheet(FluentStyle::generate(palette) + supplement);
}

void GameVaultWindow::launchEntry(const GameEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Launching game."),
        QStringLiteral("title=%1").arg(e.title));

    const QString platStr = platformName(e.platform);
    bool launched = false;
    QString emulatorPath = e.emulatorPath;

    if (emulatorPath.isEmpty() && e.platform == GamePlatform::DeSmuME) {
        const QString overridePath = GameVaultSettings::instance().emulatorPath("DeSmuME").trimmed();
        if (!overridePath.isEmpty() && QFileInfo::exists(overridePath)) {
            emulatorPath = overridePath;
        } else {
            const QStringList candidates = {
                qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
                qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"),
                qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
                qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Program Files/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Program Files (x86)/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Emulators/DeSmuME/DeSmuME.exe")
            };
            for (const QString& candidate : candidates) {
                if (!candidate.trimmed().isEmpty() && QFileInfo::exists(candidate)) {
                    emulatorPath = candidate;
                    break;
                }
            }
        }
    }

    if (!e.launchUri.isEmpty()) {
        launched = QDesktopServices::openUrl(QUrl(e.launchUri));
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to open launch URI for \"%1\".\n\nURI: %2")
                    .arg(e.title, e.launchUri));
        }
    } else if (!emulatorPath.isEmpty() && !e.executablePath.isEmpty()) {
        if (!QFileInfo::exists(emulatorPath)) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Emulator executable not found for \"%1\".\n\nEmulator: %2\nROM: %3")
                    .arg(e.title, emulatorPath, e.executablePath));
            return;
        }

        QStringList args = e.emulatorArgs;
        args << e.executablePath;
        const QString workingDir = QFileInfo(emulatorPath).absolutePath();
        launched = QProcess::startDetached(emulatorPath, args, workingDir);
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to start emulator for \"%1\".\n\nEmulator: %2\nROM: %3")
                    .arg(e.title, emulatorPath, e.executablePath));
        }
    } else if (!e.executablePath.isEmpty()) {
        launched = QProcess::startDetached(e.executablePath, {});
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to start \"%1\".\n\nPath: %2")
                    .arg(e.title, e.executablePath));
        }
    } else {
        QMessageBox::warning(this, "Launch Failed",
            QString("No launch method available for \"%1\".").arg(e.title));
    }

    if (launched) {
        PlaytimeTracker::instance().startSession(platStr, trackingIdForEntry(e));
    }
}

void GameVaultWindow::addCustomGame() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add Custom Game / App");
    dlg->setMinimumWidth(440);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(styleSheet());

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto mkRow = [&](const QString& label) -> QLineEdit* {
        auto* lbl = new QLabel(label, dlg);
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_palette.mutedForeground.name()));
        layout->addWidget(lbl);
        auto* le = new QLineEdit(dlg);
        layout->addWidget(le);
        return le;
    };

    auto* titleEdit = mkRow("Title *");
    auto* exeEdit   = mkRow("Executable path");
    auto* artEdit   = mkRow("Art URL (banner / capsule)");

    auto* browseBtn = new QPushButton("Browse…", dlg);
    browseBtn->setFixedWidth(90);
    connect(browseBtn, &QPushButton::clicked, dlg, [exeEdit, dlg]() {
        const QString path = QFileDialog::getOpenFileName(dlg,
            "Select executable", {}, "Executables (*.exe *.bat *.cmd *.lnk);;All files (*)");
        if (!path.isEmpty())
            exeEdit->setText(path);
    });

    auto* browseRow = new QHBoxLayout;
    browseRow->addStretch();
    browseRow->addWidget(browseBtn);
    layout->addLayout(browseRow);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, titleEdit, exeEdit, artEdit]() {
        const QString title = titleEdit->text().trimmed();
        if (title.isEmpty()) {
            QMessageBox::warning(dlg, "Missing title",
                "Please enter a game title.");
            return;
        }

        GameEntry entry;
        entry.title = title;
        entry.platform = GamePlatform::Unknown;
        entry.platformId = "custom_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        entry.executablePath = exeEdit->text().trimmed();
        entry.installed = !entry.executablePath.isEmpty();

        const QString artUrl = artEdit->text().trimmed();
        if (!artUrl.isEmpty())
            entry.artBannerUrl = artUrl;

        if (!entry.executablePath.isEmpty()) {
            QFileInfo fi(entry.executablePath);
            entry.installPath = fi.absolutePath();
        }

        m_model->addGame(entry);
        GameVaultSettings::instance().addOrUpdateManualGame(entry);
        updateSidebarCounts();
        dlg->accept();

        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Custom game added."),
            QStringLiteral("title=%1").arg(title));
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
}

}
