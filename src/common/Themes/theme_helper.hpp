#pragma once

#include "common/themes/window_colour.hpp"

#include <QObject>

class QComboBox;
class QWidget;

namespace wintools::themes {

class ThemeHelper {
public:
    enum class ThemeMode {
        System = 0,
        Light = 1,
        Dark = 2,
        Midnight = 3,
        Forest = 4,
        Rose = 5
    };

    static ThemeMode themeMode();
    static void setThemeMode(ThemeMode mode);

    static bool isSystemDarkTheme();
    static bool isDarkTheme();
    static ThemePalette currentPalette();
    static void applyNativeTitleBarTheme(QWidget* window, bool darkTheme);

    static void applyThemeTo(QWidget* window);
    static void applyThemeTo(QWidget* window, const QString& extraQss);

    static void notifyThemeChanged();

    static void populateThemeCombo(QComboBox* combo);
};

class ThemeNotifier : public QObject {
    Q_OBJECT
public:
    static ThemeNotifier* instance();
signals:

    void themeChanged(int themeMode);
};

}
