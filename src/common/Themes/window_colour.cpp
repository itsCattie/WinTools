#include "common/themes/window_colour.hpp"

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

ThemePalette WindowColour::midnight() {
    return {
        QColor(14, 18, 28),
        QColor(229, 236, 246),
        QColor(154, 170, 194),
        QColor(24, 30, 44),
        QColor(50, 62, 84),
        QColor(96, 165, 250),
        QColor(222, 232, 246),
        QColor(129, 180, 255),
        QColor(255, 255, 255, 10),
        QColor(255, 255, 255, 20),
        QColor(255, 255, 255, 8),
        QColor(255, 255, 255, 18),
        QColor(0, 0, 0, 72),
        QColor(74, 222, 128),
        QColor(250, 204, 21),
        QColor(248, 113, 113),
    };
}

ThemePalette WindowColour::forest() {
    return {
        QColor(15, 24, 18),
        QColor(225, 239, 229),
        QColor(150, 177, 160),
        QColor(23, 36, 28),
        QColor(52, 76, 60),
        QColor(74, 222, 128),
        QColor(216, 236, 220),
        QColor(52, 211, 153),
        QColor(255, 255, 255, 10),
        QColor(255, 255, 255, 20),
        QColor(255, 255, 255, 8),
        QColor(255, 255, 255, 18),
        QColor(0, 0, 0, 72),
        QColor(34, 197, 94),
        QColor(245, 196, 45),
        QColor(239, 99, 99),
    };
}

ThemePalette WindowColour::rose() {
    return {
        QColor(251, 246, 249),
        QColor(43, 32, 39),
        QColor(122, 97, 112),
        QColor(255, 255, 255),
        QColor(227, 206, 218),
        QColor(219, 39, 119),
        QColor(62, 45, 56),
        QColor(190, 24, 93),
        QColor(0, 0, 0, 9),
        QColor(0, 0, 0, 22),
        QColor(219, 39, 119, 10),
        QColor(0, 0, 0, 18),
        QColor(0, 0, 0, 26),
        QColor(22, 163, 74),
        QColor(217, 119, 6),
        QColor(220, 38, 38),
    };
}

}
