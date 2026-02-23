#include "game_model.hpp"

#include <QDateTime>

// GameVault: game model manages model/view data shaping.

namespace wintools::gamevault {

GameListModel::GameListModel(QObject* parent)
    : QAbstractListModel(parent) {}

void GameListModel::setGames(const QVector<GameEntry>& games) {
    beginResetModel();
    m_games = games;
    endResetModel();
}

void GameListModel::addGame(const GameEntry& game) {
    const int row = m_games.size();
    beginInsertRows({}, row, row);
    m_games.append(game);
    endInsertRows();
}

void GameListModel::clear() {
    beginResetModel();
    m_games.clear();
    endResetModel();
}

const GameEntry* GameListModel::entryAt(const QModelIndex& idx) const {
    return entryAt(idx.row());
}

const GameEntry* GameListModel::entryAt(int row) const {
    if (row < 0 || row >= m_games.size()) return nullptr;
    return &m_games[row];
}

int GameListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_games.size();
}

int GameListModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return GameCol::Count;
}

QVariant GameListModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= m_games.size()) return {};
    const GameEntry& e = m_games[idx.row()];

    switch (role) {
    case Qt::DisplayRole:
        switch (idx.column()) {
        case GameCol::Title:        return e.title;
        case GameCol::Platform:     return platformName(e.platform);
        case GameCol::System:       return e.systemTag.isEmpty() ? platformName(e.platform) : e.systemTag;
        case GameCol::Playtime:     return e.playtimeDisplay();
        case GameCol::LastPlayed:
            if (e.lastPlayedEpoch == 0) return "Never";
            return QDateTime::fromSecsSinceEpoch(e.lastPlayedEpoch).toString("dd MMM yyyy");
        case GameCol::Achievements: return e.achievementsDisplay();
        default: return {};
        }

    case Qt::ToolTipRole:
        return QString("%1\n%2\nPlaytime: %3%4")
            .arg(e.title, platformName(e.platform), e.playtimeDisplay(),
                 e.achievementsTotal > 0
                     ? QString("\nAchievements: %1").arg(e.achievementsDisplay())
                     : QString());

    case RawPlaytimeRole:    return QVariant::fromValue(e.playtimeSeconds);
    case RawLastPlayedRole:  return QVariant::fromValue(e.lastPlayedEpoch);
    case PlatformEnumRole:   return static_cast<int>(e.platform);
    case InstalledRole:      return e.installed;
    case AchievementsRole:   return e.achievementsPercent();
    case GameEntryRole:      return QVariant::fromValue(e);

    default:
        return {};
    }
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case GameCol::Title:        return "Title";
    case GameCol::Platform:     return "Platform";
    case GameCol::System:       return "System";
    case GameCol::Playtime:     return "Playtime";
    case GameCol::LastPlayed:   return "Last Played";
    case GameCol::Achievements: return "Achievements";
    default: return {};
    }
}

GameFilterProxy::GameFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void GameFilterProxy::setSearchText(const QString& text) {
    m_search = text.trimmed();
    invalidateFilter();
}

void GameFilterProxy::setPlatformFilter(GamePlatform platform) {
    m_platformFilter       = platform;
    m_platformFilterActive = true;
    invalidateFilter();
}

void GameFilterProxy::clearPlatformFilter() {
    m_platformFilterActive = false;
    invalidateFilter();
}

void GameFilterProxy::setInstalledOnly(bool installed) {
    m_installedOnly = installed;
    invalidateFilter();
}

bool GameFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex titleIdx = src->index(sourceRow, GameCol::Title, sourceParent);

    if (m_installedOnly) {
        if (!src->data(titleIdx, InstalledRole).toBool()) return false;
    }

    if (m_platformFilterActive) {
        const int plat = src->data(titleIdx, PlatformEnumRole).toInt();
        if (plat != static_cast<int>(m_platformFilter)) return false;
    }

    if (!m_search.isEmpty()) {
        const QString title    = src->data(titleIdx, Qt::DisplayRole).toString();
        const QModelIndex platIdx = src->index(sourceRow, GameCol::Platform, sourceParent);
        const QString platStr  = src->data(platIdx, Qt::DisplayRole).toString();
        const QModelIndex sysIdx  = src->index(sourceRow, GameCol::System, sourceParent);
        const QString sysStr   = src->data(sysIdx, Qt::DisplayRole).toString();

        if (!title.contains(m_search, Qt::CaseInsensitive)
            && !platStr.contains(m_search, Qt::CaseInsensitive)
            && !sysStr.contains(m_search, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

}
