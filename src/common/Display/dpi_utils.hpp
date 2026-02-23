#pragma once

// WinTools: dpi utils manages shared infrastructure.

namespace wintools::display {

class DpiUtils {
public:
    static float clampScale(float scale);
    static int scalePixels(int pixels, float scale);
    static float scaleFont(float points, float scale);
};

}
