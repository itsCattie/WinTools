#include "gui.hpp"

#include "internal/gui_detail_helpers.hpp"

#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QStyle>
#include <QTime>

#include <algorithm>
#include <cmath>

#include "../core/debug_logger.hpp"

using namespace wintools::mediabar::gui_detail;

void LyricsWindow::updateTrackInfo(const PlaybackInfo& playback, const QString& sourceName) {
    if (rebuildingUi_ || !trackLabel_ || !artistLabel_ || !sourceLabel_ || !albumArtLabel_) {
        return;
    }

    trackLabel_->setText(playback.trackName.isEmpty() ? "Unknown" : playback.trackName);
    artistLabel_->setText(playback.artistName);
    sourceLabel_->setText(sourceName);

    const QString artUrl = playback.albumArt.trimmed();
    if (artUrl.isEmpty()) {
        currentAlbumArtUrl_.clear();
        cachedAlbumArt_ = QPixmap();
        if (albumArtLabel_) {
            albumArtLabel_->setPixmap(QPixmap());
            albumArtLabel_->setText(QString::fromUtf8("🎵"));
        }
        return;
    }

    if (artUrl == currentAlbumArtUrl_) {

        if (albumArtLabel_ && albumArtLabel_->pixmap().isNull() && !cachedAlbumArt_.isNull()) {
            albumArtLabel_->setText(QString());
            albumArtLabel_->setPixmap(cachedAlbumArt_.scaled(albumArtLabel_->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        }
        return;
    }

    currentAlbumArtUrl_ = artUrl;
    loadAlbumArtFromUrl(artUrl);
}

void LyricsWindow::loadAlbumArtFromUrl(const QString& url) {
    if (url.isEmpty()) {
        return;
    }
    if (!artNetwork_) {
        artNetwork_ = new QNetworkAccessManager(this);
    }

    QNetworkReply* reply = artNetwork_->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QPixmap art;
        if (!art.loadFromData(data) || !albumArtLabel_) {
            if (albumArtLabel_) {
                albumArtLabel_->setPixmap(QPixmap());
                albumArtLabel_->setText(QString::fromUtf8("🎵"));
            }
            return;
        }

        cachedAlbumArt_ = art;

        albumArtLabel_->setText(QString());
        albumArtLabel_->setPixmap(art.scaled(albumArtLabel_->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    });
}

void LyricsWindow::updatePlayPauseButton(bool isPlaying) {
    isPlaying_ = isPlaying;
    if (rebuildingUi_) {
        return;
    }
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

    const QColor fg = appForeground();
    const QColor accent = appAccentColor();
    prevButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward), mediaIconSize, fg));
    nextButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_MediaSkipForward), mediaIconSize, fg));
    if (repeatButton_) {
        repeatButton_->setIcon(tintedIcon(style()->standardIcon(QStyle::SP_BrowserReload), QSize(14,14), fg));
        repeatButton_->setToolTip("Repeat");
    }

    const QColor playColor = isPlaying_ ? readableTextOn(accent) : fg;
    playPauseButton_->setIcon(tintedIcon(style()->standardIcon(isPlaying_ ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay), mediaIconSize, playColor));
}

void LyricsWindow::updateProgress(qint64 progressMs, qint64 durationMs) {
    if (rebuildingUi_ || !progressBar_ || !currentTimeLabel_ || !durationLabel_) {
        return;
    }

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
    if (rebuildingUi_) {
        return;
    }

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
    if (rebuildingUi_ || !lyricsList_) {
        lyrics_ = lyrics;
        return;
    }

    const int availableWidth = std::max(120, lyricsList_->viewport()->width() - 12);

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
        if (sameLyrics && lastLyricWrapWidth_ == availableWidth) {
            return;
        }
    }

    lyrics_ = lyrics;
    lyricsList_->clear();
    lastLyricWrapWidth_ = availableWidth;

    QFont baseFont;
    baseFont.setPointSize(14);
    baseFont.setBold(false);
    QFont highlightFont = baseFont;
    highlightFont.setPointSize(18);
    highlightFont.setBold(true);
    const QColor muted = appMutedForeground();
    for (const auto& line : lyrics_) {
        const QString wrappedText = line.text.simplified();
        auto* item = new QListWidgetItem(wrappedText);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setForeground(muted);
        item->setFont(baseFont);

        const int itemHeight = lyricItemHeightForText(wrappedText, highlightFont, availableWidth);
        item->setSizeHint(QSize(availableWidth, itemHeight));
        lyricsList_->addItem(item);
    }

    currentLyricIndex_ = -1;
}

void LyricsWindow::highlightCurrentLyric(int index) {
    if (rebuildingUi_ || !lyricsList_) {
        return;
    }

    if (lyricsList_->count() <= 0) {
        return;
    }

    const QColor accent = appAccentColor();
    const QColor fg = appForeground();
    const QColor muted = appMutedForeground();
    const int availableWidth = std::max(120, lyricsList_->viewport()->width() - 12);
    const auto applyStyle = [availableWidth](QListWidgetItem* item, int fontSize, bool bold, const QColor& color) {
        if (!item) {
            return;
        }
        item->setForeground(color);
        QFont f = item->font();
        f.setBold(bold);
        f.setPointSize(fontSize);
        item->setFont(f);

        const int itemHeight = lyricItemHeightForText(item->text(), f, availableWidth);
        item->setSizeHint(QSize(availableWidth, itemHeight));
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
            applyStyle(lyricsList_->item(i), 14, false, muted);
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
            applyStyle(lyricsList_->item(i), 18, true, accent);
        } else if (std::abs(i - currentLyricIndex_) == 1) {
            applyStyle(lyricsList_->item(i), 15, false, fg);
        } else {
            applyStyle(lyricsList_->item(i), 14, false, muted);
        }
    }

    if (auto* item = lyricsList_->item(currentLyricIndex_)) {
        lyricsList_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
}

