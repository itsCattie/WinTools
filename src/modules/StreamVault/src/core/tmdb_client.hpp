#pragma once

// StreamVault: tmdb client manages core logic and state.

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

using WatchProviderMap = QMap<QString, QList<ProviderEntry>>;

class TmdbClient : public QObject {
    Q_OBJECT
public:
    explicit TmdbClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    bool hasApiKey() const;

    void search(const QString& query, int page = 1);

    void fetchPoster(int tmdbId, const QString& posterPath);

    void fetchWatchProviders(int tmdbId, MediaType type);

    void cancelAll();

signals:
    void searchFinished(QVector<StreamEntry> results);
    void posterLoaded(int tmdbId, QByteArray imageData);
    void watchProvidersLoaded(int tmdbId, WatchProviderMap byCountry);
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
