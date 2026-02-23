#pragma once
// GameVault: emulator scanners manages discovery and scanning flow.

#include "i_game_scanner.hpp"

namespace wintools::gamevault {

class Rpcs3Scanner : public IGameScanner {
public:
    QString          sourceName() const override { return "RPCS3"; }
    QVector<GameEntry> scan()    const override;

private:
    QString findRpcs3DataDir() const;
    QString readSfoTitle(const QString& sfoPath) const;
};

class YuzuScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "Yuzu"; }
    QVector<GameEntry> scan()    const override;

private:
    QString findYuzuExe() const;
};

class RyujinxScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "Ryujinx"; }
    QVector<GameEntry> scan()    const override;

private:
    QString findRyujinxExe() const;
};

class DolphinScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "Dolphin"; }
    QVector<GameEntry> scan()    const override;

private:
    QString findDolphinExe() const;
    QStringList readIsoPaths() const;
};

class DeSmuMEScanner : public IGameScanner {
public:
    QString          sourceName() const override { return "DeSmuME"; }
    QVector<GameEntry> scan()    const override;

private:
    QString     findDeSmuMEExe()  const;
    QStringList recentRomPaths()  const;
    QString     lastRomDir()      const;
};

}
