#include "common/ui/theme_manager.hpp"

// WinTools: theme manager manages UI behavior and presentation.

namespace wintools::ui {

ThemeManager::ThemeManager()
    : m_currentTheme(Theme::Light) {}

Theme ThemeManager::currentTheme() const {
    return m_currentTheme;
}

}
