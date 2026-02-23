#include "game_library.hpp"
#include "logger/logger.hpp"

#include "../scanners/steam_scanner.hpp"
#include "../scanners/epic_scanner.hpp"
#include "../scanners/gog_scanner.hpp"
#include "../scanners/retroarch_scanner.hpp"
#include "../scanners/emulator_scanners.hpp"
#include "../scanners/custom_folder_scanner.hpp"

#include <algorithm>
#include <memory>
#include <QVector>

// GameVault: game library manages core logic and state.

namespace wintools::gamevault {

static constexpr const char* kLog = "GameVault/Library";

QVector<GameEntry> GameLibrary::scan() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Starting full library scan across all platforms.");
    std::vector<std::unique_ptr<IGameScanner>> scanners;
    scanners.push_back(std::make_unique<SteamScanner>());
    scanners.push_back(std::make_unique<EpicScanner>());
    scanners.push_back(std::make_unique<GogScanner>());
    scanners.push_back(std::make_unique<RetroArchScanner>());
    scanners.push_back(std::make_unique<Rpcs3Scanner>());
    scanners.push_back(std::make_unique<YuzuScanner>());
    scanners.push_back(std::make_unique<RyujinxScanner>());
    scanners.push_back(std::make_unique<DolphinScanner>());
    scanners.push_back(std::make_unique<DeSmuMEScanner>());
    scanners.push_back(std::make_unique<CustomFolderScanner>());

    QVector<GameEntry> all;
    for (const auto& s : scanners) {
        const auto results = s->scan();
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Scanner completed."),
            QStringLiteral("%1 entries found").arg(results.size()));
        all.append(results);
    }

    std::sort(all.begin(), all.end(), [](const GameEntry& a, const GameEntry& b) {
        if (a.installed != b.installed) return a.installed > b.installed;
        return a.title.toLower() < b.title.toLower();
    });

    {
        QHash<QString, int> seen;
        QVector<GameEntry> deduped;
        deduped.reserve(all.size());
        for (int i = 0; i < all.size(); ++i) {
            const QString key = all[i].title.toLower().trimmed()
                + '|' + QString::number(static_cast<int>(all[i].platform));
            auto it = seen.find(key);
            if (it == seen.end()) {
                seen.insert(key, deduped.size());
                deduped.append(all[i]);
            } else {

                auto& existing = deduped[it.value()];
                const bool existingHasAnyArt = existing.hasLocalArt()
                    || !existing.artBannerUrl.isEmpty()
                    || !existing.artCapsuleUrl.isEmpty();
                const bool newHasAnyArt = all[i].hasLocalArt()
                    || !all[i].artBannerUrl.isEmpty()
                    || !all[i].artCapsuleUrl.isEmpty();

                if (!existing.installed && all[i].installed)
                    existing.installed = true;

                if (!existingHasAnyArt && newHasAnyArt) {
                    existing.artBannerUrl = all[i].artBannerUrl;
                    existing.artCapsuleUrl = all[i].artCapsuleUrl;
                    existing.artBannerPath = all[i].artBannerPath;
                    existing.artCapsulePath = all[i].artCapsulePath;
                    existing.iconUrl = all[i].iconUrl;
                    existing.iconPath = all[i].iconPath;
                }

                if (all[i].playtimeSeconds > existing.playtimeSeconds)
                    existing.playtimeSeconds = all[i].playtimeSeconds;
                if (all[i].lastPlayedEpoch > existing.lastPlayedEpoch)
                    existing.lastPlayedEpoch = all[i].lastPlayedEpoch;

                if (all[i].achievementsTotal > existing.achievementsTotal
                    || (all[i].achievementsTotal == existing.achievementsTotal
                        && all[i].achievementsUnlocked > existing.achievementsUnlocked)) {
                    existing.achievementsTotal = all[i].achievementsTotal;
                    existing.achievementsUnlocked = all[i].achievementsUnlocked;
                }

                if (existing.executablePath.isEmpty() && !all[i].executablePath.isEmpty())
                    existing.executablePath = all[i].executablePath;
                if (existing.installPath.isEmpty() && !all[i].installPath.isEmpty())
                    existing.installPath = all[i].installPath;
                if (existing.launchUri.isEmpty() && !all[i].launchUri.isEmpty())
                    existing.launchUri = all[i].launchUri;
                if (existing.platformId.isEmpty() && !all[i].platformId.isEmpty())
                    existing.platformId = all[i].platformId;
            }
        }
        all = std::move(deduped);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Library scan finished."),
        QStringLiteral("Total %1 games").arg(all.size()));
    return all;
}

GameLibraryWorker::GameLibraryWorker(QObject* parent)
    : QObject(parent) {
    setAutoDelete(true);
}

void GameLibraryWorker::run() {
    try {
        emit scanComplete(GameLibrary::scan());
    } catch (const std::exception& ex) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Library scan exception.", QString::fromLocal8Bit(ex.what()));
        emit scanError(QString::fromLocal8Bit(ex.what()));
    } catch (...) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
            "Library scan unknown exception.");
        emit scanError("Unknown error during game library scan.");
    }
}

}
