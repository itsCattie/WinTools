#include "steam_scanner.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTextStream>

namespace wintools::gamevault {

namespace {

QHash<QString, QString> parseVdfFlat(const QString& filePath) {
    QHash<QString, QString> out;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (!line.startsWith('"')) continue;
        QStringList tokens;
        int i = 0;
        while (i < line.size()) {
            if (line[i] != '"') { ++i; continue; }
            int j = line.indexOf('"', i + 1);
            if (j < 0) break;
            tokens << line.mid(i + 1, j - i - 1);
            i = j + 1;
        }
        if (tokens.size() >= 2)
            out.insert(tokens[0].toLower(), tokens[1]);
    }
    return out;
}

QStringList findSteamLibraryRoots(const QString& steamPath) {
    QStringList roots;
    const QString defaultApps = steamPath + "/steamapps";
    if (QDir(defaultApps).exists()) roots << defaultApps;

    const QString vdfPath = defaultApps + "/libraryfolders.vdf";
    QFile vdf(vdfPath);
    if (!vdf.open(QIODevice::ReadOnly | QIODevice::Text)) return roots;

    QTextStream in(&vdf);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.startsWith('"')) continue;
        QStringList tokens;
        int i = 0;
        while (i < line.size()) {
            if (line[i] != '"') { ++i; continue; }
            int j = line.indexOf('"', i + 1);
            if (j < 0) break;
            tokens << line.mid(i + 1, j - i - 1);
            i = j + 1;
        }
        if (tokens.size() >= 2 && tokens[0].toLower() == "path") {
            QString p = tokens[1] + "/steamapps";
            if (QDir(p).exists()) roots << p;
        }
    }
    return roots;
}

GameEntry entryFromAcf(const QString& acfPath) {
    GameEntry e;
    const auto kv = parseVdfFlat(acfPath);

    e.platform     = GamePlatform::Steam;
    e.title        = kv.value("name");
    e.platformId   = kv.value("appid");
    e.installPath  = kv.value("installdir");

    const qint64 ptMin = kv.value("playtime_forever", "0").toLongLong();
    e.playtimeSeconds  = static_cast<quint64>(ptMin) * 60;
    e.lastPlayedEpoch  = kv.value("lastplayed", "0").toLongLong();

    const QString appid = e.platformId;
    if (!appid.isEmpty()) {
        e.launchUri     = "steam://rungameid/" + appid;
        e.artBannerUrl  = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + appid + "/header.jpg";
        e.artCapsuleUrl = "https://cdn.cloudflare.steamstatic.com/steam/apps/" + appid + "/library_600x900.jpg";
    }

    return e;
}

struct AchievementCount { int unlocked = 0; int total = 0; };
struct PlaytimeData { quint64 seconds = 0; qint64 lastPlayed = 0; };

QHash<QString, PlaytimeData> parseLocalConfigPlaytime(const QString& localConfigPath) {
    QHash<QString, PlaytimeData> out;
    QFile f(localConfigPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;

    QTextStream in(&f);
    QStringList stack;
    QString pendingKey;

    auto parseQuotedTokens = [](const QString& line) {
        QStringList tokens;
        int i = 0;
        while (i < line.size()) {
            if (line[i] != '"') { ++i; continue; }
            const int j = line.indexOf('"', i + 1);
            if (j < 0) break;
            tokens << line.mid(i + 1, j - i - 1);
            i = j + 1;
        }
        return tokens;
    };

    auto currentAppId = [&stack]() -> QString {
        for (int i = 0; i < stack.size(); ++i) {
            if (stack[i] != "apps") continue;
            for (int j = i + 1; j < stack.size(); ++j) {
                bool ok = false;
                stack[j].toLongLong(&ok);
                if (ok) return stack[j];
            }
        }
        return {};
    };

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line == "{") {
            if (!pendingKey.isEmpty()) {
                stack << pendingKey.toLower();
                pendingKey.clear();
            }
            continue;
        }
        if (line == "}") {
            if (!stack.isEmpty()) stack.removeLast();
            continue;
        }

        if (!line.startsWith('"')) continue;
        const QStringList tokens = parseQuotedTokens(line);
        if (tokens.isEmpty()) continue;

        if (tokens.size() == 1) {
            pendingKey = tokens[0];
            continue;
        }

        const QString key = tokens[0].toLower();
        const QString val = tokens[1];
        const QString appid = currentAppId();
        if (appid.isEmpty()) continue;

        auto& data = out[appid];
        if (key == "playtime") {
            bool ok = false;
            const qint64 minutes = val.toLongLong(&ok);
            if (ok && minutes > 0) {
                const quint64 secs = static_cast<quint64>(minutes) * 60;
                if (secs > data.seconds) data.seconds = secs;
            }
        } else if (key == "lastplayed") {
            bool ok = false;
            const qint64 epoch = val.toLongLong(&ok);
            if (ok && epoch > data.lastPlayed) data.lastPlayed = epoch;
        }
    }

    return out;
}

