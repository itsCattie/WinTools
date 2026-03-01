#pragma once

#include "game_entry.hpp"

#include <QString>
#include <QStringList>
#include <QVector>

namespace wintools::gamevault {

class GameVaultSettings {
public:
    static GameVaultSettings& instance();

    QStringList customGameFolders() const;
    void        setCustomGameFolders(const QStringList& folders);

    void        addCustomGameFolder(const QString& path);
    void        removeCustomGameFolder(const QString& path);

    QString emulatorPath(const QString& name) const;
    void    setEmulatorPath(const QString& name, const QString& path);
    void    clearEmulatorPath(const QString& name);

    QStringList emulatorOverrideNames() const;

    QString customArtPath(const QString& entryKey) const;
    void    setCustomArtPath(const QString& entryKey, const QString& path);
    void    clearCustomArtPath(const QString& entryKey);

    QVector<GameEntry> manualGames() const;
    void               setManualGames(const QVector<GameEntry>& games);
    void               addOrUpdateManualGame(const GameEntry& game);

    QString gameExecutableOverridePath(const QString& locatorKey) const;

    QString gameTrackingIdOverride(const QString& locatorKey) const;

    void    setGameExecutableOverride(const QString& locatorKey,
                                      const QString& executablePath,
                                      const QString& trackingId);

    void    clearGameExecutableOverride(const QString& locatorKey);

    QString steamGridDbApiKey() const;
    void    setSteamGridDbApiKey(const QString& key);

private:
    GameVaultSettings() = default;
    GameVaultSettings(const GameVaultSettings&) = delete;
    GameVaultSettings& operator=(const GameVaultSettings&) = delete;
};

}
