#pragma once

// WinTools: console manager manages UI behavior and presentation.

namespace wintools::ui {

class ConsoleManager {
public:
    static bool isConsoleVisible();
    static bool toggleConsole();
};

}
