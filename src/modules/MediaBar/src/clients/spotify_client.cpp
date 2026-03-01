#include "spotify_client.hpp"

#include "config.hpp"
#include "debug_logger.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QScopedPointer>
#include <QTextStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <limits>

namespace {

constexpr auto API_BASE = "https://api.spotify.com/v1";
constexpr auto ACCOUNTS_BASE = "https://accounts.spotify.com";
constexpr qint64 INTERACTIVE_AUTH_RETRY_COOLDOWN_MS = 30000;
constexpr int NETWORK_REQUEST_TIMEOUT_MS = 7000;

bool waitForReplyWithTimeout(QNetworkReply* reply, int timeoutMs = NETWORK_REQUEST_TIMEOUT_MS) {
    if (!reply) {
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start(timeoutMs);
    loop.exec();

    if (timeout.isActive() || reply->isFinished()) {
        return true;
    }

    reply->abort();
    return false;
}

QByteArray toBase64UrlNoPadding(const QByteArray& input) {
    return input.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString generatePkceVerifier() {
    QByteArray random(64, Qt::Uninitialized);
    for (int i = 0; i < random.size(); ++i) {
        random[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }

    QString verifier = QString::fromLatin1(toBase64UrlNoPadding(random));
    if (verifier.size() < 43) {
        verifier.append(QString(43 - verifier.size(), QLatin1Char('x')));
    }
    if (verifier.size() > 128) {
        verifier = verifier.left(128);
    }
    return verifier;
}

QString generatePkceChallenge(const QString& verifier) {
    const QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(toBase64UrlNoPadding(hash));
}

QString dotenvValue(const QString& key) {
    const QStringList candidates{
        QDir::current().filePath(".env"),
        QDir::current().filePath("../.env"),
        QDir::current().filePath("../../.env"),
        QDir::current().filePath("../../../.env"),
        QDir::current().filePath("../../../../.env")
    };

    for (const auto& path : candidates) {
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) {
                continue;
            }
            if (!line.startsWith(key + "=")) {
                continue;
            }

            QString value = line.mid(key.size() + 1).trimmed();
            if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
                value = value.mid(1, value.size() - 2);
            }
            return value.trimmed();
        }
    }

    return QString();
}

QNetworkReply* send(QNetworkAccessManager& network, const QNetworkRequest& request, const QString& method, const QByteArray& body) {
    const QByteArray m = method.toUtf8();
    if (m == "GET") {
        return network.get(request);
    }
    if (m == "POST") {
        return network.post(request, body);
    }
    if (m == "PUT") {
        return network.put(request, body);
    }
    if (m == "DELETE") {
        return network.sendCustomRequest(request, "DELETE", body);
    }
    return network.sendCustomRequest(request, m, body);
}

}

SpotifyClient::SpotifyClient() {
    debuglog::info("Spotify", "SpotifyClient constructed.");

    const QString savedAccess = config::settingString("spotify_access_token", "").trimmed();
    const QString savedRefresh = config::settingString("spotify_refresh_token", "").trimmed();
    const qint64 savedExpiry = static_cast<qint64>(config::settingDouble("spotify_token_expires_at", 0));

    if (!savedAccess.isEmpty()) {
        cachedAccessToken_ = savedAccess;
        debuglog::info("Spotify", "Loaded persisted access token from settings.");
    }
    if (!savedRefresh.isEmpty()) {
        cachedRefreshToken_ = savedRefresh;
        debuglog::info("Spotify", "Loaded persisted refresh token from settings.");
    }
    if (savedExpiry > 0) {
        accessTokenExpiresAtMs_ = savedExpiry;
    }
}

bool SpotifyClient::recoverAuthorization() const {
    debuglog::warn("Spotify", "RecoverAuthorization triggered.");
    if (!QCoreApplication::instance()) {
        return false;
    }

    cachedAccessToken_.clear();
    accessTokenExpiresAtMs_ = 0;
    authAttempted_ = false;
    if (refreshAccessToken()) {
        debuglog::info("Spotify", "RecoverAuthorization succeeded via refresh token.");
        return true;
    }
    debuglog::warn("Spotify", "Refresh token path failed; trying interactive authorization.");
    return interactiveAuthorize();
}

QString SpotifyClient::clientId() const {
    const QString env = qEnvironmentVariable("SPOTIFY_CLIENT_ID").trimmed();
    if (!env.isEmpty()) {
        return env;
    }
    const QString fromEnvFile = dotenvValue("SPOTIFY_CLIENT_ID");
    if (!fromEnvFile.isEmpty()) {
        return fromEnvFile;
    }

    const QString fromSettings = config::settingString("spotify_client_id", "").trimmed();
    if (!fromSettings.isEmpty()) {
        return fromSettings;
    }

    const QString legacy1 = config::settingString("spotifyClientId", "").trimmed();
    if (!legacy1.isEmpty()) {
        return legacy1;
    }

    const QString legacy2 = config::settingString("spotify_clientid", "").trimmed();
    if (!legacy2.isEmpty()) {
        return legacy2;
    }

    return QString();
}

QString SpotifyClient::clientSecret() const {
    const QString env = qEnvironmentVariable("SPOTIFY_CLIENT_SECRET").trimmed();
    if (!env.isEmpty()) {
        return env;
    }
    const QString fromEnvFile = dotenvValue("SPOTIFY_CLIENT_SECRET");
    if (!fromEnvFile.isEmpty()) {
        return fromEnvFile;
    }
    return config::settingString("spotify_client_secret", "").trimmed();
}

QString SpotifyClient::redirectUri() const {
    const QString env = qEnvironmentVariable("SPOTIFY_REDIRECT_URI").trimmed();
    if (!env.isEmpty()) {
        return env;
    }
    const QString fromEnvFile = dotenvValue("SPOTIFY_REDIRECT_URI");
    if (!fromEnvFile.isEmpty()) {
        return fromEnvFile;
    }
    const QString fromSettings = config::settingString("spotify_redirect_uri", "").trimmed();
    if (!fromSettings.isEmpty()) {
        return fromSettings;
    }
    return "http://127.0.0.1:8888/callback";
}

QString SpotifyClient::scope() const {
    return "user-read-currently-playing user-read-playback-state user-modify-playback-state user-top-read user-library-read user-library-modify user-follow-read user-read-recently-played";
}