QHash<QString, AchievementCount> parseAchievementProgressCache(const QString& jsonPath) {
    QHash<QString, AchievementCount> out;
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return out;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return out;

    const QJsonArray mapCache = doc.object().value("mapCache").toArray();
    for (const QJsonValue& item : mapCache) {
        if (!item.isArray()) continue;
        const QJsonArray pair = item.toArray();
        if (pair.size() < 2) continue;

        const QString appid = QString::number(pair[0].toInt());
        const QJsonObject v = pair[1].toObject();
        const int total = v.value("total").toInt(0);
        const int unlocked = v.value("unlocked").toInt(0);
        if (appid.isEmpty() || total <= 0) continue;

        out.insert(appid, AchievementCount{unlocked, total});
    }

    return out;
}

AchievementCount parseLibraryCacheAppJson(const QString& jsonPath) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return {};

    const QJsonArray root = doc.array();
    for (const QJsonValue& pairVal : root) {
        if (!pairVal.isArray()) continue;
        const QJsonArray pair = pairVal.toArray();
        if (pair.size() < 2) continue;
        if (pair.at(0).toString() != "achievements") continue;

        const QJsonObject achRoot = pair.at(1).toObject();
        const QJsonObject data = achRoot.value("data").toObject();
        if (data.isEmpty()) return {};

        int total = data.value("nTotal").toInt(0);
        int unlocked = data.value("nAchieved").toInt(0);

        if (total <= 0) {
            const int unachieved = data.value("vecUnachieved").toArray().size();
            const int achievedHidden = data.value("vecAchievedHidden").toArray().size();
            const int highlight = data.value("vecHighlight").toArray().size();
            total = unachieved + achievedHidden + highlight;
            unlocked = achievedHidden + highlight;
        }

        if (total <= 0) return {};
        if (unlocked > total) unlocked = total;
        return AchievementCount{unlocked, total};
    }

    return {};
}

AchievementCount parseAchievementsJson(const QString& jsonPath) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    const QJsonObject root = doc.object();

    QJsonObject achObj = root["AchievementData"].toObject();
    if (achObj.isEmpty())
        achObj = root["achievements"].toObject();
    if (achObj.isEmpty())
        return {};

    AchievementCount res;
    res.total = achObj.size();
    const bool usesEarned = root.contains("AchievementData");
    for (auto it = achObj.constBegin(); it != achObj.constEnd(); ++it) {
        const QJsonObject ach = it.value().toObject();
        const int flag = usesEarned ? ach["earned"].toInt() : ach["unlocked"].toInt();
        if (flag == 1) ++res.unlocked;
    }
    return res;
}

QHash<QString, AchievementCount> buildAchievementMap(const QString& steamPath) {
    QHash<QString, AchievementCount> map;
    const QDir userdata(steamPath + "/userdata");
    if (!userdata.exists()) return map;

    const QStringList steamids = userdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sid : steamids) {
        const QDir appDir(userdata.absolutePath() + "/" + sid);

        const QString cachePath = appDir.absolutePath() + "/config/librarycache/achievement_progress.json";
        const auto cacheMap = parseAchievementProgressCache(cachePath);
        for (auto it = cacheMap.constBegin(); it != cacheMap.constEnd(); ++it) {
            if (!map.contains(it.key()) || it.value().total > map[it.key()].total)
                map[it.key()] = it.value();
        }

        const QDir libraryCacheDir(appDir.absolutePath() + "/config/librarycache");
        if (libraryCacheDir.exists()) {
            QDirIterator it(libraryCacheDir.absolutePath(), {"*.json"}, QDir::Files);
            while (it.hasNext()) {
                const QString jsonPath = it.next();
                const QFileInfo fi(jsonPath);
                const QString base = fi.completeBaseName();
                bool isNumeric = false;
                base.toLongLong(&isNumeric);
                if (!isNumeric) continue;

                const auto ac = parseLibraryCacheAppJson(jsonPath);
                if (ac.total <= 0) continue;
                if (!map.contains(base) || ac.total > map[base].total)
                    map[base] = ac;
            }
        }

        const QStringList appids = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& appid : appids) {

            bool isNumeric = false;
            appid.toLongLong(&isNumeric);
            if (!isNumeric) continue;

            const QStringList candidates = {
                appDir.absolutePath() + "/" + appid + "/stats/achievements.json",
                appDir.absolutePath() + "/" + appid + "/achievements.json",
                appDir.absolutePath() + "/760/remote/" + appid + "/stats/achievements.json",
                appDir.absolutePath() + "/760/remote/" + appid + "/achievements.json",
                appDir.absolutePath() + "/760/remote/" + appid + "/stats/stats.json"
            };

            bool found = false;
            for (const QString& candidate : candidates) {
                if (!QFile::exists(candidate)) continue;
                const auto ac = parseAchievementsJson(candidate);
                if (ac.total > 0) {

                    if (!map.contains(appid) || ac.total > map[appid].total)
                        map[appid] = ac;
                    found = true;
                    break;
                }
            }

            if (found) continue;

            const QStringList roots = {
                appDir.absolutePath() + "/" + appid,
                appDir.absolutePath() + "/760/remote/" + appid
            };
            for (const QString& root : roots) {
                if (!QDir(root).exists()) continue;
                QDirIterator it(root,
                                {"*achievement*.json", "*achievements*.json", "stats.json"},
                                QDir::Files,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString jsonPath = it.next();
                    const auto ac = parseAchievementsJson(jsonPath);
                    if (ac.total <= 0) continue;
                    if (!map.contains(appid) || ac.total > map[appid].total)
                        map[appid] = ac;
                    found = true;
                    break;
                }
                if (found) break;
            }
        }

        const QDir remoteRoot(appDir.absolutePath() + "/760/remote");
        if (remoteRoot.exists()) {
            const QStringList remoteAppids = remoteRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& appid : remoteAppids) {
                bool isNumeric = false;
                appid.toLongLong(&isNumeric);
                if (!isNumeric) continue;

                const QString remoteAppDir = remoteRoot.absolutePath() + "/" + appid;
                QDirIterator it(remoteAppDir,
                                {"*achievement*.json", "*achievements*.json", "stats.json"},
                                QDir::Files,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString jsonPath = it.next();
                    const auto ac = parseAchievementsJson(jsonPath);
                    if (ac.total <= 0) continue;
                    if (!map.contains(appid) || ac.total > map[appid].total)
                        map[appid] = ac;
                    break;
                }
            }
        }
    }
    return map;
}

