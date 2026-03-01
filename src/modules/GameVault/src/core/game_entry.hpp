#pragma once

#include <QString>
#include <QStringList>
#include <QMetaType>
#include <cstdint>

namespace wintools::gamevault {

enum class GamePlatform {
    Steam,
    EpicGames,
    GOG,
    Xbox,
    RetroArch,
    RPCS3,
    Yuzu,
    Ryujinx,
    Dolphin,
    PCSX2,
    DeSmuME,
    Unknown
};

inline QString platformName(GamePlatform p) {
    switch (p) {
        case GamePlatform::Steam:      return "Steam";
        case GamePlatform::EpicGames:  return "Epic Games";
        case GamePlatform::GOG:        return "GOG";
        case GamePlatform::Xbox:       return "Xbox";
        case GamePlatform::RetroArch:  return "RetroArch";
        case GamePlatform::RPCS3:      return "RPCS3";
        case GamePlatform::Yuzu:       return "Yuzu";
        case GamePlatform::Ryujinx:    return "Ryujinx";
        case GamePlatform::Dolphin:    return "Dolphin";
        case GamePlatform::PCSX2:      return "PCSX2";
        case GamePlatform::DeSmuME:    return "DeSmuME";
        default:                       return "Other";
    }
}

struct GameEntry {

    QString       title;
    GamePlatform  platform  = GamePlatform::Unknown;
    QString       systemTag;
    QString       platformId;

    QString       installPath;
    QString       executablePath;

    QString       launchUri;

    QString       artBannerPath;
    QString       artCapsulePath;
    QString       iconPath;

    QString       artBannerUrl;
    QString       artCapsuleUrl;
    QString       iconUrl;

    quint64       playtimeSeconds = 0;
    qint64        lastPlayedEpoch = 0;

    int           achievementsUnlocked = 0;
    int           achievementsTotal    = 0;

    QString       emulatorPath;
    QStringList   emulatorArgs;

    bool          installed = true;

    bool isNativePC() const {
        return platform == GamePlatform::Steam
            || platform == GamePlatform::EpicGames
            || platform == GamePlatform::GOG
            || platform == GamePlatform::Xbox;
    }

    bool hasLocalArt() const {
        return !artBannerPath.isEmpty() || !artCapsulePath.isEmpty();
    }

    QString playtimeDisplay() const {
        if (playtimeSeconds == 0) return "Never played";
        quint64 h = playtimeSeconds / 3600;
        quint64 m = (playtimeSeconds % 3600) / 60;
        if (h > 0) return QString("%1 h %2 m").arg(h).arg(m, 2, 10, QChar('0'));
        return QString("%1 m").arg(m);
    }

    QString achievementsDisplay() const {
        if (achievementsTotal == 0) return {};
        return QString("%1 / %2").arg(achievementsUnlocked).arg(achievementsTotal);
    }

    int achievementsPercent() const {
        if (achievementsTotal == 0) return 0;
        return static_cast<int>(achievementsUnlocked * 100 / achievementsTotal);
    }
};

}

Q_DECLARE_METATYPE(wintools::gamevault::GameEntry)
