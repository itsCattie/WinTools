#include "gui.hpp"

#include <QtTest/QtTest>

#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>

namespace {

QLabel* findLabelByObjectName(QWidget* root, const char* objectName) {
    return root->findChild<QLabel*>(objectName);
}

QListWidget* findLyricsList(QWidget* root) {
    const auto lists = root->findChildren<QListWidget*>();
    return lists.isEmpty() ? nullptr : lists.first();
}

QProgressBar* findProgressBar(QWidget* root) {
    const auto bars = root->findChildren<QProgressBar*>();
    return bars.isEmpty() ? nullptr : bars.first();
}

QLabel* findAlbumArtLabel(QWidget* root) {
    const auto labels = root->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->minimumWidth() >= 140 && label->maximumWidth() >= 140) {
            return label;
        }
    }
    return nullptr;
}

QPair<QLabel*, QLabel*> findTimeLabels(QWidget* root) {
    const auto labels = root->findChildren<QLabel*>("time");
    if (labels.size() < 2) {
        return {nullptr, nullptr};
    }
    return {labels[0], labels[1]};
}

}

class LyricsWindowTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void hasCoreControlsAndDefaultSourceMode();
    void sourceModeSelectionSyncsButtons();
    void sidebarToggleCollapsesAndRestoresLabels();

    void updateTrackInfoSetsFallbacksWhenNoArt();
    void playPauseButtonIconReflectsPlaybackState();
    void progressFormattingAndClampingWorks();
    void favoriteStateReflectsSpotifyAvailability();
    void shuffleStateReflectsSupportAndEnabledStatus();

    void displayLyricsAndHighlightingStylesLines();
    void showNoLyricsOnlyWhenLyricsAreEmpty();
    void showWaitingResetsUiToIdleState();

private:
    QTemporaryDir tempAppData_;
};

void LyricsWindowTests::initTestCase() {
    QVERIFY2(tempAppData_.isValid(), "Expected temporary directory for isolated APPDATA.");
    const QByteArray appData = QDir::toNativeSeparators(tempAppData_.path()).toLocal8Bit();
    qputenv("APPDATA", appData);
}

void LyricsWindowTests::hasCoreControlsAndDefaultSourceMode() {
    LyricsWindow window;

    QCOMPARE(window.sourceMode(), QString("spotify"));
    QVERIFY(window.sourceSpotifyButton());
    QVERIFY(window.sourceSpotifyButton()->isChecked());
    QVERIFY(window.searchButton());
    QCOMPARE(window.searchButton()->property("fullText").toString(), QString("Search"));
    QVERIFY(window.prevButton());
    QVERIFY(window.playPauseButton());
    QVERIFY(window.nextButton());
    QVERIFY(window.shuffleButton());
    QVERIFY(!window.shuffleButton()->isEnabled());
}

void LyricsWindowTests::sourceModeSelectionSyncsButtons() {
    LyricsWindow window;

    window.setSourceMode("spotify");
    QCOMPARE(window.sourceMode(), QString("spotify"));
    QVERIFY(window.sourceSpotifyButton()->isChecked());
    QVERIFY(!window.sourceSonosButton()->isChecked());

    window.setSourceMode("sonos");
    QCOMPARE(window.sourceMode(), QString("sonos"));
    QVERIFY(window.sourceSonosButton()->isChecked());

    window.setSourceMode("invalid-mode");
    QCOMPARE(window.sourceMode(), QString("spotify"));
    QVERIFY(window.sourceSpotifyButton()->isChecked());
}

void LyricsWindowTests::sidebarToggleCollapsesAndRestoresLabels() {
    LyricsWindow window;
    QVERIFY(window.searchButton());

    window.setSidebarExpanded(false);
    QVERIFY(!window.isSidebarExpanded());

    QVERIFY(!window.sourceSpotifyButton()->isHidden());
    QCOMPARE(window.searchButton()->text(), window.searchButton()->property("iconEmoji").toString());

    window.setSidebarExpanded(true);
    QVERIFY(window.isSidebarExpanded());
    QVERIFY(!window.sourceSpotifyButton()->isHidden());
    QVERIFY(!window.sourceSonosButton()->isHidden());
    QVERIFY(window.searchButton()->text().contains("Search"));
}

void LyricsWindowTests::updateTrackInfoSetsFallbacksWhenNoArt() {
    LyricsWindow window;

    PlaybackInfo playback;
    playback.trackName = "";
    playback.artistName = "Test Artist";
    playback.albumArt = "";

    window.updateTrackInfo(playback, "Spotify");

    QLabel* track = findLabelByObjectName(&window, "track");
    QLabel* artist = findLabelByObjectName(&window, "artist");
    QLabel* source = findLabelByObjectName(&window, "source");
    QLabel* art = findAlbumArtLabel(&window);

    QVERIFY(track);
    QVERIFY(artist);
    QVERIFY(source);
    QVERIFY(art);
    QCOMPARE(track->text(), QString("Unknown"));
    QCOMPARE(artist->text(), QString("Test Artist"));
    QCOMPARE(source->text(), QString("Spotify"));
    QCOMPARE(art->text(), QString::fromUtf8("🎵"));
}

void LyricsWindowTests::playPauseButtonIconReflectsPlaybackState() {
    LyricsWindow window;
    QVERIFY(window.playPauseButton());

    window.updatePlayPauseButton(false);
    const qint64 playIconKey = window.playPauseButton()->icon().cacheKey();

    window.updatePlayPauseButton(true);
    const qint64 pauseIconKey = window.playPauseButton()->icon().cacheKey();

    QVERIFY(playIconKey != 0);
    QVERIFY(pauseIconKey != 0);
    QVERIFY(playIconKey != pauseIconKey);
}

