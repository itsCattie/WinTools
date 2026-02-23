#pragma once

// WinTools: theme manager manages UI behavior and presentation.

namespace wintools::ui {

enum class Theme {
    Light,
    Dark
};

class ThemeManager {
public:
    ThemeManager();
    Theme currentTheme() const;

private:
    Theme m_currentTheme;
};

}
