#include "streamvault_window.hpp"
#include "stream_card_delegate.hpp"
#include "logger/logger.hpp"

#include "modules/StreamVault/src/core/stream_entry.hpp"
#include "modules/StreamVault/src/core/streaming_service.hpp"
#include "modules/StreamVault/src/core/streamvault_settings.hpp"
#include "modules/StreamVault/src/core/tmdb_client.hpp"
#include "modules/StreamVault/src/core/watchlist_store.hpp"
#include "modules/StreamVault/src/core/episode_progress_store.hpp"
#include "modules/StreamVault/src/model/stream_model.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/ui/screen_relative_size.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace wintools::streamvault {

static constexpr const char* kLog = "StreamVault/Window";

void disableDialogDefaultButtonBehavior(QPushButton* button) {
    if (!button) {
        return;
    }
    button->setAutoDefault(false);
    button->setDefault(false);
}

StreamVaultWindow::StreamVaultWindow(QWidget* parent)
    : QDialog(parent, Qt::Window)
    , m_tmdb(new TmdbClient(this))
    , m_imgNam(new QNetworkAccessManager(this))
    , m_debounce(new QTimer(this))
    , m_themeListener(new wintools::themes::ThemeListener(this))
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setWindowTitle("Stream Vault");
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/streamvault.svg")));
    resize(1100, 720);
    setMinimumSize(820, 560);
    wintools::ui::enableRelativeSizeAcrossScreens(this);

    const QString apiKey = StreamVaultSettings::instance().tmdbApiKey();
    m_tmdb->setApiKey(apiKey);

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(400);

    connect(m_tmdb, &TmdbClient::searchFinished,
            this,   &StreamVaultWindow::onSearchResults);
    connect(m_tmdb, &TmdbClient::error,
            this,   &StreamVaultWindow::onSearchError);
    connect(m_tmdb, &TmdbClient::posterLoaded,
            this,   &StreamVaultWindow::onPosterLoaded);
    connect(m_tmdb, &TmdbClient::watchProvidersLoaded,
            this,   &StreamVaultWindow::onWatchProvidersLoaded);
    connect(m_tmdb, &TmdbClient::tvSeasonsLoaded,
            this,   &StreamVaultWindow::onTvSeasonsLoaded);
    connect(m_tmdb, &TmdbClient::seasonEpisodesLoaded,
            this,   &StreamVaultWindow::onSeasonEpisodesLoaded);
    connect(m_tmdb, &TmdbClient::externalIdsLoaded,
            this,   &StreamVaultWindow::onExternalIdsLoaded);
    connect(m_debounce, &QTimer::timeout,
            this,       &StreamVaultWindow::onSearchTriggered);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this,            &StreamVaultWindow::onThemeChanged);
    connect(&WatchlistStore::instance(), &WatchlistStore::changed,
            this, &StreamVaultWindow::onWatchlistChanged);

    buildUi();
    applyTheme(m_palette);
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "StreamVaultWindow opened.",
        apiKey.isEmpty() ? "No API key" : "API key present");
}

StreamVaultWindow::~StreamVaultWindow() = default;

void StreamVaultWindow::onThemeChanged(bool) {
    applyTheme(wintools::themes::ThemeHelper::currentPalette());

    if (m_stack && m_stack->currentIndex() == 1 && m_currentEntry.tmdbId > 0) {
        populateDetail(m_currentEntry);
        const QString code = m_countryCombo ? m_countryCombo->currentData().toString() : QString();
        if (!code.isEmpty()) {
            populateServiceButtons(code);
        }
    }
}

void StreamVaultWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    using wintools::themes::FluentStyle;
    m_palette = palette;

    const QString supplement = QStringLiteral(

        "QWidget#detailPage { background-color: %7; }"
        "QWidget#detailInfoWidget { background-color: %7; }"
        "QScrollArea#detailScrollArea { background-color: %7; border: none; }"
        "QScrollArea#detailScrollArea > QWidget > QWidget { background-color: %7; }"

        "QWidget#sidebarWidget { background-color: %1; }"

        "QLabel#sidebarTitle { color: %5; font-size: 15px; font-weight: bold;"
        "  padding: 18px 14px 14px 14px; background: transparent; }"
        "QPushButton#serviceNameBtn {"
        "  background: transparent; border: none; color: %5;"
        "  text-align: left; font-size: 13px; padding: 3px 4px;"
        "}"
        "QPushButton#serviceNameBtn:hover { color: %3; }"
        "QFrame#sidebarDivider { background-color: %6; min-height: 1px; max-height: 1px; margin: 10px 10px; }"

        "QListWidget#sidebar { background-color: %1; border: none; outline: none; font-size: 13px; }"
        "QListWidget#sidebar::item { color: %2; padding: 5px 12px; border-radius: 6px; }"
        "QListWidget#sidebar::item:selected { background-color: %3; color: #ffffff; }"
        "QListWidget#sidebar::item:hover:!selected { background-color: %4; color: %5; }"

        "QPushButton#backBtn { background: transparent; border: none; color: %2; font-size: 13px; padding: 4px 10px; }"
        "QPushButton#backBtn:hover { color: %3; }"

        "QPushButton#settingsBtn { background: transparent; border: 1px solid %6; color: %2; font-size: 13px; padding: 5px 10px; }"
        "QPushButton#settingsBtn:hover { color: %3; border-color: %3; }"

        "QPushButton#filterBtn { background: transparent; border: 1px solid %6; border-radius: 6px; color: %2; font-size: 12px; padding: 3px 10px; }"
        "QPushButton#filterBtn:checked { background-color: %3; border-color: %3; color: #ffffff; }"

        "QPushButton#serviceBtn { background-color: %1; border: 1px solid %6; border-radius: 8px; color: %5; font-size: 12px; padding: 6px 16px; min-width: 100px; }"
        "QPushButton#serviceBtn:hover { border-color: %3; color: %3; }"

        "QListView#grid { background-color: %7; border: none; outline: none; }"
        "QListView#grid::item { margin: 5px; border-radius: 8px; }"

        "QLabel#sectionHeader { color: %2; font-size: 11px; font-weight: bold; letter-spacing: 1px; padding: 8px 12px 2px 12px; }"

        "QLabel#statusLabel { color: %2; font-size: 12px; padding: 4px 0; }"

        "QLabel#detailTitle { color: %5; font-size: 20px; font-weight: bold; padding: 0; }"
        "QLabel#detailMeta { color: %2; font-size: 13px; }"
        "QLabel#detailOverview { color: %5; font-size: 13px; }"
        "QLabel#watchOnLabel { color: %2; font-size: 12px; font-weight: bold; letter-spacing: 1px; padding-top: 8px; }"
        "QLabel#backdropLabel { background-color: %1; color: %2; font-size: 13px; }"

        "QComboBox#countryCombo {"
        "  background-color: %1; color: %5; border: 1px solid %6;"
        "  border-radius: 6px; padding: 4px 10px; min-height: 26px;"
        "}"
        "QComboBox#countryCombo::drop-down { border: none; width: 18px; }"
        "QComboBox#countryCombo QAbstractItemView {"
        "  background-color: %1; color: %5; border: 1px solid %6;"
        "  selection-background-color: %4; selection-color: %5;"
        "}"

        "QPushButton#sidebarOpenBtn { background: transparent; border: none; color: %2; font-size: 12px; padding: 2px 6px; }"
        "QPushButton#sidebarOpenBtn:hover { color: %3; }"

        "QPushButton#watchlistBtn { background: transparent; border: 1px solid %6; border-radius: 6px; color: %5; font-size: 13px; padding: 5px 12px; }"
        "QPushButton#watchlistBtn:hover { border-color: %3; color: %3; }"

        "QPushButton#watchlistSidebarBtn { background: transparent; border: 1px solid %6; border-radius: 6px; color: %2; font-size: 12px; padding: 4px 10px; text-align: left; }"
        "QPushButton#watchlistSidebarBtn:hover { border-color: %3; color: %3; }"

        "QFrame#divider { background-color: %6; }"
    ).arg(
        palette.cardBackground.name(),
        palette.mutedForeground.name(),
        palette.accent.name(),
        palette.hoverBackground.name(),
        palette.foreground.name(),
        palette.cardBorder.name(),
        palette.windowBackground.name()
    );

    wintools::themes::ThemeHelper::applyThemeTo(this, supplement);

    if (m_delegate) {
        m_delegate->setThemePalette(m_palette);
    }
    if (m_grid && m_grid->viewport()) {
        m_grid->viewport()->update();
    }
}

