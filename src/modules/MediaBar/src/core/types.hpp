#pragma once

#include <QString>
#include <QVector>

struct LyricsLine {
    qint64 timeMs = 0;
    QString text;
    bool isInstrumental = false;
};

struct PlaybackInfo {
    bool valid = false;
    bool isPlaying = false;
    QString trackName;
    QString artistName;
    QString albumName;
    qint64 progressMs = 0;
    qint64 durationMs = 0;
    QString trackId;
    QString trackUri;
    QString albumUri;
    QString albumArt;
    int volumePercent = -1;
    QString source;
};

using LyricsList = QVector<LyricsLine>;
