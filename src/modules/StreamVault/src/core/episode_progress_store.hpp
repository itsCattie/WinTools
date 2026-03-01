#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVector>

namespace wintools::streamvault {

struct EpisodeInfo {
    int season{0};
    int episode{0};
    QString name;
    QString airDate;
    bool watched{false};
};

class EpisodeProgressStore : public QObject {
    Q_OBJECT

public:
    static EpisodeProgressStore& instance();

    bool isWatched(int tmdbId, int season, int episode) const;
    void setWatched(int tmdbId, int season, int episode, bool watched);
    void toggleWatched(int tmdbId, int season, int episode);

    void markSeasonWatched(int tmdbId, int season, int episodeCount, bool watched);

    int watchedCount(int tmdbId) const;
    int watchedCountForSeason(int tmdbId, int season) const;

    QVector<QPair<int,int>> watchedEpisodes(int tmdbId) const;

signals:
    void changed(int tmdbId);

private:
    explicit EpisodeProgressStore(QObject* parent = nullptr);
    ~EpisodeProgressStore() override;

    void ensureDb();

    QSqlDatabase m_db;
    bool m_dbReady = false;
};

}