void StreamVaultWindow::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_sidebarWidget = buildSidebar();
    root->addWidget(m_sidebarWidget);

    auto* sep = new QFrame(this);
    sep->setObjectName("divider");
    sep->setFixedWidth(1);
    sep->setFrameShape(QFrame::VLine);
    root->addWidget(sep);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildSearchPage());
    m_stack->addWidget(buildDetailPage());
    root->addWidget(m_stack, 1);
}

void StreamVaultWindow::refreshSidebar() {
    auto* root = qobject_cast<QHBoxLayout*>(layout());
    if (!root || !m_sidebarWidget) {
        return;
    }

    const int index = root->indexOf(m_sidebarWidget);
    if (index < 0) {
        return;
    }

    QWidget* oldSidebar = m_sidebarWidget;
    m_sidebarWidget = buildSidebar();
    root->insertWidget(index, m_sidebarWidget);
    root->removeWidget(oldSidebar);
    oldSidebar->deleteLater();
}

QVector<ServiceInfo> StreamVaultWindow::availableServices() const {
    QVector<ServiceInfo> services = allServices();
    const QVector<ServiceInfo> custom = StreamVaultSettings::instance().customServices();
    for (const ServiceInfo& svc : custom) {
        if (svc.name.trimmed().isEmpty() || svc.searchUrlTemplate.trimmed().isEmpty()) {
            continue;
        }
        services.push_back(svc);
    }
    return services;
}

QWidget* StreamVaultWindow::buildSidebar() {
    auto* w      = new QWidget(this);
    w->setFixedWidth(220);
    w->setObjectName("sidebarWidget");

    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    auto* header = new QLabel("STREAM VAULT", w);
    header->setObjectName("sidebarTitle");
    vbox->addWidget(header);

    auto* servicesLabel = new QLabel("SERVICES", w);
    servicesLabel->setObjectName("sectionHeader");
    vbox->addWidget(servicesLabel);

    const QVector<ServiceInfo> services = availableServices();
    for (const ServiceInfo& svc : services) {
        auto* row  = new QWidget(w);
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(10, 2, 6, 2);
        hbox->setSpacing(4);

        auto* dot = new QLabel("●", row);
        dot->setStyleSheet(
            QString("color: %1; font-size: 10px;").arg(svc.accentColor));
        dot->setFixedWidth(14);
        hbox->addWidget(dot);

        auto* nameBtn = new QPushButton(svc.name, row);
        nameBtn->setObjectName("serviceNameBtn");
        disableDialogDefaultButtonBehavior(nameBtn);
        nameBtn->setCursor(Qt::PointingHandCursor);
        connect(nameBtn, &QPushButton::clicked, this, [this, login = svc.loginUrl, home = svc.homeUrl]() {
            if (!login.trimmed().isEmpty()) {
                openUrl(login);
                return;
            }
            if (!home.trimmed().isEmpty()) {
                openUrl(home);
            }
        });
        hbox->addWidget(nameBtn, 1);

        auto* goBtn = new QPushButton("→", row);
        goBtn->setObjectName("sidebarOpenBtn");
        disableDialogDefaultButtonBehavior(goBtn);
        goBtn->setToolTip(QString("Open %1").arg(svc.name));
        goBtn->setCursor(Qt::PointingHandCursor);
        connect(goBtn, &QPushButton::clicked, this, [this, home = svc.homeUrl, searchTemplate = svc.searchUrlTemplate]() {
            if (!home.trimmed().isEmpty()) {
                openUrl(home);
                return;
            }
            if (!searchTemplate.trimmed().isEmpty()) {
                openUrl(searchTemplate.arg(QString()));
            }
        });
        hbox->addWidget(goBtn);

        vbox->addWidget(row);
    }

    auto* div = new QFrame(w);
    div->setObjectName("sidebarDivider");
    div->setFixedHeight(1);
    vbox->addWidget(div);

    auto* filterLabel = new QLabel("FILTER", w);
    filterLabel->setObjectName("sectionHeader");
    vbox->addWidget(filterLabel);

    auto* filterRow = new QWidget(w);
    auto* filterBox = new QHBoxLayout(filterRow);
    filterBox->setContentsMargins(10, 2, 10, 2);
    filterBox->setSpacing(6);

    auto makeFilterBtn = [&](const QString& text, std::function<void()> fn) {
        auto* btn = new QPushButton(text, filterRow);
        btn->setObjectName("filterBtn");
        disableDialogDefaultButtonBehavior(btn);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [fn] { fn(); });
        filterBox->addWidget(btn);
        return btn;
    };

    auto* allBtn = makeFilterBtn("All", [this]() {
        m_proxy->clearMediaTypeFilter();
    });
    allBtn->setChecked(true);

    makeFilterBtn("Movies", [this]() {
        m_proxy->setMediaTypeFilter(MediaType::Movie);
    });
    makeFilterBtn("TV", [this]() {
        m_proxy->setMediaTypeFilter(MediaType::TvShow);
    });

    filterBox->addStretch();
    vbox->addWidget(filterRow);

    auto* div2 = new QFrame(w);
    div2->setObjectName("sidebarDivider");
    div2->setFixedHeight(1);
    vbox->addWidget(div2);

    auto* watchlistLabel = new QLabel("WATCHLIST", w);
    watchlistLabel->setObjectName("sectionHeader");
    vbox->addWidget(watchlistLabel);

    auto* watchlistRow = new QWidget(w);
    auto* watchlistBox = new QHBoxLayout(watchlistRow);
    watchlistBox->setContentsMargins(10, 2, 10, 2);
    watchlistBox->setSpacing(6);

    const int wlCount = WatchlistStore::instance().count();
    m_watchlistSidebarBtn = new QPushButton(
        QStringLiteral("★ My Watchlist (%1)").arg(wlCount), watchlistRow);
    m_watchlistSidebarBtn->setObjectName("watchlistSidebarBtn");
    disableDialogDefaultButtonBehavior(m_watchlistSidebarBtn);
    m_watchlistSidebarBtn->setCursor(Qt::PointingHandCursor);
    connect(m_watchlistSidebarBtn, &QPushButton::clicked,
            this, &StreamVaultWindow::showWatchlist);
    watchlistBox->addWidget(m_watchlistSidebarBtn, 1);
    vbox->addWidget(watchlistRow);

    vbox->addStretch();

    return w;
}

