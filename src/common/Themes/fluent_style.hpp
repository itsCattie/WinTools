#pragma once

#include "common/themes/window_colour.hpp"
#include <QString>

namespace wintools::themes {

class FluentStyle {
public:

    static QString generate(const ThemePalette& p);

    static QString cardStyle(const ThemePalette& p);

    static QString buttonStyle(const ThemePalette& p);

    static QString inputStyle(const ThemePalette& p);

    static QString tabStyle(const ThemePalette& p);

    static QString tableStyle(const ThemePalette& p);

    static QString toggleStyle(const ThemePalette& p);

    static QString scrollbarStyle(const ThemePalette& p);

    static QString tooltipStyle(const ThemePalette& p);
};

}
