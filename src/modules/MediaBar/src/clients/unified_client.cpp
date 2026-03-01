#include "unified_client.hpp"

#include "debug_logger.hpp"

UnifiedMusicClient::UnifiedMusicClient() = default;

#ifdef MEDIABAR_ENABLE_TEST_HOOKS
void UnifiedMusicClient::testEnableHooks(bool enabled) {
    testHooksEnabled_ = enabled;
}

void UnifiedMusicClient::testSetSpotifyPlayback(const std::optional<PlaybackInfo>& playback) {
    testSpotifyPlayback_ = playback;
}

void UnifiedMusicClient::testSetSonosPlayback(const std::optional<PlaybackInfo>& playback) {
    testSonosPlayback_ = playback;
}

void UnifiedMusicClient::testSetTransportResults(bool spotifyNext, bool sonosNext,
                                                 bool spotifyPrev, bool sonosPrev,
                                                 bool spotifyPlayPause, bool sonosPlayPause,
                                                 bool spotifySeek, bool sonosSeek) {
    testSpotifyNextResult_ = spotifyNext;
    testSonosNextResult_ = sonosNext;
    testSpotifyPreviousResult_ = spotifyPrev;
    testSonosPreviousResult_ = sonosPrev;
    testSpotifyPlayPauseResult_ = spotifyPlayPause;
    testSonosPlayPauseResult_ = sonosPlayPause;
    testSpotifySeekResult_ = spotifySeek;
    testSonosSeekResult_ = sonosSeek;
}

void UnifiedMusicClient::testSetSpotifyShuffle(std::optional<bool> state, bool setShuffleSucceeds) {
    testSpotifyShuffleState_ = state;
    testSpotifySetShuffleSucceeds_ = setShuffleSucceeds;
}

UnifiedMusicClient::TestCallCounts UnifiedMusicClient::testCallCounts() const {
    return testCallCounts_;
}

void UnifiedMusicClient::testResetCallCounts() {
    testCallCounts_ = TestCallCounts{};
}
#endif

void UnifiedMusicClient::setSourceMode(SourceMode mode) {
    sourceMode_ = mode;
    debuglog::info("UnifiedClient", QString("setSourceMode=%1")
        .arg(mode == SourceMode::Spotify ? "spotify" : "sonos"));
}

void UnifiedMusicClient::setSourceModeFromString(const QString& mode) {
    const QString normalized = mode.trimmed().toLower();
    if (normalized == "sonos") {
        setSourceMode(SourceMode::Sonos);
        return;
    }
    setSourceMode(SourceMode::Spotify);
}

QString UnifiedMusicClient::sourceModeToString() const {
    if (sourceMode_ == SourceMode::Sonos) {
        return "sonos";
    }
    return "spotify";
}

std::optional<PlaybackInfo> UnifiedMusicClient::getSpotifyPlayback() {
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        ++testCallCounts_.spotifyGetPlayback;
        if (testSpotifyPlayback_.has_value()) {
            currentSource_ = "spotify";
        }
        return testSpotifyPlayback_;
    }
#endif
    auto playback = spotify_.getCurrentPlayback();
    if (playback.has_value()) {
        currentSource_ = "spotify";
    }
    return playback;
}

std::optional<PlaybackInfo> UnifiedMusicClient::getSonosPlayback() {
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        ++testCallCounts_.sonosGetPlayback;
        if (testSonosPlayback_.has_value()) {
            currentSource_ = "sonos";
        }
        return testSonosPlayback_;
    }
#endif
    auto playback = sonos_.getCurrentPlayback();
    if (playback.has_value()) {
        currentSource_ = "sonos";

        const auto vol = sonos_.getVolume();
        if (vol.has_value()) {
            playback->volumePercent = vol.value();
        }
    }
    return playback;
}

std::optional<PlaybackInfo> UnifiedMusicClient::getCurrentPlayback() {
    debuglog::trace("UnifiedClient", QString("getCurrentPlayback mode=%1")
        .arg(sourceModeToString()));
    if (sourceMode_ == SourceMode::Sonos) {
        return getSonosPlayback();
    }

    return getSpotifyPlayback();
}

QString UnifiedMusicClient::getSourceName() const {
    if (currentSource_ == "spotify") {
        return "Spotify";
    }
    if (currentSource_ == "sonos") {
        return "Sonos";
    }
    return "Unknown";
}

bool UnifiedMusicClient::nextTrack() {
    debuglog::trace("UnifiedClient", QString("nextTrack currentSource=%1").arg(currentSource_));
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify") {
            ++testCallCounts_.spotifyNextTrack;
            return testSpotifyNextResult_;
        }
        if (currentSource_ == "sonos") {
            ++testCallCounts_.sonosNextTrack;
            return testSonosNextResult_;
        }
        ++testCallCounts_.spotifyNextTrack;
        if (testSpotifyNextResult_) {
            return true;
        }
        ++testCallCounts_.sonosNextTrack;
        return testSonosNextResult_;
    }
#endif
    if (currentSource_ == "spotify") {
        return spotify_.nextTrack();
    }
    if (currentSource_ == "sonos") {
        return sonos_.nextTrack();
    }
    return spotify_.nextTrack() || sonos_.nextTrack();
}

