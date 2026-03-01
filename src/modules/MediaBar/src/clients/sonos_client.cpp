#include "sonos_client.hpp"

#include "config.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUdpSocket>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

constexpr auto SONOS_AV_TRANSPORT_CONTROL_PATH = "/MediaRenderer/AVTransport/Control";
constexpr auto SONOS_AV_TRANSPORT_SERVICE = "urn:schemas-upnp-org:service:AVTransport:1";
constexpr auto SONOS_CONTENT_DIRECTORY_PATH = "/MediaServer/ContentDirectory/Control";
constexpr auto SONOS_CONTENT_DIRECTORY_SERVICE = "urn:schemas-upnp-org:service:ContentDirectory:1";
constexpr auto SONOS_RENDERING_CONTROL_PATH = "/MediaRenderer/RenderingControl/Control";
constexpr auto SONOS_RENDERING_CONTROL_SERVICE = "urn:schemas-upnp-org:service:RenderingControl:1";
constexpr int REQUEST_TIMEOUT_MS = 2500;

QString xmlEscaped(const QString& value) {
    QString out = value;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    out.replace("'", "&apos;");
    return out;
}

}

SonosClient::SonosClient() = default;

bool SonosClient::isAvailable() const {
    return hasSpeakerIp();
}

bool SonosClient::hasSpeakerIp() const {
    return !speakerIp().isEmpty();
}

QString SonosClient::discoverSpeakerIp() const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!discoveredSpeakerIp_.isEmpty() && (now - lastDiscoverAtMs_) < 30000) {
        return discoveredSpeakerIp_;
    }

    lastDiscoverAtMs_ = now;

    const QByteArray request =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n\r\n";

    const QHostAddress multicastAddr("239.255.255.250");
    constexpr quint16 SSDP_PORT = 1900;
    constexpr int MAX_RETRIES = 3;
    constexpr int LISTEN_MS = 2500;

    QVector<QHostAddress> localAddrs;
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                localAddrs.push_back(entry.ip());
            }
        }
    }
    if (localAddrs.isEmpty()) {
        localAddrs.push_back(QHostAddress::AnyIPv4);
    }

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {

        QVector<QUdpSocket*> sockets;
        for (const auto& addr : localAddrs) {
            auto* sock = new QUdpSocket();
            sock->bind(addr, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
            sock->writeDatagram(request, multicastAddr, SSDP_PORT);
            sockets.push_back(sock);
        }

        QElapsedTimer timer;
        timer.start();

        while (timer.elapsed() < LISTEN_MS) {
            for (auto* sock : sockets) {
                if (!sock->waitForReadyRead(100)) continue;
                while (sock->hasPendingDatagrams()) {
                    QByteArray datagram;
                    datagram.resize(static_cast<int>(sock->pendingDatagramSize()));
                    QHostAddress sender;
                    quint16 senderPort = 0;
                    sock->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

                    const QString text = QString::fromUtf8(datagram);
                    if (!text.contains("zoneplayer", Qt::CaseInsensitive) &&
                        !text.contains("sonos", Qt::CaseInsensitive)) {
                        continue;
                    }

                    QRegularExpression locationRe(R"(LOCATION:\s*https?://([^/:\r\n]+))",
                                                  QRegularExpression::CaseInsensitiveOption);
                    const auto match = locationRe.match(text);
                    if (match.hasMatch()) {
                        discoveredSpeakerIp_ = match.captured(1).trimmed();
                        if (!discoveredSpeakerIp_.isEmpty()) {
                            qDeleteAll(sockets);
                            return discoveredSpeakerIp_;
                        }
                    }

                    if (sender.protocol() == QAbstractSocket::IPv4Protocol) {
                        const QString senderIp = sender.toString().trimmed();
                        if (!senderIp.isEmpty()) {
                            discoveredSpeakerIp_ = senderIp;
                            qDeleteAll(sockets);
                            return discoveredSpeakerIp_;
                        }
                    }
                }
            }
        }

        qDeleteAll(sockets);
    }

    return QString();
}

