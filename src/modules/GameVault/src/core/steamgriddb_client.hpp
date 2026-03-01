#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace wintools::gamevault {

struct SteamGridDBResult {
    int     gameId = 0;
    QString gameName;
};

struct SteamGridDBImage {
    QString url;
    int     width  = 0;
    int     height = 0;
};

class SteamGridDBClient : public QObject {
    Q_OBJECT
public:
    explicit SteamGridDBClient(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    bool hasApiKey() const;

    void searchGame(const QString& title);

    void fetchGrids(int gameId);

    void fetchHeroes(int gameId);

signals:
    void searchFinished(const QString& queryTitle,
                        QVector<wintools::gamevault::SteamGridDBResult> results);
    void gridsLoaded(int gameId, QVector<wintools::gamevault::SteamGridDBImage> images);
    void heroesLoaded(int gameId, QVector<wintools::gamevault::SteamGridDBImage> images);
    void error(const QString& message);

private:
    QNetworkReply* makeRequest(const QString& endpoint);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiKey;
};

}
