#include "common/themes/theme_helper.hpp"

#include <QSettings>

// WinTools: theme helper manages shared infrastructure.

namespace wintools::themes {

bool ThemeHelper::isSystemDarkTheme() {
#ifdef _WIN32
    QSettings settings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 0).toInt() == 0;
#else
    return true;
#endif
}

ThemePalette ThemeHelper::currentPalette() {
    return isSystemDarkTheme() ? WindowColour::dark() : WindowColour::light();
}

}
