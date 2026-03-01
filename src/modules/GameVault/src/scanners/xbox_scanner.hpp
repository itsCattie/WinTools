#pragma once

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class XboxScanner : public IGameScanner {
public:
    QString            sourceName() const override { return "Xbox"; }
    QVector<GameEntry> scan()      const override;
};

}
