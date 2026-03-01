#include "xbox_scanner.hpp"

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace wintools::gamevault {

namespace {

#ifdef Q_OS_WIN

struct ManifestInfo {
    QString displayName;
    QString packageFamilyName;
    QString appId;
    QString logoPath;
};

ManifestInfo parseAppxManifest(const QString& manifestPath) {
    ManifestInfo info;
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    QXmlStreamReader xml(&file);
    QString identityName;
    QString publisherId;

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const auto localName = xml.name();

        if (localName == u"Identity") {
            identityName = xml.attributes().value("Name").toString();
            publisherId  = xml.attributes().value("Publisher").toString();
        }
        else if (localName == u"DisplayName") {
            const QString text = xml.readElementText();

            if (!text.startsWith("ms-resource:") && !text.isEmpty())
                info.displayName = text;
        }
        else if (localName == u"Application") {
            if (info.appId.isEmpty())
                info.appId = xml.attributes().value("Id").toString();
        }
        else if (localName == u"Logo") {
            if (info.logoPath.isEmpty())
                info.logoPath = xml.readElementText();
        }
    }

    if (!identityName.isEmpty())
        info.packageFamilyName = identityName;

    if (info.displayName.isEmpty() && !identityName.isEmpty()) {

        QString fallback = identityName;
        int dot = fallback.lastIndexOf('.');
        if (dot >= 0)
            fallback = fallback.mid(dot + 1);

        QString spaced;
        for (int i = 0; i < fallback.size(); ++i) {
            if (i > 0 && fallback[i].isUpper() && !fallback[i - 1].isUpper())
                spaced += ' ';
            spaced += fallback[i];
        }
        info.displayName = spaced;
    }

    return info;
}

QStringList xboxInstallPaths() {
    QStringList paths;

    HKEY rootKey = nullptr;
    const wchar_t* regPath =
        L"SOFTWARE\\Microsoft\\GamingServices\\PackageRepository\\Root";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0,
                      KEY_READ | KEY_WOW64_64KEY, &rootKey) != ERROR_SUCCESS)
        return paths;

    wchar_t guidBuf[256];
    for (DWORD gi = 0;
         RegEnumKeyW(rootKey, gi, guidBuf, 256) == ERROR_SUCCESS; ++gi) {
        HKEY guidKey = nullptr;
        if (RegOpenKeyExW(rootKey, guidBuf, 0,
                          KEY_READ | KEY_WOW64_64KEY, &guidKey) != ERROR_SUCCESS)
            continue;

        wchar_t pkgBuf[512];
        for (DWORD pi = 0;
             RegEnumKeyW(guidKey, pi, pkgBuf, 512) == ERROR_SUCCESS; ++pi) {
            HKEY pkgKey = nullptr;
            if (RegOpenKeyExW(guidKey, pkgBuf, 0,
                              KEY_READ | KEY_WOW64_64KEY, &pkgKey) != ERROR_SUCCESS)
                continue;

            wchar_t valueBuf[MAX_PATH];
            DWORD valueSize = sizeof(valueBuf);
            DWORD valueType = 0;
            if (RegQueryValueExW(pkgKey, L"Root", nullptr, &valueType,
                                 reinterpret_cast<LPBYTE>(valueBuf),
                                 &valueSize) == ERROR_SUCCESS &&
                valueType == REG_SZ) {
                const QString path = QDir::fromNativeSeparators(
                    QString::fromWCharArray(valueBuf));
                if (QDir(path).exists())
                    paths.append(path);
            }
            RegCloseKey(pkgKey);
        }
        RegCloseKey(guidKey);
    }
    RegCloseKey(rootKey);

    return paths;
}

QStringList xboxGamesFolders() {
    QStringList dirs;

    for (const QFileInfo& drive : QDir::drives()) {
        const QString xboxDir = drive.absoluteFilePath() + "XboxGames";
        if (QDir(xboxDir).exists()) {
            const auto entries = QDir(xboxDir).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                if (QFile::exists(entry.absoluteFilePath()
                                  + "/Content/AppxManifest.xml") ||
                    QFile::exists(entry.absoluteFilePath()
                                  + "/AppxManifest.xml")) {
                    dirs.append(entry.absoluteFilePath());
                }
            }
        }
    }
    return dirs;
}

#endif

}

QVector<GameEntry> XboxScanner::scan() const {
    QVector<GameEntry> results;

#ifdef Q_OS_WIN
    QStringList installPaths = xboxInstallPaths();

    for (const auto& p : xboxGamesFolders()) {
        if (!installPaths.contains(p))
            installPaths.append(p);
    }

    QSet<QString> seen;

    for (const QString& gamePath : installPaths) {

        QString manifestPath = gamePath + "/AppxManifest.xml";
        if (!QFile::exists(manifestPath))
            manifestPath = gamePath + "/Content/AppxManifest.xml";
        if (!QFile::exists(manifestPath))
            continue;

        const ManifestInfo m = parseAppxManifest(manifestPath);
        if (m.displayName.isEmpty())
            continue;

        const QString familyKey = m.packageFamilyName.toLower();
        if (seen.contains(familyKey))
            continue;
        seen.insert(familyKey);

        GameEntry e;
        e.platform    = GamePlatform::Xbox;
        e.title       = m.displayName;
        e.platformId  = m.packageFamilyName;
        e.installPath = gamePath;
        e.installed   = true;

        if (!m.packageFamilyName.isEmpty() && !m.appId.isEmpty())
            e.launchUri = "shell:AppsFolder\\"
                          + m.packageFamilyName + "!" + m.appId;

        if (!m.logoPath.isEmpty()) {

            QString logoFull = gamePath + "/" + m.logoPath;

            if (!QFile::exists(logoFull)) {
                const int dotPos = logoFull.lastIndexOf('.');
                if (dotPos > 0) {
                    const QString base = logoFull.left(dotPos);
                    const QString ext  = logoFull.mid(dotPos);
                    for (const char* scale : {".scale-200", ".scale-100",
                                              ".scale-400"}) {
                        const QString variant = base + scale + ext;
                        if (QFile::exists(variant)) {
                            logoFull = variant;
                            break;
                        }
                    }
                }
            }
            if (QFile::exists(logoFull))
                e.iconPath = logoFull;
        }

        results.append(std::move(e));
    }
#endif

    return results;
}

}
