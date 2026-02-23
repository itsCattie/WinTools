#pragma once

// MediaBar: gui manages UI behavior and presentation.

#include "types.hpp"

#include <QWidget>
#include <optional>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QComboBox;
class QProgressBar;
class QWidget;
class QNetworkAccessManager;
class QNetworkReply;

enum class MediaBarIpcCmd : int {
    EnterMini  = 1,
    ExitMini   = 2,
    ToggleMini = 3,
};

class LyricsWindow : public QWidget {
    Q_OBJECT
public:
    explicit LyricsWindow(QWidget* parent = nullptr);

    static constexpr const wchar_t* IpcMsgName = L"WinTools.MediaBar.Command";

signals:
    void ipcCommand(int cmd);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

public:
    void updateTrackInfo(const PlaybackInfo& playback, const QString& sourceName);
    void updatePlayPauseButton(bool isPlaying);
    void updateProgress(qint64 progressMs, qint64 durationMs);
    void setFavoriteState(std::optional<bool> liked, bool spotifySource);
    void setShuffleState(bool enabled, bool supported);
    void displayLyrics(const LyricsList& lyrics);
    void highlightCurrentLyric(int index);
    void showNoLyrics();
    void showWaiting();

    QPushButton* prevButton() const { return prevButton_; }
    QPushButton* playPauseButton() const { return playPauseButton_; }
    QPushButton* nextButton() const { return nextButton_; }
    QPushButton* shuffleButton() const { return shuffleButton_; }
    QPushButton* favoriteButton() const { return favoriteButton_; }
    QPushButton* miniModeButton() const { return miniModeButton_; }
    QComboBox* sourceModeCombo() const { return sourceModeCombo_; }
    QPushButton* menuButton() const { return menuButton_; }
    QPushButton* searchButton() const { return searchButton_; }
    QPushButton* spotifyLibraryButton() const { return spotifyLibraryButton_; }
    QPushButton* spotifyDevicesButton() const { return spotifyDevicesButton_; }
    QPushButton* localBrowseButton() const { return localBrowseButton_; }
    QPushButton* queueButton() const { return queueButton_; }
    QPushButton* appearanceButton() const { return appearanceButton_; }
    QPushButton* libraryPathButton() const { return libraryPathButton_; }
    QPushButton* debugButton() const { return debugButton_; }
    QPushButton* sourceAutoButton() const { return sourceAutoButton_; }
    QPushButton* sourceSpotifyButton() const { return sourceSpotifyButton_; }
    QPushButton* sourceSonosButton() const { return sourceSonosButton_; }

    void setSourceMode(const QString& mode);
    QString sourceMode() const;
    void toggleSidebar();
    void setSidebarExpanded(bool expanded);
    bool isSidebarExpanded() const { return sidebarExpanded_; }

private:
    void updateTransportIcons();
    QPushButton* createSideMenuButton(const QString& icon, const QString& text);
    void syncSourceButtons();
    void updateSidebarPresentation();
    void updateShuffleButtonStyle();
    void setupUi();

    QWidget* shell_ = nullptr;
    QWidget* sideMenu_ = nullptr;
    QWidget* mainArea_ = nullptr;
    QLabel* trackLabel_ = nullptr;
    QLabel* artistLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QPushButton* menuButton_ = nullptr;
    QComboBox* sourceModeCombo_ = nullptr;
    QListWidget* lyricsList_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* prevButton_ = nullptr;
    QPushButton* playPauseButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QPushButton* shuffleButton_ = nullptr;
    QPushButton* favoriteButton_ = nullptr;
    QPushButton* miniModeButton_ = nullptr;
    QPushButton* searchButton_ = nullptr;
    QPushButton* spotifyLibraryButton_ = nullptr;
    QPushButton* spotifyDevicesButton_ = nullptr;
    QPushButton* localBrowseButton_ = nullptr;
    QPushButton* queueButton_ = nullptr;
    QPushButton* appearanceButton_ = nullptr;
    QPushButton* libraryPathButton_ = nullptr;
    QPushButton* debugButton_ = nullptr;
    QPushButton* sourceAutoButton_ = nullptr;
    QPushButton* sourceSpotifyButton_ = nullptr;
    QPushButton* sourceSonosButton_ = nullptr;
    QLabel* albumArtLabel_ = nullptr;
    QLabel* currentTimeLabel_ = nullptr;
    QLabel* durationLabel_ = nullptr;
    QNetworkAccessManager* artNetwork_ = nullptr;
    QString currentAlbumArtUrl_;
    bool isPlaying_ = false;
    bool shuffleEnabled_ = false;
    bool shuffleSupported_ = false;
    bool sidebarExpanded_ = true;

    LyricsList lyrics_;
    int currentLyricIndex_ = -1;
};
