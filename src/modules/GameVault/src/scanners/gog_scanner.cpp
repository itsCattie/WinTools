#include "gog_scanner.hpp"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace wintools::gamevault {

namespace {

QString findGogDb() {
#ifdef Q_OS_WIN
    const QString programData =
        QDir::fromNativeSeparators(qEnvironmentVariable("ProgramData", "C:/ProgramData"));
    const QString db = programData + "/GOG.com/Galaxy/storage/galaxy-2.0.db";
    return QFile::exists(db) ? db : QString();
#elif defined(Q_OS_MACOS)
    const QString db = QDir::homePath()
        + "/Library/Application Support/GOG.com/Galaxy/storage/galaxy-2.0.db";
    return QFile::exists(db) ? db : QString();
#elif defined(Q_OS_LINUX)

    const QString db = QDir::homePath() + "/.local/share/gog/galaxy-2.0.db";
    return QFile::exists(db) ? db : QString();
#else
    return {};
#endif
}

}

QVector<GameEntry> GogScanner::scan() const {
    QVector<GameEntry> results;

    const QString dbPath = findGogDb();
    if (dbPath.isEmpty()) return results;

    const QString connName = "gamevault_gog";
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(dbPath);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connName);
            return results;
        }

        QHash<QString, QString> installPaths;
        {
            QSqlQuery q(db);
            q.exec("SELECT productId, installationPath FROM InstalledProducts");
            while (q.next())
                installPaths.insert(q.value(0).toString(),
                                    QDir::fromNativeSeparators(q.value(1).toString()));
        }

        QHash<QString, quint64> playtimes;
        {
            QSqlQuery q(db);
            q.exec("SELECT gameId, SUM(duration) FROM PlaySessions GROUP BY gameId");
            while (q.next())
                playtimes.insert(q.value(0).toString(),
                                 static_cast<quint64>(q.value(1).toLongLong()));
        }

        QHash<QString, QString> titles;
        QHash<QString, QString> verticalCovers;
        {
            QSqlQuery q(db);

            q.exec(R"(
                SELECT releaseKey, value
                FROM GamePieces
                WHERE gamePieceTypeId = 'originalTitle'
                   OR gamePieceTypeId = 'title'
            )");
            while (q.next()) {
                const QString key = q.value(0).toString();
                if (!titles.contains(key))
                    titles.insert(key, q.value(1).toString().remove('"'));
            }

            q.exec(R"(
                SELECT releaseKey, value
                FROM GamePieces
                WHERE gamePieceTypeId = 'verticalCover'
            )");
            while (q.next()) {
                const QString key = q.value(0).toString();
                if (!verticalCovers.contains(key))
                    verticalCovers.insert(key, q.value(1).toString().remove('"'));
            }
        }

        db.close();

        for (auto it = titles.constBegin(); it != titles.constEnd(); ++it) {
            const QString& releaseKey = it.key();
            GameEntry e;
            e.platform        = GamePlatform::GOG;
            e.title           = it.value();
            e.platformId      = releaseKey;
            e.installPath     = installPaths.value(releaseKey);
            e.installed       = !e.installPath.isEmpty();
            e.playtimeSeconds = playtimes.value(releaseKey, 0);
            e.artCapsuleUrl   = verticalCovers.value(releaseKey);

            e.launchUri = "goggalaxy://openGame/" + releaseKey;

            results.append(std::move(e));
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return results;
}

}
