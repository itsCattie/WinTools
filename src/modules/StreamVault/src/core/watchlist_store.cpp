#include "watchlist_store.hpp"
#include "logger/logger.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace wintools::streamvault {

static constexpr const char* kLog      = "StreamVault/Watchlist";
static constexpr const char* kConnName = "streamvault_watchlist";

WatchlistStore& WatchlistStore::instance() {
    static WatchlistStore s;
    return s;
}

WatchlistStore::WatchlistStore(QObject* parent)
    : QObject(parent) {
    ensureDb();
}

WatchlistStore::~WatchlistStore() {
    if (m_dbReady) {
        m_db.close();
        QSqlDatabase::removeDatabase(kConnName);
    }
}

void WatchlistStore::ensureDb() {
    if (m_dbReady) return;

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/streamvault_watchlist.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Failed to open watchlist database.",
            m_db.lastError().text());
        return;
    }

    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS watchlist ("
           "  tmdb_id      INTEGER PRIMARY KEY,"
           "  media_type   TEXT    NOT NULL,"
           "  title        TEXT    NOT NULL,"
           "  overview     TEXT    NOT NULL DEFAULT '',"
           "  release_year TEXT    NOT NULL DEFAULT '',"
           "  vote_average REAL   NOT NULL DEFAULT 0,"
           "  poster_path  TEXT    NOT NULL DEFAULT '',"
           "  backdrop_path TEXT   NOT NULL DEFAULT '',"
           "  added_at     INTEGER NOT NULL DEFAULT 0"
           ")");

    m_dbReady = true;
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Watchlist database ready.", dbPath);
}

bool WatchlistStore::contains(int tmdbId) const {
    if (!m_dbReady) return false;

    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM watchlist WHERE tmdb_id = :id LIMIT 1");
    q.bindValue(":id", tmdbId);
    return q.exec() && q.next();
}

void WatchlistStore::add(const StreamEntry& entry) {
    if (!m_dbReady || entry.tmdbId <= 0) return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QString mt = (entry.mediaType == MediaType::TvShow) ? "tv" : "movie";

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO watchlist "
              "(tmdb_id, media_type, title, overview, release_year, vote_average, "
              " poster_path, backdrop_path, added_at) "
              "VALUES (:id, :mt, :t, :o, :ry, :va, :pp, :bp, :at) "
              "ON CONFLICT(tmdb_id) DO UPDATE SET "
              "  title = :t2, overview = :o2, vote_average = :va2, "
              "  poster_path = :pp2, backdrop_path = :bp2");
    q.bindValue(":id",  entry.tmdbId);
    q.bindValue(":mt",  mt);
    q.bindValue(":t",   entry.title);
    q.bindValue(":o",   entry.overview);
    q.bindValue(":ry",  entry.releaseYear);
    q.bindValue(":va",  entry.voteAverage);
    q.bindValue(":pp",  entry.posterPath);
    q.bindValue(":bp",  entry.backdropPath);
    q.bindValue(":at",  now);
    q.bindValue(":t2",  entry.title);
    q.bindValue(":o2",  entry.overview);
    q.bindValue(":va2", entry.voteAverage);
    q.bindValue(":pp2", entry.posterPath);
    q.bindValue(":bp2", entry.backdropPath);
    q.exec();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Added to watchlist: '%1' (id=%2)").arg(entry.title).arg(entry.tmdbId));
    emit changed();
}

void WatchlistStore::remove(int tmdbId) {
    if (!m_dbReady) return;

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM watchlist WHERE tmdb_id = :id");
    q.bindValue(":id", tmdbId);
    q.exec();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Removed from watchlist: id=%1").arg(tmdbId));
    emit changed();
}

void WatchlistStore::toggle(const StreamEntry& entry) {
    if (contains(entry.tmdbId))
        remove(entry.tmdbId);
    else
        add(entry);
}

QVector<StreamEntry> WatchlistStore::all() const {
    QVector<StreamEntry> result;
    if (!m_dbReady) return result;

    QSqlQuery q(m_db);
    q.exec("SELECT tmdb_id, media_type, title, overview, release_year, "
           "vote_average, poster_path, backdrop_path FROM watchlist "
           "ORDER BY added_at DESC");

    while (q.next()) {
        StreamEntry e;
        e.tmdbId       = q.value(0).toInt();
        e.mediaType    = (q.value(1).toString() == "tv") ? MediaType::TvShow : MediaType::Movie;
        e.title        = q.value(2).toString();
        e.overview     = q.value(3).toString();
        e.releaseYear  = q.value(4).toString();
        e.voteAverage  = q.value(5).toFloat();
        e.posterPath   = q.value(6).toString();
        e.backdropPath = q.value(7).toString();
        result.append(std::move(e));
    }

    return result;
}

int WatchlistStore::count() const {
    if (!m_dbReady) return 0;

    QSqlQuery q(m_db);
    q.exec("SELECT COUNT(*) FROM watchlist");
    return q.next() ? q.value(0).toInt() : 0;
}

}