bool UnifiedMusicClient::previousTrack() {
    debuglog::trace("UnifiedClient", QString("previousTrack currentSource=%1").arg(currentSource_));
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify") {
            ++testCallCounts_.spotifyPreviousTrack;
            return testSpotifyPreviousResult_;
        }
        if (currentSource_ == "sonos") {
            ++testCallCounts_.sonosPreviousTrack;
            return testSonosPreviousResult_;
        }
        ++testCallCounts_.spotifyPreviousTrack;
        if (testSpotifyPreviousResult_) {
            return true;
        }
        ++testCallCounts_.sonosPreviousTrack;
        return testSonosPreviousResult_;
    }
#endif
    if (currentSource_ == "spotify") {
        return spotify_.previousTrack();
    }
    if (currentSource_ == "sonos") {
        return sonos_.previousTrack();
    }
    return spotify_.previousTrack() || sonos_.previousTrack();
}

bool UnifiedMusicClient::playPause() {
    debuglog::trace("UnifiedClient", QString("playPause currentSource=%1").arg(currentSource_));
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify") {
            ++testCallCounts_.spotifyPlayPause;
            return testSpotifyPlayPauseResult_;
        }
        if (currentSource_ == "sonos") {
            ++testCallCounts_.sonosPlayPause;
            return testSonosPlayPauseResult_;
        }
        ++testCallCounts_.spotifyPlayPause;
        if (testSpotifyPlayPauseResult_) {
            return true;
        }
        ++testCallCounts_.sonosPlayPause;
        return testSonosPlayPauseResult_;
    }
#endif
    if (currentSource_ == "spotify") {
        return spotify_.playPause();
    }
    if (currentSource_ == "sonos") {
        return sonos_.playPause();
    }
    return spotify_.playPause() || sonos_.playPause();
}

bool UnifiedMusicClient::seekToPosition(qint64 positionMs) {
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify") {
            ++testCallCounts_.spotifySeek;
            return testSpotifySeekResult_;
        }
        if (currentSource_ == "sonos") {
            ++testCallCounts_.sonosSeek;
            return testSonosSeekResult_;
        }
        ++testCallCounts_.spotifySeek;
        if (testSpotifySeekResult_) {
            return true;
        }
        ++testCallCounts_.sonosSeek;
        return testSonosSeekResult_;
    }
#endif
    if (currentSource_ == "spotify") {
        return spotify_.seekToPosition(positionMs);
    }
    if (currentSource_ == "sonos") {
        return sonos_.seekToPosition(positionMs);
    }
    return spotify_.seekToPosition(positionMs) || sonos_.seekToPosition(positionMs);
}

std::optional<int> UnifiedMusicClient::getVolumePercent() {
    if (currentSource_ == "spotify") {
        return spotify_.getVolumePercent();
    }
    if (currentSource_ == "sonos") {
        return sonos_.getVolume();
    }

    const auto spotifyVolume = spotify_.getVolumePercent();
    if (spotifyVolume.has_value()) {
        return spotifyVolume;
    }
    return sonos_.getVolume();
}

bool UnifiedMusicClient::setVolumePercent(int percent) {
    if (currentSource_ == "spotify") {
        return spotify_.setVolumePercent(percent);
    }
    if (currentSource_ == "sonos") {
        return sonos_.setVolume(percent);
    }
    return spotify_.setVolumePercent(percent);
}

std::optional<bool> UnifiedMusicClient::getShuffleState() {
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify") {
            ++testCallCounts_.spotifyGetShuffle;
            return testSpotifyShuffleState_;
        }
        if (currentSource_ == "sonos") {
            return std::nullopt;
        }

        ++testCallCounts_.spotifyGetShuffle;
        return testSpotifyShuffleState_;
    }
#endif
    if (currentSource_ == "spotify") {
        return spotify_.getShuffleState();
    }
    if (currentSource_ == "sonos") {
        return std::nullopt;
    }

    const auto spotifyState = spotify_.getShuffleState();
    if (spotifyState.has_value()) {
        return spotifyState;
    }
    return std::nullopt;
}

std::optional<bool> UnifiedMusicClient::toggleShuffle() {
    const auto current = getShuffleState();
    if (!current.has_value()) {
        return std::nullopt;
    }

    const bool nextState = !current.value();
#ifdef MEDIABAR_ENABLE_TEST_HOOKS
    if (testHooksEnabled_) {
        if (currentSource_ == "spotify" || currentSource_.isEmpty()) {
            ++testCallCounts_.spotifySetShuffle;
            if (testSpotifySetShuffleSucceeds_) {
                testSpotifyShuffleState_ = nextState;
                return nextState;
            }
        }
        return std::nullopt;
    }
#endif
    if (currentSource_ == "spotify" || currentSource_.isEmpty()) {
        if (spotify_.setShuffleState(nextState)) {
            return nextState;
        }
    }
    return std::nullopt;
}

std::optional<bool> UnifiedMusicClient::setRepeatState(bool enabled) {
#ifdef MEDIABAR_ENABLE_TEST_HOOKS

#endif
    if (currentSource_ == "spotify" || currentSource_.isEmpty()) {
        if (spotify_.setRepeatState(enabled)) {
            return enabled;
        }
        return std::nullopt;
    }

    return std::nullopt;
}
