#include "emulator_scanners.hpp"
#include "../core/gamevault_settings.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace wintools::gamevault {

namespace {

QString parseSfoTitle(const QByteArray& data) {

    if (data.size() < 20 || data.mid(0, 4) != QByteArray("\x00PSF", 4))
        return {};

    const quint32 keyTableOffset  = *reinterpret_cast<const quint32*>(data.constData() + 8);
    const quint32 dataTableOffset = *reinterpret_cast<const quint32*>(data.constData() + 12);
    const quint32 numEntries      = *reinterpret_cast<const quint32*>(data.constData() + 16);

    for (quint32 i = 0; i < numEntries; ++i) {
        const int idx = 20 + static_cast<int>(i) * 16;
        if (idx + 16 > data.size()) break;

        const quint16 keyOff  = *reinterpret_cast<const quint16*>(data.constData() + idx);
        const quint32 dataOff = *reinterpret_cast<const quint32*>(data.constData() + idx + 8);
        const quint32 dataLen = *reinterpret_cast<const quint32*>(data.constData() + idx + 12);

        const QString key = QString::fromLatin1(
            data.constData() + keyTableOffset + keyOff);

        if (key == "TITLE") {
            return QString::fromUtf8(
                data.constData() + dataTableOffset + dataOff,
                static_cast<int>(dataLen) - 1);
        }
    }
    return {};
}

QString findExeByNameContains(const QString& rootDir,
                              const QString& needle,
                              int maxDepth = 3) {
    if (rootDir.isEmpty() || needle.isEmpty()) return {};
    QDir root(rootDir);
    if (!root.exists()) return {};

    const QString lowerNeedle = needle.toLower();
    QDirIterator it(root.absolutePath(), {"*.exe"}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString exe = QDir::fromNativeSeparators(it.next());
        const QString rel = root.relativeFilePath(exe);
        const int depth = rel.count('/');
        if (depth > maxDepth) continue;

        const QString fileName = QFileInfo(exe).fileName().toLower();
        if (fileName.contains(lowerNeedle)) {
            return exe;
        }
    }
    return {};
}

}

QString Rpcs3Scanner::findRpcs3DataDir() const {

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString candidate = QFileInfo(appData).absolutePath() + "/rpcs3";
    if (QDir(candidate + "/dev_hdd0/game").exists()) return candidate;

#ifdef Q_OS_WIN
    const QStringList rpcs3Paths = {
        QStringLiteral("C:/rpcs3"),
        QStringLiteral("C:/Emulators/rpcs3"),
        QStringLiteral("D:/rpcs3"),
        QStringLiteral("E:/rpcs3")
    };
#elif defined(Q_OS_MACOS)
    const QStringList rpcs3Paths = {
        QDir::homePath() + "/Library/Application Support/rpcs3",
        QStringLiteral("/Applications/rpcs3.app/Contents/MacOS")
    };
#elif defined(Q_OS_LINUX)
    const QStringList rpcs3Paths = {
        QDir::homePath() + "/rpcs3",
        QDir::homePath() + "/.config/rpcs3",
        QDir::homePath() + "/.var/app/net.rpcs3.RPCS3/config/rpcs3"
    };
#else
    const QStringList rpcs3Paths;
#endif
    for (const QString& p : rpcs3Paths) {
        if (QDir(p + "/dev_hdd0/game").exists()) return p;
    }
    return {};
}

