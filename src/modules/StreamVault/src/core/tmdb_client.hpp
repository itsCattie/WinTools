#pragma once

#include "modules/StreamVault/src/core/stream_entry.hpp"

#include <QMap>
#include <QObject>
#include <QVector>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace wintools::streamvault {

struct ProviderEntry {
    int     id;
    QString name;
    QString logoPath;
};

struct CountryProviders {
    QString               link;
    QList<ProviderEntry>  providers;
};

struct TvSeasonSummary {
    int seasonNumber{0};
    int episodeCount{0};
    QString name;
};

struct TvEpisodeSummary {
    int episodeNumber{0};
    QString name;
    QString airDate;
    QString overview;
};

using WatchProviderMap = QMap<QString, CountryProviders>;

class TmdbClient : public QObject {
    Q_OBJECT
public:
    explicit TmdbClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    bool hasApiKey() const;

    void search(const QString& query, int page = 1);

    void fetchPoster(int tmdbId, const QString& posterPath);

    void fetchWatchProviders(int tmdbId, MediaType type);

    void fetchExternalIds(int tmdbId, MediaType type);

    void fetchTvSeasons(int tmdbId);
    void fetchSeasonEpisodes(int tmdbId, int seasonNumber);

    void cancelAll();

signals:
    void searchFinished(QVector<StreamEntry> results);
    void posterLoaded(int tmdbId, QByteArray imageData);
    void watchProvidersLoaded(int tmdbId, WatchProviderMap byCountry);
    void externalIdsLoaded(int tmdbId, QString imdbId);
    void tvSeasonsLoaded(int tmdbId, QVector<TvSeasonSummary> seasons);
    void seasonEpisodesLoaded(int tmdbId, int season, QVector<TvEpisodeSummary> episodes);
    void error(QString message);

private slots:
    void onSearchReply(QNetworkReply* reply);
    void onPosterReply(int tmdbId, QNetworkReply* reply);
    void onWatchProvidersReply(int tmdbId, QNetworkReply* reply);

private:
    QNetworkAccessManager* m_nam      = nullptr;
    QString                m_apiKey;
    QNetworkReply*         m_searchReply = nullptr;

    static StreamEntry parseEntry(const QJsonObject& obj);
    static QString     resolveLanguage();
};

}
