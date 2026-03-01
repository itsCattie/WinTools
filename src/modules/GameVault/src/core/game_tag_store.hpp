#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace wintools::gamevault {

class GameTagStore : public QObject {
    Q_OBJECT

public:
    static GameTagStore& instance();

    void        addTag(const QString& platform, const QString& platformId, const QString& tag);
    void        removeTag(const QString& platform, const QString& platformId, const QString& tag);
    QStringList tags(const QString& platform, const QString& platformId) const;
    bool        hasTag(const QString& platform, const QString& platformId, const QString& tag) const;

    bool isFavourite(const QString& platform, const QString& platformId) const;
    void setFavourite(const QString& platform, const QString& platformId, bool on);
    void toggleFavourite(const QString& platform, const QString& platformId);

    QStringList allTags() const;

    int favouriteCount() const;

    static constexpr const char* kFavouriteTag = "__favourite__";

signals:
    void changed();

private:
    explicit GameTagStore(QObject* parent = nullptr);
    ~GameTagStore() override;

    void ensureDb();

    QSqlDatabase m_db;
    bool m_dbReady = false;
};

}
