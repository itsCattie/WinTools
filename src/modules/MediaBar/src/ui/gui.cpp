#include "gui.hpp"

#include "config.hpp"

#include <windows.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

// MediaBar: gui manages UI behavior and presentation.

LyricsWindow::LyricsWindow(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

bool LyricsWindow::nativeEvent(const QByteArray& eventType,
                                void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        static const UINT ipcMsg =
            RegisterWindowMessage(LyricsWindow::IpcMsgName);
        const auto* msg = static_cast<const MSG*>(message);
        if (ipcMsg && msg->message == ipcMsg) {
            emit ipcCommand(static_cast<int>(msg->wParam));
            *result = 0;
            return true;
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}

namespace {
QIcon tintedIcon(const QIcon& base, const QSize& size, const QColor& tint) {
    if (base.isNull()) {
        return QIcon();
    }

    QPixmap pix = base.pixmap(size);
    if (pix.isNull()) {
        return QIcon();
    }

    QImage image = pix.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), tint);
    painter.end();
    return QIcon(QPixmap::fromImage(image));
}
}

void LyricsWindow::setupUi() {
    setWindowTitle("MediaBar");
    setWindowIcon(QIcon(":/icons/modules/mediabar.svg"));
    resize(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    setStyleSheet(QString(
        "QWidget { background-color: %1; color: %2; font-family: '%3'; }"
        "QWidget#sideMenu { background-color: %7; border: none; }"
        "QLabel#track { font-size: 18pt; font-weight: 700; }"
        "QLabel#artist { font-size: 12pt; color: %4; }"
        "QLabel#source { font-size: 10pt; color: %5; }"
        "QLabel#time { font-size: 9pt; color: %4; }"
        "QListWidget { border: none; background-color: %1; }"
        "QListWidget::item { padding: 8px; font-size: 14pt; color: %4; }"
        "QListWidget::item:selected { background-color: transparent; color: %2; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 4px 1px 4px 1px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: %6; min-height: 28px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: %5; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QProgressBar { border: none; background-color: %6; border-radius: 3px; text-align: center; min-height: 5px; max-height: 5px; }"
        "QProgressBar::chunk { background-color: %5; border-radius: 3px; }"
        "QPushButton { background-color: %7; border: none; border-radius: 10px; padding: 8px; }"
        "QPushButton:hover { background-color: %8; }"
        "QPushButton#sideBtn { text-align: left; padding: 7px 12px; border: none; border-radius: 10px; color: %4; font-size: 10pt; }"
        "QPushButton#sideBtn:hover { background-color: %8; color: %2; }"
        "QPushButton#sideBtn:checked { background-color: %9; color: %2; }"
        "QPushButton#primaryPlay { background-color: %5; color: #000000; border: none; border-radius: 20px; font-weight: 700; padding: 10px 28px; }"
        "QPushButton#primaryPlay:hover { background-color: %10; }"
        "QPushButton#favorite { border: none; background: transparent; color: %4; padding: 2px 4px; }"
        "QPushButton#favorite:hover { color: %5; }"
        "QPushButton#secondaryControl { background-color: transparent; color: %2; border: none; border-radius: 18px; padding: 10px 18px; }"
        "QPushButton#secondaryControl:hover { background-color: %8; }"
        "QPushButton#shuffleControl { background-color: transparent; color: %4; border: none; border-radius: 14px; padding: 6px 10px; }"
        "QPushButton#shuffleControl:hover { background-color: %8; color: %2; }"
        "QPushButton#topBarBtn { background-color: transparent; border: none; border-radius: 8px; padding: 6px; color: %4; }"
        "QPushButton#topBarBtn:hover { background-color: %8; color: %2; }"
    ).arg(
        config::BACKGROUND_COLOR,
        config::TEXT_COLOR,
        config::FONT_FAMILY,
        config::INACTIVE_TEXT_COLOR,
        config::HIGHLIGHT_COLOR,
        config::BORDER_COLOR,
        config::SURFACE_COLOR,
        config::SURFACE_HOVER,
        config::CARD_COLOR
    ).arg(
        config::HIGHLIGHT_HOVER
    ));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    sideMenu_ = new QWidget(this);
    sideMenu_->setObjectName("sideMenu");
    sideMenu_->setFixedWidth(config::SIDEBAR_EXPANDED_WIDTH);
    auto* sideLayout = new QVBoxLayout(sideMenu_);
    sideLayout->setContentsMargins(8, 14, 8, 14);
    sideLayout->setSpacing(2);

    auto* sideTitle = new QLabel("MediaBar", sideMenu_);
    sideTitle->setStyleSheet(QString(
        "font-size: 13pt; font-weight: 700; padding: 8px 12px; "
        "border-radius: 11px; background-color: %1;")
        .arg(config::CARD_COLOR));
    sideLayout->addWidget(sideTitle);
    sideLayout->addSpacing(8);

    auto* sourceHeader = new QLabel("SOURCE", sideMenu_);
    sourceHeader->setObjectName("section");
    sourceHeader->setStyleSheet(QString(
        "font-size: 8pt; font-weight: 600; color: %1; padding: 6px 10px; "
        "letter-spacing: 1px; border-radius: 9px; background-color: %2;")
        .arg(config::INACTIVE_TEXT_COLOR, config::CARD_COLOR));
    sideLayout->addWidget(sourceHeader);

    sourceAutoButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x94\x8D"), "Auto Detect");
    sourceSpotifyButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x8E\xB5"), "Spotify");
    sourceSonosButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x94\x8A"), "Sonos");
    sourceAutoButton_->setCheckable(true);
    sourceSpotifyButton_->setCheckable(true);
    sourceSonosButton_->setCheckable(true);
    sideLayout->addWidget(sourceAutoButton_);
    sideLayout->addWidget(sourceSpotifyButton_);
    sideLayout->addWidget(sourceSonosButton_);
    sideLayout->addSpacing(4);

    auto* musicHeader = new QLabel("MUSIC", sideMenu_);
    musicHeader->setObjectName("section");
    musicHeader->setStyleSheet(QString(
        "font-size: 8pt; font-weight: 600; color: %1; padding: 6px 10px; "
        "letter-spacing: 1px; border-radius: 9px; background-color: %2;")
        .arg(config::INACTIVE_TEXT_COLOR, config::CARD_COLOR));
    sideLayout->addWidget(musicHeader);

    searchButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x94\x8E"), "Search");
    spotifyLibraryButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x93\x9A"), "Library");
    spotifyDevicesButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x93\xB1"), "Devices");
    localBrowseButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x93\x82"), "Browse");
    queueButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x93\x8B"), "Queue");
    sideLayout->addWidget(searchButton_);
    sideLayout->addWidget(spotifyLibraryButton_);
    sideLayout->addWidget(spotifyDevicesButton_);
    sideLayout->addWidget(localBrowseButton_);
    sideLayout->addWidget(queueButton_);
    sideLayout->addSpacing(4);

    auto* settingsHeader = new QLabel("SETTINGS", sideMenu_);
    settingsHeader->setObjectName("section");
    settingsHeader->setStyleSheet(QString(
        "font-size: 8pt; font-weight: 600; color: %1; padding: 6px 10px; "
        "letter-spacing: 1px; border-radius: 9px; background-color: %2;")
        .arg(config::INACTIVE_TEXT_COLOR, config::CARD_COLOR));
    sideLayout->addWidget(settingsHeader);

    appearanceButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x8E\xA8"), "Appearance");
    libraryPathButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x93\x81"), "Library Path");
    debugButton_ = createSideMenuButton(QString::fromUtf8("\xF0\x9F\x94\xA7"), "Debug");
    sideLayout->addWidget(appearanceButton_);
    sideLayout->addWidget(libraryPathButton_);
    sideLayout->addWidget(debugButton_);
    sideLayout->addStretch(1);

    root->addWidget(sideMenu_);

    mainArea_ = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(mainArea_);
    mainLayout->setContentsMargins(20, 14, 20, 14);
    mainLayout->setSpacing(10);

    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(6);

    menuButton_ = new QPushButton(QString::fromUtf8("\u2630"), mainArea_);
    menuButton_->setObjectName("topBarBtn");
    menuButton_->setFixedSize(34, 34);
    menuButton_->setToolTip("Toggle side menu");

    auto* titleIcon = new QLabel(mainArea_);
    titleIcon->setPixmap(QIcon(":/icons/modules/mediabar.svg").pixmap(20, 20));
    titleIcon->setFixedSize(24, 24);
    titleIcon->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("MediaBar", mainArea_);
    title->setStyleSheet("font-size: 11pt; font-weight: 600;");

    miniModeButton_ = new QPushButton(QString::fromUtf8("\u2296"), mainArea_);
    miniModeButton_->setObjectName("topBarBtn");
    miniModeButton_->setFixedSize(34, 34);
    miniModeButton_->setToolTip("Mini player mode");

    topBar->addWidget(menuButton_);
    topBar->addWidget(titleIcon);
    topBar->addWidget(title);
    topBar->addStretch(1);
    topBar->addWidget(miniModeButton_);

    sourceModeCombo_ = new QComboBox(mainArea_);
    sourceModeCombo_->addItem("Auto Detect", "auto");
    sourceModeCombo_->addItem("Spotify", "spotify");
    sourceModeCombo_->addItem("Sonos", "sonos");
    sourceModeCombo_->setCurrentIndex(0);
    sourceModeCombo_->hide();

    auto* header = new QVBoxLayout();
    header->setSpacing(6);
    header->setAlignment(Qt::AlignHCenter);

    auto* artContainer = new QFrame(mainArea_);
    artContainer->setStyleSheet(QString(
        "QFrame { background-color: %1; border: none; border-radius: 12px; }"
    ).arg(config::SURFACE_COLOR));
    artContainer->setFixedSize(148, 148);
    auto* artLayout = new QVBoxLayout(artContainer);
    artLayout->setContentsMargins(4, 4, 4, 4);

    albumArtLabel_ = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xB5"), artContainer);
    albumArtLabel_->setAlignment(Qt::AlignCenter);
    albumArtLabel_->setStyleSheet(QString(
        "font-size: 28pt; background-color: %1; border-radius: 10px;"
    ).arg(config::SURFACE_COLOR));
    albumArtLabel_->setMinimumSize(140, 140);
    albumArtLabel_->setMaximumSize(140, 140);
    artLayout->addWidget(albumArtLabel_);

    auto* artShadow = new QGraphicsDropShadowEffect(artContainer);
    artShadow->setBlurRadius(24);
    artShadow->setColor(QColor(0, 0, 0, 100));
    artShadow->setOffset(0, 4);
    artContainer->setGraphicsEffect(artShadow);

    auto* sourceRow = new QHBoxLayout();
    sourceRow->setSpacing(8);
    sourceRow->setAlignment(Qt::AlignHCenter);

    sourceLabel_ = new QLabel("Unknown", mainArea_);
    sourceLabel_->setObjectName("source");
    sourceLabel_->setAlignment(Qt::AlignCenter);

    favoriteButton_ = new QPushButton(QString::fromUtf8("\xE2\x99\xA1"), mainArea_);
    favoriteButton_->setObjectName("favorite");
    favoriteButton_->setToolTip("Like/unlike current Spotify track");
    favoriteButton_->setFixedWidth(28);
    favoriteButton_->setEnabled(false);

    sourceRow->addWidget(sourceLabel_);
    sourceRow->addWidget(favoriteButton_);

    trackLabel_ = new QLabel("No track playing", mainArea_);
    trackLabel_->setObjectName("track");
    trackLabel_->setAlignment(Qt::AlignCenter);
    artistLabel_ = new QLabel("", mainArea_);
    artistLabel_->setObjectName("artist");
    artistLabel_->setAlignment(Qt::AlignCenter);

    header->addWidget(artContainer, 0, Qt::AlignHCenter);
    header->addSpacing(4);
    header->addLayout(sourceRow);
    header->addWidget(trackLabel_);
    header->addWidget(artistLabel_);

    auto* controls = new QHBoxLayout();
    controls->setSpacing(10);
    controls->setAlignment(Qt::AlignHCenter);
    prevButton_ = new QPushButton(QString(), mainArea_);
    playPauseButton_ = new QPushButton(QString(), mainArea_);
    nextButton_ = new QPushButton(QString(), mainArea_);
    shuffleButton_ = new QPushButton(QString(), mainArea_);

    prevButton_->setFixedSize(44, 44);
    playPauseButton_->setFixedSize(52, 52);
    playPauseButton_->setObjectName("primaryPlay");
    nextButton_->setFixedSize(44, 44);
    shuffleButton_->setFixedSize(38, 38);
    shuffleButton_->setToolTip("Shuffle (Spotify)");
    shuffleButton_->setEnabled(false);
    prevButton_->setObjectName("secondaryControl");
    nextButton_->setObjectName("secondaryControl");
    shuffleButton_->setObjectName("shuffleControl");

    const QSize mediaIconSize(18, 18);
    prevButton_->setIconSize(mediaIconSize);
    playPauseButton_->setIconSize(mediaIconSize);
    nextButton_->setIconSize(mediaIconSize);
    shuffleButton_->setIconSize(QSize(16, 16));
    prevButton_->setToolTip("Previous track");
    playPauseButton_->setToolTip("Play/Pause");
    nextButton_->setToolTip("Next track");
    updateTransportIcons();

    controls->addWidget(shuffleButton_);
    controls->addSpacing(4);
    controls->addWidget(prevButton_);
    controls->addWidget(playPauseButton_);
    controls->addWidget(nextButton_);

    auto* progressRow = new QHBoxLayout();
    progressRow->setSpacing(10);

    currentTimeLabel_ = new QLabel("0:00", mainArea_);
    currentTimeLabel_->setObjectName("time");
    durationLabel_ = new QLabel("0:00", mainArea_);
    durationLabel_->setObjectName("time");

    progressBar_ = new QProgressBar(mainArea_);
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(false);

    progressRow->addWidget(currentTimeLabel_);
    progressRow->addWidget(progressBar_, 1);
    progressRow->addWidget(durationLabel_);

    auto* separator = new QFrame(mainArea_);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QString(
        "background-color: %1; min-height: 1px; max-height: 1px; border: none;"
    ).arg(config::BORDER_COLOR));

    lyricsList_ = new QListWidget(mainArea_);
    lyricsList_->setSelectionMode(QAbstractItemView::NoSelection);
    lyricsList_->setFocusPolicy(Qt::NoFocus);

    mainLayout->addLayout(topBar);
    mainLayout->addWidget(sourceModeCombo_);
    mainLayout->addLayout(header);
    mainLayout->addLayout(controls);
    mainLayout->addLayout(progressRow);
    mainLayout->addWidget(separator);
    mainLayout->addWidget(lyricsList_, 1);

    root->addWidget(mainArea_, 1);

    connect(menuButton_, &QPushButton::clicked, this, &LyricsWindow::toggleSidebar);
    connect(sourceAutoButton_, &QPushButton::clicked, [this]() { setSourceMode("auto"); });
    connect(sourceSpotifyButton_, &QPushButton::clicked, [this]() { setSourceMode("spotify"); });
    connect(sourceSonosButton_, &QPushButton::clicked, [this]() { setSourceMode("sonos"); });

    sidebarExpanded_ = config::settingBool("sidebar_show_labels", false);
    updateSidebarPresentation();

    syncSourceButtons();

    artNetwork_ = new QNetworkAccessManager(this);
}

