#include "retroarch_scanner.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

// GameVault: retroarch scanner manages discovery and scanning flow.

namespace wintools::gamevault {

namespace {

QString systemTagFromDb(const QString& dbName) {
    struct { const char* key; const char* tag; } table[] = {
        { "Nintendo - Super Nintendo",      "SNES"    },
        { "Nintendo - Nintendo Entertainment", "NES"  },
        { "Nintendo - Game Boy Advance",    "GBA"     },
        { "Nintendo - Game Boy Color",      "GBC"     },
        { "Nintendo - Game Boy",            "GB"      },
        { "Nintendo - Nintendo 64",         "N64"     },
        { "Nintendo - GameCube",            "GCN"     },
        { "Nintendo - Wii",                 "Wii"     },
        { "Sega - Mega Drive",              "Mega Drive" },
        { "Sega - Genesis",                 "Genesis" },
        { "Sega - Saturn",                  "Saturn"  },
        { "Sega - Dreamcast",               "Dreamcast" },
        { "Sony - PlayStation",             "PS1"     },
        { "Sony - PlayStation 2",           "PS2"     },
        { "Sony - PlayStation Portable",    "PSP"     },
        { "Microsoft - MSX",                "MSX"     },
        { "Arcade",                         "Arcade"  },
    };
    for (const auto& t : table)
        if (dbName.contains(t.key, Qt::CaseInsensitive))
            return t.tag;

    const int sep = dbName.lastIndexOf(" - ");
    return sep >= 0 ? dbName.mid(sep + 3) : dbName;
}

}

QString RetroArchScanner::findRetroArchExe() const {

    QSettings reg(R"(HKEY_CLASSES_ROOT\retroarch\shell\open\command)", QSettings::NativeFormat);
    QString cmd = reg.value(".").toString();
    if (!cmd.isEmpty()) {
        cmd = cmd.remove('"').split(' ').first();
        if (QFile::exists(cmd)) return cmd;
    }

    const QStringList fallbackPaths = {
        QStringLiteral("C:/RetroArch-Win64/retroarch.exe"),
        QStringLiteral("C:/RetroArch/retroarch.exe"),
        qEnvironmentVariable("APPDATA") + "/../Local/RetroArch/retroarch.exe"
    };
    for (const QString& p : fallbackPaths) {
        if (QFile::exists(p)) return QDir::fromNativeSeparators(p);
    }
    return {};
}

QString RetroArchScanner::findRetroArchDataDir() const {

    const QString exe = findRetroArchExe();
    if (!exe.isEmpty()) {
        const QString dir = QFileInfo(exe).absolutePath();
        if (QFile::exists(dir + "/retroarch.cfg")) return dir;
    }

    const QString ad = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString candidate = QFileInfo(ad).absolutePath() + "/RetroArch";
    if (QDir(candidate).exists()) return candidate;
    return {};
}

QVector<GameEntry> RetroArchScanner::scan() const {
    QVector<GameEntry> results;

    const QString dataDir = findRetroArchDataDir();
    if (dataDir.isEmpty()) return results;

    const QString retroExe = findRetroArchExe();

    const QString playlistDir = dataDir + "/playlists";
    if (!QDir(playlistDir).exists()) return results;

    QDirIterator it(playlistDir, {"*.lpl"}, QDir::Files);
    while (it.hasNext()) {
        const QString lplPath = it.next();
        QFile f(lplPath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonArray  items = root["items"].toArray();
        const QString     playlistName = QFileInfo(lplPath).baseName();

        for (const QJsonValue& v : items) {
            const QJsonObject obj = v.toObject();
            GameEntry e;
            e.platform       = GamePlatform::RetroArch;
            e.title          = obj["label"].toString();
            e.executablePath = QDir::fromNativeSeparators(obj["path"].toString());
            e.emulatorPath   = retroExe;

            const QString dbName = obj["db_name"].toString();
            e.systemTag = dbName.isEmpty() ? playlistName : systemTagFromDb(dbName);

            const QString boxArtSys = dbName.isEmpty() ? playlistName : dbName.chopped(3);
            const QString boxArt = dataDir + "/thumbnails/" + boxArtSys
                                   + "/Named_Boxarts/" + e.title + ".png";
            if (QFile::exists(boxArt)) e.artCapsulePath = boxArt;

            const QString core = QDir::fromNativeSeparators(obj["core_path"].toString());
            if (!core.isEmpty() && core != "DETECT")
                e.emulatorArgs = {"-L", core};

            if (e.title.isEmpty()) continue;
            results.append(std::move(e));
        }
    }

    return results;
}

}
