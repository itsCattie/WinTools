#include "gui.hpp"

#include "config.hpp"
#include "internal/gui_detail_helpers.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include <common/Themes/theme_helper.hpp>
#include <common/themes/color_utils.hpp>

#include "../core/debug_logger.hpp"

namespace wintools::mediabar::gui_detail {

using wintools::themes::tintedIcon;
using wintools::themes::blendColor;
using wintools::themes::compositeOver;
using wintools::themes::readableTextOn;
using wintools::themes::cssRgba;

QIcon drawnShuffleIcon(const QSize& size, const QColor& color) {
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color);
    pen.setWidthF(std::max(1.0, size.width() * 0.08));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    const qreal w = size.width();
    const qreal h = size.height();
    QPainterPath path;
    path.moveTo(w * 0.15, h * 0.25);
    path.cubicTo(w * 0.35, h * 0.25, w * 0.55, h * 0.45, w * 0.85, h * 0.25);
    path.moveTo(w * 0.15, h * 0.75);
    path.cubicTo(w * 0.35, h * 0.75, w * 0.55, h * 0.55, w * 0.85, h * 0.75);
    p.drawPath(path);

    QPainterPath a1;
    a1.moveTo(w * 0.82, h * 0.18);
    a1.lineTo(w * 0.95, h * 0.25);
    a1.lineTo(w * 0.82, h * 0.32);
    p.drawPath(a1);

    QPainterPath a2;
    a2.moveTo(w * 0.82, h * 0.68);
    a2.lineTo(w * 0.95, h * 0.75);
    a2.lineTo(w * 0.82, h * 0.82);
    p.drawPath(a2);

    p.end();
    return QIcon(pm);
}

int lyricItemHeightForText(const QString& text, const QFont& font, int availableWidth) {
    const int wrapWidth = std::max(80, availableWidth - 20);
    const QFontMetrics metrics(font);
    const QRect bounds = metrics.boundingRect(
        QRect(0, 0, wrapWidth, 10000),
        Qt::TextWordWrap | Qt::TextWrapAnywhere,
        text);

    const int minHeight = metrics.lineSpacing() + 18;
    return std::max(minHeight, bounds.height() + 18);
}

using wintools::themes::ThemeHelper;
using wintools::themes::ThemePalette;

ThemePalette palette() { return ThemeHelper::currentPalette(); }

QColor appForeground() { return palette().foreground; }
QColor appMutedForeground() { return palette().mutedForeground; }
QColor appAccentColor() { return palette().accent; }

}

using namespace wintools::mediabar::gui_detail;

