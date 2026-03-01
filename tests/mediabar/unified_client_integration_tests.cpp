#include "unified_client.hpp"

#include <QtTest/QtTest>

class UnifiedClientIntegrationTests : public QObject {
    Q_OBJECT

private slots:
    void spotifyModeReturnsSpotifyPlayback();
    void sonosModeReturnsSonosPlayback();
    void forcedSourceModeUsesSelectedProvider();
    void transportRoutesToCurrentSource();
    void shuffleBehaviorIsSpotifyOnly();
};

namespace {

PlaybackInfo playback(QString source, QString track, bool playing) {
    PlaybackInfo p;
    p.valid = true;
    p.source = std::move(source);
    p.trackName = std::move(track);
    p.isPlaying = playing;
    p.durationMs = 180000;
    p.progressMs = 30000;
    return p;
}

}

void UnifiedClientIntegrationTests::spotifyModeReturnsSpotifyPlayback() {
    UnifiedMusicClient client;
    client.testEnableHooks(true);

    client.testSetSpotifyPlayback(playback("spotify", "spotify-track", true));
    client.testSetSonosPlayback(playback("sonos", "sonos-track", true));

    client.setSourceMode(UnifiedMusicClient::SourceMode::Spotify);
    const auto result = client.getCurrentPlayback();
    QVERIFY(result.has_value());
    QCOMPARE(result->trackName, QString("spotify-track"));
    QCOMPARE(client.getSourceName(), QString("Spotify"));

    const auto calls = client.testCallCounts();
    QCOMPARE(calls.spotifyGetPlayback, 1);
    QCOMPARE(calls.sonosGetPlayback, 0);
}

void UnifiedClientIntegrationTests::sonosModeReturnsSonosPlayback() {
    UnifiedMusicClient client;
    client.testEnableHooks(true);

    client.testSetSpotifyPlayback(playback("spotify", "spotify-track", true));
    client.testSetSonosPlayback(playback("sonos", "sonos-track", true));

    client.setSourceMode(UnifiedMusicClient::SourceMode::Sonos);
    const auto result = client.getCurrentPlayback();
    QVERIFY(result.has_value());
    QCOMPARE(result->trackName, QString("sonos-track"));
    QCOMPARE(client.getSourceName(), QString("Sonos"));

    const auto calls = client.testCallCounts();
    QCOMPARE(calls.spotifyGetPlayback, 0);
    QCOMPARE(calls.sonosGetPlayback, 1);
}

void UnifiedClientIntegrationTests::forcedSourceModeUsesSelectedProvider() {
    UnifiedMusicClient client;
    client.testEnableHooks(true);

    client.testSetSpotifyPlayback(playback("spotify", "forced-spotify", true));
    client.testSetSonosPlayback(playback("sonos", "forced-sonos", true));

    client.setSourceMode(UnifiedMusicClient::SourceMode::Spotify);
    auto spotifyResult = client.getCurrentPlayback();
    QVERIFY(spotifyResult.has_value());
    QCOMPARE(spotifyResult->trackName, QString("forced-spotify"));

    client.setSourceMode(UnifiedMusicClient::SourceMode::Sonos);
    auto sonosResult = client.getCurrentPlayback();
    QVERIFY(sonosResult.has_value());
    QCOMPARE(sonosResult->trackName, QString("forced-sonos"));
}

void UnifiedClientIntegrationTests::transportRoutesToCurrentSource() {
    UnifiedMusicClient client;
    client.testEnableHooks(true);
    client.testSetTransportResults(
        false, true,
        true, false,
        false, true,
        false, true);

    client.testSetSpotifyPlayback(playback("spotify", "route", true));
    client.testSetSonosPlayback(playback("sonos", "route", true));
    client.setSourceMode(UnifiedMusicClient::SourceMode::Spotify);
    client.getCurrentPlayback();

    QVERIFY(!client.nextTrack());
    QVERIFY(client.previousTrack());
    QVERIFY(!client.playPause());
    QVERIFY(!client.seekToPosition(12345));

    auto calls = client.testCallCounts();
    QCOMPARE(calls.spotifyNextTrack, 1);
    QCOMPARE(calls.sonosNextTrack, 0);
    QCOMPARE(calls.spotifyPreviousTrack, 1);
    QCOMPARE(calls.sonosPreviousTrack, 0);
    QCOMPARE(calls.spotifyPlayPause, 1);
    QCOMPARE(calls.sonosPlayPause, 0);
    QCOMPARE(calls.spotifySeek, 1);
    QCOMPARE(calls.sonosSeek, 0);
}

void UnifiedClientIntegrationTests::shuffleBehaviorIsSpotifyOnly() {
    UnifiedMusicClient client;
    client.testEnableHooks(true);

    client.testSetSpotifyPlayback(playback("spotify", "spotify-track", true));
    client.setSourceMode(UnifiedMusicClient::SourceMode::Spotify);
    client.getCurrentPlayback();

    client.testSetSpotifyShuffle(true, true);
    auto state = client.getShuffleState();
    QVERIFY(state.has_value());
    QVERIFY(state.value());

    auto toggled = client.toggleShuffle();
    QVERIFY(toggled.has_value());
    QVERIFY(!toggled.value());

    client.testSetSonosPlayback(playback("sonos", "sonos-track", true));
    client.setSourceMode(UnifiedMusicClient::SourceMode::Sonos);
    client.getCurrentPlayback();

    QVERIFY(!client.getShuffleState().has_value());
    QVERIFY(!client.toggleShuffle().has_value());
}

QTEST_MAIN(UnifiedClientIntegrationTests)
#include "unified_client_integration_tests.moc"