QString Rpcs3Scanner::readSfoTitle(const QString& sfoPath) const {
    QFile f(sfoPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return parseSfoTitle(f.readAll());
}

QVector<GameEntry> Rpcs3Scanner::scan() const {
    QVector<GameEntry> results;

    const QString dataDir = findRpcs3DataDir();
    if (dataDir.isEmpty()) return results;

    QString rpcs3Exe;
#ifdef Q_OS_WIN
    for (const QString& p : { dataDir + "/rpcs3.exe",
                               dataDir + "/../rpcs3.exe" }) {
        if (QFile::exists(p)) { rpcs3Exe = QFileInfo(p).absoluteFilePath(); break; }
    }
#else
    for (const QString& p : { dataDir + "/rpcs3",
                               dataDir + "/../rpcs3",
                               QStringLiteral("/usr/bin/rpcs3") }) {
        if (QFile::exists(p)) { rpcs3Exe = QFileInfo(p).absoluteFilePath(); break; }
    }
#endif

    const QString gameRoot = dataDir + "/dev_hdd0/game";
    QDirIterator it(gameRoot, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString gameDir = it.next();
        const QString sfoPath = gameDir + "/PARAM.SFO";
        const QString title   = readSfoTitle(sfoPath);
        if (title.isEmpty()) continue;

        GameEntry e;
        e.platform       = GamePlatform::RPCS3;
        e.title          = title;
        e.systemTag      = "PS3";
        e.platformId     = QFileInfo(gameDir).fileName();
        e.executablePath = gameDir;
        e.emulatorPath   = rpcs3Exe;
        e.emulatorArgs   = {"--no-gui"};

        const QString icon = gameDir + "/ICON0.PNG";
        if (QFile::exists(icon)) e.iconPath = icon;

        results.append(std::move(e));
    }
    return results;
}

QString YuzuScanner::findYuzuExe() const {
#ifdef Q_OS_WIN
    const QStringList yuzuPaths = {
        QStringLiteral("C:/yuzu/yuzu.exe"),
        QStringLiteral("C:/Emulators/yuzu/yuzu.exe"),
        qEnvironmentVariable("LOCALAPPDATA") + "/yuzu/yuzu-windows-msvc/yuzu.exe"
    };
#elif defined(Q_OS_MACOS)
    const QStringList yuzuPaths = {
        QStringLiteral("/Applications/yuzu.app/Contents/MacOS/yuzu")
    };
#elif defined(Q_OS_LINUX)
    const QStringList yuzuPaths = {
        QStringLiteral("/usr/bin/yuzu"),
        QDir::homePath() + "/.local/share/yuzu/yuzu"
    };
#else
    const QStringList yuzuPaths;
#endif
    for (const QString& p : yuzuPaths) {
        if (QFile::exists(p)) return QDir::fromNativeSeparators(p);
    }
    return {};
}

QVector<GameEntry> YuzuScanner::scan() const {
    QVector<GameEntry> results;

#ifdef Q_OS_WIN
    const QString cacheDir =
        QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA")) + "/yuzu/cache/game_list";
#elif defined(Q_OS_MACOS)
    const QString cacheDir = QDir::homePath() + "/Library/Application Support/yuzu/cache/game_list";
#else
    const QString cacheDir = QDir::homePath() + "/.local/share/yuzu/cache/game_list";
#endif
    if (!QDir(cacheDir).exists()) return results;

    const QString yuzuExe = findYuzuExe();

    QDirIterator it(cacheDir, {"*.json"}, QDir::Files);
    while (it.hasNext()) {
        QFile f(it.next());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

        const QString romPath = QDir::fromNativeSeparators(obj["file_path"].toString());
        if (romPath.isEmpty() || !QFile::exists(romPath)) continue;

        GameEntry e;
        e.platform       = GamePlatform::Yuzu;
        e.title          = obj["title_name"].toString();
        e.systemTag      = "Switch";
        e.platformId     = obj["title_id"].toString();
        e.executablePath = romPath;
        e.emulatorPath   = yuzuExe;

        const QString iconB64 = obj["icon"].toString();
        if (!iconB64.isEmpty()) {

            e.artBannerUrl = "data:image/png;base64," + iconB64;
        }

        if (e.title.isEmpty()) e.title = QFileInfo(romPath).baseName();
        results.append(std::move(e));
    }
    return results;
}

QString RyujinxScanner::findRyujinxExe() const {
#ifdef Q_OS_WIN
    const QStringList ryujinxPaths = {
        QStringLiteral("C:/Ryujinx/Ryujinx.exe"),
        QStringLiteral("C:/Emulators/Ryujinx/Ryujinx.exe"),
        qEnvironmentVariable("LOCALAPPDATA") + "/Ryujinx/publish/Ryujinx.exe"
    };
#elif defined(Q_OS_MACOS)
    const QStringList ryujinxPaths = {
        QStringLiteral("/Applications/Ryujinx.app/Contents/MacOS/Ryujinx")
    };
#elif defined(Q_OS_LINUX)
    const QStringList ryujinxPaths = {
        QStringLiteral("/usr/bin/Ryujinx"),
        QDir::homePath() + "/.config/Ryujinx/Ryujinx"
    };
#else
    const QStringList ryujinxPaths;
#endif
    for (const QString& p : ryujinxPaths) {
        if (QFile::exists(p)) return QDir::fromNativeSeparators(p);
    }
    return {};
}

QVector<GameEntry> RyujinxScanner::scan() const {
    QVector<GameEntry> results;

#ifdef Q_OS_WIN
    const QString gamesDir =
        QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA")) + "/Ryujinx/games";
#elif defined(Q_OS_MACOS)
    const QString gamesDir = QDir::homePath() + "/Library/Application Support/Ryujinx/games";
#else
    const QString gamesDir = QDir::homePath() + "/.config/Ryujinx/games";
#endif
    if (!QDir(gamesDir).exists()) return results;

    const QString ryujinxExe = findRyujinxExe();

    QDirIterator it(gamesDir, {"*.json"}, QDir::Files);
    while (it.hasNext()) {
        QFile f(it.next());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

        const QString romPath = QDir::fromNativeSeparators(obj["path"].toString());
        if (romPath.isEmpty() || !QFile::exists(romPath)) continue;

        GameEntry e;
        e.platform       = GamePlatform::Ryujinx;
        e.title          = obj["title_name"].toString();
        e.systemTag      = "Switch";
        e.platformId     = obj["title_id"].toString();
        e.executablePath = romPath;
        e.emulatorPath   = ryujinxExe;

        if (e.title.isEmpty()) e.title = QFileInfo(romPath).baseName();
        results.append(std::move(e));
    }
    return results;
}

QString DolphinScanner::findDolphinExe() const {
#ifdef Q_OS_WIN
    const QStringList dolphinPaths = {
        QStringLiteral("C:/Dolphin/Dolphin.exe"),
        QStringLiteral("C:/Emulators/Dolphin/Dolphin.exe"),
        qEnvironmentVariable("LOCALAPPDATA") + "/Dolphin/Dolphin.exe"
    };
#elif defined(Q_OS_MACOS)
    const QStringList dolphinPaths = {
        QStringLiteral("/Applications/Dolphin.app/Contents/MacOS/Dolphin"),
        QDir::homePath() + "/Library/Application Support/Dolphin/dolphin-emu"
    };
#elif defined(Q_OS_LINUX)
    const QStringList dolphinPaths = {
        QStringLiteral("/usr/bin/dolphin-emu"),
        QDir::homePath() + "/.var/app/org.DolphinEmu.dolphin-emu/dolphin-emu"
    };
#else
    const QStringList dolphinPaths;
#endif
    for (const QString& p : dolphinPaths) {
        if (QFile::exists(p)) return QDir::fromNativeSeparators(p);
    }
    return {};
}

QStringList DolphinScanner::readIsoPaths() const {
    QStringList paths;
#ifdef Q_OS_WIN
    const QString cfgPath =
        QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA"))
        + "/Dolphin Emulator/Config/Dolphin.ini";
#elif defined(Q_OS_MACOS)
    const QString cfgPath = QDir::homePath()
        + "/Library/Application Support/Dolphin/Config/Dolphin.ini";
#else
    const QString cfgPath = QDir::homePath()
        + "/.config/dolphin-emu/Dolphin.ini";
#endif

    QFile f(cfgPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return paths;

    QTextStream in(&f);
    bool inGeneral = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line == "[General]") { inGeneral = true; continue; }
        if (line.startsWith('[')) { inGeneral = false; continue; }
        if (!inGeneral) continue;

        if (line.startsWith("ISOPath", Qt::CaseInsensitive)) {
            const int eq = line.indexOf('=');
            if (eq > 0) {
                const QString p = QDir::fromNativeSeparators(line.mid(eq + 1).trimmed());
                if (QDir(p).exists()) paths << p;
            }
        }
    }
    return paths;
}

