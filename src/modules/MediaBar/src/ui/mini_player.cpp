#include "mini_player.hpp"

#include "config.hpp"
#include "debug_logger.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/color_utils.hpp"

#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QColor>
#include <QScreen>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

using wintools::themes::ThemeHelper;
using wintools::themes::ThemePalette;
using wintools::themes::tintedIcon;

ThemePalette themePalette() { return ThemeHelper::currentPalette(); }

void appendMiniDebugLog(const QString& message) {
    debuglog::trace("MiniFullscreen", message);
}

#ifdef Q_OS_WIN
QString monitorIdText(HMONITOR monitor) {
    return monitor == nullptr
        ? QStringLiteral("null")
        : QString("0x%1").arg(reinterpret_cast<quintptr>(monitor), 0, 16);
}
#endif

class TrackMarquee final : public QWidget {
public:
    explicit TrackMarquee(QWidget* parent = nullptr)
        : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        timer_.setInterval(30);
        QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
            tick();
        });
    }

    void setUiScale(double scale) {
        uiScale_ = std::max(0.5, scale);
        marqueeGap_ = s(24);
        recalc();
    }

    void setTrackText(const QString& text) {
        if (text_ == text) {
            return;
        }
        text_ = text;
        scrollOffset_ = 0;
        pauseTicks_ = 0;
        recalc();
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        recalc();
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setPen(themePalette().foreground);

        QFont font(QString::fromUtf8("Segoe UI"));
        font.setPointSize(std::max(6, static_cast<int>(9 * uiScale_)));
        painter.setFont(font);

        const int baseline = s(13);
        if (!textTooLong_) {
            painter.drawText(QPoint(0, baseline), text_);
            return;
        }

        const int x1 = -static_cast<int>(scrollOffset_);
        const int x2 = x1 + cycleLength_;
        painter.drawText(QPoint(x1, baseline), text_);
        painter.drawText(QPoint(x2, baseline), text_);
    }

private:
    int s(int value) const {
        return std::max(1, static_cast<int>(std::lround(static_cast<double>(value) * uiScale_)));
    }

    void recalc() {
        QFont font(QString::fromUtf8("Segoe UI"));
        font.setPointSize(std::max(6, static_cast<int>(9 * uiScale_)));
        const QFontMetrics fm(font);

        textWidth_ = fm.horizontalAdvance(text_);
        const int viewWidth = width();
        textTooLong_ = textWidth_ > (viewWidth - s(5));

        if (!textTooLong_) {
            timer_.stop();
            scrollOffset_ = 0;
            cycleLength_ = 0;
            pauseTicks_ = 0;
            return;
        }

        marqueeGap_ = s(24);
        cycleLength_ = textWidth_ + marqueeGap_;
        scrollOffset_ = 0;
        pauseTicks_ = 90;
        if (!timer_.isActive()) {
            timer_.start();
        }
    }

    void tick() {
        if (!textTooLong_ || textWidth_ <= 0) {
            timer_.stop();
            return;
        }

        if (pauseTicks_ > 0) {
            --pauseTicks_;
            update();
            return;
        }

        scrollOffset_ += 1.0;
        if (scrollOffset_ >= static_cast<double>(cycleLength_)) {
            scrollOffset_ = 0.0;
            pauseTicks_ = 60;
        }

        update();
    }

    QTimer timer_;
    QString text_;
    double uiScale_ = 1.0;
    int textWidth_ = 0;
    int marqueeGap_ = 24;
    bool textTooLong_ = false;
    int pauseTicks_ = 0;
    int cycleLength_ = 0;
    double scrollOffset_ = 0.0;
};

}

MiniPlayer::MiniPlayer(QWidget* parent)
    : QWidget(parent) {
    setWindowTitle("MediaBar Mini");
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::Tool, true);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    topmostTimer_ = new QTimer(this);
    topmostTimer_->setInterval(350);
    QObject::connect(topmostTimer_, &QTimer::timeout, this, [this]() {
        syncDesktopMiniVisibility();
    });

    artNetwork_ = new QNetworkAccessManager(this);
    setupUi();
}