bool SpotifyClient::saveTokenState(const QString& accessToken, const QString& refreshToken, qint64 expiresInSeconds) const {
    if (accessToken.trimmed().isEmpty()) {
        return false;
    }

    cachedAccessToken_ = accessToken.trimmed();
    if (!refreshToken.trimmed().isEmpty()) {
        cachedRefreshToken_ = refreshToken.trimmed();
    }
    accessTokenExpiresAtMs_ = QDateTime::currentMSecsSinceEpoch() + (std::max<qint64>(60, expiresInSeconds) * 1000);

    auto settings = config::loadSettings();
    settings.insert("spotify_access_token", cachedAccessToken_);
    settings.insert("spotify_refresh_token", cachedRefreshToken_);
    settings.insert("spotify_token_expires_at", static_cast<double>(accessTokenExpiresAtMs_));
    return config::saveSettings(settings);
}

bool SpotifyClient::refreshAccessToken() const {
    debuglog::trace("Spotify", "refreshAccessToken called.");
    if (!QCoreApplication::instance()) {
        return false;
    }
    if (QThread::currentThread() != network_.thread()) {
        return false;
    }

    const QString spotifyClientId = ensureClientIdConfigured();
    const QString spotifyClientSecret = clientSecret();
    if (spotifyClientId.isEmpty()) {
        debuglog::error("Spotify", "Refresh token flow failed: SPOTIFY_CLIENT_ID is missing.");
        return false;
    }

    if (cachedRefreshToken_.isEmpty()) {
        cachedRefreshToken_ = config::settingString("spotify_refresh_token", "").trimmed();
    }
    if (cachedRefreshToken_.isEmpty()) {
        debuglog::warn("Spotify", "No refresh token available.");
        return false;
    }

    QUrl url(QString("%1/api/token").arg(ACCOUNTS_BASE));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    if (!spotifyClientSecret.isEmpty()) {
        const QByteArray basic = QString("%1:%2").arg(spotifyClientId, spotifyClientSecret).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + basic);
    }

    QUrlQuery bodyQuery;
    bodyQuery.addQueryItem("grant_type", "refresh_token");
    bodyQuery.addQueryItem("refresh_token", cachedRefreshToken_);
    if (spotifyClientSecret.isEmpty()) {
        bodyQuery.addQueryItem("client_id", spotifyClientId);
    }

    QNetworkReply* reply = network_.post(request, bodyQuery.query(QUrl::FullyEncoded).toUtf8());
    if (!reply) {
        return false;
    }
    const bool finished = waitForReplyWithTimeout(reply);

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const auto err = finished ? reply->error() : QNetworkReply::TimeoutError;
    reply->deleteLater();

    if (err != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        debuglog::error("Spotify", QString("Refresh token request failed status=%1 error=%2")
            .arg(statusCode)
            .arg(static_cast<int>(err)));
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    return saveTokenState(
        obj.value("access_token").toString(),
        obj.value("refresh_token").toString(),
        static_cast<qint64>(obj.value("expires_in").toDouble(3600)));
}

bool SpotifyClient::interactiveAuthorize() const {
    debuglog::warn("Spotify", "interactiveAuthorize started.");
    if (!QCoreApplication::instance()) {
        return false;
    }
    if (QThread::currentThread() != network_.thread()) {
        return false;
    }

    const QString spotifyClientId = ensureClientIdConfigured();
    const QString spotifyClientSecret = clientSecret();
    const bool publicClientMode = spotifyClientSecret.isEmpty();

    if (spotifyClientId.isEmpty()) {
        debuglog::error("Spotify", "interactiveAuthorize aborted: SPOTIFY_CLIENT_ID missing.");
        return false;
    }

    const QUrl cbUrl(redirectUri());
    if (!cbUrl.isValid() || cbUrl.host().isEmpty() || cbUrl.port() <= 0) {
        return false;
    }

    const quint16 callbackPort = static_cast<quint16>(cbUrl.port());
    QTcpServer server;
    bool listening = false;

    for (const auto& addr : {QHostAddress::AnyIPv4, QHostAddress::Any, QHostAddress::LocalHost}) {
        if (server.listen(addr, callbackPort)) {
            listening = true;
            break;
        }
    }

    if (!listening) {
        debuglog::error("Spotify", QString("interactiveAuthorize could not start callback listener on port=%1")
            .arg(callbackPort));
        return false;
    }

    const QString state = QString::number(QRandomGenerator::global()->generate64(), 16);
    const QString codeVerifier = publicClientMode ? generatePkceVerifier() : QString();
    const QString codeChallenge = publicClientMode ? generatePkceChallenge(codeVerifier) : QString();

    QUrl authUrl(QString("%1/authorize").arg(ACCOUNTS_BASE));
    QUrlQuery query;
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", spotifyClientId);
    query.addQueryItem("redirect_uri", redirectUri());
    query.addQueryItem("scope", scope());
    query.addQueryItem("state", state);
    query.addQueryItem("show_dialog", "true");
    if (publicClientMode) {
        query.addQueryItem("code_challenge_method", "S256");
        query.addQueryItem("code_challenge", codeChallenge);
    }
    authUrl.setQuery(query);

    if (!QDesktopServices::openUrl(authUrl)) {
        debuglog::error("Spotify", "interactiveAuthorize failed to open browser URL.");
        return false;
    }

    QEventLoop waitLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&server, &QTcpServer::newConnection, &waitLoop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    timeout.start(180000);
    waitLoop.exec();

    if (!server.hasPendingConnections()) {
        debuglog::error("Spotify", "interactiveAuthorize timed out waiting for callback.");
        return false;
    }

    QScopedPointer<QTcpSocket> socket(server.nextPendingConnection());
    if (!socket) {
        return false;
    }
    if (!socket->waitForReadyRead(8000)) {
        return false;
    }

    const QByteArray req = socket->readAll();
    const QList<QByteArray> lines = req.split('\n');
    if (lines.isEmpty()) {
        return false;
    }

    const QList<QByteArray> parts = lines.first().trimmed().split(' ');
    if (parts.size() < 2) {
        return false;
    }

    const QString target = QString::fromUtf8(parts[1]);
    QUrl returnedUrl(QString("http://127.0.0.1%1").arg(target));
    QUrlQuery returnedQuery(returnedUrl);
    const QString code = returnedQuery.queryItemValue("code");
    const QString returnedState = returnedQuery.queryItemValue("state");
    const QString returnedError = returnedQuery.queryItemValue("error");

    const QByteArray response(
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
        "<html><body><h3>Spotify authorization completed.</h3><p>You can close this tab.</p></body></html>");
    socket->write(response);
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();

    if (!returnedError.isEmpty()) {
        debuglog::error("Spotify", QString("interactiveAuthorize callback error=%1").arg(returnedError));
        return false;
    }

    if (code.isEmpty() || returnedState != state) {
        debuglog::error("Spotify", "interactiveAuthorize callback invalid code/state.");
        return false;
    }

    QUrl tokenUrl(QString("%1/api/token").arg(ACCOUNTS_BASE));
    QNetworkRequest tokenReq(tokenUrl);
    tokenReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    if (!publicClientMode) {
        const QByteArray basic = QString("%1:%2").arg(spotifyClientId, spotifyClientSecret).toUtf8().toBase64();
        tokenReq.setRawHeader("Authorization", "Basic " + basic);
    }

    QUrlQuery tokenBody;
    tokenBody.addQueryItem("grant_type", "authorization_code");
    tokenBody.addQueryItem("code", code);
    tokenBody.addQueryItem("redirect_uri", redirectUri());
    if (publicClientMode) {
        tokenBody.addQueryItem("client_id", spotifyClientId);
        tokenBody.addQueryItem("code_verifier", codeVerifier);
    }

    QNetworkReply* reply = network_.post(tokenReq, tokenBody.query(QUrl::FullyEncoded).toUtf8());
    if (!reply) {
        return false;
    }
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const auto err = reply->error();
    reply->deleteLater();

    if (err != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        debuglog::error("Spotify", QString("Authorization code exchange failed status=%1 error=%2")
            .arg(statusCode)
            .arg(static_cast<int>(err)));
        return false;
    }

    debuglog::info("Spotify", "interactiveAuthorize completed successfully.");

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    return saveTokenState(
        obj.value("access_token").toString(),
        obj.value("refresh_token").toString(),
        static_cast<qint64>(obj.value("expires_in").toDouble(3600)));
}