QString SonosClient::speakerIp() const {

    if (!userSelectedIp_.isEmpty()) {
        return userSelectedIp_;
    }

    const QString envIp = qEnvironmentVariable("SONOS_SPEAKER_IP").trimmed();
    if (!envIp.isEmpty()) {
        return envIp;
    }

    const QString configured = config::settingString("sonos_speaker_ip", "").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }

    return discoverSpeakerIp();
}

QUrl SonosClient::controlUrl() const {
    return QUrl(QString("http://%1:1400%2").arg(speakerIp(), SONOS_AV_TRANSPORT_CONTROL_PATH));
}

QUrl SonosClient::contentDirectoryUrl() const {
    return QUrl(QString("http://%1:1400%2").arg(speakerIp(), SONOS_CONTENT_DIRECTORY_PATH));
}

QUrl SonosClient::renderingControlUrl() const {
    return QUrl(QString("http://%1:1400%2").arg(speakerIp(), SONOS_RENDERING_CONTROL_PATH));
}

bool SonosClient::inBackoff() const {
    return QDateTime::currentMSecsSinceEpoch() < backoffUntilMs_;
}

void SonosClient::noteBackoff(int seconds) {
    const int bounded = std::clamp(seconds, 5, 60);
    backoffUntilMs_ = QDateTime::currentMSecsSinceEpoch() + (static_cast<qint64>(bounded) * 1000);
}

std::optional<QByteArray> SonosClient::sendSoapAction(const QString& action, const QString& bodyXml) {
    return sendSoapActionTo(controlUrl(), SONOS_AV_TRANSPORT_SERVICE, action, bodyXml);
}

std::optional<QByteArray> SonosClient::sendSoapActionTo(const QUrl& url, const QString& serviceUrn,
                                                         const QString& action, const QString& bodyXml) {
    if (!hasSpeakerIp() || inBackoff()) {
        return std::nullopt;
    }

    const QString envelope = QString(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:%1 xmlns:u=\"%2\">%3</u:%1>"
        "</s:Body>"
        "</s:Envelope>")
            .arg(action, serviceUrn, bodyXml);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPACTION", QString("\"%1#%2\"").arg(serviceUrn, action).toUtf8());

    QNetworkReply* reply = network_.post(req, envelope.toUtf8());
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });

    timeout.start(REQUEST_TIMEOUT_MS);
    loop.exec();

    const QByteArray payload = reply->readAll();
    const auto error = reply->error();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
        noteBackoff(20);
        return std::nullopt;
    }
    return payload;
}

QString SonosClient::extractTagValue(const QString& xml, const QString& tagName) {
    const QRegularExpression re(
        QString("<%1>(.*?)</%1>").arg(QRegularExpression::escape(tagName)),
        QRegularExpression::DotMatchesEverythingOption);
    const auto match = re.match(xml);
    if (!match.hasMatch()) {
        return QString();
    }
    return match.captured(1).trimmed();
}

qint64 SonosClient::timeToMs(const QString& text) {
    const QStringList parts = text.trimmed().split(':');
    if (parts.size() != 2 && parts.size() != 3) {
        return 0;
    }

    bool ok = false;
    int h = 0;
    int m = 0;
    int s = 0;
    if (parts.size() == 3) {
        h = parts[0].toInt(&ok);
        if (!ok) return 0;
        m = parts[1].toInt(&ok);
        if (!ok) return 0;
        s = parts[2].toInt(&ok);
        if (!ok) return 0;
    } else {
        m = parts[0].toInt(&ok);
        if (!ok) return 0;
        s = parts[1].toInt(&ok);
        if (!ok) return 0;
    }

    return static_cast<qint64>(h * 3600 + m * 60 + s) * 1000;
}

