#pragma once
// GameVault: gog scanner manages discovery and scanning flow.

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class GogScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "GOG"; }
    QVector<GameEntry> scan()    const override;
};

}
