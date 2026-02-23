#include "common/themes/icon_helper.hpp"

#include <algorithm>
#include <cmath>

// WinTools: icon helper manages shared infrastructure.

namespace wintools::themes {

int IconHelper::scaleIconSize(int basePixels, float dpiScale) {
    const int scaled = static_cast<int>(std::lround(basePixels * dpiScale));
    return std::max(16, scaled);
}

}