std::optional<PlaybackInfo> SonosClient::getCurrentPlayback() {
    auto transportResp = sendSoapAction(
        "GetTransportInfo",
        "<InstanceID>0</InstanceID>");
    if (!transportResp.has_value()) {
        return std::nullopt;
    }

    auto positionResp = sendSoapAction(
        "GetPositionInfo",
        "<InstanceID>0</InstanceID>");
    if (!positionResp.has_value()) {
        return std::nullopt;
    }

    const QString transportXml = QString::fromUtf8(transportResp.value());
    const QString positionXml = QString::fromUtf8(positionResp.value());

    const QString state = extractTagValue(transportXml, "CurrentTransportState");
    const QString trackUri = extractTagValue(positionXml, "TrackURI");
    const QString relTime = extractTagValue(positionXml, "RelTime");
    const QString duration = extractTagValue(positionXml, "TrackDuration");
    const QString metadata = extractTagValue(positionXml, "TrackMetaData");

    PlaybackInfo info;
    info.valid = true;
    info.source = "sonos";
    info.isPlaying = state == "PLAYING";
    info.trackUri = trackUri;
    info.trackId = trackUri;
    info.progressMs = timeToMs(relTime);
    info.durationMs = timeToMs(duration);

    if (!metadata.isEmpty()) {
        QXmlStreamReader xml(metadata);
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }

            const QString name = xml.name().toString();
            if (name == "title" && info.trackName.isEmpty()) {
                info.trackName = xml.readElementText().trimmed();
            } else if ((name == "creator" || name == "artist") && info.artistName.isEmpty()) {
                info.artistName = xml.readElementText().trimmed();
            } else if (name == "album" && info.albumName.isEmpty()) {
                info.albumName = xml.readElementText().trimmed();
            } else if (name == "albumArtURI" && info.albumArt.isEmpty()) {
                info.albumArt = xml.readElementText().trimmed();
            }
        }
    }

    if (info.trackName.isEmpty()) {
        info.trackName = "Unknown";
    }
    if (info.artistName.isEmpty()) {
        info.artistName = "Unknown";
    }

    if (!info.albumArt.isEmpty() && info.albumArt.startsWith('/')) {
        info.albumArt = QString("http://%1:1400%2").arg(speakerIp(), info.albumArt);
    }

    if (trackUri.isEmpty() && info.trackName == "Unknown") {
        return std::nullopt;
    }
    return info;
}

bool SonosClient::nextTrack() {
    const auto resp = sendSoapAction(
        "Next",
        "<InstanceID>0</InstanceID><Speed>1</Speed>");
    return resp.has_value();
}

bool SonosClient::previousTrack() {
    const auto resp = sendSoapAction(
        "Previous",
        "<InstanceID>0</InstanceID><Speed>1</Speed>");
    return resp.has_value();
}

bool SonosClient::playPause() {
    auto transportResp = sendSoapAction(
        "GetTransportInfo",
        "<InstanceID>0</InstanceID>");
    if (!transportResp.has_value()) {
        return false;
    }

    const QString transportXml = QString::fromUtf8(transportResp.value());
    const QString state = extractTagValue(transportXml, "CurrentTransportState");

    if (state == "PLAYING") {
        const auto pauseResp = sendSoapAction("Pause", "<InstanceID>0</InstanceID>");
        return pauseResp.has_value();
    }

    const auto playResp = sendSoapAction(
        "Play",
        "<InstanceID>0</InstanceID><Speed>1</Speed>");
    return playResp.has_value();
}

bool SonosClient::seekToPosition(qint64 positionMs) {
    const qint64 totalSeconds = std::max<qint64>(0, positionMs) / 1000;
    const qint64 h = totalSeconds / 3600;
    const qint64 m = (totalSeconds % 3600) / 60;
    const qint64 s = totalSeconds % 60;

    const QString target = QString("%1:%2:%3")
                               .arg(h)
                               .arg(m, 2, 10, QChar('0'))
                               .arg(s, 2, 10, QChar('0'));

    const auto resp = sendSoapAction(
        "Seek",
        QString("<InstanceID>0</InstanceID><Unit>REL_TIME</Unit><Target>%1</Target>")
            .arg(xmlEscaped(target)));
    return resp.has_value();
}

bool SonosClient::playUriNow(const QString& uri) {
    const QString normalized = uri.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const auto setResp = sendSoapAction(
        "SetAVTransportURI",
        QString("<InstanceID>0</InstanceID>"
                "<CurrentURI>%1</CurrentURI>"
                "<CurrentURIMetaData></CurrentURIMetaData>")
            .arg(xmlEscaped(normalized)));
    if (!setResp.has_value()) {
        return false;
    }

    const auto playResp = sendSoapAction(
        "Play",
        "<InstanceID>0</InstanceID><Speed>1</Speed>");
    return playResp.has_value();
}

