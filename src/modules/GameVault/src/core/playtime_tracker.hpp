#pragma once

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QSqlDatabase>
#include <QTimer>

namespace wintools::gamevault {

class PlaytimeTracker : public QObject {
    Q_OBJECT

public:
    static PlaytimeTracker& instance();

    void startSession(const QString& platform, const QString& platformId);

    void endSession(const QString& platform, const QString& platformId);

    quint64 playtime(const QString& platform, const QString& platformId) const;

    qint64 lastPlayed(const QString& platform, const QString& platformId) const;

private:
    explicit PlaytimeTracker(QObject* parent = nullptr);
    ~PlaytimeTracker() override;

    void ensureDb();
    QString key(const QString& platform, const QString& platformId) const;

    QSqlDatabase m_db;
    bool m_dbReady = false;

    QHash<QString, QDateTime> m_activeSessions;
};

}
