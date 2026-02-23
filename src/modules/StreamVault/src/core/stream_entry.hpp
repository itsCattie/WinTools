#pragma once

// StreamVault: stream entry manages core logic and state.

#include <QString>
#include <QStringList>
#include <QMetaType>

namespace wintools::streamvault {

enum class MediaType {
    Movie,
    TvShow,
    Unknown
};

inline QString mediaTypeName(MediaType t) {
    switch (t) {
        case MediaType::Movie:  return "Movie";
        case MediaType::TvShow: return "TV Show";
        default:                return "Unknown";
    }
}

struct StreamEntry {

    int         tmdbId      = 0;
    MediaType   mediaType   = MediaType::Unknown;

    QString     title;
    QString     originalTitle;
    QString     overview;
    QString     releaseYear;
    float       voteAverage  = 0.f;
    int         voteCount    = 0;

    QString     posterPath;
    QString     backdropPath;

    QString     posterLocalPath;
    QString     backdropLocalPath;

    QList<int>  genreIds;
};

}

Q_DECLARE_METATYPE(wintools::streamvault::StreamEntry)