QHash<QString, PlaytimeData> buildPlaytimeMap(const QString& steamPath) {
    QHash<QString, PlaytimeData> map;
    const QDir userdata(steamPath + "/userdata");
    if (!userdata.exists()) return map;

    const QStringList steamids = userdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sid : steamids) {
        const QString localConfigPath = userdata.absolutePath() + "/" + sid + "/config/localconfig.vdf";
        const auto userMap = parseLocalConfigPlaytime(localConfigPath);
        for (auto it = userMap.constBegin(); it != userMap.constEnd(); ++it) {
            auto& target = map[it.key()];
            if (it.value().seconds > target.seconds) target.seconds = it.value().seconds;
            if (it.value().lastPlayed > target.lastPlayed) target.lastPlayed = it.value().lastPlayed;
        }
    }

    return map;
}

}

QVector<GameEntry> SteamScanner::scan() const {
    QVector<GameEntry> results;

#ifdef Q_OS_WIN
    QSettings reg(R"(HKEY_CURRENT_USER\Software\Valve\Steam)", QSettings::NativeFormat);
    QString steamPath = reg.value("SteamPath").toString();
#else
    QString steamPath;
#if defined(Q_OS_MACOS)
    const QString macSteam = QDir::homePath() + "/Library/Application Support/Steam";
    if (QDir(macSteam).exists()) steamPath = macSteam;
#elif defined(Q_OS_LINUX)
    for (const QString& p : {
             QDir::homePath() + "/.steam/steam",
             QDir::homePath() + "/.local/share/Steam"
         }) {
        if (QDir(p).exists()) { steamPath = p; break; }
    }
#endif
#endif
    if (steamPath.isEmpty()) {
        steamPath = QDir::homePath() + "/.steam/root";
        if (!QDir(steamPath).exists())
            return results;
    }
    steamPath = QDir::fromNativeSeparators(steamPath);

    const QStringList roots = findSteamLibraryRoots(steamPath);

    for (const QString& appsDir : roots) {
        QDirIterator it(appsDir, {"appmanifest_*.acf"}, QDir::Files);
        while (it.hasNext()) {
            const QString acfPath = it.next();
            GameEntry e = entryFromAcf(acfPath);
            if (e.title.isEmpty()) continue;

            if (!e.installPath.isEmpty())
                e.installPath = appsDir + "/common/" + e.installPath;

            results.append(std::move(e));
        }
    }

    const auto achMap = buildAchievementMap(steamPath);
    const auto ptMap  = buildPlaytimeMap(steamPath);
    for (GameEntry& e : results) {
        if (achMap.contains(e.platformId)) {
            const auto& ac   = achMap[e.platformId];
            e.achievementsUnlocked = ac.unlocked;
            e.achievementsTotal    = ac.total;
        }
        if (ptMap.contains(e.platformId)) {
            const auto& pt = ptMap[e.platformId];
            if (pt.seconds > e.playtimeSeconds) e.playtimeSeconds = pt.seconds;
            if (pt.lastPlayed > e.lastPlayedEpoch) e.lastPlayedEpoch = pt.lastPlayed;
        }
    }

    return results;
}

}
