#include "common/themes/theme_helper.hpp"

#include "common/themes/fluent_style.hpp"

#include <QComboBox>
#include <QSettings>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#include <cstring>
#endif

namespace wintools::themes {

namespace {
constexpr const char* ThemeModeKey = "ui/global/theme_mode";

ThemeHelper::ThemeMode sanitizeThemeMode(int rawValue) {
    switch (rawValue) {
    case static_cast<int>(ThemeHelper::ThemeMode::Light):
        return ThemeHelper::ThemeMode::Light;
    case static_cast<int>(ThemeHelper::ThemeMode::Dark):
        return ThemeHelper::ThemeMode::Dark;
    case static_cast<int>(ThemeHelper::ThemeMode::Midnight):
        return ThemeHelper::ThemeMode::Midnight;
    case static_cast<int>(ThemeHelper::ThemeMode::Forest):
        return ThemeHelper::ThemeMode::Forest;
    case static_cast<int>(ThemeHelper::ThemeMode::Rose):
        return ThemeHelper::ThemeMode::Rose;
    default:
        return ThemeHelper::ThemeMode::System;
    }
}
}

ThemeHelper::ThemeMode ThemeHelper::themeMode() {
    QSettings settings;
    const int stored = settings.value(ThemeModeKey,
                                      static_cast<int>(ThemeMode::System)).toInt();
    return sanitizeThemeMode(stored);
}

void ThemeHelper::setThemeMode(ThemeMode mode) {
    QSettings settings;
    settings.setValue(ThemeModeKey, static_cast<int>(mode));

    notifyThemeChanged();
}

void ThemeHelper::notifyThemeChanged() {

    ThemeNotifier::instance()->themeChanged(static_cast<int>(themeMode()));
}

ThemeNotifier* ThemeNotifier::instance() {
    static ThemeNotifier inst;
    return &inst;
}

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

bool ThemeHelper::isDarkTheme() {
    switch (themeMode()) {
    case ThemeMode::Light:
        return false;
    case ThemeMode::Rose:
        return false;
    case ThemeMode::Dark:
    case ThemeMode::Midnight:
    case ThemeMode::Forest:
        return true;
    case ThemeMode::System:
    default:
        return isSystemDarkTheme();
    }
}

ThemePalette ThemeHelper::currentPalette() {
    switch (themeMode()) {
    case ThemeMode::Light:
        return WindowColour::light();
    case ThemeMode::Dark:
        return WindowColour::dark();
    case ThemeMode::Midnight:
        return WindowColour::midnight();
    case ThemeMode::Forest:
        return WindowColour::forest();
    case ThemeMode::Rose:
        return WindowColour::rose();
    case ThemeMode::System:
    default:
        return isSystemDarkTheme() ? WindowColour::dark() : WindowColour::light();
    }
}

void ThemeHelper::applyNativeTitleBarTheme(QWidget* window, bool darkTheme) {
#ifdef _WIN32
    if (!window) return;

    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;

    FARPROC raw = GetProcAddress(dwm, "DwmSetWindowAttribute");
    DwmSetWindowAttributeFn fn = nullptr;
    static_assert(sizeof(fn) == sizeof(raw), "Function pointer size mismatch");
    std::memcpy(&fn, &raw, sizeof(fn));
    if (!fn) {
        FreeLibrary(dwm);
        return;
    }

    const BOOL useDark = darkTheme ? TRUE : FALSE;
    const DWORD attr20 = 20;
    const DWORD attr19 = 19;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    fn(hwnd, attr20, &useDark, sizeof(useDark));
    fn(hwnd, attr19, &useDark, sizeof(useDark));

    FreeLibrary(dwm);
#else
    Q_UNUSED(window);
    Q_UNUSED(darkTheme);
#endif
}

void ThemeHelper::applyThemeTo(QWidget* window) {
    if (!window) return;
    const ThemePalette p = currentPalette();
    window->setStyleSheet(wintools::themes::FluentStyle::generate(p));
    applyNativeTitleBarTheme(window, isDarkTheme());
}

void ThemeHelper::applyThemeTo(QWidget* window, const QString& extraQss) {
    if (!window) return;
    const ThemePalette p = currentPalette();
    QString full = wintools::themes::FluentStyle::generate(p);
    if (!extraQss.isEmpty()) full += extraQss;
    window->setStyleSheet(full);
    applyNativeTitleBarTheme(window, isDarkTheme());
}

void ThemeHelper::populateThemeCombo(QComboBox* combo) {
    if (!combo) return;
    combo->addItem(QStringLiteral("System"),   static_cast<int>(ThemeMode::System));
    combo->addItem(QStringLiteral("Light"),    static_cast<int>(ThemeMode::Light));
    combo->addItem(QStringLiteral("Dark"),     static_cast<int>(ThemeMode::Dark));
    combo->addItem(QStringLiteral("Midnight"), static_cast<int>(ThemeMode::Midnight));
    combo->addItem(QStringLiteral("Forest"),   static_cast<int>(ThemeMode::Forest));
    combo->addItem(QStringLiteral("Rose"),     static_cast<int>(ThemeMode::Rose));
}

}