void MiniPlayer::setupUi() {
    setStyleSheet(QString(
        "QWidget { background-color: transparent; color: %1; font-family: '%2'; }"
        "QFrame#miniFrame { border: none; background-color: transparent; }"
        "QPushButton { background-color: transparent; color: %1; border: none; }"
        "QPushButton#sourceBtn { color: %1; font-size: 7pt; text-align: left; padding-left: 0px; }"
        "QPushButton#heartBtn { color: %3; }")
        .arg(config::TEXT_COLOR,
             config::FONT_FAMILY,
             config::INACTIVE_TEXT_COLOR));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    frame_ = new QFrame(this);
    frame_->setObjectName("miniFrame");
    frame_->setFrameShape(QFrame::NoFrame);
    root->addWidget(frame_);

    artButton_ = new QPushButton(QString::fromUtf8("♪"), frame_);
    artButton_->setCursor(Qt::PointingHandCursor);
    artButton_->setStyleSheet(artButton_->styleSheet() + QString("font-size: 20pt; color: %1;").arg(themePalette().mutedForeground.name()));

    infoContainer_ = new QWidget(frame_);
    marquee_ = new TrackMarquee(infoContainer_);

    sourceButton_ = new QPushButton(QString::fromUtf8("▶ No source"), infoContainer_);
    sourceButton_->setObjectName("sourceBtn");
    sourceButton_->setCursor(Qt::PointingHandCursor);

    controlsContainer_ = new QWidget(frame_);

    const QString btnBase = QString("font-size: %1pt; padding: 0px; ").arg(15);

    listButton_ = new QPushButton(QString::fromUtf8("☰"), controlsContainer_);
    listButton_->setCursor(Qt::PointingHandCursor);
    listButton_->setStyleSheet(listButton_->styleSheet() + btnBase + "background-color: transparent; border: none;");
    listButton_->hide();
    listButton_->setEnabled(false);

    heartButton_ = new QPushButton(QString::fromUtf8("♡"), controlsContainer_);
    heartButton_->setObjectName("heartBtn");
    heartButton_->setCursor(Qt::PointingHandCursor);
    heartButton_->setStyleSheet(heartButton_->styleSheet() + btnBase + "background-color: transparent; border: none;");

    prevButton_ = new QPushButton(QString(), controlsContainer_);
    prevButton_->setCursor(Qt::PointingHandCursor);
    prevButton_->setStyleSheet(prevButton_->styleSheet() + btnBase + "background-color: transparent; border: none;");

    playPauseButton_ = new QPushButton(QString(), controlsContainer_);
    playPauseButton_->setCursor(Qt::PointingHandCursor);
    playPauseButton_->setStyleSheet(playPauseButton_->styleSheet() + btnBase + "background-color: transparent; border: none;");

    nextButton_ = new QPushButton(QString(), controlsContainer_);
    nextButton_->setCursor(Qt::PointingHandCursor);
    nextButton_->setStyleSheet(nextButton_->styleSheet() + btnBase + "background-color: transparent; border: none;");

    applyGeometry();
    applyStyles();
    setTrackText(QString(), QString());
    setSourceText(QString());
    setPlaying(false);
    showPlaceholderArt();
    updateHeartVisual();
}

int MiniPlayer::s(int value) const {
    return std::max(1, static_cast<int>(std::lround(static_cast<double>(value) * uiScale_)));
}