bool SpotifyClient::ensureUserAccessToken() const {
    debuglog::trace("Spotify", "ensureUserAccessToken called.");
    if (!QCoreApplication::instance()) {
        return false;
    }

    const QString envToken = qEnvironmentVariable("SPOTIFY_ACCESS_TOKEN").trimmed();
    if (!envToken.isEmpty()) {
        cachedAccessToken_ = envToken;
        return true;
    }

    if (cachedAccessToken_.isEmpty()) {
        cachedAccessToken_ = config::settingString("spotify_access_token", "").trimmed();
    }
    if (cachedRefreshToken_.isEmpty()) {
        cachedRefreshToken_ = config::settingString("spotify_refresh_token", "").trimmed();
    }
    if (accessTokenExpiresAtMs_ <= 0) {
        accessTokenExpiresAtMs_ = static_cast<qint64>(config::settingDouble("spotify_token_expires_at", 0));
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!cachedAccessToken_.isEmpty() && accessTokenExpiresAtMs_ > (now + 60000)) {

        return true;
    }

    if (refreshAccessToken()) {
        debuglog::info("Spotify", "Access token refreshed.");
        return true;
    }

    if (QThread::currentThread() != network_.thread()) {
        debuglog::warn("Spotify", "Skipping interactive auth: called from non-UI thread.");
    } else if (now >= nextInteractiveAuthAttemptMs_) {
        authAttempted_ = true;
        if (interactiveAuthorize()) {
            nextInteractiveAuthAttemptMs_ = 0;
            debuglog::info("Spotify", "Access token acquired via interactive auth.");
            return true;
        }

        nextInteractiveAuthAttemptMs_ = now + INTERACTIVE_AUTH_RETRY_COOLDOWN_MS;
        debuglog::warn("Spotify", QString("Interactive auth failed; retry available in %1s.")
            .arg(INTERACTIVE_AUTH_RETRY_COOLDOWN_MS / 1000));
    } else {
        const qint64 waitMs = std::max<qint64>(0, nextInteractiveAuthAttemptMs_ - now);
        debuglog::trace("Spotify", QString("Interactive auth cooldown active; retry in %1ms.")
            .arg(waitMs));
    }

    debuglog::warn("Spotify", QString("ensureUserAccessToken fallback token_present=%1")
        .arg(!cachedAccessToken_.isEmpty() ? "1" : "0"));
    return !cachedAccessToken_.isEmpty();
}

QString SpotifyClient::ensureClientIdConfigured() const {
    const QString existing = clientId();
    if (!existing.isEmpty()) {
        return existing;
    }

    if (!QCoreApplication::instance()) {
        debuglog::error("Spotify", "Client ID missing and application instance unavailable.");
        return QString();
    }

    if (QThread::currentThread() != network_.thread()) {
        debuglog::error("Spotify", "Client ID missing and cannot prompt from non-UI thread.");
        return QString();
    }

    if (clientIdPromptAttempted_) {
        return QString();
    }
    clientIdPromptAttempted_ = true;

    const auto entered = QInputDialog::getText(
        nullptr,
        "Spotify Setup",
        "Spotify Client ID is missing.\n\nEnter your Spotify app Client ID (from Spotify Developer Dashboard):",
        QLineEdit::Normal,
        "").trimmed();

    if (entered.isEmpty()) {
        QMessageBox::warning(
            nullptr,
            "Spotify Setup",
            "Spotify Client ID is required to reconnect Spotify.\n"
            "Set SPOTIFY_CLIENT_ID env var or save it in MediaBar settings.");
        debuglog::error("Spotify", "Client ID prompt canceled or empty.");
        return QString();
    }

    auto settings = config::loadSettings();
    settings.insert("spotify_client_id", entered);
    if (!config::saveSettings(settings)) {
        debuglog::warn("Spotify", "Client ID entered but failed to persist settings.");
    } else {
        debuglog::info("Spotify", "Client ID captured via prompt and saved to settings.");
    }

    return entered;
}

