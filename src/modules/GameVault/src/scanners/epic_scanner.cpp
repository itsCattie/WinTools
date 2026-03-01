#include "epic_scanner.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace wintools::gamevault {

namespace {

int firstInt(const QJsonObject& obj, const QStringList& keys, int fallback = 0) {
    for (const QString& key : keys) {
        if (!obj.contains(key)) continue;
        const QJsonValue v = obj.value(key);
        bool ok = false;
        const int n = (v.isString() ? v.toString().toInt(&ok) : v.toInt(fallback));
        if (v.isDouble() || ok) return n;
    }
    return fallback;
}

void collectImageUrls(const QJsonValue& value, QStringList& out) {
    if (value.isString()) {
        const QString s = value.toString();
        const QString lower = s.toLower();
        const bool isImage = lower.startsWith("http")
            && (lower.contains(".jpg") || lower.contains(".jpeg") || lower.contains(".png")
                || lower.contains(".webp") || lower.contains("images") || lower.contains("image")
                || lower.contains("dieselstorefront") || lower.contains("dieselgamebox"));
        if (isImage) out << s;

        static const QRegularExpression httpUrlRe(R"((https?://[^\s\"'<>]+))",
                                                  QRegularExpression::CaseInsensitiveOption);
        auto matches = httpUrlRe.globalMatch(s);
        while (matches.hasNext()) {
            const QString url = matches.next().captured(1);
            const QString lu = url.toLower();
            if (lu.contains(".jpg") || lu.contains(".jpeg") || lu.contains(".png")
                || lu.contains(".webp") || lu.contains("image") || lu.contains("diesel")) {
                out << url;
            }
        }
        return;
    }

    if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        for (const QJsonValue& item : arr) collectImageUrls(item, out);
        return;
    }

    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            collectImageUrls(it.value(), out);
    }
}

void applyBestImageGuess(GameEntry& e, const QString& url) {
    if (url.isEmpty()) return;
    const QString lu = url.toLower();
    if ((lu.contains("tall") || lu.contains("portrait") || lu.contains("capsule")
         || lu.contains("dieselgamebox")) && e.artCapsuleUrl.isEmpty()) {
        e.artCapsuleUrl = url;
    }
    if ((lu.contains("wide") || lu.contains("banner") || lu.contains("header")
         || lu.contains("thumbnail") || lu.contains("dieselstorefront")) && e.artBannerUrl.isEmpty()) {
        e.artBannerUrl = url;
    }
    if (e.artBannerUrl.isEmpty()) e.artBannerUrl = url;
}

void applyImageByType(GameEntry& e, const QString& type, const QString& url) {
    if (url.isEmpty()) return;
    const QString t = type.toLower();
    if (t.contains("tall") || t.contains("portrait") || t.contains("capsule")) {
        if (e.artCapsuleUrl.isEmpty()) e.artCapsuleUrl = url;
        return;
    }
    if (t.contains("wide") || t.contains("banner") || t.contains("dieselgamebox") || t.contains("thumbnail")) {
        if (e.artBannerUrl.isEmpty()) e.artBannerUrl = url;
        return;
    }
    if (e.artBannerUrl.isEmpty()) e.artBannerUrl = url;
}

QString findEpicManifestDir() {
#ifdef Q_OS_WIN
    QSettings reg(R"(HKEY_LOCAL_MACHINE\SOFTWARE\Epic Games\EpicGamesLauncher)",
                  QSettings::NativeFormat);
    QString path = reg.value("AppDataPath").toString();
    if (path.isEmpty()) {
        QSettings reg32(R"(HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Epic Games\EpicGamesLauncher)",
                        QSettings::NativeFormat);
        path = reg32.value("AppDataPath").toString();
    }
    if (!path.isEmpty()) {
        path = QDir::fromNativeSeparators(path) + "/Manifests";
        if (QDir(path).exists()) return path;
    }

    const QString fb = R"(C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests)";
    return QDir(fb).exists() ? fb : QString();
#elif defined(Q_OS_MACOS)
    const QString macPath = QDir::homePath()
        + "/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests";
    return QDir(macPath).exists() ? macPath : QString();
#elif defined(Q_OS_LINUX)

    const QString heroicPath = QDir::homePath() + "/.config/heroic/store_cache";
    return QDir(heroicPath).exists() ? heroicPath : QString();
#else
    return {};
#endif
}

