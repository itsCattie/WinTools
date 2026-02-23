#pragma once

// MediaBar: mini player manages UI behavior and presentation.

#include <QWidget>

#include <optional>

class QFrame;
class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class MiniPlayer final : public QWidget {
public:
    explicit MiniPlayer(QWidget* parent = nullptr);

    QPushButton* prevButton() const { return prevButton_; }
    QPushButton* playPauseButton() const { return playPauseButton_; }
    QPushButton* nextButton() const { return nextButton_; }
    QPushButton* heartButton() const { return heartButton_; }
    QPushButton* listButton() const { return listButton_; }
    QPushButton* sourceButton() const { return sourceButton_; }
    QPushButton* artButton() const { return artButton_; }

    void setTrackText(const QString& track, const QString& artist);
    void setSourceText(const QString& sourceName);
    void setPlaying(bool playing);
    void setAlbumArtUrl(const QString& url);
    void setLikedState(std::optional<bool> liked, bool spotifySource);

    void showDesktopMini();
    void hideDesktopMini();
    void beginPopupInteraction();
    void endPopupInteraction();

private:
    void setupUi();
    void applyGeometry();
    void applyStyles();
    int s(int value) const;
    void updatePlayPauseIcon();
    void updateHeartVisual();
    void showPlaceholderArt();
    void loadAlbumArtFromUrl(const QString& url);
    void keepWindowTopmost();
    void syncDesktopMiniVisibility();
    bool isTaskbarVisible() const;
    bool isFullscreenAppActive() const;

    static constexpr int kWidth = 500;
    static constexpr int kHeight = 40;
    static constexpr int kArtSize = 40;
    static constexpr int kButtonWidth = 40;
    static constexpr int kInfoWidth = 200;

    QFrame* frame_ = nullptr;
    QWidget* infoContainer_ = nullptr;
    QWidget* controlsContainer_ = nullptr;
    QWidget* marquee_ = nullptr;

    QPushButton* artButton_ = nullptr;
    QPushButton* sourceButton_ = nullptr;
    QPushButton* listButton_ = nullptr;
    QPushButton* heartButton_ = nullptr;
    QPushButton* prevButton_ = nullptr;
    QPushButton* playPauseButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;

    QNetworkAccessManager* artNetwork_ = nullptr;
    QTimer* topmostTimer_ = nullptr;

    QString trackName_;
    QString artistName_;
    QString sourceName_;
    QString currentAlbumArtUrl_;
    bool isPlaying_ = false;
    bool spotifySource_ = false;
    bool desktopMiniEnabled_ = false;
    bool autoHiddenByTaskbar_ = false;
    std::optional<bool> likedState_;
    double uiScale_ = 1.0;
};