QVector<GameEntry> DolphinScanner::scan() const {
    QVector<GameEntry> results;

    QStringList isoPaths = readIsoPaths();

    if (isoPaths.isEmpty()) {
        const QStringList customFolders = GameVaultSettings::instance().customGameFolders();
        for (const QString& f : customFolders) {
            if (f.isEmpty()) continue;
            const QString dir = QDir::fromNativeSeparators(f);
            if (QDir(dir).exists() && !isoPaths.contains(dir)) isoPaths << dir;
        }
    }
    if (isoPaths.isEmpty()) return results;

    const QString dolphinExe = findDolphinExe();
    const QStringList exts = {"*.iso", "*.rvz", "*.wbfs", "*.gcm", "*.gcz", "*.wad"};

    for (const QString& root : isoPaths) {
        QDirIterator it(root, exts, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString romPath = it.next();
            GameEntry e;
            e.platform       = GamePlatform::Dolphin;
            e.title          = QFileInfo(romPath).baseName();
            e.executablePath = romPath;
            e.emulatorPath   = dolphinExe;
            e.emulatorArgs   = {"--exec"};

            const QString ext = QFileInfo(romPath).suffix().toLower();
            e.systemTag = (ext == "gcm" || ext == "gcz") ? "GameCube" : "Wii";

            results.append(std::move(e));
        }
    }
    return results;
}

