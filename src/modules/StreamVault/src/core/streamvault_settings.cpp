#include "streamvault_settings.hpp"

#include <QSettings>

// StreamVault: streamvault settings manages core logic and state.

namespace wintools::streamvault {

static constexpr char kGroup[]    = "wintools/streamvault";
static constexpr char kApiKey[]   = "tmdb_api_key";
static constexpr char kAdult[]    = "show_adult";
static constexpr char kLang[]     = "language";

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

QString StreamVaultSettings::posterBaseUrl() {
    return QStringLiteral("https://image.tmdb.org/t/p/w300");
}

QString StreamVaultSettings::backdropBaseUrl() {
    return QStringLiteral("https://image.tmdb.org/t/p/w780");
}

}
