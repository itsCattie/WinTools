#include "modules/GameVault/src/core/game_tag_store.hpp"

#include "logger/logger.hpp"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace wintools::gamevault {

namespace {
constexpr const char* kLog      = "GameVault/TagStore";
constexpr const char* kConnName = "gamevault_tags";
}

GameTagStore& GameTagStore::instance() {
    static GameTagStore s;
    return s;
}

GameTagStore::GameTagStore(QObject* parent)
    : QObject(parent) {
    ensureDb();
}

GameTagStore::~GameTagStore() {
    if (m_dbReady) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(kConnName);
}

void GameTagStore::ensureDb() {
    if (m_dbReady) return;

    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/gamevault_tags.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Failed to open tag database.", m_db.lastError().text());
        return;
    }

    QSqlQuery q(m_db);
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS game_tags ("
        "  platform   TEXT NOT NULL,"
        "  platform_id TEXT NOT NULL,"
        "  tag        TEXT NOT NULL,"
        "  PRIMARY KEY (platform, platform_id, tag)"
        ")"));

    if (!ok) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Failed to create game_tags table.", q.lastError().text());
        return;
    }

    m_dbReady = true;
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Tag database ready.", path);
}

void GameTagStore::addTag(const QString& platform, const QString& platformId,
                          const QString& tag) {
    if (!m_dbReady) return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_tags (platform, platform_id, tag) "
        "VALUES (:p, :pid, :t)"));
    q.bindValue(":p", platform);
    q.bindValue(":pid", platformId);
    q.bindValue(":t", tag);
    q.exec();

    emit changed();
}

void GameTagStore::removeTag(const QString& platform, const QString& platformId,
                             const QString& tag) {
    if (!m_dbReady) return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM game_tags WHERE platform = :p AND platform_id = :pid AND tag = :t"));
    q.bindValue(":p", platform);
    q.bindValue(":pid", platformId);
    q.bindValue(":t", tag);
    q.exec();

    emit changed();
}

QStringList GameTagStore::tags(const QString& platform, const QString& platformId) const {
    QStringList result;
    if (!m_dbReady) return result;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT tag FROM game_tags WHERE platform = :p AND platform_id = :pid ORDER BY tag"));
    q.bindValue(":p", platform);
    q.bindValue(":pid", platformId);
    if (q.exec()) {
        while (q.next()) {
            const QString t = q.value(0).toString();
            if (t != QLatin1String(kFavouriteTag))
                result << t;
        }
    }
    return result;
}

bool GameTagStore::hasTag(const QString& platform, const QString& platformId,
                          const QString& tag) const {
    if (!m_dbReady) return false;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM game_tags WHERE platform = :p AND platform_id = :pid AND tag = :t LIMIT 1"));
    q.bindValue(":p", platform);
    q.bindValue(":pid", platformId);
    q.bindValue(":t", tag);
    return q.exec() && q.next();
}

bool GameTagStore::isFavourite(const QString& platform, const QString& platformId) const {
    return hasTag(platform, platformId, QLatin1String(kFavouriteTag));
}

void GameTagStore::setFavourite(const QString& platform, const QString& platformId, bool on) {
    if (on)
        addTag(platform, platformId, QLatin1String(kFavouriteTag));
    else
        removeTag(platform, platformId, QLatin1String(kFavouriteTag));
}

void GameTagStore::toggleFavourite(const QString& platform, const QString& platformId) {
    setFavourite(platform, platformId, !isFavourite(platform, platformId));
}

QStringList GameTagStore::allTags() const {
    QStringList result;
    if (!m_dbReady) return result;

    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT DISTINCT tag FROM game_tags WHERE tag != '__favourite__' ORDER BY tag"))) {
        while (q.next()) {
            result << q.value(0).toString();
        }
    }
    return result;
}

int GameTagStore::favouriteCount() const {
    if (!m_dbReady) return 0;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM game_tags WHERE tag = :t"));
    q.bindValue(":t", QLatin1String(kFavouriteTag));
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

}