void MiniPlayer::applyGeometry() {
    QScreen* currentScreen = this->screen();
    if (!currentScreen) {
        currentScreen = QGuiApplication::primaryScreen();
    }
    if (currentScreen) {
        uiScale_ = std::max(0.75, currentScreen->logicalDotsPerInch() / 96.0);
    } else {
        uiScale_ = 1.0;
    }

    resize(s(kWidth), s(kHeight));
    frame_->setGeometry(0, 0, s(kWidth), s(kHeight));

    artButton_->setGeometry(s(4), 0, s(kArtSize), s(kArtSize));

    const int infoX = s(4 + kArtSize + 8);
    const int controlsX = s(260);
    const int infoWidth = std::max(s(kInfoWidth), controlsX - infoX);
    infoContainer_->setGeometry(infoX, 0, infoWidth, s(kHeight));
    marquee_->setGeometry(0, s(2), infoWidth, s(18));
    sourceButton_->setGeometry(0, s(20), infoWidth, s(14));

    controlsContainer_->setGeometry(controlsX, 0, s(240), s(kHeight));
    listButton_->setGeometry(0, 0, 0, 0);
    heartButton_->setGeometry(0, 0, s(kButtonWidth), s(kHeight));
    prevButton_->setGeometry(s(45), 0, s(kButtonWidth), s(kHeight));
    playPauseButton_->setGeometry(s(90), 0, s(kButtonWidth), s(kHeight));
    nextButton_->setGeometry(s(135), 0, s(kButtonWidth), s(kHeight));
    const QSize mediaIconSize(s(16), s(16));
    const QColor transportIconColor = themePalette().foreground;
    prevButton_->setIconSize(mediaIconSize);
    playPauseButton_->setIconSize(mediaIconSize);
    nextButton_->setIconSize(mediaIconSize);
    prevButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward), mediaIconSize, transportIconColor));
    nextButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipForward), mediaIconSize, transportIconColor));

    auto* marqueeWidget = static_cast<TrackMarquee*>(marquee_);
    marqueeWidget->setUiScale(uiScale_);
    marqueeWidget->setTrackText(trackName_.isEmpty() ? "Unknown Track" : (artistName_.isEmpty() ? trackName_ : QString("%1  •  %2").arg(trackName_, artistName_)));

    const auto geometry = currentScreen ? currentScreen->geometry() : QRect(0, 0, s(kWidth), s(kHeight));
    const int x = geometry.left();
    const int y = geometry.bottom() - height() + 1;
    setGeometry(x, y, width(), height());
}

void MiniPlayer::applyStyles() {
    const auto p = themePalette();
    const QString textColorRaw = config::settingString("mini_player_text_color", p.foreground.name()).trimmed();
    const QString bgColorRaw = config::settingString("mini_player_bg_color", p.cardBackground.name()).trimmed();
    const QString controlColorRaw = config::settingString("mini_player_control_color", p.windowBackground.name()).trimmed();
    const bool transparentMini = config::settingBool("mini_player_transparent", false);

    const QColor parsedTextColor(textColorRaw);
    const QColor parsedBgColor(bgColorRaw);
    const QColor parsedControlColor(controlColorRaw);
    const QColor textColor = parsedTextColor.isValid() ? parsedTextColor : p.foreground;
    const QColor bgColor = parsedBgColor.isValid() ? parsedBgColor : p.cardBackground;
    const QColor controlColor = parsedControlColor.isValid() ? parsedControlColor : p.windowBackground;

    const QColor frameColor = transparentMini ? QColor(0, 0, 0, 0) : bgColor;
    QColor hoverColor = transparentMini ? p.hoverBackground : controlColor.lighter(125);
    if (transparentMini) {
        hoverColor.setAlpha(96);
    }
    const QColor controlBase = transparentMini ? QColor(0, 0, 0, 0) : controlColor;
    QColor sourceBackground = transparentMini ? controlColor : controlColor;
    if (transparentMini) {
        sourceBackground.setAlpha(140);
    }

    if (frame_) {
        frame_->setStyleSheet(QString("QFrame#miniFrame { border: none; background-color: %1; }")
            .arg(frameColor.name(QColor::HexArgb)));
    }

    setStyleSheet(QString(
        "QWidget { background-color: transparent; color: %1; font-family: '%2'; }"
        "QPushButton { color: %1; border: none; }"
        "QPushButton:hover { background-color: %3; }"
        "QPushButton#sourceBtn { color: %1; font-size: 7pt; text-align: left; padding-left: 0px; }")
        .arg(textColor.name(), config::FONT_FAMILY, hoverColor.name(QColor::HexArgb)));

    const QString controlStyle = QString("font-size: %1pt; padding: 0px; background-color: %2; border: none;")
        .arg(std::max(10, static_cast<int>(15 * uiScale_)))
        .arg(controlBase.name(QColor::HexArgb));
    if (listButton_) {
        listButton_->setStyleSheet(controlStyle);
    }
    if (prevButton_) {
        prevButton_->setStyleSheet(controlStyle);
    }
    if (nextButton_) {
        nextButton_->setStyleSheet(controlStyle);
    }
    if (playPauseButton_) {
        playPauseButton_->setStyleSheet(QString("font-size: %1pt; padding: 0px; background-color: %2; border: none;")
            .arg(std::max(10, static_cast<int>(15 * uiScale_)))
            .arg((transparentMini ? QColor(0, 0, 0, 0) : controlColor.lighter(110)).name(QColor::HexArgb)));
    }
    if (sourceButton_) {
        sourceButton_->setStyleSheet(QString(
            "font-size: %1pt; text-align: left; padding: 0px 6px; background-color: %2; border: none;")
            .arg(std::max(6, static_cast<int>(7 * uiScale_)))
            .arg(sourceBackground.name(QColor::HexArgb)));
    }

    updatePlayPauseIcon();
    updateHeartVisual();
}