bool SpotifyClient::hasAccessToken() const {
    return ensureUserAccessToken();
}

QString SpotifyClient::accessToken() const {
    const QString envToken = qEnvironmentVariable("SPOTIFY_ACCESS_TOKEN").trimmed();
    if (!envToken.isEmpty()) {
        return envToken;
    }
    if (cachedAccessToken_.isEmpty()) {
        cachedAccessToken_ = config::settingString("spotify_access_token", "").trimmed();
    }
    return cachedAccessToken_;
}

bool SpotifyClient::isAvailable() const {
    return hasAccessToken();
}

std::optional<QJsonObject> SpotifyClient::requestJson(const QString& method, const QString& path, const QUrlQuery& query, const QByteArray& body) {

    if (QThread::currentThread() != network_.thread()) {
        return std::nullopt;
    }
    if (!hasAccessToken()) {
        return std::nullopt;
    }

    const auto execute = [this, &method, &path, &query, &body](int* statusOut, QNetworkReply::NetworkError* errOut) -> QByteArray {
        QUrl url(QString("%1%2").arg(API_BASE, path));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken()).toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* reply = send(network_, request, method, body);
        if (!reply) {
            if (statusOut) {
                *statusOut = 0;
            }
            if (errOut) {
                *errOut = QNetworkReply::UnknownNetworkError;
            }
            return QByteArray();
        }
        const bool finished = waitForReplyWithTimeout(reply);

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const auto err = finished ? reply->error() : QNetworkReply::TimeoutError;
        reply->deleteLater();

        if (statusOut) {
            *statusOut = statusCode;
        }
        if (errOut) {
            *errOut = err;
        }
        return payload;
    };

    int statusCode = 0;
    QNetworkReply::NetworkError err = QNetworkReply::NoError;
    QByteArray payload = execute(&statusCode, &err);
    if ((statusCode == 401 || err == QNetworkReply::ContentAccessDenied) && recoverAuthorization()) {
        debuglog::warn("Spotify", QString("requestJson retrying after auth recovery path=%1").arg(path));
        payload = execute(&statusCode, &err);
    }

    if (err != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        debuglog::error("Spotify", QString("requestJson failed method=%1 path=%2 status=%3 error=%4")
            .arg(method, path, QString::number(statusCode), QString::number(static_cast<int>(err))));
        return std::nullopt;
    }

    if (statusCode != 204) {
        debuglog::trace("Spotify", QString("requestJson succeeded path=%1 status=%2")
            .arg(path, QString::number(statusCode)));
    }
    if (payload.isEmpty()) {
        return QJsonObject{};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return std::nullopt;
    }
    return doc.object();
}

bool SpotifyClient::requestNoContent(const QString& method, const QString& path, const QUrlQuery& query, const QByteArray& body) {
    return requestNoContentDetailed(method, path, query, body, nullptr, nullptr);
}

bool SpotifyClient::requestNoContentDetailed(
    const QString& method,
    const QString& path,
    const QUrlQuery& query,
    const QByteArray& body,
    int* statusCodeOut,
    QString* responseBodyOut) {

    if (QThread::currentThread() != network_.thread()) {
        if (statusCodeOut) {
            *statusCodeOut = 0;
        }
        if (responseBodyOut) {
            *responseBodyOut = QString();
        }
        return false;
    }
    if (!hasAccessToken()) {
        if (statusCodeOut) {
            *statusCodeOut = 0;
        }
        if (responseBodyOut) {
            *responseBodyOut = QString();
        }
        return false;
    }

    const auto execute = [this, &method, &path, &query, &body](int* statusOut, QNetworkReply::NetworkError* errOut) {
        QUrl url(QString("%1%2").arg(API_BASE, path));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken()).toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* reply = send(network_, request, method, body);
        if (!reply) {
            if (statusOut) {
                *statusOut = 0;
            }
            if (errOut) {
                *errOut = QNetworkReply::UnknownNetworkError;
            }
            return QByteArray();
        }
        const bool finished = waitForReplyWithTimeout(reply);

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const auto err = finished ? reply->error() : QNetworkReply::TimeoutError;
        reply->deleteLater();
        if (statusOut) {
            *statusOut = statusCode;
        }
        if (errOut) {
            *errOut = err;
        }
        return payload;
    };

    int statusCode = 0;
    QNetworkReply::NetworkError err = QNetworkReply::NoError;
    QByteArray payload = execute(&statusCode, &err);
    if ((statusCode == 401 || err == QNetworkReply::ContentAccessDenied) && recoverAuthorization()) {
        debuglog::warn("Spotify", QString("requestNoContent retrying after auth recovery path=%1").arg(path));
        payload = execute(&statusCode, &err);
    }

    const QString bodyText = QString::fromUtf8(payload).trimmed();
    if (statusCodeOut) {
        *statusCodeOut = statusCode;
    }
    if (responseBodyOut) {
        *responseBodyOut = bodyText;
    }

    if (err != QNetworkReply::NoError) {
        debuglog::error("Spotify", QString("requestNoContent failed method=%1 path=%2 status=%3 error=%4 body=%5")
            .arg(method, path, QString::number(statusCode), QString::number(static_cast<int>(err)), bodyText.left(300)));
        return false;
    }

    return statusCode >= 200 && statusCode < 300;
}

PlaybackInfo SpotifyClient::parsePlayback(const QJsonObject& obj) {
    PlaybackInfo info;
    info.valid = true;
    info.source = "spotify";
    info.isPlaying = obj.value("is_playing").toBool(false);
    info.progressMs = static_cast<qint64>(obj.value("progress_ms").toDouble(0));

    const QJsonObject item = obj.value("item").toObject();
    info.trackName = item.value("name").toString("Unknown");
    info.durationMs = static_cast<qint64>(item.value("duration_ms").toDouble(0));
    info.trackId = item.value("id").toString();
    info.trackUri = item.value("uri").toString();

    const QJsonArray artists = item.value("artists").toArray();
    if (!artists.isEmpty()) {
        info.artistName = artists.first().toObject().value("name").toString("Unknown");
    } else {
        info.artistName = "Unknown";
    }

    const QJsonObject album = item.value("album").toObject();
    info.albumName = album.value("name").toString();
    info.albumUri = album.value("uri").toString();

    const QJsonArray images = album.value("images").toArray();
    if (!images.isEmpty()) {
        info.albumArt = images.first().toObject().value("url").toString();
    }

    const QJsonObject device = obj.value("device").toObject();
    if (device.contains("volume_percent")) {
        info.volumePercent = qBound(0, device.value("volume_percent").toInt(-1), 100);
    }

    return info;
}

