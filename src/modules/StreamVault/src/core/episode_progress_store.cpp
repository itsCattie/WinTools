#include "episode_progress_store.hpp"
#include "logger/logger.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace wintools::streamvault {

static constexpr const char* kLog      = "StreamVault/Episodes";
static constexpr const char* kConnName = "streamvault_episodes";

EpisodeProgressStore& EpisodeProgressStore::instance() {
    static EpisodeProgressStore s;
    return s;
}

EpisodeProgressStore::EpisodeProgressStore(QObject* parent)
    : QObject(parent) {
    ensureDb();
}

EpisodeProgressStore::~EpisodeProgressStore() {
    if (m_dbReady) {
        m_db.close();
        QSqlDatabase::removeDatabase(kConnName);
    }
}

void EpisodeProgressStore::ensureDb() {
    if (m_dbReady) return;

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/streamvault_episodes.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Failed to open episode progress database.",
            m_db.lastError().text());
        return;
    }

    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS episode_progress ("
           "  tmdb_id  INTEGER NOT NULL,"
           "  season   INTEGER NOT NULL,"
           "  episode  INTEGER NOT NULL,"
           "  watched  INTEGER NOT NULL DEFAULT 0,"
           "  PRIMARY KEY(tmdb_id, season, episode)"
           ")");

    m_dbReady = true;
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Episode progress database ready.", dbPath);
}

bool EpisodeProgressStore::isWatched(int tmdbId, int season, int episode) const {
    if (!m_dbReady) return false;

    QSqlQuery q(m_db);
    q.prepare("SELECT watched FROM episode_progress WHERE tmdb_id=:id AND season=:s AND episode=:e");
    q.bindValue(":id", tmdbId);
    q.bindValue(":s", season);
    q.bindValue(":e", episode);
    return q.exec() && q.next() && q.value(0).toInt() != 0;
}

void EpisodeProgressStore::setWatched(int tmdbId, int season, int episode, bool watched) {
    if (!m_dbReady) return;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO episode_progress (tmdb_id, season, episode, watched) "
              "VALUES (:id, :s, :e, :w) "
              "ON CONFLICT(tmdb_id, season, episode) DO UPDATE SET watched=:w2");
    q.bindValue(":id", tmdbId);
    q.bindValue(":s", season);
    q.bindValue(":e", episode);
    q.bindValue(":w", watched ? 1 : 0);
    q.bindValue(":w2", watched ? 1 : 0);
    if (q.exec()) {
        emit changed(tmdbId);
    }
}

void EpisodeProgressStore::toggleWatched(int tmdbId, int season, int episode) {
    setWatched(tmdbId, season, episode, !isWatched(tmdbId, season, episode));
}

void EpisodeProgressStore::markSeasonWatched(int tmdbId, int season, int episodeCount, bool watched) {
    if (!m_dbReady) return;

    m_db.transaction();
    for (int ep = 1; ep <= episodeCount; ++ep) {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO episode_progress (tmdb_id, season, episode, watched) "
                  "VALUES (:id, :s, :e, :w) "
                  "ON CONFLICT(tmdb_id, season, episode) DO UPDATE SET watched=:w2");
        q.bindValue(":id", tmdbId);
        q.bindValue(":s", season);
        q.bindValue(":e", ep);
        q.bindValue(":w", watched ? 1 : 0);
        q.bindValue(":w2", watched ? 1 : 0);
        q.exec();
    }
    m_db.commit();
    emit changed(tmdbId);
}

int EpisodeProgressStore::watchedCount(int tmdbId) const {
    if (!m_dbReady) return 0;

    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM episode_progress WHERE tmdb_id=:id AND watched=1");
    q.bindValue(":id", tmdbId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

int EpisodeProgressStore::watchedCountForSeason(int tmdbId, int season) const {
    if (!m_dbReady) return 0;

    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM episode_progress WHERE tmdb_id=:id AND season=:s AND watched=1");
    q.bindValue(":id", tmdbId);
    q.bindValue(":s", season);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

QVector<QPair<int,int>> EpisodeProgressStore::watchedEpisodes(int tmdbId) const {
    QVector<QPair<int,int>> result;
    if (!m_dbReady) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT season, episode FROM episode_progress WHERE tmdb_id=:id AND watched=1 ORDER BY season, episode");
    q.bindValue(":id", tmdbId);
    if (q.exec()) {
        while (q.next()) {
            result.push_back({q.value(0).toInt(), q.value(1).toInt()});
        }
    }
    return result;
}

}