bool SonosClient::addToQueueUri(const QString& uri, bool enqueueAsNext) {
    const QString normalized = uri.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const auto resp = sendSoapAction(
        "AddURIToQueue",
        QString("<InstanceID>0</InstanceID>"
                "<EnqueuedURI>%1</EnqueuedURI>"
                "<EnqueuedURIMetaData></EnqueuedURIMetaData>"
                "<DesiredFirstTrackNumberEnqueued>0</DesiredFirstTrackNumberEnqueued>"
                "<EnqueueAsNext>%2</EnqueueAsNext>")
            .arg(xmlEscaped(normalized), enqueueAsNext ? "1" : "0"));
    return resp.has_value();
}

bool SonosClient::removeFromQueue(int queueIndex) {

    const QString objectId = QString("Q:0/%1").arg(queueIndex + 1);

    const auto resp = sendSoapActionTo(
        contentDirectoryUrl(), SONOS_CONTENT_DIRECTORY_SERVICE,
        "DestroyObject",
        QString("<ObjectID>%1</ObjectID>").arg(xmlEscaped(objectId)));
    return resp.has_value();
}

QVector<SonosClient::SonosQueueItem> SonosClient::getQueue(int maxItems) {
    QVector<SonosQueueItem> out;

    const auto resp = sendSoapActionTo(
        contentDirectoryUrl(), SONOS_CONTENT_DIRECTORY_SERVICE,
        "Browse",
        QString("<ObjectID>Q:0</ObjectID>"
                "<BrowseFlag>BrowseDirectChildren</BrowseFlag>"
                "<Filter>dc:title,upnp:artist,upnp:album,upnp:albumArtURI,res</Filter>"
                "<StartingIndex>0</StartingIndex>"
                "<RequestedCount>%1</RequestedCount>"
                "<SortCriteria></SortCriteria>")
            .arg(maxItems));

    if (!resp.has_value()) return out;

    const QString responseXml = QString::fromUtf8(resp.value());
    QString didl = extractTagValue(responseXml, "Result");

    didl.replace("&lt;", "<");
    didl.replace("&gt;", ">");
    didl.replace("&amp;", "&");
    didl.replace("&quot;", "\"");
    didl.replace("&apos;", "'");

    if (didl.isEmpty()) return out;

    QXmlStreamReader xml(didl);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name().toString() == "item") {
            SonosQueueItem item;

            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name().toString() == "item") break;
                if (!xml.isStartElement()) continue;

                const QString tag = xml.name().toString();
                if (tag == "title") {
                    item.title = xml.readElementText().trimmed();
                } else if (tag == "creator" || tag == "artist") {
                    if (item.artist.isEmpty())
                        item.artist = xml.readElementText().trimmed();
                } else if (tag == "album") {
                    item.album = xml.readElementText().trimmed();
                } else if (tag == "albumArtURI") {
                    item.albumArtUri = xml.readElementText().trimmed();
                } else if (tag == "res") {
                    item.uri = xml.readElementText().trimmed();
                }
            }
            if (!item.title.isEmpty()) {

                if (!item.albumArtUri.isEmpty() && item.albumArtUri.startsWith('/')) {
                    item.albumArtUri = QString("http://%1:1400%2").arg(speakerIp(), item.albumArtUri);
                }
                out.push_back(item);
            }
        }
    }

    return out;
}