QPushButton* LyricsWindow::createSideMenuButton(const QString& icon, const QString& text) {
    auto* button = new QPushButton(QString("%1  %2").arg(icon, text), sideMenu_);
    button->setObjectName("sideBtn");
    button->setProperty("fullText", text);
    button->setProperty("iconEmoji", icon);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void LyricsWindow::updateTrackInfo(const PlaybackInfo& playback, const QString& sourceName) {
    trackLabel_->setText(playback.trackName.isEmpty() ? "Unknown" : playback.trackName);
    artistLabel_->setText(playback.artistName);
    sourceLabel_->setText(sourceName);

    const QString artUrl = playback.albumArt.trimmed();
    if (artUrl.isEmpty()) {
        currentAlbumArtUrl_.clear();
        albumArtLabel_->setPixmap(QPixmap());
        albumArtLabel_->setText(QString::fromUtf8("🎵"));
        return;
    }

    if (artUrl == currentAlbumArtUrl_) {
        return;
    }
    currentAlbumArtUrl_ = artUrl;

    QNetworkReply* reply = artNetwork_->get(QNetworkRequest(QUrl(artUrl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QPixmap art;
        if (!art.loadFromData(data)) {
            albumArtLabel_->setPixmap(QPixmap());
            albumArtLabel_->setText(QString::fromUtf8("🎵"));
            return;
        }

        albumArtLabel_->setText(QString());
        albumArtLabel_->setPixmap(art.scaled(albumArtLabel_->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    });
}

void LyricsWindow::updatePlayPauseButton(bool isPlaying) {
    isPlaying_ = isPlaying;
    updateTransportIcons();
}

void LyricsWindow::updateTransportIcons() {
    if (!prevButton_ || !playPauseButton_ || !nextButton_) {
        return;
    }

    const QSize mediaIconSize = playPauseButton_->iconSize().isValid() ? playPauseButton_->iconSize() : QSize(18, 18);
    prevButton_->setText(QString());
    playPauseButton_->setText(QString());
    nextButton_->setText(QString());

    prevButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward), mediaIconSize, QColor("#FFFFFF")));
    nextButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipForward), mediaIconSize, QColor("#FFFFFF")));

    const QColor playColor = isPlaying_ ? QColor("#000000") : QColor("#FFFFFF");
    playPauseButton_->setIcon(tintedIcon(style()->standardIcon(isPlaying_ ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay), mediaIconSize, playColor));
}