std::optional<PlaybackInfo> SpotifyClient::getCurrentPlayback() {

    const auto parseIfPlayable = [](const QJsonObject& obj) -> std::optional<PlaybackInfo> {
        if (obj.isEmpty()) {
            return std::nullopt;
        }
        const QJsonObject item = obj.value("item").toObject();
        if (item.isEmpty()) {
            return std::nullopt;
        }
        return parsePlayback(obj);
    };

    auto response = requestJson("GET", "/me/player/currently-playing");
    if (response.has_value()) {
        const auto parsed = parseIfPlayable(response.value());
        if (parsed.has_value()) {
            debuglog::trace("Spotify", "getCurrentPlayback returned active object from /currently-playing.");
            return parsed;
        }
        debuglog::trace("Spotify", "getCurrentPlayback /currently-playing returned empty or non-playable payload.");
    } else {
        debuglog::warn("Spotify", "getCurrentPlayback /currently-playing failed; falling back to /me/player.");
    }

    auto fallback = requestJson("GET", "/me/player");
    if (!fallback.has_value()) {
        return std::nullopt;
    }

    const auto parsedFallback = parseIfPlayable(fallback.value());
    if (!parsedFallback.has_value()) {
        debuglog::trace("Spotify", "getCurrentPlayback fallback /me/player returned non-playable payload.");
        return std::nullopt;
    }

    debuglog::trace("Spotify", "getCurrentPlayback recovered using /me/player fallback.");
    return parsedFallback;
}

bool SpotifyClient::nextTrack() {
    debuglog::info("Spotify", "nextTrack requested.");
    return requestNoContent("POST", "/me/player/next");
}

bool SpotifyClient::previousTrack() {
    debuglog::info("Spotify", "previousTrack requested.");
    return requestNoContent("POST", "/me/player/previous");
}

bool SpotifyClient::pausePlayback() {
    return requestNoContent("PUT", "/me/player/pause");
}

bool SpotifyClient::playPause() {
    debuglog::info("Spotify", "playPause requested.");
    const auto playback = getCurrentPlayback();
    if (playback.has_value() && playback->isPlaying) {
        return requestNoContent("PUT", "/me/player/pause");
    }
    return requestNoContent("PUT", "/me/player/play");
}

bool SpotifyClient::seekToPosition(qint64 positionMs) {
    QUrlQuery query;
    query.addQueryItem("position_ms", QString::number(std::max<qint64>(0, positionMs)));
    return requestNoContent("PUT", "/me/player/seek", query);
}

std::optional<int> SpotifyClient::getVolumePercent() {
    auto response = requestJson("GET", "/me/player");
    if (!response.has_value()) {
        return std::nullopt;
    }

    const QJsonObject device = response->value("device").toObject();
    if (!device.contains("volume_percent")) {
        return std::nullopt;
    }
    return qBound(0, device.value("volume_percent").toInt(0), 100);
}

bool SpotifyClient::setVolumePercent(int percent) {
    QUrlQuery query;
    query.addQueryItem("volume_percent", QString::number(qBound(0, percent, 100)));
    return requestNoContent("PUT", "/me/player/volume", query);
}

std::optional<bool> SpotifyClient::getShuffleState() {
    auto response = requestJson("GET", "/me/player");
    if (!response.has_value()) {
        return std::nullopt;
    }

    const QJsonObject obj = response.value();
    if (!obj.contains("shuffle_state")) {
        return std::nullopt;
    }
    return obj.value("shuffle_state").toBool(false);
}

bool SpotifyClient::setShuffleState(bool enabled) {
    debuglog::info("Spotify", QString("setShuffleState enabled=%1").arg(enabled ? "1" : "0"));
    QUrlQuery query;
    query.addQueryItem("state", enabled ? "true" : "false");
    return requestNoContent("PUT", "/me/player/shuffle", query);
}

bool SpotifyClient::setRepeatState(bool enabled) {
    debuglog::info("Spotify", QString("setRepeatState enabled=%1").arg(enabled ? "1" : "0"));
    QUrlQuery query;

    query.addQueryItem("state", enabled ? "context" : "off");
    return requestNoContent("PUT", "/me/player/repeat", query);
}

std::optional<bool> SpotifyClient::isTrackSaved(const QString& trackId) {
    if (QThread::currentThread() != network_.thread()) {
        return std::nullopt;
    }
    const QString id = trackId.trimmed();
    if (id.isEmpty()) {
        return std::nullopt;
    }

    if (!hasAccessToken()) {
        return std::nullopt;
    }

    const auto execute = [this, &id](int* statusOut, QNetworkReply::NetworkError* errOut) {
        QUrlQuery query;
        query.addQueryItem("ids", id);
        QUrl url(QString("%1%2").arg(API_BASE, "/me/tracks/contains"));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken()).toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* reply = send(network_, request, "GET", QByteArray());
        if (!reply) {
            if (statusOut) {
                *statusOut = 0;
            }
            if (errOut) {
                *errOut = QNetworkReply::UnknownNetworkError;
            }
            return QByteArray();
        }
        const bool finished = waitForReplyWithTimeout(reply);

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const auto err = finished ? reply->error() : QNetworkReply::TimeoutError;
        reply->deleteLater();

        if (statusOut) {
            *statusOut = statusCode;
        }
        if (errOut) {
            *errOut = err;
        }
        return payload;
    };

    int statusCode = 0;
    QNetworkReply::NetworkError err = QNetworkReply::NoError;
    QByteArray payload = execute(&statusCode, &err);
    if ((statusCode == 401 || err == QNetworkReply::ContentAccessDenied) && recoverAuthorization()) {
        payload = execute(&statusCode, &err);
    }

    if (err != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isArray()) {
        return std::nullopt;
    }

    const QJsonArray arr = doc.array();
    if (arr.isEmpty()) {
        return std::nullopt;
    }
    return arr.first().toBool(false);
}