QWidget* StreamVaultWindow::buildSearchPage() {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 14, 16, 10);
    vbox->setSpacing(8);

    auto* topBar = new QWidget(page);
    auto* topHbox = new QHBoxLayout(topBar);
    topHbox->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit(topBar);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText("Search movies & shows\u2026");
    m_searchEdit->setMinimumHeight(36);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        m_debounce->start();
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        m_debounce->stop();
        onSearchTriggered();
    });
    topHbox->addWidget(m_searchEdit, 1);

    auto* settingsBtn = new QPushButton("Settings", topBar);
    settingsBtn->setObjectName("settingsBtn");
    disableDialogDefaultButtonBehavior(settingsBtn);
    settingsBtn->setMinimumHeight(36);
    connect(settingsBtn, &QPushButton::clicked, this, &StreamVaultWindow::openSettings);
    topHbox->addWidget(settingsBtn);

    vbox->addWidget(topBar);

    m_statusLabel = new QLabel("Enter a title to search across streaming services.", page);
    m_statusLabel->setObjectName("statusLabel");
    vbox->addWidget(m_statusLabel);

    m_model    = new StreamListModel(this);
    m_proxy    = new StreamFilterProxy(this);
    m_proxy->setSourceModel(m_model);

    m_delegate = new StreamCardDelegate(this);
    m_delegate->setThemePalette(m_palette);
    m_delegate->setPosterCache(&m_posterCache);

    m_grid = new QListView(page);
    m_grid->setObjectName("grid");
    m_grid->setModel(m_proxy);
    m_grid->setItemDelegate(m_delegate);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setWrapping(true);
    m_grid->setSpacing(4);
    m_grid->setUniformItemSizes(true);
    m_grid->setMouseTracking(true);
    m_grid->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_grid, &QListView::activated,
            this,   &StreamVaultWindow::onGridActivated);
    connect(m_grid, &QListView::doubleClicked,
            this,   &StreamVaultWindow::onGridActivated);
    connect(m_grid, &QListView::customContextMenuRequested,
            this,   &StreamVaultWindow::onGridContextMenu);

    vbox->addWidget(m_grid, 1);

    return page;
}