void LyricsWindow::updateProgress(qint64 progressMs, qint64 durationMs) {
    const auto formatDuration = [](qint64 ms) {
        const int totalSeconds = static_cast<int>(std::max<qint64>(0, ms) / 1000);
        return QTime(0, 0).addSecs(totalSeconds).toString("m:ss");
    };

    if (durationMs <= 0) {
        progressBar_->setValue(0);
        currentTimeLabel_->setText("0:00");
        durationLabel_->setText("0:00");
        return;
    }

    const double ratio = static_cast<double>(progressMs) / static_cast<double>(durationMs);
    progressBar_->setValue(static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 1000.0));
    currentTimeLabel_->setText(formatDuration(progressMs));
    durationLabel_->setText(formatDuration(durationMs));
}

void LyricsWindow::setFavoriteState(std::optional<bool> liked, bool spotifySource) {
    if (!favoriteButton_) {
        return;
    }

    favoriteButton_->setEnabled(spotifySource);
    if (!spotifySource) {
        favoriteButton_->setText(QString::fromUtf8("♡"));
        return;
    }

    favoriteButton_->setText(liked.value_or(false) ? QString::fromUtf8("♥") : QString::fromUtf8("♡"));
}

void LyricsWindow::displayLyrics(const LyricsList& lyrics) {
    if (lyrics_.size() == lyrics.size()) {
        bool sameLyrics = true;
        for (int i = 0; i < lyrics.size(); ++i) {
            if (lyrics_[i].timeMs != lyrics[i].timeMs ||
                lyrics_[i].text != lyrics[i].text ||
                lyrics_[i].isInstrumental != lyrics[i].isInstrumental) {
                sameLyrics = false;
                break;
            }
        }
        if (sameLyrics) {
            return;
        }
    }

    lyrics_ = lyrics;
    lyricsList_->clear();

    for (const auto& line : lyrics_) {
        auto* item = new QListWidgetItem(line.text);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setForeground(QColor(config::INACTIVE_TEXT_COLOR));
        QFont f = item->font();
        f.setBold(false);
        f.setPointSize(14);
        item->setFont(f);
        lyricsList_->addItem(item);
    }

    currentLyricIndex_ = -1;
}

