#include "playtime_tracker.hpp"
#include "logger/logger.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

// GameVault: playtime tracker manages core logic and state.

namespace wintools::gamevault {

static constexpr const char* kLog = "GameVault/Playtime";
static constexpr const char* kConnName = "gamevault_playtime";

PlaytimeTracker& PlaytimeTracker::instance() {
    static PlaytimeTracker s;
    return s;
}

PlaytimeTracker::PlaytimeTracker(QObject* parent)
    : QObject(parent) {
    ensureDb();
}

PlaytimeTracker::~PlaytimeTracker() {

    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        const auto parts = it.key().split('|');
        if (parts.size() == 2)
            endSession(parts[0], parts[1]);
    }
    if (m_dbReady) {
        m_db.close();
        QSqlDatabase::removeDatabase(kConnName);
    }
}

void PlaytimeTracker::ensureDb() {
    if (m_dbReady) return;

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/gamevault_playtime.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Failed to open playtime database.",
            m_db.lastError().text());
        return;
    }

    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS playtime ("
           "  platform   TEXT NOT NULL,"
           "  platformId TEXT NOT NULL,"
           "  seconds    INTEGER NOT NULL DEFAULT 0,"
           "  lastPlayed INTEGER NOT NULL DEFAULT 0,"
           "  PRIMARY KEY (platform, platformId)"
           ")");

    m_dbReady = true;
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Playtime database ready.", dbPath);
}

QString PlaytimeTracker::key(const QString& platform, const QString& platformId) const {
    return platform + '|' + platformId;
}

void PlaytimeTracker::startSession(const QString& platform, const QString& platformId) {
    const QString k = key(platform, platformId);
    if (m_activeSessions.contains(k)) return;
    m_activeSessions.insert(k, QDateTime::currentDateTimeUtc());
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Session started.", QStringLiteral("%1 / %2").arg(platform, platformId));
}

void PlaytimeTracker::endSession(const QString& platform, const QString& platformId) {
    const QString k = key(platform, platformId);
    auto it = m_activeSessions.find(k);
    if (it == m_activeSessions.end()) return;

    const qint64 elapsed = it.value().secsTo(QDateTime::currentDateTimeUtc());
    m_activeSessions.erase(it);

    if (!m_dbReady || elapsed <= 0) return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO playtime (platform, platformId, seconds, lastPlayed) "
              "VALUES (:p, :id, :s, :lp) "
              "ON CONFLICT(platform, platformId) DO UPDATE SET "
              "  seconds = seconds + :s2, lastPlayed = :lp2");
    q.bindValue(":p",   platform);
    q.bindValue(":id",  platformId);
    q.bindValue(":s",   elapsed);
    q.bindValue(":lp",  now);
    q.bindValue(":s2",  elapsed);
    q.bindValue(":lp2", now);
    q.exec();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Session ended: +%1s").arg(elapsed),
        QStringLiteral("%1 / %2").arg(platform, platformId));
}

quint64 PlaytimeTracker::playtime(const QString& platform, const QString& platformId) const {
    const QString k = key(platform, platformId);

    quint64 dbSeconds = 0;
    if (m_dbReady) {
        QSqlQuery q(m_db);
        q.prepare("SELECT seconds FROM playtime WHERE platform = :p AND platformId = :id");
        q.bindValue(":p",  platform);
        q.bindValue(":id", platformId);
        if (q.exec() && q.next())
            dbSeconds = static_cast<quint64>(q.value(0).toLongLong());
    }

    const auto it = m_activeSessions.constFind(k);
    if (it != m_activeSessions.constEnd()) {
        const qint64 live = it.value().secsTo(QDateTime::currentDateTimeUtc());
        if (live > 0) dbSeconds += static_cast<quint64>(live);
    }

    return dbSeconds;
}

qint64 PlaytimeTracker::lastPlayed(const QString& platform, const QString& platformId) const {
    qint64 dbLast = 0;
    if (m_dbReady) {
        QSqlQuery q(m_db);
        q.prepare("SELECT lastPlayed FROM playtime WHERE platform = :p AND platformId = :id");
        q.bindValue(":p",  platform);
        q.bindValue(":id", platformId);
        if (q.exec() && q.next())
            dbLast = q.value(0).toLongLong();
    }

    const QString k = key(platform, platformId);
    if (m_activeSessions.contains(k)) {
        dbLast = qMax(dbLast, QDateTime::currentSecsSinceEpoch());
    }

    return dbLast;
}

}
