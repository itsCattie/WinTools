#pragma once
// GameVault: retroarch scanner manages discovery and scanning flow.

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class RetroArchScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "RetroArch"; }
    QVector<GameEntry> scan()    const override;

private:
    QString findRetroArchExe() const;
    QString findRetroArchDataDir() const;
};

}
