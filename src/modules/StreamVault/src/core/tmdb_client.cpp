#include "tmdb_client.hpp"
#include "streamvault_settings.hpp"
#include "logger/logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>

namespace wintools::streamvault {

static constexpr const char* kLog = "StreamVault/TmdbClient";

TmdbClient::TmdbClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void TmdbClient::setApiKey(const QString& key) {
    m_apiKey = key.trimmed();
}

bool TmdbClient::hasApiKey() const {
    return !m_apiKey.isEmpty();
}

void TmdbClient::search(const QString& query, int page) {
    if (m_apiKey.isEmpty()) {
        emit error(QStringLiteral("No TMDB API key configured. "
                                  "Enter your key in StreamVault settings."));
        return;
    }

    if (m_searchReply) {
        QNetworkReply* old = m_searchReply;
        m_searchReply = nullptr;
        old->abort();
        old->deleteLater();
    }

    QString lang = StreamVaultSettings::instance().defaultLanguage();
    bool    adult = StreamVaultSettings::instance().showAdultContent();

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"),            m_apiKey);
    q.addQueryItem(QStringLiteral("query"),              query);
    q.addQueryItem(QStringLiteral("language"),           lang);
    q.addQueryItem(QStringLiteral("page"),               QString::number(page));
    q.addQueryItem(QStringLiteral("include_adult"),      adult ? "true" : "false");

    QUrl url(QStringLiteral("https://api.themoviedb.org/3/search/multi"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    m_searchReply = m_nam->get(req);
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Search request sent."),
        QStringLiteral("query=%1 page=%2").arg(query).arg(page));
    connect(m_searchReply, &QNetworkReply::finished,
            this, [this, reply = m_searchReply]() { onSearchReply(reply); });
}

void TmdbClient::fetchWatchProviders(int tmdbId, MediaType type) {
    if (m_apiKey.isEmpty()) return;

    const QString endpoint = (type == MediaType::Movie)
        ? QString("https://api.themoviedb.org/3/movie/%1/watch/providers").arg(tmdbId)
        : QString("https://api.themoviedb.org/3/tv/%1/watch/providers").arg(tmdbId);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"), m_apiKey);

    QUrl url(endpoint);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_nam->get(req);
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Watch-providers request sent."),
        QStringLiteral("tmdbId=%1 type=%2").arg(tmdbId).arg(type == MediaType::Movie ? "movie" : "tv"));
    connect(reply, &QNetworkReply::finished,
            this, [this, tmdbId, reply]() { onWatchProvidersReply(tmdbId, reply); });
}

void TmdbClient::fetchExternalIds(int tmdbId, MediaType type) {
    if (!hasApiKey()) return;

    const QString endpoint = (type == MediaType::Movie)
        ? QStringLiteral("https://api.themoviedb.org/3/movie/%1/external_ids").arg(tmdbId)
        : QStringLiteral("https://api.themoviedb.org/3/tv/%1/external_ids").arg(tmdbId);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"), m_apiKey);

    QUrl url(endpoint);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, tmdbId, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) return;

        const QString imdbId = doc.object().value("imdb_id").toString();
        emit externalIdsLoaded(tmdbId, imdbId);
    });
}

void TmdbClient::fetchPoster(int tmdbId, const QString& posterPath) {
    if (posterPath.isEmpty()) return;

    QString baseUrl = StreamVaultSettings::posterBaseUrl();
    QUrl url(baseUrl + posterPath);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished,
            this, [this, tmdbId, reply]() { onPosterReply(tmdbId, reply); });
}

void TmdbClient::cancelAll() {
    if (m_searchReply) {
        QNetworkReply* old = m_searchReply;
        m_searchReply = nullptr;
        old->abort();
        old->deleteLater();
    }
}

void TmdbClient::fetchTvSeasons(int tmdbId) {
    if (m_apiKey.isEmpty()) return;

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    q.addQueryItem(QStringLiteral("language"), resolveLanguage());

    QUrl url(QStringLiteral("https://api.themoviedb.org/3/tv/%1").arg(tmdbId));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, tmdbId, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) return;

        const QJsonArray arr = doc.object().value("seasons").toArray();
        QVector<TvSeasonSummary> seasons;
        for (const QJsonValue& v : arr) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            TvSeasonSummary s;
            s.seasonNumber = obj.value("season_number").toInt();
            s.episodeCount = obj.value("episode_count").toInt();
            s.name = obj.value("name").toString();
            if (s.episodeCount > 0)
                seasons.append(s);
        }

        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("TV seasons loaded."),
            QStringLiteral("tmdbId=%1 seasons=%2").arg(tmdbId).arg(seasons.size()));
        emit tvSeasonsLoaded(tmdbId, seasons);
    });
}