QWidget* StreamVaultWindow::buildDetailPage() {
    auto* page = new QWidget(this);
    page->setObjectName("detailPage");
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    auto* topBar = new QWidget(page);
    topBar->setObjectName("sidebarWidget");
    auto* topHbox = new QHBoxLayout(topBar);
    topHbox->setContentsMargins(10, 8, 10, 8);

    auto* backBtn = new QPushButton("← Back", topBar);
    backBtn->setObjectName("backBtn");
    disableDialogDefaultButtonBehavior(backBtn);
    connect(backBtn, &QPushButton::clicked, this, &StreamVaultWindow::goToSearch);
    topHbox->addWidget(backBtn);
    topHbox->addStretch();

    m_watchlistBtn = new QPushButton("☆ Watchlist", topBar);
    m_watchlistBtn->setObjectName("watchlistBtn");
    disableDialogDefaultButtonBehavior(m_watchlistBtn);
    connect(m_watchlistBtn, &QPushButton::clicked, this, [this]() {
        WatchlistStore::instance().toggle(m_currentEntry);
    });
    topHbox->addWidget(m_watchlistBtn);

    vbox->addWidget(topBar);

    m_backdropLabel = new QLabel(page);
    m_backdropLabel->setFixedHeight(220);
    m_backdropLabel->setAlignment(Qt::AlignCenter);
        m_backdropLabel->setObjectName("backdropLabel");
    vbox->addWidget(m_backdropLabel);

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setObjectName("detailScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* infoWidget = new QWidget(scrollArea);
    infoWidget->setObjectName("detailInfoWidget");
    auto* infoVbox = new QVBoxLayout(infoWidget);
    infoVbox->setContentsMargins(24, 18, 24, 24);
    infoVbox->setSpacing(8);

    m_detailTitle = new QLabel(infoWidget);
    m_detailTitle->setObjectName("detailTitle");
    m_detailTitle->setWordWrap(true);
    infoVbox->addWidget(m_detailTitle);

    m_detailMeta = new QLabel(infoWidget);
    m_detailMeta->setObjectName("detailMeta");
    infoVbox->addWidget(m_detailMeta);

    auto* div1 = new QFrame(infoWidget);
    div1->setFrameShape(QFrame::HLine);
    div1->setObjectName("divider");
    div1->setFixedHeight(1);
    infoVbox->addWidget(div1);

    m_detailOverview = new QLabel(infoWidget);
    m_detailOverview->setObjectName("detailOverview");
    m_detailOverview->setWordWrap(true);
    infoVbox->addWidget(m_detailOverview);

    auto* watchHeaderRow = new QWidget(infoWidget);
    auto* watchHeaderBox = new QHBoxLayout(watchHeaderRow);
    watchHeaderBox->setContentsMargins(0, 8, 0, 0);
    watchHeaderBox->setSpacing(8);

    auto* watchOnLabel = new QLabel("STREAMING IN", watchHeaderRow);
    watchOnLabel->setObjectName("watchOnLabel");
    watchHeaderBox->addWidget(watchOnLabel);

    m_countryCombo = new QComboBox(watchHeaderRow);
    m_countryCombo->setObjectName("countryCombo");
    m_countryCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_countryCombo->setToolTip("Select a country to see availability");
    connect(m_countryCombo, &QComboBox::currentIndexChanged,
            this, &StreamVaultWindow::onCountryChanged);
    watchHeaderBox->addWidget(m_countryCombo);
    watchHeaderBox->addStretch();

    infoVbox->addWidget(watchHeaderRow);

    m_watchStatusLabel = new QLabel(infoWidget);
    m_watchStatusLabel->setObjectName("statusLabel");
    m_watchStatusLabel->setWordWrap(true);
    infoVbox->addWidget(m_watchStatusLabel);

    m_serviceButtonContainer = new QWidget(infoWidget);
    auto* serviceGrid = new QGridLayout(m_serviceButtonContainer);
    serviceGrid->setContentsMargins(0, 4, 0, 0);
    serviceGrid->setHorizontalSpacing(10);
    serviceGrid->setVerticalSpacing(8);
    infoVbox->addWidget(m_serviceButtonContainer);

    m_deeplinkBar = new QWidget(infoWidget);
    m_deeplinkBar->setObjectName("deeplinkBar");
    auto* dlLayout = new QHBoxLayout(m_deeplinkBar);
    dlLayout->setContentsMargins(0, 8, 0, 0);
    dlLayout->setSpacing(8);

    m_tmdbBtn = new QPushButton("Open on TMDB", m_deeplinkBar);
    m_tmdbBtn->setObjectName("backBtn");
    m_tmdbBtn->setCursor(Qt::PointingHandCursor);
    disableDialogDefaultButtonBehavior(m_tmdbBtn);
    dlLayout->addWidget(m_tmdbBtn);

    m_imdbBtn = new QPushButton("Open on IMDb", m_deeplinkBar);
    m_imdbBtn->setObjectName("backBtn");
    m_imdbBtn->setCursor(Qt::PointingHandCursor);
    m_imdbBtn->setVisible(false);
    disableDialogDefaultButtonBehavior(m_imdbBtn);
    dlLayout->addWidget(m_imdbBtn);

    m_justWatchBtn = new QPushButton("Where to Watch ↗", m_deeplinkBar);
    m_justWatchBtn->setObjectName("backBtn");
    m_justWatchBtn->setCursor(Qt::PointingHandCursor);
    m_justWatchBtn->setVisible(false);
    m_justWatchBtn->setToolTip("Open the TMDB page with direct links to streaming services");
    disableDialogDefaultButtonBehavior(m_justWatchBtn);
    dlLayout->addWidget(m_justWatchBtn);

    dlLayout->addStretch();
    infoVbox->addWidget(m_deeplinkBar);

    m_episodeSection = new QWidget(infoWidget);
    m_episodeSection->setObjectName("episodeSection");
    m_episodeSection->setVisible(false);
    auto* epVbox = new QVBoxLayout(m_episodeSection);
    epVbox->setContentsMargins(0, 12, 0, 0);
    epVbox->setSpacing(6);

    auto* epDiv = new QFrame(m_episodeSection);
    epDiv->setFrameShape(QFrame::HLine);
    epDiv->setObjectName("divider");
    epDiv->setFixedHeight(1);
    epVbox->addWidget(epDiv);

    auto* epHeaderRow = new QHBoxLayout;
    epHeaderRow->setSpacing(8);
    auto* epLabel = new QLabel("EPISODE TRACKER", m_episodeSection);
    epLabel->setObjectName("watchOnLabel");
    epHeaderRow->addWidget(epLabel);

    m_seasonCombo = new QComboBox(m_episodeSection);
    m_seasonCombo->setObjectName("countryCombo");
    m_seasonCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    epHeaderRow->addWidget(m_seasonCombo);

    m_episodeProgressLabel = new QLabel(m_episodeSection);
    m_episodeProgressLabel->setObjectName("detailMeta");
    epHeaderRow->addWidget(m_episodeProgressLabel);
    epHeaderRow->addStretch();

    m_markSeasonBtn = new QPushButton("Mark All Watched", m_episodeSection);
    m_markSeasonBtn->setObjectName("backBtn");
    disableDialogDefaultButtonBehavior(m_markSeasonBtn);
    epHeaderRow->addWidget(m_markSeasonBtn);
    epVbox->addLayout(epHeaderRow);

    m_episodeListWidget = new QWidget(m_episodeSection);
    m_episodeListLayout = new QVBoxLayout(m_episodeListWidget);
    m_episodeListLayout->setContentsMargins(0, 0, 0, 0);
    m_episodeListLayout->setSpacing(2);
    epVbox->addWidget(m_episodeListWidget);

    connect(m_seasonCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx < 0 || m_currentEntry.tmdbId <= 0) return;
        int seasonNum = m_seasonCombo->currentData().toInt();
        m_tmdb->fetchSeasonEpisodes(m_currentEntry.tmdbId, seasonNum);
    });

    connect(m_markSeasonBtn, &QPushButton::clicked, this, [this]() {
        if (m_seasonCombo->currentIndex() < 0 || m_tvSeasons.isEmpty()) return;
        int seasonIdx = m_seasonCombo->currentIndex();
        if (seasonIdx < 0 || seasonIdx >= m_tvSeasons.size()) return;
        const auto& s = m_tvSeasons[seasonIdx];
        auto& store = EpisodeProgressStore::instance();
        int watched = store.watchedCountForSeason(m_currentEntry.tmdbId, s.seasonNumber);
        bool markWatched = (watched < s.episodeCount);
        store.markSeasonWatched(m_currentEntry.tmdbId, s.seasonNumber, s.episodeCount, markWatched);
    });

    connect(&EpisodeProgressStore::instance(), &EpisodeProgressStore::changed,
            this, [this](int tmdbId) {
        if (tmdbId != m_currentEntry.tmdbId) return;

        if (m_seasonCombo->currentIndex() >= 0) {
            int seasonNum = m_seasonCombo->currentData().toInt();
            m_tmdb->fetchSeasonEpisodes(m_currentEntry.tmdbId, seasonNum);
        }
    });

    infoVbox->addWidget(m_episodeSection);

    infoVbox->addStretch();
    scrollArea->setWidget(infoWidget);
    vbox->addWidget(scrollArea, 1);

    return page;
}

void StreamVaultWindow::onSearchTriggered() {
    const QString q = m_searchEdit->text().trimmed();
    if (q.isEmpty()) {
        m_model->clear();
        m_statusLabel->setText("Enter a title to search across streaming services.");
        return;
    }

    if (!m_tmdb->hasApiKey()) {
        m_statusLabel->setText(
            "⚠  No TMDB API key set. Click ⚙ Settings to add one "
            "(free at themoviedb.org).");
        return;
    }

    m_statusLabel->setText("Searching…");
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Search initiated."), QStringLiteral("query=%1").arg(q));
    m_model->clear();
    m_posterCache.clear();
    m_tmdb->search(q);
}

void StreamVaultWindow::onSearchResults(QVector<StreamEntry> results) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Search results received."),
        QStringLiteral("%1 results").arg(results.size()));
    m_model->setResults(results);
    const int n = results.size();
    if (n == 0) {
        m_statusLabel->setText("No results found.");
    } else {
        m_statusLabel->setText(
            QString("%1 result%2").arg(n).arg(n == 1 ? "" : "s"));

        if (m_model->rowCount() > 0) {
            m_grid->setCurrentIndex(m_proxy->index(0, 0));
        }
    }

    QTimer::singleShot(0, this, &StreamVaultWindow::requestVisiblePosters);
}

