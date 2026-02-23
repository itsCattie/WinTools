#include "lyrics_timing.hpp"

#include <QtTest/QtTest>

// WinTools: lyrics timing tests manages feature behavior.

class LyricsTimingTests : public QObject {
    Q_OBJECT

private slots:
    void emptyLyricsReturnsNoSelection();
    void beforeFirstLyricReturnsNoSelection();
    void selectsLatestLineAtOrBeforeProgress();
    void returnsLastLineAfterTrackEnd();
    void instrumentalAdvanceHappensAfterThreshold();
    void noAdvanceWhenNextLineIsNotInstrumental();
};

void LyricsTimingTests::emptyLyricsReturnsNoSelection() {
    LyricsList lyrics;
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1000), -1);
}

void LyricsTimingTests::beforeFirstLyricReturnsNoSelection() {
    LyricsList lyrics;
    lyrics.push_back({1000, "line 1", false});
    lyrics.push_back({2000, "line 2", false});
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 999), -1);
}

void LyricsTimingTests::selectsLatestLineAtOrBeforeProgress() {
    LyricsList lyrics;
    lyrics.push_back({0, "line 1", false});
    lyrics.push_back({1500, "line 2", false});
    lyrics.push_back({3000, "line 3", false});

    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 0), 0);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1499), 0);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1500), 1);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 2500), 1);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 3000), 2);
}

void LyricsTimingTests::returnsLastLineAfterTrackEnd() {
    LyricsList lyrics;
    lyrics.push_back({0, "line 1", false});
    lyrics.push_back({1000, "line 2", false});
    lyrics.push_back({2000, "line 3", false});
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 999999), 2);
}

void LyricsTimingTests::instrumentalAdvanceHappensAfterThreshold() {
    LyricsList lyrics;
    lyrics.push_back({1000, "sung", false});
    lyrics.push_back({1800, "instrumental", true});
    lyrics.push_back({2600, "next sung", false});

    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1149), 0);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1150), 1);
    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1799), 1);
}

void LyricsTimingTests::noAdvanceWhenNextLineIsNotInstrumental() {
    LyricsList lyrics;
    lyrics.push_back({1000, "line 1", false});
    lyrics.push_back({1800, "line 2", false});

    QCOMPARE(mediabar::findCurrentLyricLine(lyrics, 1150), 0);
}

QTEST_MAIN(LyricsTimingTests)
#include "lyrics_timing_tests.moc"
