#pragma once

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

namespace wintools::ui { class TrayManager; }

namespace wintools::streamvault {

struct CachedShowState {
    int lastKnownSeasonCount  = 0;
    int lastKnownEpisodeCount = 0;
    QDateTime lastChecked;
};

class ContentNotificationService : public QObject {
    Q_OBJECT
public:
    explicit ContentNotificationService(wintools::ui::TrayManager* tray,
                                         QObject* parent = nullptr);

    void start(int intervalMinutes = 60);

    void stop();

    void checkNow();

    bool isRunning() const;

signals:

    void newContentDetected(int tmdbId, const QString& title,
                            const QString& description);

private slots:
    void onPollTick();

private:
    void checkNextItem();
    void onDetailReply(QNetworkReply* reply, int tmdbId, const QString& title);
    void loadCache();
    void saveCache();
    QString apiKey() const;

    wintools::ui::TrayManager*  m_tray = nullptr;
    QNetworkAccessManager*      m_nam  = nullptr;
    QTimer                      m_timer;

    struct PollItem {
        int     tmdbId;
        int     mediaType;
        QString title;
    };
    QVector<PollItem>                m_queue;
    int                              m_queueIndex = 0;
    QHash<int, CachedShowState>      m_cache;
    bool                             m_running = false;
};

}
