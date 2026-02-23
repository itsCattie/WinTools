#pragma once

// WinTools: icon helper manages shared infrastructure.

namespace wintools::themes {

class IconHelper {
public:
    static int scaleIconSize(int basePixels, float dpiScale);
};

}