void MiniPlayer::setTrackText(const QString& track, const QString& artist) {
    trackName_ = track.trimmed();
    artistName_ = artist.trimmed();

    QString text = trackName_.isEmpty() ? QString("Unknown Track") : trackName_;
    if (!artistName_.isEmpty()) {
        text += QString("  •  ") + artistName_;
    }

    auto* marqueeWidget = static_cast<TrackMarquee*>(marquee_);
    marqueeWidget->setTrackText(text);
}

void MiniPlayer::setSourceText(const QString& sourceName) {
    sourceName_ = sourceName.trimmed();
    const QString label = sourceName_.isEmpty() ? QString("No source") : sourceName_;
    sourceButton_->setText(QString::fromUtf8("▶ %1").arg(label));
}

void MiniPlayer::setPlaying(bool playing) {
    isPlaying_ = playing;
    updatePlayPauseIcon();
}

void MiniPlayer::setAlbumArtUrl(const QString& url) {
    const QString normalized = url.trimmed();
    if (normalized.isEmpty()) {
        currentAlbumArtUrl_.clear();
        showPlaceholderArt();
        return;
    }

    if (normalized == currentAlbumArtUrl_) {
        return;
    }

    currentAlbumArtUrl_ = normalized;
    loadAlbumArtFromUrl(currentAlbumArtUrl_);
}

void MiniPlayer::setLikedState(std::optional<bool> liked, bool spotifySource) {
    likedState_ = liked;
    spotifySource_ = spotifySource;
    updateHeartVisual();
}

void MiniPlayer::showDesktopMini() {
    desktopMiniEnabled_ = true;
    applyGeometry();

    const double opacity = std::clamp(config::settingDouble("mini_player_opacity", 1.0), 0.3, 1.0);
    setWindowOpacity(opacity);

    syncDesktopMiniVisibility();
    if (topmostTimer_) {
        topmostTimer_->start();
    }
    applyStyles();
}

void MiniPlayer::hideDesktopMini() {
    desktopMiniEnabled_ = false;
    autoHiddenByTaskbar_ = false;
    if (topmostTimer_) {
        topmostTimer_->stop();
    }
    hide();
}

