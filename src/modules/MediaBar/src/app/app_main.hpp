#pragma once

// MediaBar: app main manages feature behavior.

#include "gui.hpp"
#include "lyrics_fetcher.hpp"
#include "mini_player.hpp"
#include "types.hpp"
#include "unified_client.hpp"

#include <QMutex>
#include <QHash>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QMenu>
#include <optional>

class LyricsApp {
public:
    LyricsApp();
    int run(int argc, char** argv);

    void initInProcess();

    void showMainWindow();
    void showMiniPlayer();

private:
    struct SpotifyLibraryCacheEntry {
        QVector<SpotifyCatalogItem> items;
        qint64 fetchedAtMs = 0;
    };

    void updateUi();
    void refreshPlayback();
    void fetchLyricsAsync(const PlaybackInfo& playback);
    void enterMiniMode();
    void exitMiniMode();
    void toggleMiniMode();
    void updateMiniPlayer(const PlaybackInfo& playback, const QString& sourceName);
    void setupTray();
    void wireMainWindowActions();
    void showSearchDialog();
    void showSpotifyLibraryDialog();
    void showSpotifyDevicesDialog();
    void showLocalLibraryDialog();
    void showQueueDialog();
    void showAppearanceDialog();
    void showLibraryPathDialog();
    void showDebugDialog();
    void showMiniSourceMenu();
    void toggleCurrentTrackLike();
    void showMiniPinnedMenu();
    void showMiniTopItemsMenu();
    bool playSpotifyCatalogItemNow(const SpotifyCatalogItem& item);
    bool queueSpotifyCatalogItem(const SpotifyCatalogItem& item, const QString& target);
    QString normalizeSpotifyUri(const QString& input) const;
    void refreshLikeState(const PlaybackInfo& playback);

    UnifiedMusicClient client_;
    LyricsFetcher lyricsFetcher_;
    std::optional<PlaybackInfo> cachedPlayback_;
    std::optional<LyricsList> currentLyrics_;
    QString currentTrackId_;

    QMutex playbackMutex_;
    QMutex lyricsMutex_;

    QTimer* pollingTimer_ = nullptr;
    QTimer* uiTimer_ = nullptr;
    LyricsWindow* mainWindow_ = nullptr;
    MiniPlayer* miniPlayerWindow_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    bool miniMode_ = false;
    std::optional<bool> currentTrackLiked_;
    std::optional<bool> currentShuffleEnabled_;
    QString currentShuffleSource_;
    QString currentLikedTrackId_;
    PlaybackInfo latestPlayback_;
    QString latestSourceName_;
    bool refreshInFlight_ = false;
    qint64 lastPlaybackSnapshotAtMs_ = 0;
    qint64 lastPlaybackHealthLogAtMs_ = 0;
    int stalePlaybackTickCount_ = 0;
    int consecutivePlaybackMisses_ = 0;
    static constexpr int kTransientPlaybackMissLimit_ = 3;
    static constexpr int kPlaybackPollIntervalMs_ = 1200;
    static constexpr int kUiRefreshIntervalMs_ = 200;
    static constexpr qint64 kStalePlaybackWarnMs_ = 10 * 1000;
    static constexpr qint64 kPlaybackHealthLogThrottleMs_ = 10 * 1000;
    QHash<int, SpotifyLibraryCacheEntry> spotifyLibraryCache_;
    static constexpr qint64 kSpotifyLibraryCacheTtlMs = 5 * 60 * 1000;
};