QVector<SonosClient::SonosFavourite> SonosClient::getFavourites(int maxItems) {
    QVector<SonosFavourite> out;

    const auto resp = sendSoapActionTo(
        contentDirectoryUrl(), SONOS_CONTENT_DIRECTORY_SERVICE,
        "Browse",
        QString("<ObjectID>FV:2</ObjectID>"
                "<BrowseFlag>BrowseDirectChildren</BrowseFlag>"
                "<Filter>dc:title,res,upnp:albumArtURI,upnp:class</Filter>"
                "<StartingIndex>0</StartingIndex>"
                "<RequestedCount>%1</RequestedCount>"
                "<SortCriteria></SortCriteria>")
            .arg(maxItems));

    if (!resp.has_value()) return out;

    const QString responseXml = QString::fromUtf8(resp.value());
    QString didl = extractTagValue(responseXml, "Result");
    didl.replace("&lt;", "<");
    didl.replace("&gt;", ">");
    didl.replace("&amp;", "&");
    didl.replace("&quot;", "\"");
    didl.replace("&apos;", "'");

    if (didl.isEmpty()) return out;

    QXmlStreamReader xml(didl);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString tag = xml.name().toString();
        if (tag != "item" && tag != "container") continue;

        SonosFavourite fav;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isEndElement() && (xml.name().toString() == "item" || xml.name().toString() == "container")) break;
            if (!xml.isStartElement()) continue;

            const QString inner = xml.name().toString();
            if (inner == "title") {
                fav.title = xml.readElementText().trimmed();
            } else if (inner == "res") {
                fav.uri = xml.readElementText().trimmed();
            } else if (inner == "albumArtURI") {
                fav.albumArtUri = xml.readElementText().trimmed();
            } else if (inner == "class") {
                const QString cls = xml.readElementText().trimmed().toLower();
                if (cls.contains("track") || cls.contains("audio")) fav.type = "track";
                else if (cls.contains("playlist")) fav.type = "playlist";
                else if (cls.contains("album")) fav.type = "album";
                else if (cls.contains("radio") || cls.contains("broadcast")) fav.type = "radio";
                else fav.type = "other";
            }
        }
        if (!fav.title.isEmpty()) {
            if (!fav.albumArtUri.isEmpty() && fav.albumArtUri.startsWith('/')) {
                fav.albumArtUri = QString("http://%1:1400%2").arg(speakerIp(), fav.albumArtUri);
            }
            out.push_back(fav);
        }
    }

    return out;
}

QVector<SonosClient::SonosQueueItem> SonosClient::browsePlaylist(const QString& uri, int maxItems) {
    QVector<SonosQueueItem> out;

    const auto resp = sendSoapActionTo(
        contentDirectoryUrl(), SONOS_CONTENT_DIRECTORY_SERVICE,
        "Browse",
        QString("<ObjectID>%1</ObjectID>"
                "<BrowseFlag>BrowseDirectChildren</BrowseFlag>"
                "<Filter>dc:title,upnp:artist,upnp:album,upnp:albumArtURI,res</Filter>"
                "<StartingIndex>0</StartingIndex>"
                "<RequestedCount>%2</RequestedCount>"
                "<SortCriteria></SortCriteria>")
            .arg(xmlEscaped(uri))
            .arg(maxItems));

    if (!resp.has_value()) return out;

    const QString responseXml = QString::fromUtf8(resp.value());
    QString didl = extractTagValue(responseXml, "Result");
    didl.replace("&lt;", "<");
    didl.replace("&gt;", ">");
    didl.replace("&amp;", "&");
    didl.replace("&quot;", "\"");
    didl.replace("&apos;", "'");

    if (didl.isEmpty()) return out;

    QXmlStreamReader xml(didl);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name().toString() == "item") {
            SonosQueueItem item;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name().toString() == "item") break;
                if (!xml.isStartElement()) continue;
                const QString t = xml.name().toString();
                if (t == "title") item.title = xml.readElementText().trimmed();
                else if (t == "creator" || t == "artist") {
                    if (item.artist.isEmpty()) item.artist = xml.readElementText().trimmed();
                }
                else if (t == "album") item.album = xml.readElementText().trimmed();
                else if (t == "albumArtURI") item.albumArtUri = xml.readElementText().trimmed();
                else if (t == "res") item.uri = xml.readElementText().trimmed();
            }
            if (!item.title.isEmpty()) {
                if (!item.albumArtUri.isEmpty() && item.albumArtUri.startsWith('/'))
                    item.albumArtUri = QString("http://%1:1400%2").arg(speakerIp(), item.albumArtUri);
                out.push_back(item);
            }
        }
    }

    return out;
}

