#pragma once

// WinTools: theme helper manages shared infrastructure.

#include "common/themes/window_colour.hpp"

namespace wintools::themes {

class ThemeHelper {
public:
    static bool isSystemDarkTheme();
    static ThemePalette currentPalette();
};

}
