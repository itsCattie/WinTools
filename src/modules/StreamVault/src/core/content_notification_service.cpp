#include "content_notification_service.hpp"
#include "streamvault_settings.hpp"
#include "watchlist_store.hpp"
#include "common/tray/tray_manager.hpp"
#include "logger/logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace wintools::streamvault {

static constexpr const char* kLog = "StreamVault/Notifications";

ContentNotificationService::ContentNotificationService(
    wintools::ui::TrayManager* tray, QObject* parent)
    : QObject(parent)
    , m_tray(tray)
    , m_nam(new QNetworkAccessManager(this))
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &ContentNotificationService::onPollTick);
    loadCache();
}

void ContentNotificationService::start(int intervalMinutes) {
    if (intervalMinutes < 1) intervalMinutes = 60;
    m_timer.setInterval(intervalMinutes * 60 * 1000);
    m_running = true;
    m_timer.start();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Notification polling started."),
        QStringLiteral("interval=%1 min").arg(intervalMinutes));

    QTimer::singleShot(5000, this, &ContentNotificationService::onPollTick);
}

void ContentNotificationService::stop() {
    m_timer.stop();
    m_running = false;
    m_queue.clear();
    m_queueIndex = 0;
}

void ContentNotificationService::checkNow() {
    onPollTick();
}

bool ContentNotificationService::isRunning() const { return m_running; }

void ContentNotificationService::onPollTick() {
    const QString key = apiKey();
    if (key.isEmpty()) return;

    const auto items = WatchlistStore::instance().all();
    m_queue.clear();
    m_queueIndex = 0;
    for (const auto& e : items) {
        PollItem pi;
        pi.tmdbId    = e.tmdbId;
        pi.mediaType = (e.mediaType == MediaType::TvShow) ? 1 : 0;
        pi.title     = e.title;
        m_queue.append(pi);
    }

    if (m_queue.isEmpty()) return;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Poll cycle started."),
        QStringLiteral("items=%1").arg(m_queue.size()));

    checkNextItem();
}

void ContentNotificationService::checkNextItem() {
    if (m_queueIndex >= m_queue.size()) {

        saveCache();
        return;
    }

    const PollItem& item = m_queue[m_queueIndex];
    const QString key = apiKey();

    const QString endpoint = (item.mediaType == 1)
        ? QStringLiteral("https://api.themoviedb.org/3/tv/%1").arg(item.tmdbId)
        : QStringLiteral("https://api.themoviedb.org/3/movie/%1").arg(item.tmdbId);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api_key"), key);
    QUrl url(endpoint);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools/1.0");

    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, item]() {
        onDetailReply(reply, item.tmdbId, item.title);
    });
}

