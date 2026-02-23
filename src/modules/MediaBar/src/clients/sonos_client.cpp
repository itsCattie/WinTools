#include "sonos_client.hpp"

#include "config.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUdpSocket>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>

// MediaBar: sonos client manages service client integration.

namespace {

constexpr auto SONOS_AV_TRANSPORT_CONTROL_PATH = "/MediaRenderer/AVTransport/Control";
constexpr auto SONOS_AV_TRANSPORT_SERVICE = "urn:schemas-upnp-org:service:AVTransport:1";
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

    QUdpSocket socket;
    socket.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    const QByteArray request =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 1\r\n"
        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n\r\n";

    socket.writeDatagram(request, QHostAddress("239.255.255.250"), 1900);

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < 1800) {
        if (!socket.waitForReadyRead(250)) {
            continue;
        }
        while (socket.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(static_cast<int>(socket.pendingDatagramSize()));
            QHostAddress sender;
            quint16 senderPort = 0;
            socket.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

            const QString text = QString::fromUtf8(datagram);
            if (!text.contains("zoneplayer", Qt::CaseInsensitive) && !text.contains("sonos", Qt::CaseInsensitive)) {
                continue;
            }

            QRegularExpression locationRe(R"(LOCATION:\s*https?://([^/:\r\n]+))", QRegularExpression::CaseInsensitiveOption);
            const auto match = locationRe.match(text);
            if (match.hasMatch()) {
                discoveredSpeakerIp_ = match.captured(1).trimmed();
                if (!discoveredSpeakerIp_.isEmpty()) {
                    return discoveredSpeakerIp_;
                }
            }

            if (sender.protocol() == QAbstractSocket::IPv4Protocol) {
                const QString senderIp = sender.toString().trimmed();
                if (!senderIp.isEmpty()) {
                    discoveredSpeakerIp_ = senderIp;
                    return discoveredSpeakerIp_;
                }
            }
        }
    }

    return QString();
}

QString SonosClient::speakerIp() const {
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

bool SonosClient::inBackoff() const {
    return QDateTime::currentMSecsSinceEpoch() < backoffUntilMs_;
}

void SonosClient::noteBackoff(int seconds) {
    const int bounded = std::clamp(seconds, 5, 60);
    backoffUntilMs_ = QDateTime::currentMSecsSinceEpoch() + (static_cast<qint64>(bounded) * 1000);
}

std::optional<QByteArray> SonosClient::sendSoapAction(const QString& action, const QString& bodyXml) {
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
            .arg(action, SONOS_AV_TRANSPORT_SERVICE, bodyXml);

    QNetworkRequest req(controlUrl());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPACTION", QString("\"%1#%2\"").arg(SONOS_AV_TRANSPORT_SERVICE, action).toUtf8());

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