GameEntry entryFromItem(const QString& itemPath) {
    GameEntry e;
    QFile f(itemPath);
    if (!f.open(QIODevice::ReadOnly)) return e;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

    e.platform    = GamePlatform::EpicGames;
    e.title       = obj["DisplayName"].toString();
    e.platformId  = obj["CatalogItemId"].toString();
    if (e.platformId.isEmpty()) e.platformId = obj["AppName"].toString();
    if (e.platformId.isEmpty()) e.platformId = obj["MainGameCatalogNamespace"].toString();
    e.installPath = QDir::fromNativeSeparators(obj["InstallLocation"].toString());

    if (!e.platformId.isEmpty())
        e.launchUri = "com.epicgames.launcher://apps/" + e.platformId + "?action=launch&silent=true";

    const QString exeFile = obj["LaunchExecutable"].toString();
    if (!e.installPath.isEmpty() && !exeFile.isEmpty())
        e.executablePath = e.installPath + "/" + exeFile;

    if (!e.executablePath.isEmpty() && !QFile::exists(e.executablePath)) {
        const QString wanted = QFileInfo(exeFile).fileName().toLower();
        if (!wanted.isEmpty() && QDir(e.installPath).exists()) {
            QDirIterator exeIt(e.installPath, {"*.exe"}, QDir::Files, QDirIterator::Subdirectories);
            while (exeIt.hasNext()) {
                const QString candidate = QDir::fromNativeSeparators(exeIt.next());
                if (QFileInfo(candidate).fileName().toLower() == wanted) {
                    e.executablePath = candidate;
                    break;
                }
            }
        }
    }

    if (e.executablePath.isEmpty() && QDir(e.installPath).exists()) {
        QDirIterator exeIt(e.installPath, {"*.exe"}, QDir::Files, QDirIterator::Subdirectories);
        if (exeIt.hasNext()) {
            e.executablePath = QDir::fromNativeSeparators(exeIt.next());
        }
    }

    if (!e.executablePath.isEmpty() && QFile::exists(e.executablePath)) {
        e.iconPath = e.executablePath;
    }

    const QString vaultThumb = obj.value("VaultThumbnailUrl").toString().trimmed();
    if (vaultThumb.startsWith("http", Qt::CaseInsensitive)) {
        applyImageByType(e, QStringLiteral("vaultthumbnail"), vaultThumb);
    }

    const QJsonArray keyImages = obj["KeyImages"].toArray();
    for (const auto& img : keyImages) {
        const QJsonObject ki = img.toObject();
        applyImageByType(e, ki["Type"].toString(), ki["Url"].toString());
    }

    const QJsonObject customAttrs = obj["CustomAttributes"].toObject();
    for (auto it = customAttrs.constBegin(); it != customAttrs.constEnd(); ++it) {
        const QJsonObject attrObj = it.value().toObject();
        const QString value = attrObj.value("Value").toString();
        if (value.isEmpty()) continue;

        if (value.startsWith("http", Qt::CaseInsensitive)) {
            applyImageByType(e, it.key(), value);
            continue;
        }

        QStringList urls;
        collectImageUrls(QJsonValue(value), urls);
        urls.removeDuplicates();
        for (const QString& url : urls) {
            applyImageByType(e, it.key(), url);
        }

        const QJsonDocument nested = QJsonDocument::fromJson(value.toUtf8());
        if (nested.isObject() || nested.isArray()) {
            QStringList nestedUrls;
            collectImageUrls(nested.isObject() ? QJsonValue(nested.object()) : QJsonValue(nested.array()), nestedUrls);
            nestedUrls.removeDuplicates();
            for (const QString& url : nestedUrls) {
                applyImageByType(e, it.key(), url);
            }
        }
    }

    if (e.artBannerUrl.isEmpty() && e.artCapsuleUrl.isEmpty()) {
        QStringList imageUrls;
        collectImageUrls(obj, imageUrls);
        imageUrls.removeDuplicates();
        for (const QString& url : imageUrls) {
            applyBestImageGuess(e, url);
            if (!e.artBannerUrl.isEmpty() && !e.artCapsuleUrl.isEmpty()) break;
        }
    }

    e.achievementsTotal = firstInt(obj, {
        "AchievementCount", "AchievementsTotal", "TotalAchievements"
    }, 0);
    e.achievementsUnlocked = firstInt(obj, {
        "AchievementUnlockedCount", "AchievementsUnlocked", "UnlockedAchievements"
    }, 0);
    if (e.achievementsUnlocked > e.achievementsTotal) {
        e.achievementsUnlocked = e.achievementsTotal;
    }

    const int playSeconds = firstInt(obj, {
        "PlaytimeSeconds", "TotalPlaytimeSeconds", "TimePlayedSeconds"
    }, -1);
    const int playMinutes = firstInt(obj, {
        "PlaytimeMinutes", "TotalPlaytime", "MinutesPlayed", "PlayTime"
    }, -1);
    if (playSeconds > 0) {
        e.playtimeSeconds = static_cast<quint64>(playSeconds);
    } else if (playMinutes > 0) {
        e.playtimeSeconds = static_cast<quint64>(playMinutes) * 60;
    }

    const int lastPlayed = firstInt(obj, {
        "LastPlayed", "LastPlayedEpoch", "LastPlayedTime"
    }, 0);
    if (lastPlayed > 0) {
        e.lastPlayedEpoch = static_cast<qint64>(lastPlayed);
    }

    if (!e.installPath.isEmpty() && QDir(e.installPath).exists()
        && (e.artBannerUrl.isEmpty() || e.artCapsuleUrl.isEmpty())) {
        QDirIterator imgIt(e.installPath,
                           {"*.jpg", "*.jpeg", "*.png", "*.webp"},
                           QDir::Files,
                           QDirIterator::Subdirectories);
        while (imgIt.hasNext()) {
            const QString localPath = QDir::fromNativeSeparators(imgIt.next());
            const QString name = QFileInfo(localPath).fileName().toLower();
            const QString localUrl = QUrl::fromLocalFile(localPath).toString();
            if ((name.contains("cover") || name.contains("capsule") || name.contains("portrait")
                 || name.contains("box")) && e.artCapsuleUrl.isEmpty()) {
                e.artCapsuleUrl = localUrl;
            }
            if ((name.contains("banner") || name.contains("wide") || name.contains("header")
                 || name.contains("thumbnail")) && e.artBannerUrl.isEmpty()) {
                e.artBannerUrl = localUrl;
            }
            if (e.artBannerUrl.isEmpty()) e.artBannerUrl = localUrl;
            if (!e.artBannerUrl.isEmpty() && !e.artCapsuleUrl.isEmpty()) break;
        }
    }

    return e;
}

}

QVector<GameEntry> EpicScanner::scan() const {
    QVector<GameEntry> results;

    const QString manifestDir = findEpicManifestDir();
    if (manifestDir.isEmpty()) return results;

    QDirIterator it(manifestDir, {"*.item"}, QDir::Files);
    while (it.hasNext()) {
        const QString itemPath = it.next();
        GameEntry e = entryFromItem(itemPath);
        if (e.title.isEmpty()) continue;
        results.append(std::move(e));
    }

    return results;
}

}