void LyricsWindow::highlightCurrentLyric(int index) {
    if (lyricsList_->count() <= 0) {
        return;
    }

    const auto applyStyle = [](QListWidgetItem* item, int fontSize, bool bold, const QColor& color) {
        if (!item) {
            return;
        }
        item->setForeground(color);
        QFont f = item->font();
        f.setBold(bold);
        f.setPointSize(fontSize);
        item->setFont(f);
    };

    if (index < 0 || index >= lyricsList_->count()) {
        if (currentLyricIndex_ < 0) {
            return;
        }

        const int previous = currentLyricIndex_;
        currentLyricIndex_ = -1;

        for (int i = previous - 1; i <= previous + 1; ++i) {
            if (i < 0 || i >= lyricsList_->count()) {
                continue;
            }
            applyStyle(lyricsList_->item(i), 14, false, QColor(config::INACTIVE_TEXT_COLOR));
        }
        return;
    }

    if (index == currentLyricIndex_) {
        return;
    }

    const int previous = currentLyricIndex_;
    currentLyricIndex_ = index;

    QVector<int> indexesToUpdate{index - 1, index, index + 1};
    if (previous >= 0) {
        indexesToUpdate << previous - 1 << previous << previous + 1;
    }

    for (const int i : indexesToUpdate) {
        if (i < 0 || i >= lyricsList_->count()) {
            continue;
        }

        if (i == currentLyricIndex_) {
            applyStyle(lyricsList_->item(i), 18, true, QColor(config::HIGHLIGHT_HOVER));
        } else if (std::abs(i - currentLyricIndex_) == 1) {
            applyStyle(lyricsList_->item(i), 15, false, QColor(config::TEXT_COLOR));
        } else {
            applyStyle(lyricsList_->item(i), 14, false, QColor(config::INACTIVE_TEXT_COLOR));
        }
    }

    if (auto* item = lyricsList_->item(currentLyricIndex_)) {
        lyricsList_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
}

void LyricsWindow::setShuffleState(bool enabled, bool supported) {
    shuffleSupported_ = supported;
    shuffleEnabled_ = supported ? enabled : false;

    if (shuffleButton_) {
        shuffleButton_->setEnabled(shuffleSupported_);
        updateShuffleButtonStyle();
    }
}

void LyricsWindow::updateShuffleButtonStyle() {
    if (!shuffleButton_) {
        return;
    }

    if (!shuffleSupported_) {
        shuffleButton_->setText(QString());
        shuffleButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_BrowserReload), shuffleButton_->iconSize(), QColor(config::INACTIVE_TEXT_COLOR)));
        shuffleButton_->setStyleSheet(QString(
            "background-color: transparent; color: %2; border: none; border-radius: 14px; "
            "padding: 6px 10px; font-size: 10px; font-weight: 600;")
            .arg(config::INACTIVE_TEXT_COLOR));
        return;
    }

    if (shuffleEnabled_) {
        shuffleButton_->setText(QString());
        shuffleButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_BrowserReload), shuffleButton_->iconSize(), QColor("#000000")));
        shuffleButton_->setStyleSheet(QString(
            "background-color: %1; color: #000000; border: none; border-radius: 14px; "
            "padding: 6px 10px; font-size: 10px; font-weight: 700;")
            .arg(config::HIGHLIGHT_COLOR));
        return;
    }

    shuffleButton_->setText(QString());
    shuffleButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_BrowserReload), shuffleButton_->iconSize(), QColor(config::TEXT_COLOR)));
    shuffleButton_->setStyleSheet(QString(
        "background-color: transparent; color: %2; border: none; border-radius: 14px; "
        "padding: 6px 10px; font-size: 10px; font-weight: 600;")
        .arg(config::SURFACE_COLOR, config::TEXT_COLOR));
}