void ContentNotificationService::onDetailReply(
    QNetworkReply* reply, int tmdbId, const QString& title)
{
    reply->deleteLater();

    const bool isTvShow = [&]() {
        for (const auto& pi : std::as_const(m_queue)) {
            if (pi.tmdbId == tmdbId) return pi.mediaType == 1;
        }
        return false;
    }();

    if (reply->error() != QNetworkReply::NoError) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("TMDB detail request failed."),
            QStringLiteral("id=%1 err=%2").arg(tmdbId).arg(reply->errorString()));

        ++m_queueIndex;

        QTimer::singleShot(250, this, &ContentNotificationService::checkNextItem);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject   obj = doc.object();

    if (isTvShow) {
        const int seasons  = obj.value("number_of_seasons").toInt();
        const int episodes = obj.value("number_of_episodes").toInt();

        auto it = m_cache.find(tmdbId);
        if (it == m_cache.end()) {

            CachedShowState state;
            state.lastKnownSeasonCount  = seasons;
            state.lastKnownEpisodeCount = episodes;
            state.lastChecked           = QDateTime::currentDateTimeUtc();
            m_cache.insert(tmdbId, state);
        } else {
            CachedShowState& state = it.value();
            bool hasNew = false;
            QString desc;

            if (seasons > state.lastKnownSeasonCount) {
                hasNew = true;
                desc = QStringLiteral("New season available! (Season %1)").arg(seasons);
            } else if (episodes > state.lastKnownEpisodeCount) {
                hasNew = true;
                const int newEps = episodes - state.lastKnownEpisodeCount;
                desc = QStringLiteral("%1 new episode%2 available.")
                           .arg(newEps).arg(newEps > 1 ? "s" : "");
            }

            state.lastKnownSeasonCount  = seasons;
            state.lastKnownEpisodeCount = episodes;
            state.lastChecked           = QDateTime::currentDateTimeUtc();

            if (hasNew) {
                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                    QStringLiteral("New content detected."),
                    QStringLiteral("%1: %2").arg(title, desc));

                if (m_tray)
                    m_tray->showMessage(QStringLiteral("StreamVault — %1").arg(title),
                                        desc);
                emit newContentDetected(tmdbId, title, desc);
            }
        }
    } else {

        const QString status  = obj.value("status").toString();
        const QString relDate = obj.value("release_date").toString();

        auto it = m_cache.find(tmdbId);
        if (it == m_cache.end()) {
            CachedShowState state;
            state.lastKnownSeasonCount  = 0;
            state.lastKnownEpisodeCount = (status == "Released") ? 1 : 0;
            state.lastChecked           = QDateTime::currentDateTimeUtc();
            m_cache.insert(tmdbId, state);
        } else {
            CachedShowState& state = it.value();
            const bool wasReleased = (state.lastKnownEpisodeCount == 1);
            const bool isReleased  = (status == "Released");

            if (!wasReleased && isReleased) {
                QString desc = QStringLiteral("Now released!");
                if (!relDate.isEmpty())
                    desc = QStringLiteral("Released on %1.").arg(relDate);

                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                    QStringLiteral("Movie release detected."),
                    QStringLiteral("%1: %2").arg(title, desc));

                if (m_tray)
                    m_tray->showMessage(QStringLiteral("StreamVault — %1").arg(title),
                                        desc);
                emit newContentDetected(tmdbId, title, desc);
            }

            state.lastKnownEpisodeCount = isReleased ? 1 : 0;
            state.lastChecked           = QDateTime::currentDateTimeUtc();
        }
    }

    ++m_queueIndex;

    QTimer::singleShot(300, this, &ContentNotificationService::checkNextItem);
}

static const QString kSettingsGroup = QStringLiteral("StreamVault/NotificationCache");

void ContentNotificationService::loadCache() {
    QSettings s;
    s.beginGroup(kSettingsGroup);
    const auto keys = s.childKeys();
    for (const auto& key : keys) {
        bool ok = false;
        const int tmdbId = key.toInt(&ok);
        if (!ok) continue;

        const QStringList parts = s.value(key).toString().split('|');
        if (parts.size() < 3) continue;

        CachedShowState state;
        state.lastKnownSeasonCount  = parts[0].toInt();
        state.lastKnownEpisodeCount = parts[1].toInt();
        state.lastChecked           = QDateTime::fromString(parts[2], Qt::ISODate);
        m_cache.insert(tmdbId, state);
    }
    s.endGroup();
}

void ContentNotificationService::saveCache() {
    QSettings s;
    s.beginGroup(kSettingsGroup);

    s.remove("");
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        const auto& st = it.value();
        const QString val = QStringLiteral("%1|%2|%3")
            .arg(st.lastKnownSeasonCount)
            .arg(st.lastKnownEpisodeCount)
            .arg(st.lastChecked.toString(Qt::ISODate));
        s.setValue(QString::number(it.key()), val);
    }
    s.endGroup();
}

QString ContentNotificationService::apiKey() const {
    return StreamVaultSettings::instance().tmdbApiKey();
}

}