void MiniPlayer::updatePlayPauseIcon() {
    if (!playPauseButton_) {
        return;
    }
    playPauseButton_->setText(QString());
    const QSize mediaIconSize = playPauseButton_->iconSize().isValid() ? playPauseButton_->iconSize() : QSize(s(16), s(16));
    const QColor iconColor = isPlaying_ ? QColor(config::HIGHLIGHT_COLOR) : themePalette().foreground;
    playPauseButton_->setIcon(tintedIcon(style()->standardIcon(isPlaying_ ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay), mediaIconSize, iconColor));
}

void MiniPlayer::updateHeartVisual() {
    if (!heartButton_) {
        return;
    }

    const auto p = themePalette();
    const QString controlColorRaw = config::settingString("mini_player_control_color", p.windowBackground.name()).trimmed();
    const bool transparentMini = config::settingBool("mini_player_transparent", false);
    const QColor parsedControlColor(controlColorRaw);
    const QColor controlColor = parsedControlColor.isValid() ? parsedControlColor : p.windowBackground;
    const QString heartBackground = (transparentMini ? QColor(0, 0, 0, 0) : controlColor).name(QColor::HexArgb);

    if (!spotifySource_) {
        heartButton_->setText(QString::fromUtf8("♡"));
        heartButton_->setStyleSheet(QString("color: %1; font-size: 15pt; background-color: %2; border: none;")
            .arg(config::INACTIVE_TEXT_COLOR, heartBackground));
        return;
    }

    const bool liked = likedState_.has_value() && likedState_.value();
    heartButton_->setText(liked ? QString::fromUtf8("♥") : QString::fromUtf8("♡"));
    heartButton_->setStyleSheet(QString("color: %1; font-size: 15pt; background-color: %2; border: none;")
        .arg(liked ? config::HIGHLIGHT_COLOR : config::INACTIVE_TEXT_COLOR, heartBackground));
}

void MiniPlayer::setRepeatState(bool enabled, bool supported) {
    repeatSupported_ = supported;
    repeatEnabled_ = enabled;

    if (!nextButton_) return;
    updateRepeatVisual();
}

void MiniPlayer::updateRepeatVisual() {
    if (!nextButton_) return;

    const auto p = themePalette();
    const QString controlColorRaw = config::settingString("mini_player_control_color", p.windowBackground.name()).trimmed();
    const bool transparentMini = config::settingBool("mini_player_transparent", false);
    const QColor parsedControlColor(controlColorRaw);
    const QColor controlColor = parsedControlColor.isValid() ? parsedControlColor : p.windowBackground;

    const QString bg = (transparentMini ? QColor(0,0,0,0).name(QColor::HexArgb) : controlColor.name(QColor::HexArgb));

    if (!repeatSupported_) {
        nextButton_->setStyleSheet(QString("font-size: %1pt; padding: 0px; background-color: %2; border: none;")
            .arg(std::max(10, static_cast<int>(15 * uiScale_)))
            .arg(bg));
        return;
    }

    if (repeatEnabled_) {
        nextButton_->setStyleSheet(QString("font-size: %1pt; padding: 0px; background-color: %2; border: 2px solid %3; border-radius: 6px;")
            .arg(std::max(10, static_cast<int>(15 * uiScale_)))
            .arg(bg)
            .arg(config::HIGHLIGHT_COLOR));
    } else {
        nextButton_->setStyleSheet(QString("font-size: %1pt; padding: 0px; background-color: %2; border: none;")
            .arg(std::max(10, static_cast<int>(15 * uiScale_)))
            .arg(bg));
    }
}

void MiniPlayer::showPlaceholderArt() {
    if (!artButton_) {
        return;
    }

    artButton_->setIcon(QIcon());
    artButton_->setText(QString::fromUtf8("♪"));
    artButton_->setStyleSheet(QString("font-size: %1pt; color: %2; background-color: transparent; border: none;")
                                  .arg(std::max(12, static_cast<int>(20 * uiScale_)))
                                  .arg(themePalette().mutedForeground.name()));
}

