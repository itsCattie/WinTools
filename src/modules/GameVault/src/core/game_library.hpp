#pragma once

// GameVault: game library manages core logic and state.

#include "game_entry.hpp"
#include <QObject>
#include <QRunnable>
#include <QVector>

namespace wintools::gamevault {

class GameLibrary {
public:

    static QVector<GameEntry> scan();
};

class GameLibraryWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit GameLibraryWorker(QObject* parent = nullptr);
    void run() override;

signals:
    void scanComplete(QVector<wintools::gamevault::GameEntry> games);
    void scanError(QString message);
};

}
