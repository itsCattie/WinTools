#pragma once

#include "modules/StreamVault/src/core/stream_entry.hpp"

#include <QObject>
#include <QSqlDatabase>
#include <QVector>

namespace wintools::streamvault {

class WatchlistStore : public QObject {
    Q_OBJECT

public:
    static WatchlistStore& instance();

    bool contains(int tmdbId) const;
    void add(const StreamEntry& entry);
    void remove(int tmdbId);
    void toggle(const StreamEntry& entry);

    QVector<StreamEntry> all() const;
    int count() const;

signals:
    void changed();

private:
    explicit WatchlistStore(QObject* parent = nullptr);
    ~WatchlistStore() override;

    void ensureDb();

    QSqlDatabase m_db;
    bool m_dbReady = false;
};

}
