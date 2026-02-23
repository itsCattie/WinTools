#include "streamvault_window.hpp"
#include "stream_card_delegate.hpp"
#include "logger/logger.hpp"

#include "modules/StreamVault/src/core/stream_entry.hpp"
#include "modules/StreamVault/src/core/streaming_service.hpp"
#include "modules/StreamVault/src/core/streamvault_settings.hpp"
#include "modules/StreamVault/src/core/tmdb_client.hpp"
#include "modules/StreamVault/src/model/stream_model.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/ui/screen_relative_size.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
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

// StreamVault: streamvault window manages UI behavior and presentation.

namespace wintools::streamvault {

static constexpr const char* kLog = "StreamVault/Window";

StreamVaultWindow::StreamVaultWindow(QWidget* parent)
    : QDialog(parent)
    , m_tmdb(new TmdbClient(this))
    , m_imgNam(new QNetworkAccessManager(this))
    , m_debounce(new QTimer(this))
    , m_themeListener(new wintools::themes::ThemeListener(this))
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setWindowTitle("StreamVault");
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
    connect(m_debounce, &QTimer::timeout,
            this,       &StreamVaultWindow::onSearchTriggered);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this,            &StreamVaultWindow::onThemeChanged);

    buildUi();
    applyTheme(m_palette);
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "StreamVaultWindow opened.",
        apiKey.isEmpty() ? "No API key" : "API key present");
}

StreamVaultWindow::~StreamVaultWindow() = default;

void StreamVaultWindow::onThemeChanged(bool) {
    applyTheme(wintools::themes::ThemeHelper::currentPalette());
}

void StreamVaultWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    using wintools::themes::FluentStyle;
    m_palette = palette;

    const QString supplement = QStringLiteral(

        "QWidget#sidebarWidget { background-color: %1; }"

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

        "QPushButton#sidebarOpenBtn { background: transparent; border: none; color: %2; font-size: 12px; padding: 2px 6px; }"
        "QPushButton#sidebarOpenBtn:hover { color: %3; }"

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

    setStyleSheet(FluentStyle::generate(palette) + supplement);
}

void StreamVaultWindow::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildSidebar());

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

QWidget* StreamVaultWindow::buildSidebar() {
    auto* w      = new QWidget(this);
    w->setFixedWidth(220);
    w->setObjectName("sidebarWidget");

    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    auto* header = new QLabel("STREAM VAULT", w);
    header->setObjectName("sectionHeader");
    header->setStyleSheet(
        QString("font-size: 15px; font-weight: bold; color: %1;"
                "padding: 18px 14px 14px 14px; background: transparent;")
        .arg(m_palette.foreground.name()));
    vbox->addWidget(header);

    auto* servicesLabel = new QLabel("SERVICES", w);
    servicesLabel->setObjectName("sectionHeader");
    vbox->addWidget(servicesLabel);

    const QVector<ServiceInfo> services = allServices();
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
        nameBtn->setStyleSheet(
            QString("QPushButton { background: transparent; border: none; color: %1;"
                    "text-align: left; font-size: 13px; padding: 3px 4px; }"
                    "QPushButton:hover { color: %2; }")
            .arg(m_palette.foreground.name(), m_palette.accent.name()));
        nameBtn->setCursor(Qt::PointingHandCursor);
        connect(nameBtn, &QPushButton::clicked, this, [this, url = svc.loginUrl]() {
            openUrl(url);
        });
        hbox->addWidget(nameBtn, 1);

        auto* goBtn = new QPushButton("→", row);
        goBtn->setObjectName("sidebarOpenBtn");
        goBtn->setToolTip(QString("Open %1").arg(svc.name));
        goBtn->setCursor(Qt::PointingHandCursor);
        connect(goBtn, &QPushButton::clicked, this, [this, url = svc.homeUrl]() {
            openUrl(url);
        });
        hbox->addWidget(goBtn);

        vbox->addWidget(row);
    }

    auto* div = new QFrame(w);
    div->setObjectName("divider");
    div->setFixedHeight(1);
    div->setStyleSheet(QString("background-color: %1; margin: 10px 10px;")
                       .arg(m_palette.divider.name()));
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
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    auto* topBar = new QWidget(page);
    topBar->setObjectName("sidebarWidget");
    auto* topHbox = new QHBoxLayout(topBar);
    topHbox->setContentsMargins(10, 8, 10, 8);

    auto* backBtn = new QPushButton("← Back", topBar);
    backBtn->setObjectName("backBtn");
    connect(backBtn, &QPushButton::clicked, this, &StreamVaultWindow::goToSearch);
    topHbox->addWidget(backBtn);
    topHbox->addStretch();

    vbox->addWidget(topBar);

    m_backdropLabel = new QLabel(page);
    m_backdropLabel->setFixedHeight(220);
    m_backdropLabel->setAlignment(Qt::AlignCenter);
    m_backdropLabel->setStyleSheet(
        QString("background-color: %1;").arg(m_palette.cardBackground.name()));
    vbox->addWidget(m_backdropLabel);

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* infoWidget = new QWidget(scrollArea);
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

        m_grid->setFocus();
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
        QString("QMenu { background:%1; color:%2; border:1px solid %3; }"
                "QMenu::item:selected { background:%4; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.foreground.name(),
             m_palette.cardBorder.name(),
             m_palette.accent.name()));

    menu.addSection(e->title);

    const QVector<ServiceInfo> services = allServices();
    for (const ServiceInfo& svc : services) {
        const QString encoded = QUrl::toPercentEncoding(e->title);
        const QString url     = svc.searchUrlTemplate.arg(encoded);
        menu.addAction(QString("Search on %1").arg(svc.name),
                       this, [this, url]() { openUrl(url); });
    }

    menu.exec(m_grid->viewport()->mapToGlobal(pos));
}

