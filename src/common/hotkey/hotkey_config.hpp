#pragma once

// WinTools: hotkey config manages shared infrastructure.

#include "common/hotkey/hotkey_engine.hpp"

namespace wintools::hotkeys {

class HotkeyConfig {
public:

    static QList<HotkeyBinding> load();

    static void save(const QList<HotkeyBinding>& bindings);

    static QList<HotkeyBinding> defaults();

private:
    static QString configPath();
};

}