void MiniPlayer::loadAlbumArtFromUrl(const QString& url) {
    if (!artNetwork_ || url.isEmpty()) {
        showPlaceholderArt();
        return;
    }

    QNetworkReply* reply = artNetwork_->get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        if (url != currentAlbumArtUrl_) {
            return;
        }

        QPixmap pix;
        if (!pix.loadFromData(data)) {
            showPlaceholderArt();
            return;
        }

        const QPixmap scaled = pix.scaled(s(kArtSize), s(kArtSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        artButton_->setText(QString());
        artButton_->setIcon(QIcon(scaled));
        artButton_->setIconSize(QSize(s(kArtSize), s(kArtSize)));
        artButton_->setStyleSheet("background-color: transparent; border: none;");
    });
}

void MiniPlayer::keepWindowTopmost() {
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd != nullptr) {
        RECT rc;
        if (GetWindowRect(hwnd, &rc)) {
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            SetWindowPos(hwnd, HWND_TOPMOST, rc.left, rc.top, w, h,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
            return;
        }
    }
#endif
    raise();
}

void MiniPlayer::beginPopupInteraction() {
    if (topmostTimer_) {
        topmostTimer_->stop();
    }
}

void MiniPlayer::endPopupInteraction() {
    syncDesktopMiniVisibility();
    if (topmostTimer_ && desktopMiniEnabled_) {
        topmostTimer_->start();
    }
}

void MiniPlayer::syncDesktopMiniVisibility() {
    if (!desktopMiniEnabled_) {
        if (isVisible()) {
            hide();
        }
        autoHiddenByTaskbar_ = false;
        return;
    }

    if (isFullscreenAppActive()) {
        if (isVisible()) {
            hide();
        }
        autoHiddenByTaskbar_ = true;
        return;
    }

    if (!isVisible() || autoHiddenByTaskbar_) {
        show();
        autoHiddenByTaskbar_ = false;
    }
    keepWindowTopmost();
}