void TmdbClient::fetchSeasonEpisodes(int tmdbId, int seasonNumber) {
    if (m_apiKey.isEmpty()) return;

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    q.addQueryItem(QStringLiteral("language"), resolveLanguage());

    QUrl url(QStringLiteral("https://api.themoviedb.org/3/tv/%1/season/%2").arg(tmdbId).arg(seasonNumber));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, tmdbId, seasonNumber, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) return;

        const QJsonArray arr = doc.object().value("episodes").toArray();
        QVector<TvEpisodeSummary> episodes;
        for (const QJsonValue& v : arr) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            TvEpisodeSummary ep;
            ep.episodeNumber = obj.value("episode_number").toInt();
            ep.name = obj.value("name").toString();
            ep.airDate = obj.value("air_date").toString();
            ep.overview = obj.value("overview").toString();
            episodes.append(ep);
        }

        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Season episodes loaded."),
            QStringLiteral("tmdbId=%1 season=%2 episodes=%3").arg(tmdbId).arg(seasonNumber).arg(episodes.size()));
        emit seasonEpisodesLoaded(tmdbId, seasonNumber, episodes);
    });
}

void TmdbClient::onSearchReply(QNetworkReply* reply) {
    if (reply != m_searchReply) { reply->deleteLater(); return; }
    m_searchReply = nullptr;

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            QStringLiteral("Search network error."), reply->errorString());
        emit error(QStringLiteral("Network error: ") + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        emit error(QStringLiteral("JSON parse error: ") + parseErr.errorString());
        return;
    }

    QJsonObject root = doc.object();

    if (root.contains("status_message")) {
        emit error(root.value("status_message").toString());
        return;
    }

    QVector<StreamEntry> results;
    const QJsonArray arr = root.value("results").toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        StreamEntry e = parseEntry(v.toObject());
        if (e.title.isEmpty()) continue;
        results.append(e);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Search results received."),
        QStringLiteral("%1 results").arg(results.size()));
    emit searchFinished(results);
}

void TmdbClient::onPosterReply(int tmdbId, QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        emit posterLoaded(tmdbId, reply->readAll());
    }
    reply->deleteLater();
}

void TmdbClient::onWatchProvidersReply(int tmdbId, QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) return;

    const QJsonObject results = doc.object().value("results").toObject();

    WatchProviderMap byCountry;
    for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
        const QString countryCode = it.key();
        const QJsonObject countryData = it.value().toObject();
        const QJsonArray flatrate = countryData.value("flatrate").toArray();

        if (flatrate.isEmpty()) continue;

        CountryProviders cp;
        cp.link = countryData.value("link").toString();

        for (const QJsonValue& v : flatrate) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            ProviderEntry pe;
            pe.id       = obj.value("provider_id").toInt();
            pe.name     = obj.value("provider_name").toString();
            pe.logoPath = obj.value("logo_path").toString();
            cp.providers.append(pe);
        }

        if (!cp.providers.isEmpty())
            byCountry.insert(countryCode, cp);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Watch-providers loaded."),
        QStringLiteral("tmdbId=%1 countries=%2").arg(tmdbId).arg(byCountry.size()));
    emit watchProvidersLoaded(tmdbId, byCountry);
}

StreamEntry TmdbClient::parseEntry(const QJsonObject& obj) {
    StreamEntry e;

    QString mediaType = obj.value("media_type").toString();
    if (mediaType == "movie") {
        e.mediaType = MediaType::Movie;
        e.title     = obj.value("title").toString();
        if (e.title.isEmpty())
            e.title = obj.value("original_title").toString();
        e.originalTitle = obj.value("original_title").toString();

        QString release = obj.value("release_date").toString();
        if (release.length() >= 4)
            e.releaseYear = release.left(4);

    } else if (mediaType == "tv") {
        e.mediaType = MediaType::TvShow;
        e.title     = obj.value("name").toString();
        if (e.title.isEmpty())
            e.title = obj.value("original_name").toString();
        e.originalTitle = obj.value("original_name").toString();

        QString firstAir = obj.value("first_air_date").toString();
        if (firstAir.length() >= 4)
            e.releaseYear = firstAir.left(4);

    } else {

        return {};
    }

    e.tmdbId      = obj.value("id").toInt();
    e.overview    = obj.value("overview").toString();
    e.voteAverage = static_cast<float>(obj.value("vote_average").toDouble());
    e.voteCount   = obj.value("vote_count").toInt();
    e.posterPath  = obj.value("poster_path").toString();
    e.backdropPath = obj.value("backdrop_path").toString();

    const QJsonArray genres = obj.value("genre_ids").toArray();
    for (const QJsonValue& g : genres)
        e.genreIds.append(g.toInt());

    return e;
}

QString TmdbClient::resolveLanguage() {
    return StreamVaultSettings::instance().defaultLanguage();
}

}
