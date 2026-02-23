#include "common/display/dpi_utils.hpp"

#include <algorithm>
#include <cmath>

// WinTools: dpi utils manages shared infrastructure.

namespace wintools::display {

namespace {
constexpr float MinScale = 0.85f;
constexpr float MaxScale = 2.25f;
}

float DpiUtils::clampScale(float scale) {
    return std::clamp(scale, MinScale, MaxScale);
}

int DpiUtils::scalePixels(int pixels, float scale) {
    return static_cast<int>(std::lround(static_cast<double>(pixels) * clampScale(scale)));
}

float DpiUtils::scaleFont(float points, float scale) {
    return std::max(8.5f, points * clampScale(scale));
}

}
