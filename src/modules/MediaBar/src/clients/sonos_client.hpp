#pragma once

#include "types.hpp"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QUrl>
#include <optional>

class SonosClient {
public:
    SonosClient();

    bool isAvailable() const;
    std::optional<PlaybackInfo> getCurrentPlayback();

    bool nextTrack();
    bool previousTrack();
    bool playPause();
    bool seekToPosition(qint64 positionMs);
    bool playUriNow(const QString& uri);
    bool addToQueueUri(const QString& uri, bool enqueueAsNext = true);
    bool removeFromQueue(int queueIndex);

    struct SonosQueueItem {
        QString title;
        QString artist;
        QString album;
        QString uri;
        QString albumArtUri;
    };
    QVector<SonosQueueItem> getQueue(int maxItems = 100);

    struct SonosFavourite {
        QString title;
        QString uri;
        QString albumArtUri;
        QString type;
    };
    QVector<SonosFavourite> getFavourites(int maxItems = 100);
    QVector<SonosQueueItem> browsePlaylist(const QString& uri, int maxItems = 100);

    std::optional<int> getVolume();
    bool setVolume(int percent);

    struct SonosZone {
        QString name;
        QString ip;
        QString uuid;
        QString groupId;
        bool isCoordinator = false;
    };
    struct SonosGroup {
        QString coordinatorName;
        QString coordinatorIp;
        QVector<SonosZone> members;
    };
    QVector<SonosGroup> getZoneGroups();
    void setTargetSpeakerIp(const QString& ip);
    QString targetSpeakerIp() const;

    QString sourceName() const { return "Sonos"; }

private:
    bool hasSpeakerIp() const;
    QString discoverSpeakerIp() const;
    QString speakerIp() const;
    QUrl controlUrl() const;
    QUrl contentDirectoryUrl() const;
    QUrl renderingControlUrl() const;
    bool inBackoff() const;
    void noteBackoff(int seconds = 20);
    std::optional<QByteArray> sendSoapAction(const QString& action, const QString& bodyXml);
    std::optional<QByteArray> sendSoapActionTo(const QUrl& url, const QString& serviceUrn,
                                                const QString& action, const QString& bodyXml);
    static QString extractTagValue(const QString& xml, const QString& tagName);
    static qint64 timeToMs(const QString& text);

    mutable QNetworkAccessManager network_;
    mutable QString discoveredSpeakerIp_;
    mutable qint64 lastDiscoverAtMs_ = 0;
    qint64 backoffUntilMs_ = 0;
    QString userSelectedIp_;
};