std::optional<int> SonosClient::getVolume() {
    const auto resp = sendSoapActionTo(
        renderingControlUrl(), SONOS_RENDERING_CONTROL_SERVICE,
        "GetVolume",
        "<InstanceID>0</InstanceID><Channel>Master</Channel>");

    if (!resp.has_value()) return std::nullopt;

    const QString xml = QString::fromUtf8(resp.value());
    const QString vol = extractTagValue(xml, "CurrentVolume");
    bool ok = false;
    const int value = vol.toInt(&ok);
    return ok ? std::optional<int>(value) : std::nullopt;
}

bool SonosClient::setVolume(int percent) {
    const int clamped = std::clamp(percent, 0, 100);
    const auto resp = sendSoapActionTo(
        renderingControlUrl(), SONOS_RENDERING_CONTROL_SERVICE,
        "SetVolume",
        QString("<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>%1</DesiredVolume>")
            .arg(clamped));
    return resp.has_value();
}

void SonosClient::setTargetSpeakerIp(const QString& ip) {
    userSelectedIp_ = ip.trimmed();

    discoveredSpeakerIp_.clear();
    backoffUntilMs_ = 0;
}

QString SonosClient::targetSpeakerIp() const {
    return speakerIp();
}

QVector<SonosClient::SonosGroup> SonosClient::getZoneGroups() {
    QVector<SonosGroup> groups;

    const QString ip = speakerIp();
    if (ip.isEmpty()) return groups;

    QNetworkRequest req(QUrl(QString("http://%1:1400/status/topology").arg(ip)));
    req.setTransferTimeout(REQUEST_TIMEOUT_MS);

    QNetworkReply* reply = network_.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        if (reply->isRunning()) reply->abort();
    });
    timeout.start(REQUEST_TIMEOUT_MS);
    loop.exec();

    const QByteArray data = reply->readAll();
    const auto error = reply->error();
    reply->deleteLater();

    if (error != QNetworkReply::NoError || data.isEmpty()) return groups;

    struct RawZone {
        QString name;
        QString ip;
        QString uuid;
        QString groupId;
        bool isCoordinator = false;
    };
    QVector<RawZone> rawZones;

    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name().toString() != "ZonePlayer") continue;

        RawZone z;
        z.name = xml.attributes().value("name").toString();
        z.uuid = xml.attributes().value("uuid").toString();
        z.groupId = xml.attributes().value("group").toString();
        z.isCoordinator = (xml.attributes().value("coordinator").toString() == "true");

        const QString location = xml.attributes().value("location").toString();

        QRegularExpression ipRe(R"(https?://([^/:\r\n]+))");
        const auto match = ipRe.match(location);
        if (match.hasMatch()) {
            z.ip = match.captured(1);
        }

        if (!z.name.isEmpty() && !z.groupId.isEmpty()) {
            rawZones.push_back(z);
        }
    }

    QHash<QString, int> groupIndexMap;
    for (const auto& z : rawZones) {
        SonosZone zone;
        zone.name = z.name;
        zone.ip = z.ip;
        zone.uuid = z.uuid;
        zone.groupId = z.groupId;
        zone.isCoordinator = z.isCoordinator;

        auto it = groupIndexMap.find(z.groupId);
        if (it == groupIndexMap.end()) {
            SonosGroup grp;
            grp.members.push_back(zone);
            if (z.isCoordinator) {
                grp.coordinatorName = z.name;
                grp.coordinatorIp = z.ip;
            }
            groupIndexMap.insert(z.groupId, groups.size());
            groups.push_back(grp);
        } else {
            auto& grp = groups[it.value()];
            grp.members.push_back(zone);
            if (z.isCoordinator) {
                grp.coordinatorName = z.name;
                grp.coordinatorIp = z.ip;
            }
        }
    }

    for (auto& grp : groups) {
        if (grp.coordinatorName.isEmpty() && !grp.members.isEmpty()) {
            grp.coordinatorName = grp.members.first().name;
            grp.coordinatorIp = grp.members.first().ip;
        }
    }

    return groups;
}
