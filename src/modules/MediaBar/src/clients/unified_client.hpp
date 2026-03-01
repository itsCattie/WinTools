#pragma once

#include "sonos_client.hpp"
#include "spotify_client.hpp"
#include "types.hpp"

#include <optional>

class UnifiedMusicClient {
public:
    enum class SourceMode {
        Spotify,
        Sonos
    };

    UnifiedMusicClient();

    std::optional<PlaybackInfo> getCurrentPlayback();
    void setSourceMode(SourceMode mode);
    void setSourceModeFromString(const QString& mode);
    QString sourceModeToString() const;
    QString getSourceName() const;

    bool nextTrack();
    bool previousTrack();
    bool playPause();
    bool seekToPosition(qint64 positionMs);
    std::optional<int> getVolumePercent();
    bool setVolumePercent(int percent);
    std::optional<bool> getShuffleState();
    std::optional<bool> toggleShuffle();
    std::optional<bool> setRepeatState(bool enabled);

    SpotifyClient& spotify() { return spotify_; }
    SonosClient& sonos() { return sonos_; }

#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    struct TestCallCounts {
        int spotifyGetPlayback = 0;
        int sonosGetPlayback = 0;
        int spotifyNextTrack = 0;
        int sonosNextTrack = 0;
        int spotifyPreviousTrack = 0;
        int sonosPreviousTrack = 0;
        int spotifyPlayPause = 0;
        int sonosPlayPause = 0;
        int spotifySeek = 0;
        int sonosSeek = 0;
        int spotifyGetShuffle = 0;
        int spotifySetShuffle = 0;
    };

    void testEnableHooks(bool enabled);
    void testSetSpotifyPlayback(const std::optional<PlaybackInfo>& playback);
    void testSetSonosPlayback(const std::optional<PlaybackInfo>& playback);
    void testSetTransportResults(bool spotifyNext, bool sonosNext,
                                 bool spotifyPrev, bool sonosPrev,
                                 bool spotifyPlayPause, bool sonosPlayPause,
                                 bool spotifySeek, bool sonosSeek);
    void testSetSpotifyShuffle(std::optional<bool> state, bool setShuffleSucceeds);
    TestCallCounts testCallCounts() const;
    void testResetCallCounts();
#endif

private:
    std::optional<PlaybackInfo> getSpotifyPlayback();
    std::optional<PlaybackInfo> getSonosPlayback();

    SpotifyClient spotify_;
    SonosClient sonos_;
    SourceMode sourceMode_ = SourceMode::Spotify;
    QString currentSource_;

#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    bool testHooksEnabled_ = false;
    std::optional<PlaybackInfo> testSpotifyPlayback_;
    std::optional<PlaybackInfo> testSonosPlayback_;

    bool testSpotifyNextResult_ = false;
    bool testSonosNextResult_ = false;
    bool testSpotifyPreviousResult_ = false;
    bool testSonosPreviousResult_ = false;
    bool testSpotifyPlayPauseResult_ = false;
    bool testSonosPlayPauseResult_ = false;
    bool testSpotifySeekResult_ = false;
    bool testSonosSeekResult_ = false;

    std::optional<bool> testSpotifyShuffleState_;
    bool testSpotifySetShuffleSucceeds_ = true;
    TestCallCounts testCallCounts_;
#endif
};
