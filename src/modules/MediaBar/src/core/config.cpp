#include "config.hpp"

#include "debug_logger.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

// MediaBar: config manages core logic and state.

namespace {

QString ensureDir(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

}

namespace config {

QString appDataDir() {
    QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return ensureDir(QDir(appData).filePath("MediaBar"));
}

QString albumArtCacheDir() {
    return ensureDir(QDir(appDataDir()).filePath("album_art_cache"));
}

QString settingsFilePath() {
    return QDir(appDataDir()).filePath("settings.json");
}

QJsonObject defaultSettings() {
    QJsonObject obj;
    obj.insert("music_library_path", QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    obj.insert("show_placeholder_art", true);
    obj.insert("placeholder_art_style", "gradient");
    obj.insert("auto_download_art", true);
    obj.insert("debug_mode", false);
    obj.insert("show_source_indicator", true);
    obj.insert("mini_player_opacity", 1.0);
    obj.insert("theme", "dark");
    obj.insert("sidebar_show_labels", false);
    obj.insert("large_text", false);
    obj.insert("high_contrast", false);
    obj.insert("reduced_motion", false);
    obj.insert("spotify_access_token", "");
    obj.insert("spotify_refresh_token", "");
    obj.insert("spotify_token_expires_at", 0);
    obj.insert("mini_player_transparent", false);
    obj.insert("mini_player_bg_color", "#111111");
    obj.insert("mini_player_control_color", "#000000");
    obj.insert("mini_player_text_color", "#FFFFFF");
    obj.insert("sonos_speaker_ip", "");
    obj.insert("playback_source", "auto");
    obj.insert("search_sources", QJsonArray{QString("spotify"), QString("sonos")});
    obj.insert("favorite_items", QJsonArray{});
    return obj;
}

QJsonObject loadSettings() {
    debuglog::trace("Config", QString("loadSettings path=%1").arg(settingsFilePath()));
    const QString path = settingsFilePath();
    QFile file(path);
    QJsonObject defaults = defaultSettings();

    if (!file.exists()) {
        debuglog::warn("Config", "Settings file missing; writing defaults.");
        saveSettings(defaults);
        return defaults;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        debuglog::error("Config", "Could not open settings file for read; using defaults.");
        return defaults;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        debuglog::warn("Config", "Settings JSON invalid object; using defaults.");
        return defaults;
    }

    QJsonObject merged = defaults;
    const QJsonObject user = doc.object();
    for (auto it = user.begin(); it != user.end(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return merged;
}

bool saveSettings(const QJsonObject& settings) {
    debuglog::trace("Config", QString("saveSettings path=%1").arg(settingsFilePath()));
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        debuglog::error("Config", "Failed to open settings file for write.");
        return false;
    }
    file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented));
    file.close();
    debuglog::info("Config", "Settings saved.");
    return true;
}

QString settingString(const QString& key, const QString& fallback) {
    const auto settings = loadSettings();
    if (!settings.contains(key)) {
        return fallback;
    }
    return settings.value(key).toString(fallback);
}

bool settingBool(const QString& key, bool fallback) {
    const auto settings = loadSettings();
    if (!settings.contains(key)) {
        return fallback;
    }
    return settings.value(key).toBool(fallback);
}

double settingDouble(const QString& key, double fallback) {
    const auto settings = loadSettings();
    if (!settings.contains(key)) {
        return fallback;
    }
    return settings.value(key).toDouble(fallback);
}

}
