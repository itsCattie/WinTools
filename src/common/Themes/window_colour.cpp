#include "common/themes/window_colour.hpp"

// WinTools: window colour manages shared infrastructure.

namespace wintools::themes {

ThemePalette WindowColour::light() {
    return {
        QColor(246, 246, 248),
        QColor(24, 24, 24),
        QColor(90, 90, 95),
        QColor(255, 255, 255),
        QColor(205, 205, 210),
        QColor(0, 120, 212),
        QColor(30, 30, 30),
        QColor(0, 102, 190),
        QColor(0, 0, 0, 10),
        QColor(0, 0, 0, 25),
        QColor(0, 0, 0, 6),
        QColor(0, 0, 0, 20),
        QColor(0, 0, 0, 30),
        QColor(16, 163, 74),
        QColor(200, 155, 20),
        QColor(220, 38, 38),
    };
}

ThemePalette WindowColour::dark() {
    return {
        QColor(20, 20, 20),
        QColor(220, 220, 220),
        QColor(160, 160, 165),
        QColor(30, 30, 30),
        QColor(55, 55, 60),
        QColor(96, 165, 250),
        QColor(220, 220, 220),
        QColor(135, 206, 250),
        QColor(255, 255, 255, 10),
        QColor(255, 255, 255, 20),
        QColor(255, 255, 255, 6),
        QColor(255, 255, 255, 15),
        QColor(0, 0, 0, 60),
        QColor(34, 197, 94),
        QColor(234, 179, 8),
        QColor(239, 68, 68),
    };
}

}