void LyricsWindow::showNoLyrics() {
    if (lyrics_.isEmpty()) {
        lyricsList_->clear();
        auto* item = new QListWidgetItem("No synchronized lyrics found for this track.");
        item->setTextAlignment(Qt::AlignHCenter);
        lyricsList_->addItem(item);
    }
}

void LyricsWindow::showWaiting() {
    trackLabel_->setText("No track playing");
    artistLabel_->setText("Waiting for playback...");
    sourceLabel_->setText("Unknown");
    progressBar_->setValue(0);
    currentTimeLabel_->setText("0:00");
    durationLabel_->setText("0:00");
    currentAlbumArtUrl_.clear();
    albumArtLabel_->setPixmap(QPixmap());
    albumArtLabel_->setText(QString::fromUtf8("🎵"));
    setFavoriteState(std::nullopt, false);
    lyricsList_->clear();
    auto* item = new QListWidgetItem("Waiting for music source...");
    item->setTextAlignment(Qt::AlignHCenter);
    lyricsList_->addItem(item);
}

void LyricsWindow::setSourceMode(const QString& mode) {
    if (!sourceModeCombo_) {
        return;
    }

    const QString normalized = mode.trimmed().toLower();
    for (int i = 0; i < sourceModeCombo_->count(); ++i) {
        if (sourceModeCombo_->itemData(i).toString() == normalized) {
            sourceModeCombo_->setCurrentIndex(i);
            syncSourceButtons();
            return;
        }
    }
    sourceModeCombo_->setCurrentIndex(0);
    syncSourceButtons();
}