void LyricsWindow::setShuffleState(bool enabled, bool supported) {
    if (rebuildingUi_) {
        return;
    }

    shuffleSupported_ = supported;
    shuffleEnabled_ = supported ? enabled : false;

    if (shuffleButton_) {
        shuffleButton_->setEnabled(shuffleSupported_);
        updateShuffleButtonStyle();
    }
}

void LyricsWindow::setRepeatState(bool enabled, bool supported) {
    if (rebuildingUi_) {
        return;
    }

    repeatSupported_ = supported;
    repeatEnabled_ = supported ? enabled : false;

    if (repeatButton_) {
        repeatButton_->setEnabled(repeatSupported_);
        updateRepeatButtonStyle();
    }
}

void LyricsWindow::updateRepeatButtonStyle() {
    if (!repeatButton_) return;
    const QColor fg = appForeground();
    const QColor muted = appMutedForeground();
    const QColor accent = appAccentColor();

    const QSize iconSz = repeatButton_->iconSize().isValid() ? repeatButton_->iconSize() : QSize(14, 14);

    QIcon repeatBase = style()->standardIcon(QStyle::SP_BrowserReload);

    repeatButton_->setText(QString());

    if (!repeatSupported_) {
        repeatButton_->setEnabled(false);
        repeatButton_->setIcon(tintedIcon(repeatBase, iconSz, muted));
        repeatButton_->setStyleSheet(QString("background-color: transparent; color: %1; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 600;")
            .arg(muted.name()));
        return;
    }

    repeatButton_->setEnabled(true);

    if (repeatEnabled_) {
        const QColor bg = accent;
        const QColor activeText = readableTextOn(bg);
        repeatButton_->setIcon(tintedIcon(repeatBase, iconSz, activeText));
        repeatButton_->setStyleSheet(QString("background-color: %1; color: %2; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 700;")
            .arg(bg.name(), activeText.name()));
        return;
    }

    repeatButton_->setIcon(tintedIcon(repeatBase, iconSz, fg));
    repeatButton_->setStyleSheet(QString("background-color: transparent; color: %1; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 600;")
        .arg(fg.name()));
}

void LyricsWindow::updateShuffleButtonStyle() {
    if (!shuffleButton_) {
        return;
    }
    const QColor fg = appForeground();
    const QColor muted = appMutedForeground();
    const QColor accent = appAccentColor();

    const QSize iconSz = shuffleButton_->iconSize().isValid() ? shuffleButton_->iconSize() : QSize(16, 16);

    QIcon shuffleBase;
    const QString shufflePath = ":/icons/mediabar/shuffle.svg";
    shuffleBase = QIcon(shufflePath);
    if (shuffleBase.isNull()) {
        debuglog::warn("GUI", QString("updateShuffleButtonStyle: resource %1 not found, using drawn fallback").arg(shufflePath));
        shuffleBase = drawnShuffleIcon(iconSz, fg);
    }

    shuffleButton_->setText(QString());

    if (!shuffleSupported_) {
        shuffleButton_->setEnabled(false);
        shuffleButton_->setIcon(tintedIcon(shuffleBase, iconSz, muted));
        shuffleButton_->setStyleSheet(QString("background-color: transparent; color: %1; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 600;")
            .arg(muted.name()));
        return;
    }

    shuffleButton_->setEnabled(true);

    if (shuffleEnabled_) {

        const QColor bg = accent;
        const QColor activeText = readableTextOn(bg);

        shuffleButton_->setIcon(tintedIcon(shuffleBase, iconSz, activeText));

        shuffleButton_->setStyleSheet(QString(
            "background-color: %1; color: %2; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 700;")
            .arg(bg.name(), activeText.name()));
        return;
    }

    shuffleButton_->setIcon(tintedIcon(shuffleBase, iconSz, fg));
    shuffleButton_->setStyleSheet(QString("background-color: transparent; color: %1; border: none; border-radius: 14px; padding: 6px 10px; font-weight: 600;")
        .arg(fg.name()));
}

void LyricsWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    if (rebuildingUi_ || !lyricsList_ || lyrics_.isEmpty()) {
        return;
    }

    const int currentIndex = currentLyricIndex_;
    displayLyrics(lyrics_);
    highlightCurrentLyric(currentIndex);
}

void LyricsWindow::setVolumeControlState(int volumePercent, bool supported) {
    if (rebuildingUi_ || !volumeSlider_ || !volumeValueLabel_) {
        return;
    }

    if (!volumeSlider_ || !volumeValueLabel_) {
        return;
    }

    applyingVolumeUiState_ = true;
    volumeSlider_->setEnabled(supported);
    if (!supported || volumePercent < 0) {
        volumeValueLabel_->setText("--%");
        if (!supported) {
            collapsedVolumePanelVisible_ = false;
        }
    } else {
        const int clamped = qBound(0, volumePercent, 100);
        volumeSlider_->setValue(clamped);
        volumeValueLabel_->setText(QString("%1%").arg(clamped));
    }
    if (volumePanel_) {
        volumePanel_->setEnabled(supported);
    }
    applyingVolumeUiState_ = false;
    updateSidebarPresentation();
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
