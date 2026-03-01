#include "steamgriddb_client.hpp"
#include "logger/logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace wintools::gamevault {

static constexpr const char* kLog      = "GameVault/SteamGridDB";
static constexpr const char* kBaseUrl  = "https://www.steamgriddb.com/api/v2";

SteamGridDBClient::SteamGridDBClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void SteamGridDBClient::setApiKey(const QString& key) {
    m_apiKey = key.trimmed();
}

bool SteamGridDBClient::hasApiKey() const {
    return !m_apiKey.isEmpty();
}

QNetworkReply* SteamGridDBClient::makeRequest(const QString& endpoint) {
    QUrl url(QStringLiteral("%1%2").arg(kBaseUrl, endpoint));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");
    return m_nam->get(req);
}

void SteamGridDBClient::searchGame(const QString& title) {
    if (!hasApiKey()) {
        emit error(QStringLiteral("No SteamGridDB API key configured."));
        return;
    }

    const QString endpoint = QStringLiteral("/search/autocomplete/%1")
                                 .arg(QString(QUrl::toPercentEncoding(title)));

    auto* reply = makeRequest(endpoint);
    const QString queryTitle = title;

    connect(reply, &QNetworkReply::finished, this, [this, reply, queryTitle]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                QStringLiteral("Search failed."),
                QStringLiteral("query=%1 err=%2").arg(queryTitle, reply->errorString()));
            emit searchFinished(queryTitle, {});
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject root  = doc.object();
        const QJsonArray  data  = root.value("data").toArray();

        QVector<SteamGridDBResult> results;
        results.reserve(data.size());
        for (const auto& val : data) {
            const QJsonObject obj = val.toObject();
            SteamGridDBResult r;
            r.gameId   = obj.value("id").toInt();
            r.gameName = obj.value("name").toString();
            results.append(r);
        }

        emit searchFinished(queryTitle, results);
    });
}

void SteamGridDBClient::fetchGrids(int gameId) {
    if (!hasApiKey()) return;

    const QString endpoint = QStringLiteral("/grids/game/%1?dimensions=600x900&types=static")
                                 .arg(gameId);

    auto* reply = makeRequest(endpoint);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameId]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit gridsLoaded(gameId, {});
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray data   = doc.object().value("data").toArray();

        QVector<SteamGridDBImage> images;
        for (const auto& val : data) {
            const QJsonObject obj = val.toObject();
            SteamGridDBImage img;
            img.url    = obj.value("url").toString();
            img.width  = obj.value("width").toInt();
            img.height = obj.value("height").toInt();
            if (!img.url.isEmpty())
                images.append(img);
        }

        emit gridsLoaded(gameId, images);
    });
}

void SteamGridDBClient::fetchHeroes(int gameId) {
    if (!hasApiKey()) return;

    const QString endpoint = QStringLiteral("/heroes/game/%1?types=static").arg(gameId);

    auto* reply = makeRequest(endpoint);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameId]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit heroesLoaded(gameId, {});
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray data   = doc.object().value("data").toArray();

        QVector<SteamGridDBImage> images;
        for (const auto& val : data) {
            const QJsonObject obj = val.toObject();
            SteamGridDBImage img;
            img.url    = obj.value("url").toString();
            img.width  = obj.value("width").toInt();
            img.height = obj.value("height").toInt();
            if (!img.url.isEmpty())
                images.append(img);
        }

        emit heroesLoaded(gameId, images);
    });
}

}