QString DeSmuMEScanner::findDeSmuMEExe() const {

#ifdef Q_OS_WIN
    const QStringList appPathKeys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\DeSmuME.exe"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\DeSmuME.exe"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\DeSmuME.exe")
    };
    for (const QString& key : appPathKeys) {
        QSettings appPaths(key, QSettings::NativeFormat);
        const QString regPath = QDir::fromNativeSeparators(appPaths.value(".").toString());
        if (!regPath.isEmpty() && QFile::exists(regPath)) return regPath;
    }

    const QStringList candidates = {
        QStringLiteral("C:/Program Files/DeSmuME/DeSmuME.exe"),
        QStringLiteral("C:/Program Files (x86)/DeSmuME/DeSmuME.exe"),
        QStringLiteral("C:/DeSmuME/DeSmuME.exe"),
        QStringLiteral("C:/Emulators/DeSmuME/DeSmuME.exe"),
        qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
        qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"),
        qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
        qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe")
    };
    for (const QString& p : candidates)
        if (!p.trimmed().isEmpty() && QFile::exists(p))
            return QDir::fromNativeSeparators(p);

    const QStringList searchRoots = {
        qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games"),
        qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games"),
        QStringLiteral("C:/Emulators"),
        QStringLiteral("D:/Emulators"),
        QStringLiteral("E:/Emulators")
    };

    for (const QString& root : searchRoots) {
        const QString found = findExeByNameContains(QDir::fromNativeSeparators(root), "desmume", 4);
        if (!found.isEmpty()) return found;
    }
#elif defined(Q_OS_MACOS)
    const QStringList candidates = {
        QStringLiteral("/Applications/DeSmuME.app/Contents/MacOS/DeSmuME")
    };
    for (const QString& p : candidates)
        if (QFile::exists(p)) return p;
#elif defined(Q_OS_LINUX)
    const QStringList candidates = {
        QStringLiteral("/usr/bin/desmume"),
        QDir::homePath() + "/.config/desmume/desmume"
    };
    for (const QString& p : candidates)
        if (QFile::exists(p)) return p;
#endif

    return {};
}

QStringList DeSmuMEScanner::recentRomPaths() const {

    QStringList paths;
#ifdef Q_OS_WIN
    QSettings root(QStringLiteral("HKEY_CURRENT_USER\\Software\\DeSmuME"),
                   QSettings::NativeFormat);
    const QStringList versions = root.childGroups();
    for (const QString& ver : versions) {
        QSettings recent(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\DeSmuME\\%1\\Recent Files").arg(ver),
            QSettings::NativeFormat);
        const QStringList keys = recent.allKeys();
        for (const QString& key : keys) {

            if (!key.startsWith("File", Qt::CaseInsensitive)) continue;
            const QString p = QDir::fromNativeSeparators(recent.value(key).toString());
            if (!p.isEmpty() && QFile::exists(p))
                paths << p;
        }
    }
#endif
    return paths;
}

QString DeSmuMEScanner::lastRomDir() const {
#ifdef Q_OS_WIN
    QSettings root(QStringLiteral("HKEY_CURRENT_USER\\Software\\DeSmuME"),
                   QSettings::NativeFormat);
    for (const QString& ver : root.childGroups()) {
        QSettings s(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\DeSmuME\\%1").arg(ver),
            QSettings::NativeFormat);
        const QString dir = QDir::fromNativeSeparators(s.value("LastRomDir").toString());
        if (!dir.isEmpty() && QDir(dir).exists()) return dir;
    }
#endif
    return {};
}

QVector<GameEntry> DeSmuMEScanner::scan() const {
    QVector<GameEntry> results;

    const QString override = GameVaultSettings::instance().emulatorPath("DeSmuME");
    const QString desmExe  = override.isEmpty() ? findDeSmuMEExe() : override;

    const QStringList dsExts = {"*.nds", "*.dsi", "*.ids", "*.zip"};

    QStringList romPaths = recentRomPaths();

    const QString romDir = lastRomDir();
    if (!romDir.isEmpty()) {
        QDirIterator it(romDir, dsExts, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            romPaths << it.next();
    }

    const QStringList customFolders = GameVaultSettings::instance().customGameFolders();
    for (const QString& folder : customFolders) {
        if (!QDir(folder).exists()) continue;
        QDirIterator it(folder, dsExts, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            romPaths << it.next();
    }

    QStringList seen;
    for (const QString& rom : romPaths) {
        const QString canonical = QFileInfo(rom).canonicalFilePath();
        if (canonical.isEmpty() || seen.contains(canonical)) continue;
        seen << canonical;

        GameEntry e;
        e.platform       = GamePlatform::DeSmuME;
        e.systemTag      = QStringLiteral("DS");

        e.title          = QFileInfo(rom).completeBaseName();
        e.executablePath = rom;
        e.emulatorPath   = desmExe;

        results.append(std::move(e));
    }

    return results;
}

}
