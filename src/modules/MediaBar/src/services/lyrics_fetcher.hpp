#pragma once

// MediaBar: lyrics fetcher manages supporting service operations.

#include "types.hpp"

#include <QByteArray>
#include <QUrlQuery>
#include <optional>

class LyricsFetcher {
public:
    LyricsFetcher();

    std::optional<LyricsList> fetchLyrics(const QString& trackName, const QString& artistName);
    int currentLyricIndex(const LyricsList& lyrics, qint64 positionMs) const;

private:
    std::optional<LyricsList> fetchFromLrcLib(const QString& trackName, const QString& artistName);
    std::optional<LyricsList> parseLrcFormat(const QString& lrcText) const;
    std::optional<QByteArray> getWithRetries(const QUrl& url, const QUrlQuery& query, int maxAttempts) const;
};
