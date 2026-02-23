#pragma once

// WinTools: window colour manages shared infrastructure.

#include <QColor>

namespace wintools::themes {

struct ThemePalette {
    QColor windowBackground;
    QColor foreground;
    QColor mutedForeground;
    QColor cardBackground;
    QColor cardBorder;
    QColor accent;
    QColor statusNeutral;
    QColor statusUpdate;

    QColor hoverBackground;
    QColor pressedBackground;
    QColor surfaceOverlay;
    QColor divider;
    QColor shadow;
    QColor successGreen;
    QColor warningYellow;
    QColor dangerRed;
};

class WindowColour {
public:
    static ThemePalette light();
    static ThemePalette dark();
};

}
