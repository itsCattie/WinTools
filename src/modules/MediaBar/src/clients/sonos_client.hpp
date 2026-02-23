#pragma once

// MediaBar: sonos client manages service client integration.

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

    QString sourceName() const { return "Sonos"; }

private:
    bool hasSpeakerIp() const;
    QString discoverSpeakerIp() const;
    QString speakerIp() const;
    QUrl controlUrl() const;
    bool inBackoff() const;
    void noteBackoff(int seconds = 20);
    std::optional<QByteArray> sendSoapAction(const QString& action, const QString& bodyXml);
    static QString extractTagValue(const QString& xml, const QString& tagName);
    static qint64 timeToMs(const QString& text);

    mutable QNetworkAccessManager network_;
    mutable QString discoveredSpeakerIp_;
    mutable qint64 lastDiscoverAtMs_ = 0;
    qint64 backoffUntilMs_ = 0;
};