bool SpotifyClient::saveTrack(const QString& trackId) {
    const QString id = trackId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QUrlQuery query;
    query.addQueryItem("ids", id);
    return requestNoContent("PUT", "/me/tracks", query);
}

bool SpotifyClient::removeTrack(const QString& trackId) {
    const QString id = trackId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    QUrlQuery query;
    query.addQueryItem("ids", id);
    return requestNoContent("DELETE", "/me/tracks", query);
}

bool SpotifyClient::addToQueue(const QString& uri) {
    const QString normalized = uri.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    QUrlQuery query;
    query.addQueryItem("uri", normalized);
    return requestNoContent("POST", "/me/player/queue", query);
}

QVector<SpotifyTrackItem> SpotifyClient::getQueue() {
    QVector<SpotifyTrackItem> out;

    auto response = requestJson("GET", "/me/player/queue");
    if (!response.has_value()) return out;

    const QJsonObject current = response->value("currently_playing").toObject();
    if (!current.isEmpty() && current.value("type").toString() == "track") {
        SpotifyTrackItem item;
        item.id   = current.value("id").toString();
        item.name = current.value("name").toString();
        item.uri  = current.value("uri").toString();
        item.album = current.value("album").toObject().value("name").toString();
        const QJsonArray imgs = current.value("album").toObject().value("images").toArray();
        if (!imgs.isEmpty())
            item.albumArtUrl = imgs.first().toObject().value("url").toString();
        const QJsonArray artists = current.value("artists").toArray();
        if (!artists.isEmpty())
            item.artist = artists.first().toObject().value("name").toString();
        out.push_back(item);
    }

    const QJsonArray queue = response->value("queue").toArray();
    out.reserve(out.size() + queue.size());
    for (const auto& val : queue) {
        const QJsonObject obj = val.toObject();
        if (obj.value("type").toString() != "track") continue;
        SpotifyTrackItem item;
        item.id   = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri  = obj.value("uri").toString();
        item.album = obj.value("album").toObject().value("name").toString();
        const QJsonArray imgs = obj.value("album").toObject().value("images").toArray();
        if (!imgs.isEmpty())
            item.albumArtUrl = imgs.first().toObject().value("url").toString();
        const QJsonArray artists = obj.value("artists").toArray();
        if (!artists.isEmpty())
            item.artist = artists.first().toObject().value("name").toString();
        out.push_back(item);
    }

    return out;
}

