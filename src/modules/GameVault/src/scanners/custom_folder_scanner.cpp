#include "custom_folder_scanner.hpp"
#include "../core/gamevault_settings.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

// GameVault: custom folder scanner manages discovery and scanning flow.

namespace wintools::gamevault {

namespace {

static const QRegularExpression kSkipPattern(
    QStringLiteral("(uninstall|setup|install|redist|vcredist|directx|dxsetup|update"
                   "|crashpad|crash_handler|crash_reporter|helper|launcher_stub"
                   "|vc_redist|dotnet|cudaconfig|nvdisplay|reg_|unarc)\\.exe"),
    QRegularExpression::CaseInsensitiveOption);

bool isBoring(const QString& exeName) {
    return kSkipPattern.match(exeName).hasMatch();
}

QString pickBestExe(const QStringList& exes, const QString& folderName) {
    if (exes.size() == 1) return exes.first();

    QString best;
    int     bestScore = -1;
    const QString folderLower = folderName.toLower();

    for (const QString& exe : exes) {
        if (isBoring(QFileInfo(exe).fileName())) continue;

        const QString stemLower = QFileInfo(exe).baseName().toLower();

        int score = 0;
        for (int i = 0; i < qMin(stemLower.size(), folderLower.size()); ++i) {
            if (stemLower[i] == folderLower[i]) ++score;
            else break;
        }
        score += stemLower.size();

        if (score > bestScore) {
            bestScore = score;
            best = exe;
        }
    }
    return best.isEmpty() ? exes.first() : best;
}

void scanRoot(const QString& rootPath, QVector<GameEntry>& out) {
    const QDir root(rootPath);
    if (!root.exists()) return;

    const QStringList subDirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sub : subDirs) {
        const QString subPath = root.absoluteFilePath(sub);
        const QStringList exes = QDir(subPath).entryList({"*.exe"}, QDir::Files);

        QStringList goodExes;
        for (const QString& exe : exes) {
            if (!isBoring(exe))
                goodExes << subPath + "/" + exe;
        }
        if (goodExes.isEmpty()) continue;

        const QString exePath = pickBestExe(goodExes, sub);

        GameEntry e;
        e.platform       = GamePlatform::Unknown;
        e.title          = sub;
        e.executablePath = exePath;
        e.installPath    = subPath;
        e.installed      = true;
        out.append(std::move(e));
    }

    const QStringList rootExes = root.entryList({"*.exe"}, QDir::Files);
    for (const QString& exe : rootExes) {
        if (isBoring(exe)) continue;
        GameEntry e;
        e.platform       = GamePlatform::Unknown;
        e.title          = QFileInfo(exe).baseName();
        e.executablePath = root.absoluteFilePath(exe);
        e.installPath    = rootPath;
        e.installed      = true;
        out.append(std::move(e));
    }
}

}

QVector<GameEntry> CustomFolderScanner::scan() const {
    QVector<GameEntry> results;

    const QStringList folders = GameVaultSettings::instance().customGameFolders();
    for (const QString& folder : folders)
        scanRoot(folder, results);

    return results;
}

}
