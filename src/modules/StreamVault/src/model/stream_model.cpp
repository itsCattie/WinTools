#include "stream_model.hpp"

namespace wintools::streamvault {

StreamListModel::StreamListModel(QObject* parent) : QAbstractListModel(parent) {}

void StreamListModel::setResults(const QVector<StreamEntry>& entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void StreamListModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void StreamListModel::appendResults(const QVector<StreamEntry>& entries) {
    if (entries.isEmpty()) return;
    const int first = static_cast<int>(m_entries.size());
    const int last  = first + static_cast<int>(entries.size()) - 1;
    beginInsertRows({}, first, last);
    m_entries.append(entries);
    endInsertRows();
}

const StreamEntry* StreamListModel::entryAt(const QModelIndex& idx) const {
    return entryAt(idx.row());
}

const StreamEntry* StreamListModel::entryAt(int row) const {
    if (row < 0 || row >= static_cast<int>(m_entries.size()))
        return nullptr;
    return &m_entries[row];
}

int StreamListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_entries.size());
}

QVariant StreamListModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= static_cast<int>(m_entries.size()))
        return {};

    const StreamEntry& e = m_entries[idx.row()];

    switch (role) {
        case Qt::DisplayRole:       return e.title;
        case StreamEntryRole:       return QVariant::fromValue(e);
        case MediaTypeRole:         return static_cast<int>(e.mediaType);
        case PosterPathRole:        return e.posterPath;
        case VoteAverageRole:       return e.voteAverage;
        case TmdbIdRole:            return e.tmdbId;
        default:                    return {};
    }
}

StreamFilterProxy::StreamFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterRole(Qt::DisplayRole);
}

void StreamFilterProxy::setMediaTypeFilter(MediaType type) {
    m_typeFilter       = type;
    m_typeFilterActive = true;
    invalidateFilter();
}

void StreamFilterProxy::clearMediaTypeFilter() {
    m_typeFilterActive = false;
    invalidateFilter();
}

bool StreamFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!filterRegularExpression().pattern().isEmpty()) {
        const QString title = sourceModel()->data(idx, Qt::DisplayRole).toString();
        if (!title.contains(filterRegularExpression()))
            return false;
    }

    if (m_typeFilterActive) {
        const auto t = static_cast<MediaType>(
            sourceModel()->data(idx, MediaTypeRole).toInt());
        if (t != m_typeFilter)
            return false;
    }

    return true;
}

}