void LyricsWindow::setupUi() {
    rebuildingUi_ = true;

    if (layout()) {
        delete layout();
    }
    const auto oldChildren = children();
    for (QObject* obj : oldChildren) {
        if (obj == themeListener_) continue;
        if (QWidget* w = qobject_cast<QWidget*>(obj)) {

            delete w;
        }
    }

    sideMenu_ = nullptr;
    mainArea_ = nullptr;
    trackLabel_ = nullptr;
    artistLabel_ = nullptr;
    sourceLabel_ = nullptr;
    albumArtLabel_ = nullptr;
    volumePanel_ = nullptr;
    volumeGlyphLabel_ = nullptr;
    volumeSlider_ = nullptr;
    volumeValueLabel_ = nullptr;
    volumeToggleButton_ = nullptr;
    favoriteButton_ = nullptr;
    prevButton_ = nullptr;
    playPauseButton_ = nullptr;
    nextButton_ = nullptr;
    shuffleButton_ = nullptr;
    progressBar_ = nullptr;
    lyricsList_ = nullptr;
    menuButton_ = nullptr;
    miniModeButton_ = nullptr;
    sourceModeCombo_ = nullptr;
    searchButton_ = nullptr;
    spotifyLibraryButton_ = nullptr;
    spotifyDevicesButton_ = nullptr;
    localBrowseButton_ = nullptr;
    queueButton_ = nullptr;
    appearanceButton_ = nullptr;
    libraryPathButton_ = nullptr;
    debugButton_ = nullptr;
    lastLyricWrapWidth_ = -1;
    collapsedVolumePanelVisible_ = false;

    const ThemePalette& pal = ThemeHelper::currentPalette();
    const QColor background = pal.windowBackground;
    const QColor text = pal.foreground;
    const QColor inactive = pal.mutedForeground;
    const QColor accent = pal.accent;
    const QColor border = pal.cardBorder;
    const QColor surface = pal.cardBackground;
    const bool isDark = background.lightness() < 128;
    const QColor accentHover = accent.lighter(isDark ? 112 : 106);
    const QColor primaryPlayText = readableTextOn(accent);

    const QColor btnHover = compositeOver(surface, pal.hoverBackground);

    const QString sideHoverCss  = cssRgba(text, 15);
    const QString sideSelectedCss = cssRgba(accent, 30);

    setWindowTitle("MediaBar");
    setWindowIcon(QIcon(":/icons/modules/mediabar.svg"));
    if (width() <= 1 || height() <= 1) {
        resize(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    }

    const QString qss = QStringLiteral(
        "QWidget { background-color: %1; color: %2; font-family: '%3'; }"
        "QWidget#sideMenu { background-color: %7; border: none;"
        " border-top-left-radius: 0px; border-bottom-left-radius: 0px;"
        " border-top-right-radius: 18px; border-bottom-right-radius: 18px; }"
        "QLabel#track { font-size: 20pt; font-weight: 700; letter-spacing: 0.5px; }"
        "QLabel#artist { font-size: 13pt; color: %4; }"
        "QLabel#source { font-size: 11pt; color: %5; }"
        "QLabel#time { font-size: 10pt; color: %4; }"
        "QListWidget { border: none; background-color: %1; border-radius: 12px; }"
        "QListWidget::item { padding: 10px; font-size: 15pt; color: %4; border-radius: 8px; }"
        "QListWidget::item:selected { background-color: %8; color: %2; }"
        "QScrollBar:vertical { background: transparent; width: 7px; margin: 4px 1px 4px 1px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %6; min-height: 28px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: %5; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QProgressBar { border: none; background-color: %6; border-radius: 4px; text-align: center; min-height: 6px; max-height: 6px; }"
        "QProgressBar::chunk { background-color: %5; border-radius: 4px; }"
        "QPushButton { background-color: %7; border: none; border-radius: 12px; padding: 10px 16px; font-size: 11pt; }"
        "QPushButton:hover { background-color: %8; }"
        "QPushButton#sideBtn { text-align: left; padding: 10px 18px; border: none;"
        " border-radius: 0px; color: %2; background-color: transparent; background: transparent;"
        " outline: none; margin: 0; font-size: 11pt; font-weight: 500; }"
        "QPushButton#sideBtn:hover { background-color: %9; color: %2; border-radius: 10px; }"
        "QPushButton#sideBtn:checked { background-color: %10; color: %5; font-weight: 700; border-radius: 10px; }"
        "QPushButton#sideBtn:checked:hover { background-color: %10; color: %5; border-radius: 10px; }"
        "QPushButton#primaryPlay { background-color: %5; color: %11; border: none; border-radius: 22px; font-weight: 700; padding: 12px 32px; font-size: 13pt; }"
        "QPushButton#primaryPlay:hover { background-color: %12; }"
        "QPushButton#favorite { border: none; background: transparent; color: %4; padding: 3px 6px; font-size: 15pt; }"
        "QPushButton#favorite:hover { color: %5; }"
        "QPushButton#secondaryControl { background-color: transparent; color: %2; border: none; border-radius: 20px; padding: 12px 20px; font-size: 12pt; }"
        "QPushButton#secondaryControl:hover { background-color: %8; }"
        "QPushButton#shuffleControl { background-color: transparent; color: %4; border: none; border-radius: 16px; padding: 8px 12px; font-size: 11pt; }"
        "QPushButton#shuffleControl:hover { background-color: %8; color: %2; }"
        "QPushButton#topBarBtn { background-color: transparent; border: none; border-radius: 10px; padding: 8px; color: %4; font-size: 12pt; }"
        "QPushButton#topBarBtn:hover { background-color: %8; color: %2; }"
    ).arg(
        background.name(),
        text.name(),
        config::FONT_FAMILY,
        inactive.name(),
        accent.name(),
        border.name(),
        surface.name(),
        btnHover.name(),
        sideHoverCss,
        sideSelectedCss,
        primaryPlayText.name(),
        accentHover.name()
    );
    ThemeHelper::applyThemeTo(this, qss);

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
        "font-size: 13pt; font-weight: 700; padding: 8px 12px; background: transparent;"));
    sideLayout->addWidget(sideTitle);
    sideLayout->addSpacing(8);

    auto* sourceHeader = new QLabel("SOURCE", sideMenu_);
    sourceHeader->setObjectName("section");
    sourceHeader->setStyleSheet(QString(
        "font-size: 8pt; font-weight: 600; color: %1; padding: 6px 10px; "
        "letter-spacing: 1px; background: transparent;")
        .arg(inactive.name()));
    sideLayout->addWidget(sourceHeader);

    sourceSpotifyButton_ = createSideMenuButton(QIcon(":/icons/mediabar/spotify.svg"), "Spotify");
    sourceSonosButton_ = createSideMenuButton(QIcon(":/icons/mediabar/sonos.svg"), "Sonos");
    sourceSpotifyButton_->setCheckable(true);
    sourceSonosButton_->setCheckable(true);
    sideLayout->addWidget(sourceSpotifyButton_);
    sideLayout->addWidget(sourceSonosButton_);
    sideLayout->addSpacing(4);

    auto* musicHeader = new QLabel("MUSIC", sideMenu_);
    musicHeader->setObjectName("section");
    musicHeader->setStyleSheet(QString(
        "font-size: 8pt; font-weight: 600; color: %1; padding: 6px 10px; "
        "letter-spacing: 1px; background: transparent;")
        .arg(inactive.name()));
    sideLayout->addWidget(musicHeader);

    searchButton_ = createSideMenuButton(QIcon(":/icons/mediabar/search.svg"), "Search");
    spotifyLibraryButton_ = createSideMenuButton(QIcon(":/icons/mediabar/library.svg"), "Library");
    spotifyDevicesButton_ = createSideMenuButton(QIcon(":/icons/mediabar/devices.svg"), "Devices");
    localBrowseButton_ = createSideMenuButton(QIcon(":/icons/mediabar/browse.svg"), "Browse");
    queueButton_ = createSideMenuButton(QIcon(":/icons/mediabar/queue.svg"), "Queue");
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
        "letter-spacing: 1px; background: transparent;")
        .arg(inactive.name()));
    sideLayout->addWidget(settingsHeader);

    appearanceButton_ = createSideMenuButton(QIcon(":/icons/mediabar/appearance.svg"), "Mini Style");
    libraryPathButton_ = createSideMenuButton(QIcon(":/icons/mediabar/library_path.svg"), "Library Path");
    debugButton_ = createSideMenuButton(QIcon(":/icons/mediabar/debug.svg"), "Debug");
    sideLayout->addWidget(appearanceButton_);
    sideLayout->addWidget(libraryPathButton_);
    sideLayout->addWidget(debugButton_);

    sideLayout->addStretch(1);

    volumePanel_ = new QWidget(sideMenu_);
    volumePanel_->setStyleSheet(QString("QWidget { background-color: %1; border-radius: 10px; }")
        .arg(surface.name()));
    auto* volumePanelLayout = new QBoxLayout(QBoxLayout::LeftToRight, volumePanel_);
    volumePanelLayout->setContentsMargins(8, 6, 8, 6);
    volumePanelLayout->setSpacing(6);

    volumeGlyphLabel_ = new QLabel(volumePanel_);
    volumeGlyphLabel_->setObjectName("volumeGlyph");
    volumeGlyphLabel_->setFixedWidth(22);
    volumeGlyphLabel_->setAlignment(Qt::AlignCenter);
    volumeGlyphLabel_->setPixmap(
        tintedIcon(style()->standardIcon(QStyle::SP_MediaVolume), QSize(18, 18), text)
            .pixmap(QSize(18, 18)));

    volumeSlider_ = new QSlider(Qt::Horizontal, volumePanel_);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setSingleStep(1);
    volumeSlider_->setPageStep(5);
    volumeSlider_->setValue(50);
    volumeSlider_->setEnabled(false);
    volumeSlider_->setFixedWidth(150);
    volumeSlider_->setStyleSheet(QString(
        "QSlider::groove:horizontal { height: 8px; background: %1; border-radius: 4px; }"
        "QSlider::add-page:horizontal { background: %1; border-radius: 4px; }"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 4px; }"
        "QSlider::handle:horizontal { background: %3; width: 16px; margin: -4px 0; border-radius: 8px; }"
        "QSlider::groove:vertical { width: 8px; background: %1; border-radius: 4px; }"
        "QSlider::add-page:vertical { background: %2; border-radius: 4px; }"
        "QSlider::sub-page:vertical { background: %1; border-radius: 4px; }"
        "QSlider::handle:vertical { background: %3; height: 16px; margin: 0 -4px; border-radius: 8px; }")
        .arg(border.name(), accent.name(), text.name()));

    volumeValueLabel_ = new QLabel("--%", volumePanel_);
    volumeValueLabel_->setObjectName("volumeValue");
    volumeValueLabel_->setFixedWidth(36);
    volumeValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeValueLabel_->hide();

    volumePanelLayout->addWidget(volumeGlyphLabel_);
    volumePanelLayout->addWidget(volumeSlider_, 1);
    volumePanelLayout->addWidget(volumeValueLabel_);

    volumeToggleButton_ = new QPushButton(sideMenu_);
    volumeToggleButton_->setCursor(Qt::PointingHandCursor);
    volumeToggleButton_->setToolTip("Volume");
    volumeToggleButton_->setText(QString());
    volumeToggleButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaVolume), QSize(22, 22), text));
    volumeToggleButton_->setIconSize(QSize(22, 22));
    volumeToggleButton_->setStyleSheet(QString(
        "QPushButton { background-color: transparent; border: none; border-radius: 10px; padding: 6px; }"
        "QPushButton:hover { background-color: %1; }")
        .arg(btnHover.name()));

    sideLayout->addWidget(volumePanel_);
    sideLayout->addWidget(volumeToggleButton_);

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
    ).arg(surface.name()));
    artContainer->setFixedSize(148, 148);
    auto* artLayout = new QVBoxLayout(artContainer);
    artLayout->setContentsMargins(4, 4, 4, 4);

    albumArtLabel_ = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xB5"), artContainer);
    albumArtLabel_->setAlignment(Qt::AlignCenter);
    albumArtLabel_->setStyleSheet(QString(
        "font-size: 28pt; background-color: %1; border-radius: 10px;"
    ).arg(surface.name()));
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
    repeatButton_ = new QPushButton(QString(), mainArea_);
    shuffleButton_ = new QPushButton(QString(), mainArea_);

    prevButton_->setFixedSize(44, 44);
    playPauseButton_->setFixedSize(52, 52);
    playPauseButton_->setObjectName("primaryPlay");
    nextButton_->setFixedSize(44, 44);
    repeatButton_->setFixedSize(38, 38);
    shuffleButton_->setFixedSize(38, 38);
    shuffleButton_->setToolTip("Shuffle (Spotify)");
    shuffleButton_->setEnabled(false);
    prevButton_->setObjectName("secondaryControl");
    nextButton_->setObjectName("secondaryControl");
    repeatButton_->setObjectName("secondaryControl");
    shuffleButton_->setObjectName("shuffleControl");

    shuffleButton_->setFocusPolicy(Qt::NoFocus);

    shuffleButton_->setFlat(false);

    connect(shuffleButton_, &QPushButton::clicked, this, &LyricsWindow::shuffleRequested);
    connect(prevButton_, &QPushButton::clicked, this, &LyricsWindow::prevRequested);
    connect(playPauseButton_, &QPushButton::clicked, this, &LyricsWindow::playPauseRequested);
    connect(nextButton_, &QPushButton::clicked, this, &LyricsWindow::nextRequested);
    connect(repeatButton_, &QPushButton::clicked, this, &LyricsWindow::repeatRequested);
    connect(miniModeButton_, &QPushButton::clicked, this, &LyricsWindow::miniModeRequested);
    connect(favoriteButton_, &QPushButton::clicked, this, &LyricsWindow::favoriteRequested);

    connect(searchButton_, &QPushButton::clicked, this, &LyricsWindow::searchRequested);
    connect(spotifyLibraryButton_, &QPushButton::clicked, this, &LyricsWindow::spotifyLibraryRequested);
    connect(spotifyDevicesButton_, &QPushButton::clicked, this, &LyricsWindow::spotifyDevicesRequested);
    connect(localBrowseButton_, &QPushButton::clicked, this, &LyricsWindow::localBrowseRequested);
    connect(queueButton_, &QPushButton::clicked, this, &LyricsWindow::queueRequested);
    connect(appearanceButton_, &QPushButton::clicked, this, &LyricsWindow::appearanceRequested);
    connect(libraryPathButton_, &QPushButton::clicked, this, &LyricsWindow::libraryPathRequested);
    connect(debugButton_, &QPushButton::clicked, this, &LyricsWindow::debugRequested);
    connect(volumeToggleButton_, &QPushButton::clicked, this, [this]() {
        if (!volumePanel_ || !volumeSlider_) {
            return;
        }
        collapsedVolumePanelVisible_ = !collapsedVolumePanelVisible_;
        if (!volumeSlider_->isEnabled()) {
            collapsedVolumePanelVisible_ = false;
        }
        updateSidebarPresentation();
    });
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (volumeValueLabel_) {
            volumeValueLabel_->setText(QString("%1%").arg(qBound(0, value, 100)));
        }
    });
    connect(volumeSlider_, &QSlider::sliderReleased, this, [this]() {
        if (!volumeSlider_ || applyingVolumeUiState_ || !volumeSlider_->isEnabled()) {
            return;
        }
        emit volumeChanged(volumeSlider_->value());
    });

    const QSize mediaIconSize(18, 18);
    prevButton_->setIconSize(mediaIconSize);
    playPauseButton_->setIconSize(mediaIconSize);
    nextButton_->setIconSize(mediaIconSize);
    shuffleButton_->setIconSize(QSize(16, 16));
    prevButton_->setToolTip("Previous track");
    playPauseButton_->setToolTip("Play/Pause");
    nextButton_->setToolTip("Next track");
    updateTransportIcons();

    const QIcon shuffleResource(":/icons/mediabar/shuffle.svg");
    shuffleButton_->setProperty("origIcon", QVariant::fromValue(shuffleResource));

    updateShuffleButtonStyle();

    updateRepeatButtonStyle();

    controls->addWidget(shuffleButton_);
    controls->addSpacing(4);
    controls->addWidget(prevButton_);
    controls->addWidget(playPauseButton_);
    controls->addWidget(nextButton_);
    controls->addWidget(repeatButton_);

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
    ).arg(border.name()));

    lyricsList_ = new QListWidget(mainArea_);
    lyricsList_->setSelectionMode(QAbstractItemView::NoSelection);
    lyricsList_->setFocusPolicy(Qt::NoFocus);
    lyricsList_->setWordWrap(true);
    lyricsList_->setSpacing(4);

    mainLayout->addLayout(topBar);
    mainLayout->addWidget(sourceModeCombo_);
    mainLayout->addLayout(header);
    mainLayout->addLayout(controls);
    mainLayout->addLayout(progressRow);
    mainLayout->addWidget(separator);
    mainLayout->addWidget(lyricsList_, 1);

    root->addWidget(mainArea_, 1);

    connect(menuButton_, &QPushButton::clicked, this, &LyricsWindow::toggleSidebar);
    connect(sourceSpotifyButton_, &QPushButton::clicked, [this]() { setSourceMode("spotify"); });
    connect(sourceSonosButton_, &QPushButton::clicked, [this]() { setSourceMode("sonos"); });

    sidebarExpanded_ = config::settingBool("sidebar_show_labels", false);
    updateSidebarPresentation();

    syncSourceButtons();

    artNetwork_ = new QNetworkAccessManager(this);
    if (!currentAlbumArtUrl_.isEmpty()) {
        loadAlbumArtFromUrl(currentAlbumArtUrl_);
    }

    rebuildingUi_ = false;
}

QPushButton* LyricsWindow::createSideMenuButton(const QIcon& icon, const QString& text) {
    auto* button = new QPushButton(text, sideMenu_);
    button->setObjectName("sideBtn");
    button->setProperty("fullText", text);

    const QColor icColor = appForeground();

    if (icon.isNull()) {
        debuglog::warn("GUI", QString("createSideMenuButton: provided icon for '%1' is null").arg(text));
    }

    button->setIcon(tintedIcon(icon, QSize(22, 22), icColor));

    button->setProperty("origIcon", QVariant::fromValue(icon));
    button->setIconSize(QSize(22, 22));
    button->setFlat(true);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