bool MiniPlayer::isTaskbarVisible() const {
#ifdef Q_OS_WIN
    const HWND miniHwnd = reinterpret_cast<HWND>(winId());
    const HMONITOR miniMonitor = miniHwnd != nullptr
        ? MonitorFromWindow(miniHwnd, MONITOR_DEFAULTTONEAREST)
        : nullptr;

    const HWND primaryTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (primaryTaskbar == nullptr) {
        return true;
    }

    if (miniMonitor == nullptr) {
        return IsWindowVisible(primaryTaskbar) != FALSE;
    }

    const HMONITOR primaryTaskbarMonitor = MonitorFromWindow(primaryTaskbar, MONITOR_DEFAULTTONEAREST);
    if (primaryTaskbarMonitor == miniMonitor) {
        return IsWindowVisible(primaryTaskbar) != FALSE;
    }

    HWND secondaryTaskbar = nullptr;
    while ((secondaryTaskbar = FindWindowExW(nullptr, secondaryTaskbar, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        const HMONITOR secondaryMonitor = MonitorFromWindow(secondaryTaskbar, MONITOR_DEFAULTTONEAREST);
        if (secondaryMonitor == miniMonitor) {
            return IsWindowVisible(secondaryTaskbar) != FALSE;
        }
    }

    return true;
#else
    return true;
#endif
}

bool MiniPlayer::isFullscreenAppActive() const {
#ifdef Q_OS_WIN
    QString reason = "unknown";
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        reason = "no_foreground_window";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1").arg(reason));
        return false;
    }

    const HWND miniHwnd = reinterpret_cast<HWND>(winId());
    if (miniHwnd != nullptr && foreground == miniHwnd) {
        reason = "foreground_is_miniplayer";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1").arg(reason));
        return false;
    }

    if (!IsWindowVisible(foreground) || IsIconic(foreground)) {
        reason = "foreground_not_visible_or_iconic";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1").arg(reason));
        return false;
    }

    RECT miniRect{};
    if (miniHwnd != nullptr) {
        if (!GetWindowRect(miniHwnd, &miniRect)) {
            reason = "mini_rect_unavailable";
            appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1").arg(reason));
            return false;
        }
    }

    wchar_t className[128] = {0};
    GetClassNameW(foreground, className, 128);
    const std::wstring cls(className);
    if (cls == L"Shell_TrayWnd" || cls == L"Progman" || cls == L"WorkerW") {
        reason = "shell_window_class";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1 class=%2")
            .arg(reason, QString::fromStdWString(cls)));
        return false;
    }

    RECT fgRect{};
    if (!GetWindowRect(foreground, &fgRect)) {
        reason = "foreground_rect_unavailable";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1")
            .arg(reason));
        return false;
    }

    HMONITOR miniMonitor = nullptr;
    if (miniHwnd != nullptr) {
        miniMonitor = MonitorFromWindow(miniHwnd, MONITOR_DEFAULTTONEAREST);
    }
    const HMONITOR foregroundMonitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);

    if (miniHwnd != nullptr && miniMonitor != nullptr && foregroundMonitor != nullptr && miniMonitor != foregroundMonitor) {
        reason = "different_monitor";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1 class=%2 mini_monitor=%3 foreground_monitor=%4")
            .arg(reason, QString::fromStdWString(cls), monitorIdText(miniMonitor), monitorIdText(foregroundMonitor)));
        return false;
    }

    if (miniHwnd != nullptr) {
        RECT overlapRect{};
        if (!IntersectRect(&overlapRect, &miniRect, &fgRect)) {
            reason = "no_overlap_with_miniplayer";
            appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1 class=%2 mini_monitor=%3 foreground_monitor=%4")
                .arg(reason, QString::fromStdWString(cls), monitorIdText(miniMonitor), monitorIdText(foregroundMonitor)));
            return false;
        }
    }

    if (foregroundMonitor == nullptr) {
        reason = "foreground_monitor_unavailable";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1 class=%2")
            .arg(reason, QString::fromStdWString(cls)));
        return false;
    }

    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfoW(foregroundMonitor, &mi)) {
        reason = "foreground_monitor_info_unavailable";
        appendMiniDebugLog(QString("fullscreen_check result=0 reason=%1 class=%2 foreground_monitor=%3")
            .arg(reason, QString::fromStdWString(cls), monitorIdText(foregroundMonitor)));
        return false;
    }

    constexpr int kTolerancePx = 2;
    const bool fillsMonitor =
        fgRect.left <= (mi.rcMonitor.left + kTolerancePx) &&
        fgRect.top <= (mi.rcMonitor.top + kTolerancePx) &&
        fgRect.right >= (mi.rcMonitor.right - kTolerancePx) &&
        fgRect.bottom >= (mi.rcMonitor.bottom - kTolerancePx);

    reason = fillsMonitor ? "fullscreen_same_monitor" : "not_fullscreen";
    appendMiniDebugLog(QString(
        "fullscreen_check result=%1 reason=%2 class=%3 mini_monitor=%4 foreground_monitor=%5 fg_rect=[%6,%7,%8,%9] monitor_rect=[%10,%11,%12,%13]")
        .arg(fillsMonitor ? "1" : "0")
        .arg(reason)
        .arg(QString::fromStdWString(cls))
        .arg(monitorIdText(miniMonitor))
        .arg(monitorIdText(foregroundMonitor))
        .arg(fgRect.left)
        .arg(fgRect.top)
        .arg(fgRect.right)
        .arg(fgRect.bottom)
        .arg(mi.rcMonitor.left)
        .arg(mi.rcMonitor.top)
        .arg(mi.rcMonitor.right)
        .arg(mi.rcMonitor.bottom));

    return fillsMonitor;
#else
    return false;
#endif
}
