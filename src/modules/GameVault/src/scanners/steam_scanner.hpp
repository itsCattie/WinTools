#pragma once
// GameVault: steam scanner manages discovery and scanning flow.

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class SteamScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "Steam"; }
    QVector<GameEntry> scan()    const override;
};

}
