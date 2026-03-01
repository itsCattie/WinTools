#pragma once

#include <QJsonObject>
#include <QString>

namespace config {

inline constexpr int UPDATE_INTERVAL_MS = 500;
inline constexpr int WINDOW_WIDTH = 650;
inline constexpr int WINDOW_HEIGHT = 850;

inline constexpr const char* FONT_FAMILY = "Segoe UI";
inline constexpr const char* HIGHLIGHT_COLOR = "#1DB954";
inline constexpr const char* HIGHLIGHT_HOVER = "#1ED760";
inline constexpr const char* TEXT_COLOR = "#FFFFFF";
inline constexpr const char* BACKGROUND_COLOR = "#0D0D0D";
inline constexpr const char* SURFACE_COLOR = "#181818";
inline constexpr const char* SURFACE_HOVER = "#252525";
inline constexpr const char* INACTIVE_TEXT_COLOR = "#A0A0A0";
inline constexpr const char* BORDER_COLOR = "#333333";
inline constexpr const char* CARD_COLOR = "#1E1E1E";
inline constexpr const char* CARD_HOVER = "#2A2A2A";
inline constexpr int SIDEBAR_EXPANDED_WIDTH = 240;
inline constexpr int SIDEBAR_COLLAPSED_WIDTH = 56;

QString appDataDir();
QString albumArtCacheDir();
QString settingsFilePath();

QJsonObject defaultSettings();
QJsonObject loadSettings();
bool saveSettings(const QJsonObject& settings);

QString settingString(const QString& key, const QString& fallback = QString());
bool settingBool(const QString& key, bool fallback = false);
double settingDouble(const QString& key, double fallback = 0.0);

}
