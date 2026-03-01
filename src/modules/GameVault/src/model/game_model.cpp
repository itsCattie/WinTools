#include "game_model.hpp"
#include "modules/GameVault/src/core/game_tag_store.hpp"

#include <QDateTime>

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
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
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

void GameFilterProxy::setFavouritesOnly(bool on) {
    m_favouritesOnly = on;
    invalidateFilter();
}

void GameFilterProxy::setTagFilter(const QString& tag) {
    m_tagFilter = tag;
    invalidateFilter();
}

void GameFilterProxy::clearTagFilter() {
    m_tagFilter.clear();
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

    if (m_favouritesOnly || !m_tagFilter.isEmpty()) {
        const QVariant v = src->data(titleIdx, GameEntryRole);
        if (v.isValid()) {
            const auto e = v.value<GameEntry>();
            const QString platStr = platformName(e.platform);
            if (m_favouritesOnly && !GameTagStore::instance().isFavourite(platStr, e.platformId))
                return false;
            if (!m_tagFilter.isEmpty() && !GameTagStore::instance().hasTag(platStr, e.platformId, m_tagFilter))
                return false;
        }
    }

    return true;
}

void GameFilterProxy::setSortMode(SortMode mode) {
    m_sortMode = mode;
    invalidate();
    sort(0, (mode == SortByPlaytime || mode == SortByLastPlayed || mode == SortByRecentlyAdded)
             ? Qt::DescendingOrder : Qt::AscendingOrder);
}

bool GameFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    const QAbstractItemModel* src = sourceModel();
    if (!src) return false;

    switch (m_sortMode) {
    case SortByPlatform: {
        const QString lp = src->data(src->index(left.row(), GameCol::Platform), Qt::DisplayRole).toString();
        const QString rp = src->data(src->index(right.row(), GameCol::Platform), Qt::DisplayRole).toString();
        if (lp != rp) return lp.compare(rp, Qt::CaseInsensitive) < 0;

        const QString ln = src->data(src->index(left.row(), GameCol::Title), Qt::DisplayRole).toString();
        const QString rn = src->data(src->index(right.row(), GameCol::Title), Qt::DisplayRole).toString();
        return ln.compare(rn, Qt::CaseInsensitive) < 0;
    }
    case SortByPlaytime: {
        const qint64 lp = src->data(src->index(left.row(), GameCol::Title), RawPlaytimeRole).toLongLong();
        const qint64 rp = src->data(src->index(right.row(), GameCol::Title), RawPlaytimeRole).toLongLong();
        return lp < rp;
    }
    case SortByLastPlayed: {
        const qint64 lp = src->data(src->index(left.row(), GameCol::Title), RawLastPlayedRole).toLongLong();
        const qint64 rp = src->data(src->index(right.row(), GameCol::Title), RawLastPlayedRole).toLongLong();
        return lp < rp;
    }
    case SortByRecentlyAdded: {

        return left.row() < right.row();
    }
    case SortByName:
    default: {
        const QString ln = src->data(src->index(left.row(), GameCol::Title), Qt::DisplayRole).toString();
        const QString rn = src->data(src->index(right.row(), GameCol::Title), Qt::DisplayRole).toString();
        return ln.compare(rn, Qt::CaseInsensitive) < 0;
    }
    }
}

}
