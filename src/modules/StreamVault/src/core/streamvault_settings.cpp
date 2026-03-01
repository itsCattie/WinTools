#include "streamvault_settings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace wintools::streamvault {

static constexpr char kGroup[]    = "wintools/streamvault";
static constexpr char kApiKey[]   = "tmdb_api_key";
static constexpr char kAdult[]    = "show_adult";
static constexpr char kLang[]     = "language";
static constexpr char kCustom[]   = "custom_services";

StreamVaultSettings& StreamVaultSettings::instance() {
    static StreamVaultSettings s;
    return s;
}

QString StreamVaultSettings::tmdbApiKey() const {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    return cfg.value(kApiKey, QString{}).toString();
}

void StreamVaultSettings::setTmdbApiKey(const QString& key) {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    cfg.setValue(kApiKey, key.trimmed());
}

bool StreamVaultSettings::showAdultContent() const {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    return cfg.value(kAdult, false).toBool();
}

void StreamVaultSettings::setShowAdultContent(bool enabled) {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    cfg.setValue(kAdult, enabled);
}

QString StreamVaultSettings::defaultLanguage() const {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    return cfg.value(kLang, QStringLiteral("en-US")).toString();
}

void StreamVaultSettings::setDefaultLanguage(const QString& lang) {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    cfg.setValue(kLang, lang);
}

QVector<ServiceInfo> StreamVaultSettings::customServices() const {
    QSettings cfg;
    cfg.beginGroup(kGroup);
    const QString serialized = cfg.value(kCustom, QString{}).toString().trimmed();
    if (serialized.isEmpty()) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(serialized.toUtf8());
    if (!doc.isArray()) {
        return {};
    }

    QVector<ServiceInfo> services;
    const QJsonArray arr = doc.array();
    for (const QJsonValue& value : arr) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject obj = value.toObject();
        ServiceInfo entry;
        entry.id = StreamingService::COUNT;
        entry.name = obj.value("name").toString().trimmed();
        entry.accentColor = obj.value("accent").toString().trimmed();
        entry.homeUrl = obj.value("home").toString().trimmed();
        entry.loginUrl = obj.value("login").toString().trimmed();
        entry.searchUrlTemplate = obj.value("search").toString().trimmed();
        if (!entry.searchUrlTemplate.contains("%1")) {
            continue;
        }
        if (entry.name.isEmpty()) {
            continue;
        }
        if (entry.accentColor.isEmpty()) {
            entry.accentColor = QStringLiteral("#9AA4AF");
        }
        services.push_back(entry);
    }

    return services;
}

void StreamVaultSettings::setCustomServices(const QVector<ServiceInfo>& services) {
    QJsonArray arr;
    for (const ServiceInfo& svc : services) {
        const QString name = svc.name.trimmed();
        const QString search = svc.searchUrlTemplate.trimmed();
        if (name.isEmpty() || search.isEmpty() || !search.contains("%1")) {
            continue;
        }

        QJsonObject obj;
        obj.insert("name", name);
        obj.insert("accent", svc.accentColor.trimmed().isEmpty() ? QStringLiteral("#9AA4AF") : svc.accentColor.trimmed());
        obj.insert("home", svc.homeUrl.trimmed());
        obj.insert("login", svc.loginUrl.trimmed());
        obj.insert("search", search);
        arr.push_back(obj);
    }

    QSettings cfg;
    cfg.beginGroup(kGroup);
    cfg.setValue(kCustom, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QString StreamVaultSettings::posterBaseUrl() {
    return QStringLiteral("https://image.tmdb.org/t/p/w300");
}

QString StreamVaultSettings::backdropBaseUrl() {
    return QStringLiteral("https://image.tmdb.org/t/p/w780");
}

}
