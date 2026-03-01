#pragma once

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class EpicScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "Epic Games"; }
    QVector<GameEntry> scan()    const override;
};

}
