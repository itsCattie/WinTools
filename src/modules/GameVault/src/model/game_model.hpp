#pragma once

// GameVault: game model manages model/view data shaping.

#include "modules/GameVault/src/core/game_entry.hpp"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVector>

namespace wintools::gamevault {

namespace GameCol {
    inline constexpr int Title        = 0;
    inline constexpr int Platform     = 1;
    inline constexpr int System       = 2;
    inline constexpr int Playtime     = 3;
    inline constexpr int LastPlayed   = 4;
    inline constexpr int Achievements = 5;
    inline constexpr int Count        = 6;
}

enum GameRole {
    RawPlaytimeRole     = Qt::UserRole + 1,
    RawLastPlayedRole   = Qt::UserRole + 2,
    PlatformEnumRole    = Qt::UserRole + 3,
    GameEntryRole       = Qt::UserRole + 4,
    InstalledRole       = Qt::UserRole + 5,
    AchievementsRole    = Qt::UserRole + 6,
};

class GameListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit GameListModel(QObject* parent = nullptr);

    void setGames(const QVector<GameEntry>& games);
    void addGame(const GameEntry& game);
    void clear();

    const GameEntry* entryAt(const QModelIndex& idx) const;
    const GameEntry* entryAt(int row) const;

    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<GameEntry> m_games;
};

class GameFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit GameFilterProxy(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setPlatformFilter(GamePlatform platform);
    void clearPlatformFilter();
    void setInstalledOnly(bool installed);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString      m_search;
    GamePlatform m_platformFilter = GamePlatform::Unknown;
    bool         m_platformFilterActive = false;
    bool         m_installedOnly  = false;
};

}
