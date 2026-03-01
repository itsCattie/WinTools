#pragma once

#include "types.hpp"

#include <QUrlQuery>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QVector>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <optional>

struct SpotifyTrackItem {
    QString id;
    QString name;
    QString artist;
    QString album;
    QString uri;
    QString albumArtUrl;
};

struct SpotifyCatalogItem {
    QString id;
    QString name;
    QString subtitle;
    QString uri;
    QString imageUrl;
    QString type;
};

struct SpotifyDeviceItem {
    QString id;
    QString name;
    bool isActive = false;
    bool isRestricted = false;
};

class SpotifyClient {
public:
    SpotifyClient();

    bool isAvailable() const;
    std::optional<PlaybackInfo> getCurrentPlayback();

    bool nextTrack();
    bool previousTrack();
    bool playPause();
    bool pausePlayback();
    bool seekToPosition(qint64 positionMs);
    std::optional<int> getVolumePercent();
    bool setVolumePercent(int percent);
    std::optional<bool> getShuffleState();
    bool setShuffleState(bool enabled);

    bool setRepeatState(bool enabled);
    std::optional<bool> isTrackSaved(const QString& trackId);
    bool saveTrack(const QString& trackId);
    bool removeTrack(const QString& trackId);
    bool addToQueue(const QString& uri);
    QVector<SpotifyTrackItem> getQueue();
    QVector<SpotifyTrackItem> searchTracks(const QString& query, int limit = 20);
    QVector<SpotifyCatalogItem> searchCatalog(const QString& query, int limit = 30);
    QVector<SpotifyTrackItem> getSavedTracks(
        int limit = 50,
        const std::function<void(int fetchedCount)>& onProgress = {});
    QVector<SpotifyCatalogItem> getFollowedArtists(int limit = 50);
    QVector<SpotifyCatalogItem> getSavedAlbums(int limit = 50);
    QVector<SpotifyCatalogItem> getUserPlaylists(int limit = 50);
    QVector<SpotifyTrackItem> getTopTracks(int limit = 20);
    QVector<SpotifyDeviceItem> getDevices();
    bool transferPlaybackToDevice(const QString& deviceId, bool play = true);
    bool startPlaybackUris(const QStringList& uris);
    bool startPlaybackUri(const QString& uri);
    bool startPlaybackContext(const QString& contextUri, const QString& offsetUri = QString());
    QVector<SpotifyTrackItem> getAlbumTracks(const QString& albumId, int limit = 50);
    QVector<SpotifyTrackItem> getPlaylistTracks(const QString& playlistId, int limit = 50);
    QVector<SpotifyTrackItem> getArtistTopTracks(const QString& artistId, int limit = 10);

    QString sourceName() const { return "Spotify"; }

private:
    bool recoverAuthorization() const;
    QString ensureClientIdConfigured() const;
    bool ensureUserAccessToken() const;
    bool refreshAccessToken() const;
    bool interactiveAuthorize() const;
    bool saveTokenState(const QString& accessToken, const QString& refreshToken, qint64 expiresInSeconds) const;
    QString clientId() const;
    QString clientSecret() const;
    QString redirectUri() const;
    QString scope() const;
    bool hasAccessToken() const;
    QString accessToken() const;
    std::optional<QJsonObject> requestJson(const QString& method, const QString& path, const QUrlQuery& query = QUrlQuery(), const QByteArray& body = QByteArray());
    bool requestNoContent(const QString& method, const QString& path, const QUrlQuery& query = QUrlQuery(), const QByteArray& body = QByteArray());
    bool requestNoContentDetailed(
        const QString& method,
        const QString& path,
        const QUrlQuery& query,
        const QByteArray& body,
        int* statusCodeOut,
        QString* responseBodyOut);
    static PlaybackInfo parsePlayback(const QJsonObject& obj);

    mutable QNetworkAccessManager network_;
    mutable QString cachedAccessToken_;
    mutable QString cachedRefreshToken_;
    mutable qint64 accessTokenExpiresAtMs_ = 0;
    mutable bool authAttempted_ = false;
    mutable qint64 nextInteractiveAuthAttemptMs_ = 0;
    mutable bool clientIdPromptAttempted_ = false;
};