void StreamVaultWindow::goToDetail(const StreamEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Navigated to detail page."),
        QStringLiteral("title=%1 tmdbId=%2").arg(e.title).arg(e.tmdbId));
    m_currentEntry = e;
    populateDetail(e);
    m_stack->setCurrentIndex(1);
}

void StreamVaultWindow::goToSearch() {
    m_stack->setCurrentIndex(0);
}

void StreamVaultWindow::openSettings() {

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("StreamVault Settings");
    dlg->setModal(true);
    dlg->setStyleSheet(
        QString("QDialog { background:%1; color:%2; }"
                "QLabel  { color:%2; font-size:13px; }"
                "QLineEdit { background:%3; color:%2; border:1px solid %4;"
                "            border-radius:5px; padding:5px 10px; font-size:13px; }"
                "QLineEdit:focus { border-color:%5; }"
                "QPushButton { background:%3; color:%2; border:1px solid %4;"
                "              border-radius:5px; padding:5px 16px; }"
                "QPushButton:hover { background:%5; border-color:%5; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.foreground.name(),
             m_palette.hoverBackground.name(),
             m_palette.cardBorder.name(),
             m_palette.accent.name()));
    dlg->resize(440, 180);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setContentsMargins(20, 18, 20, 18);
    vbox->setSpacing(12);

    auto* infoLabel = new QLabel(
        QString("Enter your free TMDB API key to enable search.\n"
                "Get one at <a href='https://www.themoviedb.org/settings/api' "
                "style='color:%1;'>themoviedb.org/settings/api</a>")
        .arg(m_palette.accent.name()), dlg);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    vbox->addWidget(infoLabel);

    auto* keyEdit = new QLineEdit(dlg);
    keyEdit->setPlaceholderText("TMDB API Key (v3 auth)");
    keyEdit->setText(StreamVaultSettings::instance().tmdbApiKey());
    vbox->addWidget(keyEdit);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, keyEdit]() {
        const QString key = keyEdit->text().trimmed();
        StreamVaultSettings::instance().setTmdbApiKey(key);
        m_tmdb->setApiKey(key);
        dlg->accept();
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
    m_backdropLabel->setStyleSheet(
        QString("background-color: %1; color: %2; font-size: 13px;")
        .arg(m_palette.cardBackground.name(), m_palette.mutedForeground.name()));

    if (m_backdropCache.contains(e.tmdbId)) {
        QPixmap pix = m_backdropCache[e.tmdbId];
        m_backdropLabel->setPixmap(
            pix.scaled(m_backdropLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
        m_backdropLabel->setObjectName("");
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

    const QList<ProviderEntry> providers = map.value(countryCode);
    if (providers.isEmpty()) {
        m_watchStatusLabel->setText(
            QString("Not available for subscription streaming in %1.").arg(countryCode));
        return;
    }

    m_watchStatusLabel->clear();
    const QString encoded = QUrl::toPercentEncoding(m_currentEntry.title);

    QSet<StreamingService> added;

    for (const ProviderEntry& pe : providers) {
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

void StreamVaultWindow::openUrl(const QString& url) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Opening service URL."), url);
    QDesktopServices::openUrl(QUrl(url));
}

}