void StreamVaultWindow::onSearchError(QString message) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
        QStringLiteral("Search error."), message);
    m_statusLabel->setText("⚠  " + message);
}

void StreamVaultWindow::onPosterLoaded(int tmdbId, QByteArray imageData) {
    QPixmap pix;
    if (!pix.loadFromData(imageData)) return;
    m_posterCache[tmdbId] = pix;

    m_grid->viewport()->update();
}

void StreamVaultWindow::onGridActivated(const QModelIndex& idx) {
    if (!idx.isValid()) return;
    const StreamEntry* e = m_model->entryAt(m_proxy->mapToSource(idx));
    if (!e) return;
    goToDetail(*e);
}

void StreamVaultWindow::onGridContextMenu(const QPoint& pos) {
    const QModelIndex idx = m_grid->indexAt(pos);
    if (!idx.isValid()) return;
    const StreamEntry* e = m_model->entryAt(m_proxy->mapToSource(idx));
    if (!e) return;

    QMenu menu(this);
    menu.setStyleSheet(
        QString("QMenu { background:%1; color:%2; border:1px solid %3; padding: 4px; }"
            "QMenu::item { padding: 6px 14px; border-radius: 6px; }"
            "QMenu::item:selected { background:%4; color:%2; }"
            "QMenu::separator { height: 1px; background: %3; margin: 6px 8px; }"
            "QMenu::item:disabled { color: %5; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.foreground.name(),
             m_palette.cardBorder.name(),
             m_palette.hoverBackground.name(),
             m_palette.mutedForeground.name()));

    menu.addSection(e->title);

    const bool inWatchlist = WatchlistStore::instance().contains(e->tmdbId);
    const StreamEntry entryCopy = *e;
    menu.addAction(inWatchlist ? "★ Remove from Watchlist" : "☆ Add to Watchlist",
                   this, [this, entryCopy]() {
                       WatchlistStore::instance().toggle(entryCopy);
                   });

    menu.addSeparator();

    const QVector<ServiceInfo> services = availableServices();
    for (const ServiceInfo& svc : services) {
        const QString encoded = QUrl::toPercentEncoding(e->title);
        const QString url     = svc.searchUrlTemplate.arg(encoded);
        menu.addAction(QString("Search on %1").arg(svc.name),
                       this, [this, url]() { openUrl(url); });
    }

    menu.addSeparator();

    const QString typeSlug = (e->mediaType == MediaType::Movie)
        ? QStringLiteral("movie") : QStringLiteral("tv");
    const QString tmdbUrl = QStringLiteral("https://www.themoviedb.org/%1/%2")
        .arg(typeSlug).arg(e->tmdbId);
    menu.addAction("Open on TMDB", this, [this, tmdbUrl]() { openUrl(tmdbUrl); });

    if (m_imdbIdCache.contains(e->tmdbId) && !m_imdbIdCache[e->tmdbId].isEmpty()) {
        const QString imdbUrl = QStringLiteral("https://www.imdb.com/title/%1")
            .arg(m_imdbIdCache[e->tmdbId]);
        menu.addAction("Open on IMDb", this, [this, imdbUrl]() { openUrl(imdbUrl); });
    }

    menu.exec(m_grid->viewport()->mapToGlobal(pos));
}

void StreamVaultWindow::goToDetail(const StreamEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Navigated to detail page."),
        QStringLiteral("title=%1 tmdbId=%2").arg(e.title).arg(e.tmdbId));
    m_currentEntry = e;
    populateDetail(e);
    updateWatchlistButton();
    m_stack->setCurrentIndex(1);
}

void StreamVaultWindow::goToSearch() {
    m_showingWatchlist = false;
    m_stack->setCurrentIndex(0);
}

