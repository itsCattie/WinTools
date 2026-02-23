#pragma once
// GameVault: custom folder scanner manages discovery and scanning flow.

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class CustomFolderScanner : public IGameScanner {
public:
    QString            sourceName() const override { return "Custom"; }
    QVector<GameEntry> scan()       const override;
};

}
