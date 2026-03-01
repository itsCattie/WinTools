#pragma once

#include "../core/game_entry.hpp"
#include <QVector>

namespace wintools::gamevault {

class IGameScanner {
public:
    virtual ~IGameScanner() = default;

    virtual QString sourceName() const = 0;

    virtual QVector<GameEntry> scan() const = 0;
};

}