QString LyricsWindow::sourceMode() const {
    if (!sourceModeCombo_) {
        return "auto";
    }
    return sourceModeCombo_->currentData().toString();
}

void LyricsWindow::toggleSidebar() {
    setSidebarExpanded(!sidebarExpanded_);
}

void LyricsWindow::setSidebarExpanded(bool expanded) {
    if (!sideMenu_) {
        return;
    }

    sidebarExpanded_ = expanded;
    updateSidebarPresentation();

    auto settings = config::loadSettings();
    settings.insert("sidebar_show_labels", sidebarExpanded_);
    config::saveSettings(settings);
}

void LyricsWindow::syncSourceButtons() {
    if (!sourceAutoButton_ || !sourceSpotifyButton_ || !sourceSonosButton_) {
        return;
    }

    const QString mode = sourceMode();
    sourceAutoButton_->setChecked(mode == "auto");
    sourceSpotifyButton_->setChecked(mode == "spotify");
    sourceSonosButton_->setChecked(mode == "sonos");
}

void LyricsWindow::updateSidebarPresentation() {
    if (!sideMenu_) {
        return;
    }

    const int expandedWidth = config::SIDEBAR_EXPANDED_WIDTH;
    const int collapsedWidth = config::SIDEBAR_COLLAPSED_WIDTH;
    sideMenu_->setFixedWidth(sidebarExpanded_ ? expandedWidth : collapsedWidth);

    const auto sideButtons = sideMenu_->findChildren<QPushButton*>();
    for (auto* button : sideButtons) {
        const QString fullText = button->property("fullText").toString();
        const QString iconEmoji = button->property("iconEmoji").toString();
        if (fullText.isEmpty()) {
            continue;
        }

        if (sidebarExpanded_) {
            button->setText(QString("%1  %2").arg(iconEmoji, fullText));
            button->setToolTip(QString());
            button->setFixedWidth(expandedWidth - 20);
            continue;
        }

        button->setText(iconEmoji);
        button->setToolTip(fullText);
        button->setFixedWidth(collapsedWidth - 14);
    }

    const auto sectionLabels = sideMenu_->findChildren<QLabel*>();
    for (auto* label : sectionLabels) {
        if (label->text() == "MediaBar") {
            label->setVisible(sidebarExpanded_);
            continue;
        }
        if (label->objectName() == "section") {
            label->setVisible(sidebarExpanded_);
        }
    }

    sourceAutoButton_->setVisible(sidebarExpanded_);
    sourceSpotifyButton_->setVisible(sidebarExpanded_);
    sourceSonosButton_->setVisible(sidebarExpanded_);
}