void LyricsWindowTests::progressFormattingAndClampingWorks() {
    LyricsWindow window;
    QProgressBar* progress = findProgressBar(&window);
    const auto [currentTime, duration] = findTimeLabels(&window);

    QVERIFY(progress);
    QVERIFY(currentTime);
    QVERIFY(duration);

    window.updateProgress(65000, 240000);
    QCOMPARE(progress->value(), 270);
    QCOMPARE(currentTime->text(), QString("1:05"));
    QCOMPARE(duration->text(), QString("4:00"));

    window.updateProgress(999999, 1000);
    QCOMPARE(progress->value(), 1000);

    window.updateProgress(500, 0);
    QCOMPARE(progress->value(), 0);
    QCOMPARE(currentTime->text(), QString("0:00"));
    QCOMPARE(duration->text(), QString("0:00"));
}

void LyricsWindowTests::favoriteStateReflectsSpotifyAvailability() {
    LyricsWindow window;
    QVERIFY(window.favoriteButton());

    window.setFavoriteState(std::nullopt, false);
    QVERIFY(!window.favoriteButton()->isEnabled());
    QCOMPARE(window.favoriteButton()->text(), QString::fromUtf8("♡"));

    window.setFavoriteState(true, true);
    QVERIFY(window.favoriteButton()->isEnabled());
    QCOMPARE(window.favoriteButton()->text(), QString::fromUtf8("♥"));

    window.setFavoriteState(false, true);
    QCOMPARE(window.favoriteButton()->text(), QString::fromUtf8("♡"));
}

void LyricsWindowTests::shuffleStateReflectsSupportAndEnabledStatus() {
    LyricsWindow window;
    QVERIFY(window.shuffleButton());

    window.setShuffleState(false, false);
    QVERIFY(!window.shuffleButton()->isEnabled());

    window.setShuffleState(true, true);
    QVERIFY(window.shuffleButton()->isEnabled());
    QVERIFY(window.shuffleButton()->styleSheet().contains("background-color"));

    window.setShuffleState(false, true);
    QVERIFY(window.shuffleButton()->isEnabled());
    QVERIFY(window.shuffleButton()->styleSheet().contains("transparent"));
}

void LyricsWindowTests::displayLyricsAndHighlightingStylesLines() {
    LyricsWindow window;
    QListWidget* list = findLyricsList(&window);
    QVERIFY(list);

    LyricsList lyrics;
    lyrics.push_back({0, "first", false});
    lyrics.push_back({1000, "second", false});
    lyrics.push_back({2000, "third", false});

    window.displayLyrics(lyrics);
    QCOMPARE(list->count(), 3);
    QCOMPARE(list->item(0)->text(), QString("first"));
    QCOMPARE(list->item(1)->text(), QString("second"));
    QCOMPARE(list->item(2)->text(), QString("third"));

    window.highlightCurrentLyric(1);
    QCOMPARE(list->item(1)->font().pointSize(), 18);
    QVERIFY(list->item(1)->font().bold());
    QCOMPARE(list->item(0)->font().pointSize(), 15);
    QCOMPARE(list->item(2)->font().pointSize(), 15);

    window.highlightCurrentLyric(-1);
    QCOMPARE(list->item(0)->font().pointSize(), 14);
    QCOMPARE(list->item(1)->font().pointSize(), 14);
    QCOMPARE(list->item(2)->font().pointSize(), 14);
    QVERIFY(!list->item(0)->font().bold());
    QVERIFY(!list->item(1)->font().bold());
    QVERIFY(!list->item(2)->font().bold());

    window.displayLyrics(lyrics);
    QCOMPARE(list->count(), 3);
}

void LyricsWindowTests::showNoLyricsOnlyWhenLyricsAreEmpty() {
    LyricsWindow window;
    QListWidget* list = findLyricsList(&window);
    QVERIFY(list);

    LyricsList lyrics;
    lyrics.push_back({0, "line", false});
    window.displayLyrics(lyrics);
    QCOMPARE(list->count(), 1);

    window.showNoLyrics();
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QString("line"));

    LyricsWindow emptyWindow;
    QListWidget* emptyList = findLyricsList(&emptyWindow);
    QVERIFY(emptyList);
    emptyWindow.showNoLyrics();
    QCOMPARE(emptyList->count(), 1);
    QCOMPARE(emptyList->item(0)->text(), QString("No synchronized lyrics found for this track."));
}

void LyricsWindowTests::showWaitingResetsUiToIdleState() {
    LyricsWindow window;
    QLabel* track = findLabelByObjectName(&window, "track");
    QLabel* artist = findLabelByObjectName(&window, "artist");
    QLabel* source = findLabelByObjectName(&window, "source");
    QListWidget* list = findLyricsList(&window);
    QProgressBar* progress = findProgressBar(&window);

    QVERIFY(track);
    QVERIFY(artist);
    QVERIFY(source);
    QVERIFY(list);
    QVERIFY(progress);

    PlaybackInfo playback;
    playback.trackName = "Song";
    playback.artistName = "Artist";
    window.updateTrackInfo(playback, "Spotify");
    window.updateProgress(120000, 240000);
    window.setFavoriteState(true, true);

    window.showWaiting();

    QCOMPARE(track->text(), QString("No track playing"));
    QCOMPARE(artist->text(), QString("Waiting for playback..."));
    QCOMPARE(source->text(), QString("Unknown"));
    QCOMPARE(progress->value(), 0);
    QVERIFY(!window.favoriteButton()->isEnabled());
    QCOMPARE(window.favoriteButton()->text(), QString::fromUtf8("♡"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QString("Waiting for music source..."));
}

QTEST_MAIN(LyricsWindowTests)
#include "lyrics_window_tests.moc"