void StreamVaultWindow::openSettings() {
    const auto palette = wintools::themes::ThemeHelper::currentPalette();

    auto* dlg = new QDialog(this);
    dlg->setObjectName("streamVaultSettingsDialog");
    dlg->setWindowTitle("StreamVault Settings");
    dlg->setModal(true);
    dlg->setStyleSheet(
        QString("QDialog#streamVaultSettingsDialog { background:%1; color:%2; }"
                "QDialog#streamVaultSettingsDialog QLabel  { color:%2; font-size:13px; }"
                "QDialog#streamVaultSettingsDialog QLineEdit {"
                "  background:%6; color:%2; border:1px solid %4;"
                "  border-radius:5px; padding:5px 10px; font-size:13px;"
                "  selection-background-color:%5; selection-color:%2;"
                "}"
                "QDialog#streamVaultSettingsDialog QLineEdit:focus { border-color:%5; }"
                "QDialog#streamVaultSettingsDialog QLineEdit:disabled { color:%3; background:%1; }"
                "QDialog#streamVaultSettingsDialog QLineEdit::placeholder { color:%3; }"
                "QDialog#streamVaultSettingsDialog QListWidget { background:%6; color:%2; border:1px solid %4; border-radius:6px; }"
                "QDialog#streamVaultSettingsDialog QListWidget::item:selected { background:%5; color:%2; }"
                "QDialog#streamVaultSettingsDialog QPushButton { background:%6; color:%2; border:1px solid %4;"
                "              border-radius:5px; padding:5px 16px; }"
                "QDialog#streamVaultSettingsDialog QPushButton:hover { background:%5; border-color:%5; }"
                "QDialog#streamVaultSettingsDialog QDialogButtonBox QPushButton { min-width: 88px; }")
        .arg(palette.cardBackground.name(),
             palette.foreground.name(),
             palette.hoverBackground.name(),
             palette.cardBorder.name(),
             palette.accent.name(),
             palette.windowBackground.name()));
    dlg->resize(680, 520);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setContentsMargins(20, 18, 20, 18);
    vbox->setSpacing(12);

    auto* infoLabel = new QLabel(
        QString("Enter your free TMDB API key to enable search.\n"
                "Get one at <a href='https://www.themoviedb.org/settings/api' "
                "style='color:%1;'>themoviedb.org/settings/api</a>")
        .arg(palette.accent.name()), dlg);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    vbox->addWidget(infoLabel);

    auto* keyEdit = new QLineEdit(dlg);
    keyEdit->setPlaceholderText("TMDB API Key (v3 auth)");
    keyEdit->setText(StreamVaultSettings::instance().tmdbApiKey());
    vbox->addWidget(keyEdit);

    auto* customHeader = new QLabel("Custom services", dlg);
    customHeader->setStyleSheet(QString("font-size: 14px; font-weight: 600; color: %1;").arg(palette.foreground.name()));
    vbox->addWidget(customHeader);

    auto* customHint = new QLabel(
        "Add custom services for private/NAS platforms. Use %1 where the title should be inserted in the search URL.",
        dlg);
    customHint->setWordWrap(true);
    customHint->setStyleSheet(QString("color: %1;").arg(palette.mutedForeground.name()));
    vbox->addWidget(customHint);

    QVector<ServiceInfo> customServices = StreamVaultSettings::instance().customServices();

    auto* customList = new QListWidget(dlg);
    customList->setMinimumHeight(150);
    vbox->addWidget(customList);

    auto* formWidget = new QWidget(dlg);
    auto* form = new QFormLayout(formWidget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    auto* nameEdit = new QLineEdit(formWidget);
    nameEdit->setPlaceholderText("My NAS");
    auto* searchTemplateEdit = new QLineEdit(formWidget);
    searchTemplateEdit->setPlaceholderText("https://my-nas.local/search?q=%1");
    auto* homeUrlEdit = new QLineEdit(formWidget);
    homeUrlEdit->setPlaceholderText("https://my-nas.local/");
    auto* loginUrlEdit = new QLineEdit(formWidget);
    loginUrlEdit->setPlaceholderText("https://my-nas.local/login (optional)");
    auto* accentEdit = new QLineEdit(formWidget);
    accentEdit->setPlaceholderText("#4B6EFF (optional)");

    form->addRow("Name", nameEdit);
    form->addRow("Search URL", searchTemplateEdit);
    form->addRow("Home URL", homeUrlEdit);
    form->addRow("Login URL", loginUrlEdit);
    form->addRow("Accent", accentEdit);
    vbox->addWidget(formWidget);

    const auto refreshCustomList = [&]() {
        customList->clear();
        for (const ServiceInfo& svc : customServices) {
            customList->addItem(QString("%1  —  %2").arg(svc.name, svc.searchUrlTemplate));
        }
    };

    auto* customButtons = new QHBoxLayout();
    auto* addOrUpdateBtn = new QPushButton("Add / Update", dlg);
    auto* removeBtn = new QPushButton("Remove Selected", dlg);
    customButtons->addWidget(addOrUpdateBtn);
    customButtons->addWidget(removeBtn);
    customButtons->addStretch(1);
    vbox->addLayout(customButtons);

    connect(customList, &QListWidget::currentRowChanged, dlg, [=, &customServices](int row) {
        if (row < 0 || row >= customServices.size()) {
            return;
        }
        const ServiceInfo& svc = customServices[row];
        nameEdit->setText(svc.name);
        searchTemplateEdit->setText(svc.searchUrlTemplate);
        homeUrlEdit->setText(svc.homeUrl);
        loginUrlEdit->setText(svc.loginUrl);
        accentEdit->setText(svc.accentColor);
    });

    connect(addOrUpdateBtn, &QPushButton::clicked, dlg, [=, &customServices]() {
        const QString name = nameEdit->text().trimmed();
        QString searchTemplate = searchTemplateEdit->text().trimmed();
        if (name.isEmpty() || searchTemplate.isEmpty()) {
            QMessageBox::warning(dlg, "Custom Service", "Name and Search URL are required.");
            return;
        }
        if (!searchTemplate.contains("%1")) {
            searchTemplate += (searchTemplate.contains("?") ? "&q=%1" : "?q=%1");
        }

        ServiceInfo candidate;
        candidate.id = StreamingService::COUNT;
        candidate.name = name;
        candidate.searchUrlTemplate = searchTemplate;
        candidate.homeUrl = homeUrlEdit->text().trimmed();
        candidate.loginUrl = loginUrlEdit->text().trimmed();
        candidate.accentColor = accentEdit->text().trimmed();
        if (candidate.accentColor.isEmpty()) {
            candidate.accentColor = QStringLiteral("#9AA4AF");
        }

        int existingIndex = -1;
        for (int i = 0; i < customServices.size(); ++i) {
            if (customServices[i].name.compare(name, Qt::CaseInsensitive) == 0) {
                existingIndex = i;
                break;
            }
        }

        if (existingIndex >= 0) {
            customServices[existingIndex] = candidate;
            customList->setCurrentRow(existingIndex);
        } else {
            customServices.push_back(candidate);
            customList->setCurrentRow(customServices.size() - 1);
        }

        refreshCustomList();
    });

    connect(removeBtn, &QPushButton::clicked, dlg, [=, &customServices]() {
        const int row = customList->currentRow();
        if (row < 0 || row >= customServices.size()) {
            return;
        }
        customServices.removeAt(row);
        refreshCustomList();
        if (!customServices.isEmpty()) {
            const int maxRow = static_cast<int>(customServices.size()) - 1;
            customList->setCurrentRow(std::min(row, maxRow));
        }
    });

    refreshCustomList();

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, keyEdit, &customServices]() {
        const QString key = keyEdit->text().trimmed();
        StreamVaultSettings::instance().setTmdbApiKey(key);
        StreamVaultSettings::instance().setCustomServices(customServices);
        m_tmdb->setApiKey(key);
        dlg->accept();
        refreshSidebar();
        if (!key.isEmpty()) {
            m_statusLabel->setText("API key saved. Start searching!");
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vbox->addWidget(buttons);

    dlg->exec();
}

void StreamVaultWindow::populateDetail(const StreamEntry& e) {

    m_detailTitle->setText(e.title);

    QStringList metaParts;
    if (!e.releaseYear.isEmpty())              metaParts << e.releaseYear;
    if (e.voteAverage > 0)
        metaParts << QString("★ %1 / 10").arg(e.voteAverage, 0, 'f', 1);
    metaParts << mediaTypeName(e.mediaType);
    if (e.voteCount > 0)
        metaParts << QString("%1 votes").arg(e.voteCount);
    m_detailMeta->setText(metaParts.join("  ·  "));

    m_detailOverview->setText(
        e.overview.isEmpty() ? "(No overview available.)" : e.overview);

    m_backdropLabel->setText("Loading\u2026");
    m_backdropLabel->setObjectName("backdropLabel");

    if (m_backdropCache.contains(e.tmdbId)) {
        QPixmap pix = m_backdropCache[e.tmdbId];
        m_backdropLabel->setPixmap(
            pix.scaled(m_backdropLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    } else if (!e.backdropPath.isEmpty()) {
        fetchBackdrop(e);
    } else if (m_posterCache.contains(e.tmdbId)) {

        QPixmap pix = m_posterCache[e.tmdbId];
        m_backdropLabel->setPixmap(
            pix.scaled(m_backdropLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    } else {
        m_backdropLabel->setText("");
    }

    clearDetailServiceButtons();
    m_countryCombo->blockSignals(true);
    m_countryCombo->clear();
    m_countryCombo->blockSignals(false);

    if (m_providersCache.contains(e.tmdbId)) {
        onWatchProvidersLoaded(e.tmdbId, m_providersCache[e.tmdbId]);
    } else {
        m_watchStatusLabel->setText("Loading streaming availability…");
        m_tmdb->fetchWatchProviders(e.tmdbId, e.mediaType);
    }

    {
        const QString typeSlug = (e.mediaType == MediaType::Movie)
            ? QStringLiteral("movie") : QStringLiteral("tv");
        const QString tmdbUrl = QStringLiteral("https://www.themoviedb.org/%1/%2")
            .arg(typeSlug).arg(e.tmdbId);

        m_tmdbBtn->disconnect();
        connect(m_tmdbBtn, &QPushButton::clicked,
                this, [tmdbUrl]() { openUrl(tmdbUrl); });

        if (m_imdbIdCache.contains(e.tmdbId)) {
            const QString imdbId = m_imdbIdCache[e.tmdbId];
            if (!imdbId.isEmpty()) {
                m_imdbBtn->setVisible(true);
                m_imdbBtn->disconnect();
                connect(m_imdbBtn, &QPushButton::clicked,
                        this, [imdbId]() { openUrl(QStringLiteral("https://www.imdb.com/title/%1").arg(imdbId)); });
            } else {
                m_imdbBtn->setVisible(false);
            }
        } else {
            m_imdbBtn->setVisible(false);
            m_tmdb->fetchExternalIds(e.tmdbId, e.mediaType);
        }

        m_justWatchBtn->setVisible(false);
    }

    if (e.mediaType == MediaType::TvShow) {
        m_episodeSection->setVisible(true);
        m_seasonCombo->blockSignals(true);
        m_seasonCombo->clear();
        m_seasonCombo->blockSignals(false);
        m_tvSeasons.clear();

        while (m_episodeListLayout->count() > 0) {
            auto* item = m_episodeListLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        m_episodeProgressLabel->clear();
        m_tmdb->fetchTvSeasons(e.tmdbId);
    } else {
        m_episodeSection->setVisible(false);
    }
}

void StreamVaultWindow::clearDetailServiceButtons() {
    QLayout* layout = m_serviceButtonContainer->layout();
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
}

void StreamVaultWindow::onWatchProvidersLoaded(int tmdbId,
                                                WatchProviderMap byCountry) {

    m_providersCache[tmdbId] = byCountry;

    if (m_currentEntry.tmdbId != tmdbId) return;

    m_countryCombo->blockSignals(true);
    m_countryCombo->clear();

    if (byCountry.isEmpty()) {
        m_watchStatusLabel->setText(
            "No streaming availability data found for this title.");
        m_countryCombo->blockSignals(false);
        return;
    }

    QStringList countryCodes = byCountry.keys();
    countryCodes.sort();

    const QString systemCode = QLocale::system().name().mid(3, 2);
    QString defaultCode = countryCodes.contains(systemCode) ? systemCode
                        : countryCodes.contains("US")       ? "US"
                        : countryCodes.first();

    int defaultIndex = 0;
    for (const QString& code : countryCodes) {
        QLocale loc(QLocale::AnyLanguage, QLocale::codeToTerritory(code));
        const QString countryName = loc.nativeTerritoryName().isEmpty()
            ? code
            : loc.nativeTerritoryName();
        m_countryCombo->addItem(QString("%1  (%2)").arg(countryName, code), code);
        if (code == defaultCode)
            defaultIndex = m_countryCombo->count() - 1;
    }

    m_countryCombo->setCurrentIndex(defaultIndex);
    m_countryCombo->blockSignals(false);

    populateServiceButtons(m_countryCombo->currentData().toString());
}

void StreamVaultWindow::onCountryChanged(int ) {
    const QString code = m_countryCombo->currentData().toString();
    if (!code.isEmpty())
        populateServiceButtons(code);
}

void StreamVaultWindow::populateServiceButtons(const QString& countryCode) {
    clearDetailServiceButtons();

    if (!m_providersCache.contains(m_currentEntry.tmdbId)) return;
    const WatchProviderMap& map = m_providersCache[m_currentEntry.tmdbId];

    if (!map.contains(countryCode)) {
        m_watchStatusLabel->setText(
            QString("Not available for subscription streaming in %1.").arg(countryCode));
        if (m_justWatchBtn) m_justWatchBtn->setVisible(false);
        return;
    }

    const CountryProviders& cp = map[countryCode];

    if (m_justWatchBtn) {
        if (!cp.link.isEmpty()) {
            m_justWatchBtn->setVisible(true);
            m_justWatchBtn->disconnect();
            connect(m_justWatchBtn, &QPushButton::clicked,
                    this, [link = cp.link]() { openUrl(link); });
        } else {
            m_justWatchBtn->setVisible(false);
        }
    }

    if (cp.providers.isEmpty()) {
        m_watchStatusLabel->setText(
            QString("Not available for subscription streaming in %1.").arg(countryCode));
        return;
    }

    m_watchStatusLabel->clear();
    const QString encoded = QUrl::toPercentEncoding(m_currentEntry.title);

    QSet<StreamingService> added;

    for (const ProviderEntry& pe : cp.providers) {
        const ServiceInfo* known = findServiceByTmdbId(pe.id);
        if (known) {
            if (added.contains(known->id)) continue;
            added.insert(known->id);
            addKnownServiceButton(*known, encoded);
        } else {
            addUnknownProviderButton(pe, encoded);
        }
    }
}

void StreamVaultWindow::addKnownServiceButton(const ServiceInfo& svc,
                                               const QString& titleEncoded) {
    auto* btn = new QPushButton(svc.name, m_serviceButtonContainer);
    btn->setObjectName("serviceBtn");
    disableDialogDefaultButtonBehavior(btn);
    btn->setStyleSheet(
        QString("QPushButton#serviceBtn { background-color: %1;"
                "  border: 2px solid %2; border-radius:8px; color:%3;"
                "  font-size:13px; padding:6px 16px; min-width:110px; }"
                "QPushButton#serviceBtn:hover { background-color: %2;"
                "  color:#ffffff; }")
        .arg(m_palette.cardBackground.name(), svc.accentColor,
             m_palette.foreground.name()));
    btn->setCursor(Qt::PointingHandCursor);

    const QString url = svc.searchUrlTemplate.arg(titleEncoded);
    connect(btn, &QPushButton::clicked, this, [this, url]() { openUrl(url); });

    auto* grid = qobject_cast<QGridLayout*>(m_serviceButtonContainer->layout());
    if (!grid) return;
    const int count = grid->count();
    grid->addWidget(btn, count / 4, count % 4);
}

void StreamVaultWindow::addUnknownProviderButton(const ProviderEntry& pe,
                                                  const QString& ) {

    auto* btn = new QPushButton(pe.name, m_serviceButtonContainer);
    btn->setObjectName("serviceBtn");
    disableDialogDefaultButtonBehavior(btn);
    btn->setStyleSheet(
        QString("QPushButton#serviceBtn { background-color: %1;"
                "  border: 1px solid %2; border-radius:8px; color:%3;"
                "  font-size:12px; padding:6px 16px; min-width:110px; }"
                "QPushButton#serviceBtn:hover { border-color:%3; color:%4; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.cardBorder.name(),
             m_palette.mutedForeground.name(),
             m_palette.foreground.name()));
    btn->setToolTip(QString("Available on %1 (no direct link)").arg(pe.name));
    btn->setEnabled(false);

    auto* grid = qobject_cast<QGridLayout*>(m_serviceButtonContainer->layout());
    if (!grid) return;
    const int count = grid->count();
    grid->addWidget(btn, count / 4, count % 4);
}

void StreamVaultWindow::requestVisiblePosters() {
    const int rows = m_proxy->rowCount();
    for (int i = 0; i < rows; ++i) {
        const QModelIndex proxyIdx  = m_proxy->index(i, 0);
        const QModelIndex sourceIdx = m_proxy->mapToSource(proxyIdx);
        const StreamEntry* e        = m_model->entryAt(sourceIdx);
        if (!e || e->posterPath.isEmpty()) continue;
        if (m_posterCache.contains(e->tmdbId)) continue;
        m_tmdb->fetchPoster(e->tmdbId, e->posterPath);
    }
}

void StreamVaultWindow::fetchBackdrop(const StreamEntry& e) {
    const QString url =
        StreamVaultSettings::backdropBaseUrl() + e.backdropPath;

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_imgNam->get(req);
    const int tmdbId = e.tmdbId;

    connect(reply, &QNetworkReply::finished, this, [this, reply, tmdbId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pix;
            if (pix.loadFromData(data)) {
                m_backdropCache[tmdbId] = pix;

                if (m_currentEntry.tmdbId == tmdbId) {
                    m_backdropLabel->setPixmap(
                        pix.scaled(m_backdropLabel->size(),
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation));
                }
            }
        }
        reply->deleteLater();
    });
}

void StreamVaultWindow::showWatchlist() {
    const QVector<StreamEntry> entries = WatchlistStore::instance().all();
    m_showingWatchlist = true;
    m_model->setResults(entries);
    m_posterCache.clear();

    const int n = entries.size();
    m_statusLabel->setText(
        n == 0 ? QStringLiteral("Your watchlist is empty.")
               : QStringLiteral("Watchlist — %1 title%2").arg(n).arg(n == 1 ? "" : "s"));

    m_stack->setCurrentIndex(0);
    QTimer::singleShot(0, this, &StreamVaultWindow::requestVisiblePosters);

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Showing watchlist."),
        QStringLiteral("%1 entries").arg(n));
}

void StreamVaultWindow::onWatchlistChanged() {

    if (m_watchlistSidebarBtn) {
        const int n = WatchlistStore::instance().count();
        m_watchlistSidebarBtn->setText(
            QStringLiteral("★ My Watchlist (%1)").arg(n));
    }

    updateWatchlistButton();

    if (m_showingWatchlist) {
        showWatchlist();
    }
}

void StreamVaultWindow::updateWatchlistButton() {
    if (!m_watchlistBtn || m_currentEntry.tmdbId <= 0) return;

    const bool inList = WatchlistStore::instance().contains(m_currentEntry.tmdbId);
    m_watchlistBtn->setText(inList ? QStringLiteral("★ In Watchlist")
                                   : QStringLiteral("☆ Add to Watchlist"));
}

void StreamVaultWindow::openUrl(const QString& url) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Opening service URL."), url);
    QDesktopServices::openUrl(QUrl(url));
}

void StreamVaultWindow::onTvSeasonsLoaded(int tmdbId, QVector<TvSeasonSummary> seasons) {
    if (tmdbId != m_currentEntry.tmdbId) return;

    m_tvSeasons = seasons;
    m_seasonCombo->blockSignals(true);
    m_seasonCombo->clear();
    for (const auto& s : seasons) {
        QString label = s.name.isEmpty()
            ? QStringLiteral("Season %1").arg(s.seasonNumber)
            : s.name;
        label += QStringLiteral(" (%1 eps)").arg(s.episodeCount);
        m_seasonCombo->addItem(label, s.seasonNumber);
    }
    m_seasonCombo->blockSignals(false);

    if (!seasons.isEmpty()) {
        m_seasonCombo->setCurrentIndex(0);
        m_tmdb->fetchSeasonEpisodes(tmdbId, seasons.first().seasonNumber);
    }
}

void StreamVaultWindow::onSeasonEpisodesLoaded(int tmdbId, int season, QVector<TvEpisodeSummary> episodes) {
    if (tmdbId != m_currentEntry.tmdbId) return;

    while (m_episodeListLayout->count() > 0) {
        auto* item = m_episodeListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto& store = EpisodeProgressStore::instance();
    int watchedCount = 0;

    for (const auto& ep : episodes) {
        bool watched = store.isWatched(tmdbId, season, ep.episodeNumber);
        if (watched) ++watchedCount;

        auto* row = new QWidget(m_episodeListWidget);
        row->setObjectName("appRow");
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(8, 4, 8, 4);
        hbox->setSpacing(8);

        auto* cb = new QCheckBox(row);
        cb->setChecked(watched);
        hbox->addWidget(cb);

        auto* epLabel = new QLabel(
            QStringLiteral("E%1 — %2").arg(ep.episodeNumber, 2, 10, QLatin1Char('0')).arg(ep.name),
            row);
        epLabel->setWordWrap(true);
        hbox->addWidget(epLabel, 1);

        if (!ep.airDate.isEmpty()) {
            auto* dateLabel = new QLabel(ep.airDate, row);
            dateLabel->setObjectName("detailMeta");
            hbox->addWidget(dateLabel);
        }

        const int epNum = ep.episodeNumber;
        connect(cb, &QCheckBox::toggled, this, [this, tmdbId, season, epNum](bool checked) {
            EpisodeProgressStore::instance().setWatched(tmdbId, season, epNum, checked);
        });

        m_episodeListLayout->addWidget(row);
    }

    m_episodeProgressLabel->setText(
        QStringLiteral("%1 / %2 watched").arg(watchedCount).arg(episodes.size()));

    int seasonIdx = m_seasonCombo->currentIndex();
    if (seasonIdx >= 0 && seasonIdx < m_tvSeasons.size()) {
        bool allWatched = (watchedCount >= episodes.size());
        m_markSeasonBtn->setText(allWatched ? QStringLiteral("Unmark All") : QStringLiteral("Mark All Watched"));
    }
}

void StreamVaultWindow::onExternalIdsLoaded(int tmdbId, QString imdbId) {
    m_imdbIdCache[tmdbId] = imdbId;

    if (tmdbId != m_currentEntry.tmdbId) return;

    if (!imdbId.isEmpty()) {
        m_imdbBtn->setVisible(true);
        m_imdbBtn->disconnect();
        connect(m_imdbBtn, &QPushButton::clicked,
                this, [imdbId]() { openUrl(QStringLiteral("https://www.imdb.com/title/%1").arg(imdbId)); });
    } else {
        m_imdbBtn->setVisible(false);
    }
}

}
