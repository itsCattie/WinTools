#pragma once

// StreamVault: stream model manages model/view data shaping.

#include "modules/StreamVault/src/core/stream_entry.hpp"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVector>

namespace wintools::streamvault {

enum StreamRole {
    StreamEntryRole  = Qt::UserRole + 1,
    MediaTypeRole    = Qt::UserRole + 2,
    PosterPathRole   = Qt::UserRole + 3,
    VoteAverageRole  = Qt::UserRole + 4,
    TmdbIdRole       = Qt::UserRole + 5,
};

class StreamListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit StreamListModel(QObject* parent = nullptr);

    void setResults(const QVector<StreamEntry>& entries);
    void clear();
    void appendResults(const QVector<StreamEntry>& entries);

    const StreamEntry* entryAt(const QModelIndex& idx) const;
    const StreamEntry* entryAt(int row) const;

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;

private:
    QVector<StreamEntry> m_entries;
};

class StreamFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit StreamFilterProxy(QObject* parent = nullptr);

    void setMediaTypeFilter(MediaType type);
    void clearMediaTypeFilter();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    MediaType m_typeFilter           = MediaType::Unknown;
    bool      m_typeFilterActive     = false;
};

}
