#include "lyrics_fetcher.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QThread>
#include <QUrlQuery>

// MediaBar: lyrics fetcher manages supporting service operations.

namespace {

constexpr qint64 INSTRUMENTAL_GAP_THRESHOLD_MS = 5000;
constexpr auto MUSIC_NOTE_SYMBOL = u8"♪";

QString cleanString(QString value) {
    value.replace(QRegularExpression(R"(\s*[\(\[].*?[\)\]])"), "");
    return value.trimmed();
}

}

LyricsFetcher::LyricsFetcher() = default;

std::optional<LyricsList> LyricsFetcher::fetchLyrics(const QString& trackName, const QString& artistName) {
    return fetchFromLrcLib(trackName, artistName);
}

int LyricsFetcher::currentLyricIndex(const LyricsList& lyrics, qint64 positionMs) const {
    if (lyrics.isEmpty()) {
        return -1;
    }

    for (int i = lyrics.size() - 1; i >= 0; --i) {
        if (positionMs >= lyrics[i].timeMs) {
            return i;
        }
    }
    return -1;
}

std::optional<LyricsList> LyricsFetcher::fetchFromLrcLib(const QString& trackName, const QString& artistName) {
    const QString cleanTrack = cleanString(trackName);
    const QString cleanArtist = cleanString(artistName);

    QUrl url("https://lrclib.net/api/search");
    QUrlQuery query;
    query.addQueryItem("track_name", cleanTrack);
    query.addQueryItem("artist_name", cleanArtist);

    auto payload = getWithRetries(url, query, 3);
    if (!payload.has_value()) {
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload.value());
    if (!doc.isArray()) {
        return std::nullopt;
    }

    const QJsonArray results = doc.array();
    if (results.isEmpty()) {
        return std::nullopt;
    }

    const QJsonObject first = results.first().toObject();
    const QString synced = first.value("syncedLyrics").toString();
    if (synced.isEmpty()) {
        return std::nullopt;
    }

    return parseLrcFormat(synced);
}

std::optional<LyricsList> LyricsFetcher::parseLrcFormat(const QString& lrcText) const {
    static const QRegularExpression pattern(R"(\[(\d{2}):(\d{2})\.(\d{2,3})\](.*))");

    LyricsList lines;
    const auto rawLines = lrcText.split('\n');
    for (const QString& rawLine : rawLines) {
        const auto match = pattern.match(rawLine);
        if (!match.hasMatch()) {
            continue;
        }

        const int minutes = match.captured(1).toInt();
        const int seconds = match.captured(2).toInt();
        QString frac = match.captured(3);
        const QString text = match.captured(4).trimmed();
        if (text.isEmpty()) {
            continue;
        }

        int fracMs = 0;
        if (frac.size() == 2) {
            fracMs = frac.toInt() * 10;
        } else if (frac.size() == 3) {
            fracMs = frac.toInt();
        } else {
            frac = frac.left(3).leftJustified(3, '0');
            fracMs = frac.toInt();
        }

        const qint64 timeMs = (minutes * 60 * 1000) + (seconds * 1000) + fracMs;
        lines.push_back(LyricsLine{timeMs, text, false});
    }

    if (lines.isEmpty()) {
        return std::nullopt;
    }

    LyricsList withNotes;
    for (int i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (i == 0) {
            if (line.timeMs >= INSTRUMENTAL_GAP_THRESHOLD_MS) {
                withNotes.push_back(LyricsLine{0, QString::fromUtf8(MUSIC_NOTE_SYMBOL), true});
            }
        } else {
            const auto& prev = lines[i - 1];
            const qint64 gap = line.timeMs - prev.timeMs;
            if (gap >= INSTRUMENTAL_GAP_THRESHOLD_MS) {
                withNotes.push_back(LyricsLine{prev.timeMs + 1000, QString::fromUtf8(MUSIC_NOTE_SYMBOL), true});
            }
        }
        withNotes.push_back(line);
    }

    return withNotes;
}

std::optional<QByteArray> LyricsFetcher::getWithRetries(const QUrl& baseUrl, const QUrlQuery& query, int maxAttempts) const {
    QNetworkAccessManager network;

    for (int attempt = 0; attempt < std::max(1, maxAttempts); ++attempt) {
        QUrl url(baseUrl);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "MediaBarCpp/1.0");

        QNetworkReply* reply = network.get(request);
        if (!reply) {
            return std::nullopt;
        }
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto error = reply->error();
        reply->deleteLater();

        if (error == QNetworkReply::NoError && statusCode == 200) {
            return body;
        }

        if (statusCode == 429) {
            QThread::sleep(2);
            continue;
        }

        if (attempt < maxAttempts - 1) {
            QThread::msleep(600 * (attempt + 1));
        }
    }

    return std::nullopt;
}