QVector<SpotifyTrackItem> SpotifyClient::searchTracks(const QString& queryText, int limit) {
    QVector<SpotifyTrackItem> out;
    const QString queryStr = queryText.trimmed();
    if (queryStr.isEmpty()) {
        return out;
    }

    QUrlQuery query;
    query.addQueryItem("q", queryStr);
    query.addQueryItem("type", "track");
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", "/search", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("tracks").toObject().value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject obj = value.toObject();
        SpotifyTrackItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.album = obj.value("album").toObject().value("name").toString();
        const QJsonArray albumImages = obj.value("album").toObject().value("images").toArray();
        if (!albumImages.isEmpty()) {
            item.albumArtUrl = albumImages.first().toObject().value("url").toString();
        }
        const QJsonArray artists = obj.value("artists").toArray();
        if (!artists.isEmpty()) {
            item.artist = artists.first().toObject().value("name").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyCatalogItem> SpotifyClient::searchCatalog(const QString& queryText, int limit) {
    QVector<SpotifyCatalogItem> out;
    const QString queryStr = queryText.trimmed();
    if (queryStr.isEmpty()) {
        return out;
    }

    QUrlQuery query;
    query.addQueryItem("q", queryStr);
    query.addQueryItem("type", "track,artist,album");
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", "/search", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray trackItems = response->value("tracks").toObject().value("items").toArray();
    for (const auto& value : trackItems) {
        const QJsonObject obj = value.toObject();
        SpotifyCatalogItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.type = "track";

        const QJsonObject albumObj = obj.value("album").toObject();
        const QString albumName = albumObj.value("name").toString();
        const QJsonArray artists = obj.value("artists").toArray();
        const QString artistName = artists.isEmpty() ? QString() : artists.first().toObject().value("name").toString();
        item.subtitle = QString("%1%2%3")
            .arg(artistName)
            .arg(artistName.isEmpty() || albumName.isEmpty() ? "" : " • ")
            .arg(albumName);

        const QJsonArray images = albumObj.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }

    const QJsonArray artistItems = response->value("artists").toObject().value("items").toArray();
    for (const auto& value : artistItems) {
        const QJsonObject obj = value.toObject();
        SpotifyCatalogItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.type = "artist";
        item.subtitle = QString("Artist • %1 followers")
            .arg(static_cast<qint64>(obj.value("followers").toObject().value("total").toDouble(0.0)));

        const QJsonArray images = obj.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }

    const QJsonArray albumItems = response->value("albums").toObject().value("items").toArray();
    for (const auto& value : albumItems) {
        const QJsonObject obj = value.toObject();
        SpotifyCatalogItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.type = "album";

        const QJsonArray artists = obj.value("artists").toArray();
        const QString artistName = artists.isEmpty() ? QString() : artists.first().toObject().value("name").toString();
        item.subtitle = artistName.isEmpty() ? QString("Album") : QString("Album • %1").arg(artistName);

        const QJsonArray images = obj.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }

    return out;
}

QVector<SpotifyTrackItem> SpotifyClient::getSavedTracks(
    int limit,
    const std::function<void(int fetchedCount)>& onProgress) {
    QVector<SpotifyTrackItem> out;
    constexpr int pageSize = 50;
    const bool fetchAll = limit <= 0;

    if (onProgress) {
        onProgress(0);
    }

    int remaining = fetchAll ? std::numeric_limits<int>::max() : limit;
    int offset = 0;

    while (remaining > 0) {
        const int pageLimit = std::clamp(remaining, 1, pageSize);
        QUrlQuery query;
        query.addQueryItem("limit", QString::number(pageLimit));
        query.addQueryItem("offset", QString::number(offset));

        auto response = requestJson("GET", "/me/tracks", query);
        if (!response.has_value()) {
            return out;
        }

        const QJsonArray items = response->value("items").toArray();
        if (items.isEmpty()) {
            break;
        }

        out.reserve(out.size() + items.size());
        for (const auto& value : items) {
            const QJsonObject track = value.toObject().value("track").toObject();
            if (track.isEmpty()) {
                continue;
            }

            SpotifyTrackItem item;
            item.id = track.value("id").toString();
            item.name = track.value("name").toString();
            item.uri = track.value("uri").toString();
            item.album = track.value("album").toObject().value("name").toString();
            const QJsonArray albumImages = track.value("album").toObject().value("images").toArray();
            if (!albumImages.isEmpty()) {
                item.albumArtUrl = albumImages.first().toObject().value("url").toString();
            }
            const QJsonArray artists = track.value("artists").toArray();
            if (!artists.isEmpty()) {
                item.artist = artists.first().toObject().value("name").toString();
            }
            out.push_back(item);
        }

        const int fetchedCount = items.size();
        if (!fetchAll) {
            remaining -= fetchedCount;
        }
        offset += fetchedCount;

        const QString nextPage = response->value("next").toString();
        if (onProgress) {
            onProgress(out.size());
        }
        if (nextPage.trimmed().isEmpty() || fetchedCount < pageLimit) {
            break;
        }
    }

    return out;
}

QVector<SpotifyCatalogItem> SpotifyClient::getFollowedArtists(int limit) {
    QVector<SpotifyCatalogItem> out;
    QUrlQuery query;
    query.addQueryItem("type", "artist");
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", "/me/following", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("artists").toObject().value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject obj = value.toObject();
        SpotifyCatalogItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.type = "artist";
        item.subtitle = QString("Artist • %1 followers")
            .arg(static_cast<qint64>(obj.value("followers").toObject().value("total").toDouble(0.0)));
        const QJsonArray images = obj.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyCatalogItem> SpotifyClient::getSavedAlbums(int limit) {
    QVector<SpotifyCatalogItem> out;
    QUrlQuery query;
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", "/me/albums", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject album = value.toObject().value("album").toObject();
        if (album.isEmpty()) {
            continue;
        }

        SpotifyCatalogItem item;
        item.id = album.value("id").toString();
        item.name = album.value("name").toString();
        item.uri = album.value("uri").toString();
        item.type = "album";
        const QJsonArray artists = album.value("artists").toArray();
        const QString artistName = artists.isEmpty() ? QString() : artists.first().toObject().value("name").toString();
        item.subtitle = artistName.isEmpty() ? QString("Album") : QString("Album • %1").arg(artistName);
        const QJsonArray images = album.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyCatalogItem> SpotifyClient::getUserPlaylists(int limit) {
    QVector<SpotifyCatalogItem> out;
    QUrlQuery query;
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", "/me/playlists", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject obj = value.toObject();
        SpotifyCatalogItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.type = "playlist";
        item.subtitle = QString("Playlist • %1 tracks")
            .arg(static_cast<qint64>(obj.value("tracks").toObject().value("total").toDouble(0.0)));
        const QJsonArray images = obj.value("images").toArray();
        if (!images.isEmpty()) {
            item.imageUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyTrackItem> SpotifyClient::getTopTracks(int limit) {
    QVector<SpotifyTrackItem> out;
    QUrlQuery query;
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));
    query.addQueryItem("time_range", "medium_term");

    auto response = requestJson("GET", "/me/top/tracks", query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject obj = value.toObject();
        SpotifyTrackItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.album = obj.value("album").toObject().value("name").toString();
        const QJsonArray albumImages = obj.value("album").toObject().value("images").toArray();
        if (!albumImages.isEmpty()) {
            item.albumArtUrl = albumImages.first().toObject().value("url").toString();
        }
        const QJsonArray artists = obj.value("artists").toArray();
        if (!artists.isEmpty()) {
            item.artist = artists.first().toObject().value("name").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyDeviceItem> SpotifyClient::getDevices() {
    QVector<SpotifyDeviceItem> out;
    auto response = requestJson("GET", "/me/player/devices");
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray devices = response->value("devices").toArray();
    out.reserve(devices.size());
    for (const auto& value : devices) {
        const QJsonObject obj = value.toObject();
        SpotifyDeviceItem device;
        device.id = obj.value("id").toString();
        device.name = obj.value("name").toString();
        device.isActive = obj.value("is_active").toBool(false);
        device.isRestricted = obj.value("is_restricted").toBool(false);
        out.push_back(device);
    }
    return out;
}

bool SpotifyClient::transferPlaybackToDevice(const QString& deviceId, bool play) {
    const QString id = deviceId.trimmed();
    if (id.isEmpty()) {
        return false;
    }

    QJsonObject body;
    QJsonArray ids;
    ids.append(id);
    body.insert("device_ids", ids);
    body.insert("play", play);

    return requestNoContent("PUT", "/me/player", QUrlQuery(), QJsonDocument(body).toJson(QJsonDocument::Compact));
}

bool SpotifyClient::startPlaybackUris(const QStringList& uris) {
    if (uris.isEmpty()) {
        return false;
    }

    QJsonObject body;
    QJsonArray arr;
    for (const auto& uri : uris) {
        const QString normalized = uri.trimmed();
        if (!normalized.isEmpty()) {
            arr.append(normalized);
        }
    }
    if (arr.isEmpty()) {
        return false;
    }

    body.insert("uris", arr);
    int statusCode = 0;
    QString responseBody;
    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    if (requestNoContentDetailed("PUT", "/me/player/play", QUrlQuery(), payload, &statusCode, &responseBody)) {
        return true;
    }

    if (statusCode == 404) {
        debuglog::warn("Spotify", QString("startPlaybackUris returned 404; attempting device activation. body=%1")
            .arg(responseBody.left(300)));

        const auto devices = getDevices();
        if (devices.isEmpty()) {
            debuglog::error("Spotify", "No Spotify devices available to activate for playback retry.");
            return false;
        }

        QString selectedDeviceId;
        QString selectedDeviceName;

        for (const auto& device : devices) {
            if (device.isActive && !device.isRestricted && !device.id.trimmed().isEmpty()) {
                selectedDeviceId = device.id.trimmed();
                selectedDeviceName = device.name;
                break;
            }
        }

        if (selectedDeviceId.isEmpty()) {
            for (const auto& device : devices) {
                if (!device.isRestricted && !device.id.trimmed().isEmpty()) {
                    selectedDeviceId = device.id.trimmed();
                    selectedDeviceName = device.name;
                    break;
                }
            }
        }

        if (selectedDeviceId.isEmpty()) {
            debuglog::error("Spotify", "All discovered Spotify devices are restricted or invalid.");
            return false;
        }

        debuglog::info("Spotify", QString("Transferring playback to device for retry: %1 (%2)")
            .arg(selectedDeviceName, selectedDeviceId));

        if (!transferPlaybackToDevice(selectedDeviceId, true)) {
            debuglog::error("Spotify", "transferPlaybackToDevice failed during playback retry.");
            return false;
        }

        QThread::msleep(250);
        if (requestNoContentDetailed("PUT", "/me/player/play", QUrlQuery(), payload, &statusCode, &responseBody)) {
            debuglog::info("Spotify", "Playback retry succeeded after device activation.");
            return true;
        }

        debuglog::error("Spotify", QString("Playback retry failed after device activation. status=%1 body=%2")
            .arg(statusCode)
            .arg(responseBody.left(300)));
    }

    return false;
}

bool SpotifyClient::startPlaybackUri(const QString& uri) {
    const QString normalized = uri.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    return startPlaybackUris(QStringList{normalized});
}

bool SpotifyClient::startPlaybackContext(const QString& contextUri, const QString& offsetUri) {
    if (contextUri.trimmed().isEmpty()) {
        return false;
    }

    QJsonObject body;
    body.insert("context_uri", contextUri.trimmed());
    if (!offsetUri.trimmed().isEmpty()) {
        QJsonObject offset;
        offset.insert("uri", offsetUri.trimmed());
        body.insert("offset", offset);
    }

    int statusCode = 0;
    QString responseBody;
    const auto payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    if (requestNoContentDetailed("PUT", "/me/player/play", QUrlQuery(), payload, &statusCode, &responseBody)) {
        return true;
    }

    if (statusCode == 404) {
        debuglog::warn("Spotify", "startPlaybackContext 404; attempting device activation.");
        const auto devices = getDevices();
        QString selectedId;
        for (const auto& d : devices) {
            if (d.isActive && !d.isRestricted && !d.id.trimmed().isEmpty()) {
                selectedId = d.id.trimmed();
                break;
            }
        }
        if (selectedId.isEmpty()) {
            for (const auto& d : devices) {
                if (!d.isRestricted && !d.id.trimmed().isEmpty()) {
                    selectedId = d.id.trimmed();
                    break;
                }
            }
        }
        if (!selectedId.isEmpty() && transferPlaybackToDevice(selectedId, true)) {
            QThread::msleep(250);
            return requestNoContentDetailed("PUT", "/me/player/play", QUrlQuery(), payload, &statusCode, &responseBody);
        }
    }
    return false;
}

QVector<SpotifyTrackItem> SpotifyClient::getAlbumTracks(const QString& albumId, int limit) {
    QVector<SpotifyTrackItem> out;
    const QString id = albumId.trimmed();
    if (id.isEmpty()) {
        return out;
    }

    QUrlQuery query;
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", QString("/albums/%1/tracks").arg(id), query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject obj = value.toObject();
        SpotifyTrackItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.album = QString();
        const QJsonArray artists = obj.value("artists").toArray();
        if (!artists.isEmpty()) {
            item.artist = artists.first().toObject().value("name").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyTrackItem> SpotifyClient::getPlaylistTracks(const QString& playlistId, int limit) {
    QVector<SpotifyTrackItem> out;
    const QString id = playlistId.trimmed();
    if (id.isEmpty()) {
        return out;
    }

    QUrlQuery query;
    query.addQueryItem("limit", QString::number(std::clamp(limit, 1, 50)));

    auto response = requestJson("GET", QString("/playlists/%1/tracks").arg(id), query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("items").toArray();
    out.reserve(items.size());
    for (const auto& value : items) {
        const QJsonObject track = value.toObject().value("track").toObject();
        if (track.isEmpty()) {
            continue;
        }

        SpotifyTrackItem item;
        item.id = track.value("id").toString();
        item.name = track.value("name").toString();
        item.uri = track.value("uri").toString();
        item.album = track.value("album").toObject().value("name").toString();
        const QJsonArray artists = track.value("artists").toArray();
        if (!artists.isEmpty()) {
            item.artist = artists.first().toObject().value("name").toString();
        }
        const QJsonArray albumImages = track.value("album").toObject().value("images").toArray();
        if (!albumImages.isEmpty()) {
            item.albumArtUrl = albumImages.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }
    return out;
}

QVector<SpotifyTrackItem> SpotifyClient::getArtistTopTracks(const QString& artistId, int limit) {
    QVector<SpotifyTrackItem> out;
    const QString id = artistId.trimmed();
    if (id.isEmpty()) {
        return out;
    }

    QUrlQuery query;
    query.addQueryItem("market", "from_token");

    auto response = requestJson("GET", QString("/artists/%1/top-tracks").arg(id), query);
    if (!response.has_value()) {
        return out;
    }

    const QJsonArray items = response->value("tracks").toArray();
    const int bounded = std::clamp(limit, 1, 50);
    out.reserve(std::min(static_cast<int>(items.size()), bounded));
    for (int i = 0; i < items.size() && out.size() < bounded; ++i) {
        const QJsonObject obj = items[i].toObject();
        SpotifyTrackItem item;
        item.id = obj.value("id").toString();
        item.name = obj.value("name").toString();
        item.uri = obj.value("uri").toString();
        item.album = obj.value("album").toObject().value("name").toString();
        const QJsonArray artists = obj.value("artists").toArray();
        if (!artists.isEmpty()) {
            item.artist = artists.first().toObject().value("name").toString();
        }
        const QJsonArray images = obj.value("album").toObject().value("images").toArray();
        if (!images.isEmpty()) {
            item.albumArtUrl = images.first().toObject().value("url").toString();
        }
        out.push_back(item);
    }
    return out;
}
