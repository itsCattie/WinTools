#include "gamevault_window.hpp"
#include "game_card_delegate.hpp"

#include "logger/logger.hpp"
#include "modules/GameVault/src/core/game_entry.hpp"
#include "modules/GameVault/src/core/game_library.hpp"
#include "modules/GameVault/src/core/gamevault_settings.hpp"
#include "modules/GameVault/src/core/playtime_tracker.hpp"
#include "modules/GameVault/src/core/game_tag_store.hpp"
#include "modules/GameVault/src/core/steamgriddb_client.hpp"
#include "modules/GameVault/src/model/game_model.hpp"
#include "common/themes/fluent_style.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "common/ui/screen_relative_size.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QThreadPool>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace wintools::gamevault {

static constexpr const char* kLog = "GameVault/Window";

namespace {

QString programFiles() {
    QString pf = qEnvironmentVariable("ProgramFiles");
    return pf.isEmpty() ? QStringLiteral("C:/Program Files") : pf;
}
QString programFilesX86() {
    QString pf = qEnvironmentVariable("ProgramFiles(x86)");
    return pf.isEmpty() ? QStringLiteral("C:/Program Files (x86)") : pf;
}
}

QStringList platformExecutableIconCandidates(GamePlatform p) {
    const QString pf    = programFiles();
    const QString pfx86 = programFilesX86();

    switch (p) {
        case GamePlatform::Steam:
            return {
                pfx86 + QStringLiteral("/Steam/steam.exe"),
                pf    + QStringLiteral("/Steam/steam.exe")
            };
        case GamePlatform::EpicGames:
            return {
                pfx86 + QStringLiteral("/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe"),
                pf    + QStringLiteral("/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe")
            };
        case GamePlatform::GOG:
            return {
                pfx86 + QStringLiteral("/GOG Galaxy/GalaxyClient.exe"),
                pf    + QStringLiteral("/GOG Galaxy/GalaxyClient.exe")
            };
        case GamePlatform::Xbox:
            return {
                pf + QStringLiteral("/Xbox/XboxApp.exe")
            };
        case GamePlatform::RetroArch:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("RetroArch")).trimmed(),
                QStringLiteral("C:/RetroArch/retroarch.exe"),
                pf + QStringLiteral("/RetroArch/retroarch.exe")
            };
        case GamePlatform::RPCS3:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("RPCS3")).trimmed(),
                QStringLiteral("C:/RPCS3/rpcs3.exe"),
                pf + QStringLiteral("/RPCS3/rpcs3.exe")
            };
        case GamePlatform::Yuzu:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Yuzu")).trimmed(),
                QStringLiteral("C:/Yuzu/yuzu.exe"),
                pf + QStringLiteral("/yuzu/yuzu.exe")
            };
        case GamePlatform::Ryujinx:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Ryujinx")).trimmed(),
                QStringLiteral("C:/Ryujinx/Ryujinx.exe"),
                pf + QStringLiteral("/Ryujinx/Ryujinx.exe")
            };
        case GamePlatform::Dolphin:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Dolphin")).trimmed(),
                QStringLiteral("C:/Dolphin/Dolphin.exe"),
                QStringLiteral("C:/Emulators/Dolphin/Dolphin.exe"),
                pf    + QStringLiteral("/Dolphin/Dolphin.exe"),
                pfx86 + QStringLiteral("/Dolphin/Dolphin.exe"),
                qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Dolphin/Dolphin.exe")
            };
        case GamePlatform::PCSX2:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("PCSX2")).trimmed(),
                QStringLiteral("C:/PCSX2/pcsx2-qt.exe"),
                pf + QStringLiteral("/PCSX2/pcsx2-qt.exe")
            };
        case GamePlatform::DeSmuME:
            return {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("DeSmuME")).trimmed(),
                QStringLiteral("C:/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Emulators/DeSmuME/DeSmuME.exe"),
                pf    + QStringLiteral("/DeSmuME/DeSmuME.exe"),
                pfx86 + QStringLiteral("/DeSmuME/DeSmuME.exe"),
                qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe")
            };
        case GamePlatform::Unknown:
            return {};
    }
    return {};
}

QIcon platformIcon(GamePlatform p) {
    static QHash<int, QIcon> cache;
    const int key = static_cast<int>(p);
    if (cache.contains(key)) {
        return cache.value(key);
    }

    static QFileIconProvider iconProvider;
    for (const QString& candidate : platformExecutableIconCandidates(p)) {
        if (candidate.trimmed().isEmpty()) continue;
        if (!QFileInfo::exists(candidate)) continue;
        const QIcon icon = iconProvider.icon(QFileInfo(candidate));
        if (!icon.isNull()) {
            cache.insert(key, icon);
            return icon;
        }
    }

    const QIcon fallback = QIcon(QStringLiteral(":/icons/modules/gamevault.svg"));
    cache.insert(key, fallback);
    return fallback;
}

QIcon uiIcon(const char* name) {
    return QIcon(QStringLiteral(":/icons/gamevault/ui/%1.svg").arg(QString::fromLatin1(name)));
}

QString platformSupportLabel(GamePlatform p) {
    switch (p) {
        case GamePlatform::Steam:
        case GamePlatform::GOG:
            return QStringLiteral("Windows, macOS, Linux");
        case GamePlatform::EpicGames:
        case GamePlatform::Xbox:
        case GamePlatform::RetroArch:
        case GamePlatform::RPCS3:
        case GamePlatform::Yuzu:
        case GamePlatform::Ryujinx:
        case GamePlatform::Dolphin:
        case GamePlatform::PCSX2:
        case GamePlatform::DeSmuME:
        case GamePlatform::Unknown:
            return QStringLiteral("Windows");
    }
    return QStringLiteral("Windows");
}

QString platformLauncherActionText(GamePlatform p) {
    switch (p) {
        case GamePlatform::Steam:     return QStringLiteral("Open Steam");
        case GamePlatform::EpicGames: return QStringLiteral("Open Epic Games Launcher");
        case GamePlatform::GOG:       return QStringLiteral("Open GOG Galaxy");
        case GamePlatform::Xbox:      return QStringLiteral("Open Xbox App");
        case GamePlatform::RetroArch:
        case GamePlatform::RPCS3:
        case GamePlatform::Yuzu:
        case GamePlatform::Ryujinx:
        case GamePlatform::Dolphin:
        case GamePlatform::PCSX2:
        case GamePlatform::DeSmuME:
            return QStringLiteral("Open %1").arg(platformName(p));
        case GamePlatform::Unknown:
            return QStringLiteral("Open game client");
    }
    return QStringLiteral("Open game client");
}

bool launchFirstExistingExecutable(const QStringList& candidates) {
    for (const QString& candidate : candidates) {
        if (candidate.trimmed().isEmpty()) continue;
        if (!QFileInfo::exists(candidate)) continue;
        if (QProcess::startDetached(candidate, {}, QFileInfo(candidate).absolutePath())) {
            return true;
        }
    }
    return false;
}

bool openPlatformProgram(GamePlatform p, QString* errorMessage) {
    auto setError = [&](const QString& message) {
        if (errorMessage) *errorMessage = message;
    };

    const QString pf    = programFiles();
    const QString pfx86 = programFilesX86();

    switch (p) {
        case GamePlatform::Steam:
            if (QDesktopServices::openUrl(QUrl(QStringLiteral("steam://open/main")))) return true;
            if (launchFirstExistingExecutable({
                    pfx86 + QStringLiteral("/Steam/steam.exe"),
                    pf    + QStringLiteral("/Steam/steam.exe")
                })) {
                return true;
            }
            setError(QStringLiteral("Steam client not found."));
            return false;
        case GamePlatform::EpicGames:
            if (QDesktopServices::openUrl(QUrl(QStringLiteral("com.epicgames.launcher://apps")))) return true;
            if (launchFirstExistingExecutable({
                    pfx86 + QStringLiteral("/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe"),
                    pf    + QStringLiteral("/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe")
                })) {
                return true;
            }
            setError(QStringLiteral("Epic Games Launcher not found."));
            return false;
        case GamePlatform::GOG:
            if (QDesktopServices::openUrl(QUrl(QStringLiteral("goggalaxy://open")))) return true;
            if (launchFirstExistingExecutable({
                    pfx86 + QStringLiteral("/GOG Galaxy/GalaxyClient.exe"),
                    pf    + QStringLiteral("/GOG Galaxy/GalaxyClient.exe")
                })) {
                return true;
            }
            setError(QStringLiteral("GOG Galaxy not found."));
            return false;
        case GamePlatform::Xbox:
            if (QDesktopServices::openUrl(QUrl(QStringLiteral("xbox:")))) return true;
            setError(QStringLiteral("Xbox app could not be opened."));
            return false;
        case GamePlatform::RetroArch: {
            const QString configured = GameVaultSettings::instance().emulatorPath(QStringLiteral("RetroArch")).trimmed();
            if (!configured.isEmpty() && QFileInfo::exists(configured)
                && QProcess::startDetached(configured, {}, QFileInfo(configured).absolutePath())) {
                return true;
            }
            if (launchFirstExistingExecutable({
                    QStringLiteral("C:/RetroArch/retroarch.exe"),
                    pf + QStringLiteral("/RetroArch/retroarch.exe")
                })) {
                return true;
            }
            setError(QStringLiteral("RetroArch executable not found."));
            return false;
        }
        case GamePlatform::RPCS3:
        case GamePlatform::Yuzu:
        case GamePlatform::Ryujinx:
        case GamePlatform::Dolphin:
        case GamePlatform::PCSX2:
        case GamePlatform::DeSmuME: {
            const QString name = platformName(p);
            const QString configured = GameVaultSettings::instance().emulatorPath(name).trimmed();
            if (!configured.isEmpty() && QFileInfo::exists(configured)
                && QProcess::startDetached(configured, {}, QFileInfo(configured).absolutePath())) {
                return true;
            }

            QStringList candidates;
            if (p == GamePlatform::RPCS3) {
                candidates = {
                    QStringLiteral("C:/RPCS3/rpcs3.exe"),
                    pf + QStringLiteral("/RPCS3/rpcs3.exe")
                };
            } else if (p == GamePlatform::Yuzu) {
                candidates = {
                    QStringLiteral("C:/Yuzu/yuzu.exe"),
                    pf + QStringLiteral("/yuzu/yuzu.exe")
                };
            } else if (p == GamePlatform::Ryujinx) {
                candidates = {
                    QStringLiteral("C:/Ryujinx/Ryujinx.exe"),
                    pf + QStringLiteral("/Ryujinx/Ryujinx.exe")
                };
            } else if (p == GamePlatform::Dolphin) {
                candidates = {
                    QStringLiteral("C:/Dolphin/Dolphin.exe"),
                    pf + QStringLiteral("/Dolphin/Dolphin.exe")
                };
            } else if (p == GamePlatform::PCSX2) {
                candidates = {
                    QStringLiteral("C:/PCSX2/pcsx2-qt.exe"),
                    pf + QStringLiteral("/PCSX2/pcsx2-qt.exe")
                };
            } else if (p == GamePlatform::DeSmuME) {
                candidates = {
                    QStringLiteral("C:/DeSmuME/DeSmuME.exe"),
                    pf + QStringLiteral("/DeSmuME/DeSmuME.exe")
                };
            }

            if (launchFirstExistingExecutable(candidates)) {
                return true;
            }

            setError(QStringLiteral("%1 executable not found.").arg(name));
            return false;
        }
        case GamePlatform::Unknown:
            if (openPlatformProgram(GamePlatform::Steam, nullptr)) return true;
            if (openPlatformProgram(GamePlatform::EpicGames, nullptr)) return true;
            if (openPlatformProgram(GamePlatform::GOG, nullptr)) return true;
            if (openPlatformProgram(GamePlatform::Xbox, nullptr)) return true;
            setError(QStringLiteral("No known launcher found."));
            return false;
    }

    setError(QStringLiteral("No launcher available."));
    return false;
}

QColor blendColor(const QColor& a, const QColor& b, float alpha) {
    return QColor(
        static_cast<int>(a.red() * (1.0f - alpha) + b.red() * alpha),
        static_cast<int>(a.green() * (1.0f - alpha) + b.green() * alpha),
        static_cast<int>(a.blue() * (1.0f - alpha) + b.blue() * alpha));
}

QColor compositeOver(const QColor& base, const QColor& overlay) {
    if (overlay.alpha() >= 255) return overlay;
    const float alpha = static_cast<float>(overlay.alpha()) / 255.0f;
    return blendColor(base, overlay, alpha);
}

QString cssColor(const QColor& c) {
    if (c.alpha() < 255) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(c.alpha());
    }
    return c.name();
}

QColor bestTextColorFor(const QColor& background,
                        const QColor& preferred,
                        const wintools::themes::ThemePalette& p) {
    auto score = [](const QColor& bg, const QColor& fg) -> int {
        const int dr = qAbs(bg.red() - fg.red());
        const int dg = qAbs(bg.green() - fg.green());
        const int db = qAbs(bg.blue() - fg.blue());
        return dr + dg + db;
    };

    const QVector<QColor> candidates = {
        preferred,
        p.foreground,
        p.mutedForeground,
        QColor(Qt::white),
        QColor(Qt::black)
    };

    QColor best = preferred;
    int bestScore = score(background, preferred);
    for (const QColor& c : candidates) {
        const int s = score(background, c);
        if (s > bestScore) {
            bestScore = s;
            best = c;
        }
    }
    return best;
}

QString buildGameVaultQss(const wintools::themes::ThemePalette& p) {
    using wintools::themes::FluentStyle;

    const QColor sidebarSelectedBg = compositeOver(p.cardBackground, p.accent);
    const QColor sidebarHoverBg = compositeOver(p.cardBackground, p.hoverBackground);
    const QColor sidebarSelectedText = bestTextColorFor(sidebarSelectedBg, p.foreground, p);
    const QColor sidebarHoverText = bestTextColorFor(sidebarHoverBg, p.foreground, p);
    const QColor heroOverlay = compositeOver(p.cardBackground, p.surfaceOverlay);

    const QString supplement = QStringLiteral(
        "QWidget#sidebarPanel { background-color: %1; }"
        "QFrame#divider { background-color: %2; }"
        "QLabel#sidebarHeader { color: %3; }"
        "QListWidget#sidebar { background-color: %1; border: none; outline: none; font-size: 13px; }"
        "QListWidget#sidebar::item { color: %3; padding: 6px 16px; border-radius: 6px; margin: 1px 6px; }"
        "QListWidget#sidebar::item:selected { background-color: %4; color: %5; }"
        "QListWidget#sidebar::item:selected:active { background-color: %4; color: %5; }"
        "QListWidget#sidebar::item:selected:!active { background-color: %4; color: %5; }"
        "QListWidget#sidebar::item:hover:!selected { background-color: %6; color: %7; }"
        "QListWidget#sidebar::item:disabled { color: %8; }"
        "QPushButton#installedToggle, QPushButton#settingsBtn { text-align: left; margin: 0 10px 6px 10px; }"
        "QPushButton#playBtn {"
        "  background: %9;"
        "  border: 1px solid %9;"
        "  color: %10;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "  padding: 8px 24px;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#playBtn:hover { background: %11; border-color: %11; }"
        "QPushButton#playBtn:pressed { background: %12; border-color: %12; }"
        "QPushButton#backBtn {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  color: %8;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "}"
        "QPushButton#backBtn:hover { background: %6; color: %3; }"
        "QListView#gridView { background-color: %13; border: none; outline: none; }"
        "QListView#gridView::item:selected, QListView#gridView::item:hover { background: transparent; }"
        "QScrollArea#detailScroll, QWidget#detailPage { background-color: %13; border: none; }"
        "QWidget#heroArea { background-color: %1; }"
        "QLabel#heroImage { background-color: %14; }"
        "QLabel#heroTitle {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 transparent, stop:1 %15);"
        "  color: %3;"
        "  font-size: 28px;"
        "  font-weight: bold;"
        "}"
        "QLabel#detailPlatform, QLabel#playtimeSub, QLabel#achLabel, QLabel#sectionHeader, QLabel#statusLabel { color: %8; }"
        "QLabel#playtimeBig { color: %3; font-size: 22px; font-weight: bold; }"
        "QFrame#sectionDivider { color: %2; }"
        "QProgressBar#achBar { border: none; background-color: %2; border-radius: 3px; max-height: 6px; }"
        "QProgressBar#achBar::chunk { background-color: %9; border-radius: 3px; }"
        "QTextBrowser#newsBrowser { background-color: %14; color: %3; border: 1px solid %2; border-radius: 6px; padding: 6px; }"
        "QDialog#settingsDlg QListWidget { background-color: %14; border: 1px solid %2; border-radius: 6px; }"
        "QDialog#settingsDlg QListWidget::item:selected { background-color: %6; color: %7; }"
    ).arg(
        p.cardBackground.name(),
        p.cardBorder.name(),
        p.foreground.name(),
        cssColor(sidebarSelectedBg),
        cssColor(sidebarSelectedText),
        cssColor(sidebarHoverBg),
        cssColor(sidebarHoverText),
        p.mutedForeground.name(),
        p.accent.name(),
        cssColor(bestTextColorFor(p.accent, p.foreground, p)),
        cssColor(compositeOver(p.accent, p.hoverBackground)),
        cssColor(compositeOver(p.accent, p.pressedBackground)),
        p.windowBackground.name(),
        cssColor(compositeOver(p.windowBackground, p.surfaceOverlay)),
        cssColor(heroOverlay));

    return FluentStyle::generate(p) + supplement;
}

static constexpr int kSidebarPlatformRole = Qt::UserRole + 100;
static constexpr int kSidebarIsSeparator  = Qt::UserRole + 101;

QString trackingIdForEntry(const GameEntry& e) {
    if (!e.platformId.trimmed().isEmpty()) return e.platformId.trimmed();

    const QString stable = QString("%1|%2|%3|%4")
        .arg(static_cast<int>(e.platform))
        .arg(e.title.trimmed().toLower())
        .arg(e.executablePath.trimmed().toLower())
        .arg(e.installPath.trimmed().toLower());

    const QByteArray hash = QCryptographicHash::hash(stable.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QString("local_%1").arg(QString::fromLatin1(hash.left(16)));
}

QString settingsKeyForEntry(const GameEntry& e) {
    return QString("%1|%2").arg(static_cast<int>(e.platform)).arg(trackingIdForEntry(e));
}

QString overrideLocatorForEntry(const GameEntry& e) {
    return QString("%1|%2|%3")
        .arg(static_cast<int>(e.platform))
        .arg(e.title.trimmed().toLower())
        .arg(e.installPath.trimmed().toLower());
}

void applyCustomArtOverride(GameEntry& e) {
    const QString key = settingsKeyForEntry(e);
    const QString path = GameVaultSettings::instance().customArtPath(key).trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    const QString artUrl = QUrl::fromLocalFile(path).toString();
    e.artBannerUrl = artUrl;
    e.artCapsuleUrl = artUrl;
}

QStringList newsTokensForTitle(const QString& title) {
    QString normalized = title.toLower();
    normalized.replace(QRegularExpression("[^a-z0-9]+"), " ");
    const QStringList raw = normalized.split(' ', Qt::SkipEmptyParts);

    QStringList tokens;
    tokens.reserve(raw.size());
    for (const QString& token : raw) {
        if (token.size() < 3) continue;
        if (token == "the" || token == "and" || token == "for" || token == "edition"
            || token == "game" || token == "classic") {
            continue;
        }
        tokens << token;
    }
    tokens.removeDuplicates();
    return tokens;
}

bool epicNewsItemMatchesGame(const QString& title, const QString& link, const QStringList& tokens) {
    if (tokens.isEmpty()) return false;

    const QString hay = (title + " " + link).toLower();
    int matched = 0;
    for (const QString& token : tokens) {
        if (hay.contains(token)) ++matched;
    }

    if (tokens.size() <= 2) return matched >= 1;
    return matched >= 2;
}

QString mutedHtml(const wintools::themes::ThemePalette& palette, const QString& text) {
    return QString("<span style='color:%1'>%2</span>")
        .arg(palette.mutedForeground.name(), text.toHtmlEscaped());
}

QString mutedHtmlWithLink(const wintools::themes::ThemePalette& palette,
                         const QString& text,
                         const QString& url,
                         const QString& linkText) {
    return QString("<span style='color:%1'>%2 <a href='%3' style='color:%4'>%5</a></span>")
        .arg(palette.mutedForeground.name(),
             text.toHtmlEscaped(),
             url,
             palette.accent.name(),
             linkText.toHtmlEscaped());
}

QString newsItemHtml(const wintools::themes::ThemePalette& palette,
                    const QString& url,
                    const QString& title,
                    const QString& date) {
    return QString("<div style='margin-bottom:8px'><a href='%1' style='color:%2;text-decoration:none'><b>%3</b></a><br><span style='color:%4;font-size:11px'>%5</span></div>")
        .arg(url,
             palette.accent.name(),
             title.toHtmlEscaped(),
             palette.mutedForeground.name(),
             date.toHtmlEscaped());
}

void applyExecutableIconArtFallback(GameEntry& e, QHash<QString, QPixmap>& cache) {
    if (!e.artBannerUrl.isEmpty() || !e.artCapsuleUrl.isEmpty()) return;

    QString iconSource = e.iconPath;
    if (iconSource.isEmpty()) iconSource = e.executablePath;
    if (iconSource.isEmpty() || !QFileInfo::exists(iconSource)) return;

    QIcon icon(iconSource);
    QPixmap iconPx = icon.pixmap(128, 128);
    if (iconPx.isNull()) {
        QFileIconProvider provider;
        icon = provider.icon(QFileInfo(iconSource));
        iconPx = icon.pixmap(128, 128);
    }
    if (iconPx.isNull()) return;

    const QString key = QString("local-art://%1").arg(trackingIdForEntry(e));
    if (!cache.contains(key)) {
        QPixmap banner(460, 215);
        banner.fill(QColor(0x14, 0x1C, 0x26));

        QPainter painter(&banner);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QLinearGradient grad(0, 0, 460, 215);
        grad.setColorAt(0.0, QColor(0x18, 0x24, 0x31));
        grad.setColorAt(1.0, QColor(0x10, 0x16, 0x20));
        painter.fillRect(banner.rect(), grad);

        const QSize iconSize(96, 96);
        QPixmap scaled = iconPx.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPoint pt((banner.width() - scaled.width()) / 2, (banner.height() - scaled.height()) / 2 - 6);
        painter.drawPixmap(pt, scaled);

        painter.setPen(QColor(0x8F, 0x98, 0xA0));
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        painter.drawText(QRect(12, banner.height() - 30, banner.width() - 24, 20), Qt::AlignCenter,
                         e.title.left(40));
        painter.end();

        cache.insert(key, banner);
    }

    e.artBannerUrl = key;
    e.artCapsuleUrl = key;
}

GameVaultWindow::GameVaultWindow(QWidget* parent)
    : QDialog(parent)
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setWindowTitle("Game Vault");
    setWindowIcon(QIcon(QStringLiteral(":/icons/modules/gamevault.svg")));
    setMinimumSize(1100, 680);
    resize(1360, 820);
    wintools::ui::enableRelativeSizeAcrossScreens(this);

    m_model = new GameListModel(this);
    m_proxy = new GameFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::DisplayRole);
    m_proxy->sort(GameCol::Title, Qt::AscendingOrder);

    m_nam = new QNetworkAccessManager(this);

    m_sgdbClient = new SteamGridDBClient(this);
    const QString sgdbKey = GameVaultSettings::instance().steamGridDbApiKey();
    if (!sgdbKey.isEmpty())
        m_sgdbClient->setApiKey(sgdbKey);

    buildUi();
    applyTheme(m_palette);

    m_themeListener = new wintools::themes::ThemeListener(this);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, &GameVaultWindow::onThemeChanged);
    startScan();
}

GameVaultWindow::~GameVaultWindow() {
    if (m_cardReply)   m_cardReply->abort();
    if (m_bannerReply) m_bannerReply->abort();
    if (m_newsReply)   m_newsReply->abort();
}

void GameVaultWindow::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildSidebar());

    auto* div = new QFrame(this);
    div->setObjectName("divider");
    div->setFixedWidth(1);
    root->addWidget(div);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildLibraryPage());
    m_stack->addWidget(buildDetailPage());
    m_stack->setCurrentIndex(kLibraryPage);
    root->addWidget(m_stack, 1);
}

QWidget* GameVaultWindow::buildSidebar() {
    auto* panel = new QWidget(this);
    panel->setObjectName("sidebarPanel");
    panel->setFixedWidth(220);

    auto* vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(0, 0, 0, 12);
    vbox->setSpacing(0);

    auto* headerRow = new QWidget(panel);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(16, 18, 16, 14);
    headerLayout->setSpacing(8);

    auto* headerIcon = new QLabel(headerRow);
    headerIcon->setPixmap(QIcon(QStringLiteral(":/icons/modules/gamevault.svg")).pixmap(18, 18));
    headerIcon->setFixedSize(18, 18);
    headerIcon->setAlignment(Qt::AlignCenter);

    auto* header = new QLabel("GAME VAULT", headerRow);
    header->setObjectName("sidebarHeader");
    QFont headerFont = header->font();
    headerFont.setPixelSize(15);
    headerFont.setBold(true);
    headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    header->setFont(headerFont);

    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(header);
    headerLayout->addStretch();
    vbox->addWidget(headerRow);

    m_sidebar = new QListWidget(panel);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setIconSize(QSize(16, 16));
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);

    auto addItem = [&](const QString& text, int platformVal, bool separator = false) {
        auto* item = new QListWidgetItem(text, m_sidebar);
        item->setData(kSidebarPlatformRole, platformVal);
        item->setData(kSidebarIsSeparator,  separator);
        if (separator) {
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(m_palette.mutedForeground);
            QFont f = item->font();
            f.setPixelSize(10);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            item->setFont(f);
        } else if (platformVal >= static_cast<int>(GamePlatform::Steam)
                   && platformVal <= static_cast<int>(GamePlatform::Unknown)) {
            item->setIcon(platformIcon(static_cast<GamePlatform>(platformVal)));
        }
        return item;
    };

    m_allGamesItem = addItem("ALL GAMES", -1);
    m_sidebar->setCurrentItem(m_allGamesItem);

    m_favouritesItem = addItem("★ FAVOURITES", -2);

    addItem("  PLATFORMS", -999, true);

    for (int p = static_cast<int>(GamePlatform::Steam);
             p <= static_cast<int>(GamePlatform::Unknown); ++p)
    {
        if (p == static_cast<int>(GamePlatform::Unknown)) continue;
        m_platformItems[p] = addItem("  " + platformName(static_cast<GamePlatform>(p)), p);
        m_platformItems[p]->setHidden(true);
    }

    m_platformItems[static_cast<int>(GamePlatform::Unknown)] =
        addItem("  " + platformName(GamePlatform::Unknown), static_cast<int>(GamePlatform::Unknown));
    m_platformItems[static_cast<int>(GamePlatform::Unknown)]->setHidden(true);

    connect(m_sidebar, &QListWidget::currentRowChanged,
            this, &GameVaultWindow::onSidebarItemClicked);
    connect(m_sidebar, &QListWidget::customContextMenuRequested,
            this, &GameVaultWindow::onSidebarContextMenu);

    vbox->addWidget(m_sidebar, 1);

    auto* hr = new QFrame(panel);
    hr->setObjectName("divider");
    hr->setFrameShape(QFrame::HLine);
    hr->setStyleSheet(QString("color: %1; margin: 6px 12px;").arg(m_palette.divider.name()));
    vbox->addWidget(hr);

    m_installedToggle = new QPushButton("Installed only", panel);
    m_installedToggle->setIcon(uiIcon("check_off"));
    m_installedToggle->setObjectName("installedToggle");
    m_installedToggle->setCheckable(true);
    m_installedToggle->setFlat(true);
    connect(m_installedToggle, &QPushButton::toggled, this, &GameVaultWindow::onInstalledOnlyToggled);
    vbox->addWidget(m_installedToggle);

    m_settingsBtn = new QPushButton("Scan Paths…", panel);
    m_settingsBtn->setIcon(uiIcon("settings"));
    m_settingsBtn->setObjectName("settingsBtn");
    m_settingsBtn->setFlat(true);
    connect(m_settingsBtn, &QPushButton::clicked, this, &GameVaultWindow::openSettings);
    vbox->addWidget(m_settingsBtn);

    return panel;
}

QWidget* GameVaultWindow::buildLibraryPage() {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(16, 12, 16, 0);
    vbox->setSpacing(8);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);

    m_search = new QLineEdit(page);
    m_search->setPlaceholderText("Search your library…");
    m_search->addAction(uiIcon("search"), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(320);
    connect(m_search, &QLineEdit::textChanged, this, &GameVaultWindow::onSearchChanged);

    m_sortCombo = new QComboBox(page);
    m_sortCombo->setFixedWidth(140);
    m_sortCombo->addItem(QStringLiteral("Name"),          static_cast<int>(GameFilterProxy::SortByName));
    m_sortCombo->addItem(QStringLiteral("Platform"),      static_cast<int>(GameFilterProxy::SortByPlatform));
    m_sortCombo->addItem(QStringLiteral("Playtime"),      static_cast<int>(GameFilterProxy::SortByPlaytime));
    m_sortCombo->addItem(QStringLiteral("Last Played"),   static_cast<int>(GameFilterProxy::SortByLastPlayed));
    m_sortCombo->addItem(QStringLiteral("Recently Added"),static_cast<int>(GameFilterProxy::SortByRecentlyAdded));
    m_sortCombo->setToolTip(QStringLiteral("Sort library by…"));
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const auto mode = static_cast<GameFilterProxy::SortMode>(m_sortCombo->itemData(idx).toInt());
        m_proxy->setSortMode(mode);
    });

    m_rescanBtn = new QPushButton("Rescan", page);
    m_rescanBtn->setIcon(uiIcon("rescan"));
    m_rescanBtn->setFixedWidth(96);
    connect(m_rescanBtn, &QPushButton::clicked, this, &GameVaultWindow::rescan);

    auto* addGameBtn = new QPushButton("Add Game", page);
    addGameBtn->setIcon(uiIcon("add"));
    addGameBtn->setFixedWidth(110);
    connect(addGameBtn, &QPushButton::clicked, this, &GameVaultWindow::addCustomGame);

    toolbar->addWidget(m_search);
    toolbar->addWidget(m_sortCombo);
    toolbar->addStretch(1);
    toolbar->addWidget(addGameBtn);
    toolbar->addWidget(m_rescanBtn);
    vbox->addLayout(toolbar);

    m_cardDelegate = new GameCardDelegate(this);
    m_cardDelegate->setArtCache(&m_artCache);

    m_gridView = new QListView(page);
    m_gridView->setObjectName("gridView");
    m_gridView->setModel(m_proxy);
    m_gridView->setItemDelegate(m_cardDelegate);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setWrapping(true);
    m_gridView->setSpacing(0);
    m_gridView->setUniformItemSizes(true);
    m_gridView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_gridView->setMouseTracking(true);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gridView, &QListView::activated,
            this, &GameVaultWindow::onGridActivated);
    connect(m_gridView, &QListView::doubleClicked,
            this, &GameVaultWindow::onGridActivated);
    connect(m_gridView, &QListView::customContextMenuRequested,
            this, &GameVaultWindow::onGridContextMenu);
    vbox->addWidget(m_gridView, 1);

    m_statusLabel = new QLabel("Scanning library…", page);
    m_statusLabel->setObjectName("statusLabel");
    vbox->addWidget(m_statusLabel);

    return page;
}

QWidget* GameVaultWindow::buildDetailPage() {
    auto* page = new QWidget(this);
    page->setObjectName("detailPage");

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("detailScroll");
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(16, 10, 16, 10);
    topBar->setSpacing(12);

    auto* backBtn = new QPushButton("Back to Library", page);
    backBtn->setIcon(uiIcon("back"));
    backBtn->setObjectName("backBtn");
    connect(backBtn, &QPushButton::clicked, this, &GameVaultWindow::goToLibrary);
    topBar->addWidget(backBtn);
    topBar->addStretch(1);

    m_folderBtn = new QPushButton("Browse", page);
    m_folderBtn->setIcon(uiIcon("folder"));
    connect(m_folderBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentEntry.installPath.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentEntry.installPath));
    });
    topBar->addWidget(m_folderBtn);

    m_customArtBtn = new QPushButton("Custom Art", page);
    m_customArtBtn->setIcon(uiIcon("image"));
    connect(m_customArtBtn, &QPushButton::clicked, this, [this]() {
        const QString art = QFileDialog::getOpenFileName(
            this,
            QString("Select custom art for %1").arg(m_currentEntry.title),
            {},
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (art.isEmpty()) return;

        const QString key = settingsKeyForEntry(m_currentEntry);
        GameVaultSettings::instance().setCustomArtPath(key, QDir::fromNativeSeparators(art));
        startScan();
    });
    topBar->addWidget(m_customArtBtn);

    m_playBtn = new QPushButton("PLAY", page);
    m_playBtn->setIcon(uiIcon("play"));
    m_playBtn->setObjectName("playBtn");
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        launchEntry(m_currentEntry);
    });
    topBar->addWidget(m_playBtn);

    outer->addLayout(topBar);

    auto* heroArea = new QWidget(page);
    heroArea->setObjectName("heroArea");
    heroArea->setFixedHeight(310);
    auto* heroLayout = new QVBoxLayout(heroArea);
    heroLayout->setContentsMargins(0, 0, 0, 0);

    m_heroLabel = new QLabel(heroArea);
    m_heroLabel->setAlignment(Qt::AlignCenter);
    m_heroLabel->setMinimumHeight(216);
    m_heroLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_heroLabel->setObjectName("heroImage");
    heroLayout->addWidget(m_heroLabel, 1);

    m_heroTitle = new QLabel(heroArea);
    m_heroTitle->setObjectName("heroTitle");
    m_heroTitle->setWordWrap(true);
    m_heroTitle->setContentsMargins(20, 8, 20, 8);
    heroLayout->addWidget(m_heroTitle);

    outer->addWidget(heroArea);

    auto* body = new QWidget(page);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(30, 20, 30, 30);
    bodyLayout->setSpacing(0);

    m_detailPlatform = new QLabel(body);
    m_detailPlatform->setObjectName("detailPlatform");
    bodyLayout->addWidget(m_detailPlatform);
    bodyLayout->addSpacing(20);

    auto mkSection = [&](const QString& title) {
        auto* lbl = new QLabel(title.toUpper(), body);
        lbl->setObjectName("sectionHeader");
        bodyLayout->addWidget(lbl);
        auto* hr2 = new QFrame(body);
        hr2->setFrameShape(QFrame::HLine);
        hr2->setObjectName("sectionDivider");
        hr2->setStyleSheet("margin: 4px 0 12px 0;");
        bodyLayout->addWidget(hr2);
    };

    mkSection("Playtime");

    m_detailPlaytime = new QLabel(body);
    m_detailPlaytime->setObjectName("playtimeBig");
    bodyLayout->addWidget(m_detailPlaytime);

    m_detailLastPlay = new QLabel(body);
    m_detailLastPlay->setObjectName("playtimeSub");
    bodyLayout->addWidget(m_detailLastPlay);

    bodyLayout->addSpacing(24);

    m_achSection = new QFrame(body);
    m_achSection->setFrameShape(QFrame::NoFrame);
    auto* achLayout = new QVBoxLayout(m_achSection);
    achLayout->setContentsMargins(0, 0, 0, 0);
    achLayout->setSpacing(6);

    auto* achHeader = new QLabel("ACHIEVEMENTS", m_achSection);
    achHeader->setObjectName("sectionHeader");
    achLayout->addWidget(achHeader);

    auto* achHr = new QFrame(m_achSection);
    achHr->setFrameShape(QFrame::HLine);
    achHr->setObjectName("sectionDivider");
    achHr->setStyleSheet("margin: 4px 0 12px 0;");
    achLayout->addWidget(achHr);

    m_achBar = new QProgressBar(m_achSection);
    m_achBar->setObjectName("achBar");
    m_achBar->setTextVisible(false);
    m_achBar->setFixedHeight(6);
    m_achBar->setRange(0, 100);
    achLayout->addWidget(m_achBar);

    m_achLabel = new QLabel(m_achSection);
    m_achLabel->setObjectName("achLabel");
    achLayout->addWidget(m_achLabel);

    bodyLayout->addWidget(m_achSection);
    bodyLayout->addSpacing(20);

    mkSection("News");
    m_newsBrowser = new QTextBrowser(body);
    m_newsBrowser->setOpenExternalLinks(true);
    m_newsBrowser->setMaximumHeight(180);
    m_newsBrowser->setObjectName("newsBrowser");
    m_newsBrowser->setHtml(mutedHtml(m_palette, "No news loaded."));
    bodyLayout->addWidget(m_newsBrowser);

    bodyLayout->addStretch(1);

    outer->addWidget(body, 1);

    return scroll;
}

void GameVaultWindow::startScan() {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Library scan started.");
    m_rescanBtn->setEnabled(false);
    setStatusText("Scanning…");
    m_model->clear();
    m_artCache.clear();
    m_artQueue.clear();
    if (m_cardReply) { m_cardReply->abort(); m_cardReply = nullptr; }

    auto* worker = new GameLibraryWorker(nullptr);
    connect(worker, &GameLibraryWorker::scanComplete,
            this, &GameVaultWindow::onScanComplete, Qt::QueuedConnection);
    connect(worker, &GameLibraryWorker::scanError,
            this, &GameVaultWindow::onScanError,    Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(worker);
}

void GameVaultWindow::rescan() { startScan(); }

void GameVaultWindow::onScanComplete(QVector<wintools::gamevault::GameEntry> games) {

    const QVector<GameEntry> manualGames = GameVaultSettings::instance().manualGames();
    for (GameEntry mg : manualGames) {
        games.push_back(std::move(mg));
    }

    for (GameEntry& g : games) {

        const QString locatorKey = overrideLocatorForEntry(g);
        const QString overrideExe = GameVaultSettings::instance().gameExecutableOverridePath(locatorKey).trimmed();
        if (!overrideExe.isEmpty()) {
            g.executablePath = overrideExe;
            if (g.installPath.trimmed().isEmpty()) {
                g.installPath = QFileInfo(overrideExe).absolutePath();
            }
            g.installed = QFileInfo::exists(overrideExe);
        }

        const QString trackingOverride = GameVaultSettings::instance().gameTrackingIdOverride(locatorKey).trimmed();
        if (!trackingOverride.isEmpty() && g.platformId.trimmed().isEmpty()) {
            g.platformId = trackingOverride;
        }

        const QString plat = platformName(g.platform);
        const QString trackId = trackingIdForEntry(g);
        const quint64 trackedPlaytime = PlaytimeTracker::instance().playtime(plat, trackId);
        const qint64 trackedLast = PlaytimeTracker::instance().lastPlayed(plat, trackId);

        if (trackedPlaytime > g.playtimeSeconds) g.playtimeSeconds = trackedPlaytime;
        if (trackedLast > g.lastPlayedEpoch) g.lastPlayedEpoch = trackedLast;

        applyCustomArtOverride(g);
        applyExecutableIconArtFallback(g, m_artCache);
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Library scan complete."),
        QStringLiteral("%1 games found").arg(games.size()));
    m_model->setGames(games);
    m_proxy->sort(GameCol::Title, Qt::AscendingOrder);
    setStatusText(QString("%1 games in library").arg(games.size()));
    m_rescanBtn->setEnabled(true);
    updateSidebarCounts();
    updateFavouritesCount();
    rebuildArtQueue();
    fetchNextCardArt();
    startSteamGridDbLookups();
}

void GameVaultWindow::onScanError(QString message) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Error,
        QStringLiteral("Library scan error."), message);
    setStatusText("Scan error: " + message);
    m_rescanBtn->setEnabled(true);
}

void GameVaultWindow::onSearchChanged(const QString& text) {
    m_proxy->setSearchText(text);
    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onSidebarItemClicked(int row) {
    if (row < 0) return;
    const QListWidgetItem* item = m_sidebar->item(row);
    if (!item) return;

    wintools::logger::Logger::log(
        kLog,
        wintools::logger::Severity::Pass,
        QStringLiteral("Sidebar item clicked."),
        QStringLiteral("row=%1 text=%2 platformRole=%3 separator=%4 dark=%5")
            .arg(row)
            .arg(item->text())
            .arg(item->data(kSidebarPlatformRole).toInt())
            .arg(item->data(kSidebarIsSeparator).toBool())
            .arg(wintools::themes::ThemeHelper::isDarkTheme()));

    if (item->data(kSidebarIsSeparator).toBool()) {

        m_sidebar->clearSelection();
        return;
    }

    const int platVal = item->data(kSidebarPlatformRole).toInt();
    if (platVal == -1) {
        m_proxy->clearPlatformFilter();
        m_proxy->setFavouritesOnly(false);
    } else if (platVal == -2) {
        m_proxy->clearPlatformFilter();
        m_proxy->setFavouritesOnly(true);
    } else {
        m_proxy->setFavouritesOnly(false);
        m_proxy->setPlatformFilter(static_cast<GamePlatform>(platVal));
    }

    updateSidebarItemStyles();

    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onSidebarContextMenu(const QPoint& pos) {
    if (!m_sidebar) return;

    QListWidgetItem* item = m_sidebar->itemAt(pos);
    if (!item) return;
    if (item->data(kSidebarIsSeparator).toBool()) return;

    const int platVal = item->data(kSidebarPlatformRole).toInt();
    if (platVal < static_cast<int>(GamePlatform::Steam)
        || platVal > static_cast<int>(GamePlatform::Unknown)) {
        return;
    }

    const GamePlatform platform = static_cast<GamePlatform>(platVal);

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; }"
        "QMenu::item:selected { background-color: %4; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.foreground.name(),
             m_palette.cardBorder.name(),
             m_palette.hoverBackground.name()));

    const QString actionText = platformLauncherActionText(platform);
    menu.addAction(uiIcon("launch"), actionText, this, [this, platform]() {
        QString error;
        if (openPlatformProgram(platform, &error)) return;

        QMessageBox::information(
            this,
            QStringLiteral("Launcher not available"),
            error.isEmpty()
                ? QStringLiteral("Unable to open launcher for %1.").arg(platformName(platform))
                : error);
    });

    menu.exec(m_sidebar->viewport()->mapToGlobal(pos));
}

void GameVaultWindow::onInstalledOnlyToggled(bool checked) {
    m_installedOnly = checked;
    m_installedToggle->setText("Installed only");
    m_installedToggle->setIcon(checked ? uiIcon("check_on") : uiIcon("check_off"));
    m_proxy->setInstalledOnly(checked);
    setStatusText(QString("%1 games shown").arg(m_proxy->rowCount()));
}

void GameVaultWindow::onGridActivated(const QModelIndex& idx) {
    if (!idx.isValid()) return;
    const QModelIndex src = m_proxy->mapToSource(idx);
    const GameEntry*  e   = m_model->entryAt(src);
    if (e) goToDetail(*e);
}

void GameVaultWindow::onGridContextMenu(const QPoint& pos) {
    const QModelIndex proxyIdx = m_gridView->indexAt(pos);
    if (!proxyIdx.isValid()) return;
    const GameEntry* e = m_model->entryAt(m_proxy->mapToSource(proxyIdx));
    if (!e) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; }"
        "QMenu::item:selected { background-color: %4; }")
        .arg(m_palette.cardBackground.name(),
             m_palette.foreground.name(),
             m_palette.cardBorder.name(),
             m_palette.hoverBackground.name()));

    menu.addAction(uiIcon("play"), "Play", this, [this, e]() { launchEntry(*e); });
    menu.addAction(uiIcon("info"), "View details", this, [this, e]() { goToDetail(*e); });

    {
        const QString platStr = platformName(e->platform);
        const bool isFav = GameTagStore::instance().isFavourite(platStr, e->platformId);
        menu.addAction(isFav ? "★ Remove from Favourites" : "☆ Add to Favourites",
                       this, [this, platStr, pid = e->platformId]() {
                           GameTagStore::instance().toggleFavourite(platStr, pid);
                           updateFavouritesCount();
                       });
    }

    {
        const QString platStr = platformName(e->platform);
        const QStringList existing = GameTagStore::instance().tags(platStr, e->platformId);
        const QStringList allTags  = GameTagStore::instance().allTags();

        QMenu* tagMenu = menu.addMenu("Tags");
        tagMenu->setStyleSheet(menu.styleSheet());

        for (const QString& t : existing) {
            tagMenu->addAction("✓ " + t, this, [this, platStr, pid = e->platformId, t]() {
                GameTagStore::instance().removeTag(platStr, pid, t);
            });
        }

        for (const QString& t : allTags) {
            if (existing.contains(t)) continue;
            tagMenu->addAction("   " + t, this, [this, platStr, pid = e->platformId, t]() {
                GameTagStore::instance().addTag(platStr, pid, t);
            });
        }

        if (!allTags.isEmpty() || !existing.isEmpty())
            tagMenu->addSeparator();

        tagMenu->addAction("New tag…", this, [this, platStr, pid = e->platformId]() {
            bool ok = false;
            const QString tag = QInputDialog::getText(
                this, "Add Tag", "Tag name:", QLineEdit::Normal, {}, &ok);
            if (ok && !tag.trimmed().isEmpty()) {
                GameTagStore::instance().addTag(platStr, pid, tag.trimmed());
            }
        });
    }

    menu.addAction("Copy title",      this, [e]() {
        QGuiApplication::clipboard()->setText(e->title);
    });
    if (e->platform == GamePlatform::Unknown) {
        menu.addAction("Change executable…", this, [this, e]() {
            const QString newExe = QFileDialog::getOpenFileName(
                this,
                QString("Select executable for %1").arg(e->title),
                e->installPath,
                "Executables (*.exe *.bat *.cmd *.lnk);;All files (*)");
            if (newExe.trimmed().isEmpty()) return;

            const QString oldTrackId = trackingIdForEntry(*e);
            auto& settings = GameVaultSettings::instance();

            bool updatedManual = false;
            QVector<GameEntry> manualGames = settings.manualGames();
            for (GameEntry& mg : manualGames) {
                if (settingsKeyForEntry(mg) != settingsKeyForEntry(*e)) {
                    continue;
                }

                mg.executablePath = QDir::fromNativeSeparators(newExe);
                mg.installPath = QFileInfo(newExe).absolutePath();
                mg.installed = QFileInfo::exists(newExe);
                if (mg.platformId.trimmed().isEmpty()) {
                    mg.platformId = oldTrackId;
                }
                updatedManual = true;
                break;
            }

            if (updatedManual) {
                settings.setManualGames(manualGames);
            } else {
                const QString locatorKey = overrideLocatorForEntry(*e);
                settings.setGameExecutableOverride(
                    locatorKey,
                    QDir::fromNativeSeparators(newExe),
                    oldTrackId);
            }

            wintools::logger::Logger::log(
                kLog,
                wintools::logger::Severity::Pass,
                QStringLiteral("Game executable updated."),
                QStringLiteral("title=%1 newExe=%2").arg(e->title, newExe));
            startScan();
        });
    }
    menu.addSeparator();
    menu.addAction(uiIcon("image"), "Set custom art…", this, [this, e]() {
        const QString art = QFileDialog::getOpenFileName(
            this,
            QString("Select custom art for %1").arg(e->title),
            {},
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (art.isEmpty()) return;

        const QString key = settingsKeyForEntry(*e);
        GameVaultSettings::instance().setCustomArtPath(key, QDir::fromNativeSeparators(art));
        startScan();
    });
    menu.addAction("🧹  Clear custom art", this, [this, e]() {
        const QString key = settingsKeyForEntry(*e);
        GameVaultSettings::instance().clearCustomArtPath(key);
        startScan();
    });
    if (!e->installPath.isEmpty()) {
        menu.addAction(uiIcon("folder"), "Open folder", this, [e]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(e->installPath));
        });
    }
    menu.exec(m_gridView->viewport()->mapToGlobal(pos));
}

void GameVaultWindow::goToDetail(const GameEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Viewing game detail."),
        QStringLiteral("title=%1 platform=%2").arg(e.title).arg(static_cast<int>(e.platform)));
    m_currentEntry = e;

    m_heroTitle->setText(e.title);
    m_heroLabel->setPixmap({});
    m_heroLabel->setText("Loading…");

    QString meta = QStringLiteral("<b>%1</b>")
        .arg(platformName(e.platform).toHtmlEscaped());
    if (!e.systemTag.isEmpty() && e.systemTag != platformName(e.platform))
        meta += "  ·  " + e.systemTag;
    const QString support = platformSupportLabel(e.platform);
    if (!support.isEmpty()) {
        meta += QStringLiteral("  ·  Supports: %1").arg(support);
    }
    m_detailPlatform->setText(meta);

    const QString platStr = platformName(e.platform);
    quint64 totalPlaytime = e.playtimeSeconds;
    qint64  lastPlayEpoch = e.lastPlayedEpoch;

    const QString trackId = trackingIdForEntry(e);
    quint64 tracked = PlaytimeTracker::instance().playtime(platStr, trackId);
    totalPlaytime = qMax(totalPlaytime, tracked);
    qint64 trackedLast = PlaytimeTracker::instance().lastPlayed(platStr, trackId);
    if (trackedLast > lastPlayEpoch)
        lastPlayEpoch = trackedLast;

    if (totalPlaytime > 0) {
        const quint64 h = totalPlaytime / 3600;
        const quint64 m = (totalPlaytime % 3600) / 60;
        const quint64 s = totalPlaytime % 60;
        if (h > 0) {
            m_detailPlaytime->setText(
                QString("%1.%2 hours on record").arg(h).arg(m / 6));
        } else if (m > 0) {
            m_detailPlaytime->setText(QString("%1 minutes on record").arg(m));
        } else {
            m_detailPlaytime->setText(QString("%1 seconds on record").arg(s));
        }
    } else {
        if (e.platform == GamePlatform::EpicGames) {
            m_detailPlaytime->setText("Epic playtime unavailable");
        } else {
            m_detailPlaytime->setText("No playtime recorded");
        }
    }

    if (lastPlayEpoch > 0)
        m_detailLastPlay->setText(
            "Last played  " +
            QDateTime::fromSecsSinceEpoch(lastPlayEpoch).toString("d MMMM yyyy"));
    else
        m_detailLastPlay->setText("Never played");

    if (e.achievementsTotal > 0) {
        m_achBar->setValue(e.achievementsPercent());
        m_achLabel->setText(
            QString("%1 of %2 achievements  (%3%)")
                .arg(e.achievementsUnlocked)
                .arg(e.achievementsTotal)
                .arg(e.achievementsPercent()));
        m_achSection->setVisible(true);
    } else if (e.platform == GamePlatform::EpicGames) {
        m_achBar->setValue(0);
        m_achLabel->setText("Epic achievements not detected locally for this title.");
        m_achSection->setVisible(true);
    } else {
        m_achSection->setVisible(false);
    }

    m_folderBtn->setVisible(!e.installPath.isEmpty());

    if (m_newsReply) {
        m_newsReply->abort();
        m_newsReply = nullptr;
    }

    if (e.platform == GamePlatform::Steam && !e.platformId.isEmpty()) {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml(mutedHtml(m_palette, "Loading Steam news…"));

        QUrl api("https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/");
        QUrlQuery query;
        query.addQueryItem("appid", e.platformId);
        query.addQueryItem("count", "5");
        query.addQueryItem("maxlength", "180");
        query.addQueryItem("format", "json");
        api.setQuery(query);

        QNetworkRequest req(api);
        req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools-GameVault/1.0");
        m_newsReply = m_nam->get(req);
        connect(m_newsReply, &QNetworkReply::finished, this, [this]() {
            auto* reply = qobject_cast<QNetworkReply*>(sender());
            if (!reply) return;
            reply->deleteLater();
            if (reply != m_newsReply) return;
            m_newsReply = nullptr;

            if (reply->error() != QNetworkReply::NoError) {
                m_newsBrowser->setHtml(mutedHtml(m_palette, "Unable to load Steam news."));
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray items = root.value("appnews").toObject().value("newsitems").toArray();
            if (items.isEmpty()) {
                m_newsBrowser->setHtml(mutedHtml(m_palette, "No recent Steam news for this game."));
                return;
            }

            QString html;
            for (const QJsonValue& value : items) {
                const QJsonObject n = value.toObject();
                const QString title = n.value("title").toString();
                const QString url = n.value("url").toString();
                const qint64 epoch = n.value("date").toInteger();
                const QString date = epoch > 0
                    ? QDateTime::fromSecsSinceEpoch(epoch).toString("d MMM yyyy")
                    : QString();

                html += newsItemHtml(m_palette, url, title, date);
            }
            m_newsBrowser->setHtml(html);
        });
    } else if (e.platform == GamePlatform::EpicGames) {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml(mutedHtml(m_palette, "Loading Epic Games Store news…"));

        QNetworkRequest req(QUrl("https://store.epicgames.com/en-US/news/rss"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "WinTools-GameVault/1.0");
        m_newsReply = m_nam->get(req);
        connect(m_newsReply, &QNetworkReply::finished, this, [this, title = e.title]() {
            auto* reply = qobject_cast<QNetworkReply*>(sender());
            if (!reply) return;
            reply->deleteLater();
            if (reply != m_newsReply) return;
            m_newsReply = nullptr;

            if (reply->error() != QNetworkReply::NoError) {
                m_newsBrowser->setHtml(
                    mutedHtmlWithLink(
                        m_palette,
                        "Unable to load Epic news feed.",
                        "https://store.epicgames.com/en-US/news",
                        "Open Epic News"));
                return;
            }

            QXmlStreamReader xml(reply->readAll());
            struct Item { QString t; QString l; QString d; };
            QVector<Item> items;
            Item current;
            bool inItem = false;

            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == "item") {
                    inItem = true;
                    current = Item{};
                } else if (xml.isEndElement() && xml.name() == "item") {
                    inItem = false;
                    if (!current.t.isEmpty() && !current.l.isEmpty()) {
                        items.push_back(current);
                    }
                } else if (inItem && xml.isStartElement()) {
                    if (xml.name() == "title") current.t = xml.readElementText();
                    else if (xml.name() == "link") current.l = xml.readElementText();
                    else if (xml.name() == "pubDate") current.d = xml.readElementText();
                }
            }

            if (items.isEmpty()) {
                m_newsBrowser->setHtml(
                    mutedHtmlWithLink(
                        m_palette,
                        "No Epic news items found.",
                        "https://store.epicgames.com/en-US/news",
                        "Open Epic News"));
                return;
            }

            const QStringList tokens = newsTokensForTitle(title);
            QString html;
            int count = 0;
            for (const Item& item : items) {
                if (!epicNewsItemMatchesGame(item.t, item.l, tokens)) {
                    continue;
                }
                html += newsItemHtml(m_palette, item.l, item.t, item.d);
                ++count;
                if (count >= 5) break;
            }

            if (html.isEmpty()) {
                m_newsBrowser->setHtml(
                    mutedHtmlWithLink(
                        m_palette,
                        "No game-specific Epic news found for this title.",
                        "https://store.epicgames.com/en-US/news",
                        "Open Epic News"));
                return;
            }

            m_newsBrowser->setHtml(html);
        });
    } else {
        m_newsBrowser->setVisible(true);
        m_newsBrowser->setHtml(mutedHtml(m_palette, "News feed is available for Steam titles."));
    }

    m_stack->setCurrentIndex(kDetailPage);

    const QString artUrl = e.artBannerUrl.isEmpty() ? e.artCapsuleUrl : e.artBannerUrl;
    if (!artUrl.isEmpty()) {
        if (m_artCache.contains(artUrl)) {

            const QPixmap& px = m_artCache[artUrl];
            m_heroLabel->setPixmap(
                px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation));
        } else {
            fetchDetailBanner(artUrl);
        }
    } else {
        QString iconSource = e.iconPath;
        if (iconSource.isEmpty()) iconSource = e.executablePath;

        if (!iconSource.isEmpty()) {
            QFileIconProvider provider;
            const QIcon icon = provider.icon(QFileInfo(iconSource));
            const QPixmap iconPx = icon.pixmap(160, 160);
            if (!iconPx.isNull()) {
                m_heroLabel->setPixmap(iconPx);
            } else {
                m_heroLabel->setText(platformName(e.platform));
            }
        } else {
            m_heroLabel->setText(platformName(e.platform));
        }
    }
}

void GameVaultWindow::goToLibrary() {
    if (m_bannerReply) { m_bannerReply->abort(); m_bannerReply = nullptr; }
    m_stack->setCurrentIndex(kLibraryPage);
}

void GameVaultWindow::updateSidebarCounts() {

    QHash<int, int> counts;
    int total = 0;
    for (int r = 0; r < m_model->rowCount(); ++r) {
        const GameEntry* e = m_model->entryAt(r);
        if (!e) continue;
        ++counts[static_cast<int>(e->platform)];
        ++total;
    }

    if (m_allGamesItem)
        m_allGamesItem->setText(QString("ALL GAMES  (%1)").arg(total));

    for (auto it = m_platformItems.begin(); it != m_platformItems.end(); ++it) {
        const int platKey = it.key();
        const int cnt     = counts.value(platKey, 0);
        QListWidgetItem* item = it.value();
        if (!item) continue;
        if (cnt == 0) {
            item->setHidden(true);
        } else {
            const QString name = platformName(static_cast<GamePlatform>(platKey));
            item->setIcon(platformIcon(static_cast<GamePlatform>(platKey)));
            item->setText(QString("  %1  (%2)").arg(name).arg(cnt));
            item->setHidden(false);
        }
    }

    updateSidebarItemStyles();
}

void GameVaultWindow::updateFavouritesCount() {
    if (!m_favouritesItem) return;
    const int n = GameTagStore::instance().favouriteCount();
    m_favouritesItem->setText(n > 0 ? QStringLiteral("★ FAVOURITES  (%1)").arg(n)
                                    : QStringLiteral("★ FAVOURITES"));
}

void GameVaultWindow::rebuildArtQueue() {
    m_artQueue.clear();
    for (int r = 0; r < m_model->rowCount(); ++r) {
        const GameEntry* e = m_model->entryAt(r);
        if (!e) continue;
        const QString url = e->artBannerUrl.isEmpty() ? e->artCapsuleUrl : e->artBannerUrl;
        if (!url.isEmpty() && !m_artCache.contains(url))
            m_artQueue << url;
    }

    m_artQueue.removeDuplicates();
}

void GameVaultWindow::fetchNextCardArt() {
    if (m_cardReply || m_artQueue.isEmpty()) return;

    const QString url = m_artQueue.takeFirst();
    m_cardReplyUrl = url;

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_cardReply = m_nam->get(req);
    connect(m_cardReply, &QNetworkReply::finished,
            this, &GameVaultWindow::onCardArtLoaded);
}

void GameVaultWindow::onCardArtLoaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_cardReply) return;
    m_cardReply = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap px;
        if (px.loadFromData(reply->readAll()) && !px.isNull())
            m_artCache[m_cardReplyUrl] = px;
    }

    QRect dirty;
    if (!m_cardReplyUrl.isEmpty()) {
        for (int r = 0; r < m_proxy->rowCount(); ++r) {
            const QModelIndex proxyIdx = m_proxy->index(r, 0);
            if (!proxyIdx.isValid()) continue;
            const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
            const GameEntry* e = m_model->entryAt(srcIdx);
            if (!e) continue;
            const QString cardUrl = e->artBannerUrl.isEmpty() ? e->artCapsuleUrl : e->artBannerUrl;
            if (cardUrl != m_cardReplyUrl) continue;

            const QRect rect = m_gridView->visualRect(proxyIdx);
            if (!rect.isValid() || rect.isEmpty()) continue;
            dirty = dirty.isNull() ? rect : dirty.united(rect);
        }
    }
    if (!dirty.isNull()) {
        m_gridView->viewport()->update(dirty.adjusted(-2, -2, 2, 2));
    } else {
        m_gridView->viewport()->update();
    }

    const QString curUrl = m_currentEntry.artBannerUrl.isEmpty()
                           ? m_currentEntry.artCapsuleUrl
                           : m_currentEntry.artBannerUrl;
    if (m_stack->currentIndex() == kDetailPage && curUrl == m_cardReplyUrl
        && m_artCache.contains(curUrl)) {
        const QPixmap& px = m_artCache[curUrl];
        m_heroLabel->setPixmap(
            px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    }

    fetchNextCardArt();
}

void GameVaultWindow::fetchDetailBanner(const QString& url) {
    if (m_bannerReply) { m_bannerReply->abort(); m_bannerReply = nullptr; }

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_bannerReply = m_nam->get(req);
    connect(m_bannerReply, &QNetworkReply::finished,
            this, &GameVaultWindow::onBannerLoaded);
}

void GameVaultWindow::onBannerLoaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_bannerReply) return;
    m_bannerReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        m_heroLabel->setText("(no art)");
        return;
    }

    QPixmap px;
    if (px.loadFromData(reply->readAll()) && !px.isNull()) {
        const QString url = m_currentEntry.artBannerUrl.isEmpty()
                            ? m_currentEntry.artCapsuleUrl
                            : m_currentEntry.artBannerUrl;
        m_artCache[url] = px;
        m_heroLabel->setPixmap(
            px.scaled(m_heroLabel->size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation));
    } else {
        m_heroLabel->setText("(no art)");
    }
}

void GameVaultWindow::openSettings() {
    auto* dlg = new QDialog(this);
    dlg->setObjectName("settingsDlg");
    dlg->setWindowTitle("Game Vault – Scan Paths");
    dlg->setMinimumSize(540, 420);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(styleSheet());

    auto& settings = GameVaultSettings::instance();
    auto* layout   = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* folderLbl = new QLabel(
        QStringLiteral("<b style='color:%1'>Custom game folders</b><br>"
                       "<span style='color:%2;font-size:11px'>"
                       "Each folder is scanned for game sub-directories and executables.</span>")
            .arg(m_palette.foreground.name(), m_palette.mutedForeground.name()),
        dlg);
    folderLbl->setTextFormat(Qt::RichText);
    layout->addWidget(folderLbl);

    auto* folderList = new QListWidget(dlg);
    folderList->addItems(settings.customGameFolders());
    layout->addWidget(folderList, 1);

    auto* folderBtns = new QHBoxLayout;
    auto* addFolderBtn = new QPushButton("+ Add folder…", dlg);
    auto* remFolderBtn = new QPushButton("− Remove",       dlg);
    folderBtns->addWidget(addFolderBtn);
    folderBtns->addWidget(remFolderBtn);
    folderBtns->addStretch();
    layout->addLayout(folderBtns);

    connect(addFolderBtn, &QPushButton::clicked, dlg, [=, &settings]() {
        const QString dir = QFileDialog::getExistingDirectory(dlg, "Select game folder");
        if (!dir.isEmpty()) {
            settings.addCustomGameFolder(dir);
            folderList->addItem(dir);
        }
    });
    connect(remFolderBtn, &QPushButton::clicked, dlg, [=, &settings]() {
        for (auto* item : folderList->selectedItems()) {
            settings.removeCustomGameFolder(item->text());
            delete item;
        }
    });

    auto* emuInfoLabel = new QLabel(
        QString("<b style='color:%1'>Emulator executable overrides</b><br>"
                "<span style='color:%2;font-size:11px'>"
                "Leave blank to auto-detect.</span>")
            .arg(m_palette.foreground.name(), m_palette.mutedForeground.name()),
        dlg);
    emuInfoLabel->setTextFormat(Qt::RichText);
    layout->addWidget(emuInfoLabel);

    const QStringList emulatorNames = {"RPCS3", "Yuzu", "Ryujinx", "Dolphin", "DeSmuME"};
    QHash<QString, QLineEdit*> emuEdits;

    auto* emuWidget = new QWidget(dlg);
    auto* emuGrid   = new QVBoxLayout(emuWidget);
    emuGrid->setSpacing(4);
    for (const QString& name : emulatorNames) {
        auto* row    = new QHBoxLayout;
        auto* lbl    = new QLabel(name + ":", emuWidget);
        lbl->setFixedWidth(80);
        lbl->setStyleSheet(QString("color:%1").arg(m_palette.mutedForeground.name()));
        auto* edit   = new QLineEdit(settings.emulatorPath(name), emuWidget);
        edit->setPlaceholderText("Auto-detect");
        auto* browse = new QPushButton("…", emuWidget);
        browse->setFixedWidth(30);
        connect(browse, &QPushButton::clicked, dlg, [edit, dlg, name]() {
            const QString p = QFileDialog::getOpenFileName(
                dlg, "Select " + name, {}, "Executables (*.exe)");
            if (!p.isEmpty()) edit->setText(p);
        });
        row->addWidget(lbl);
        row->addWidget(edit, 1);
        row->addWidget(browse);
        emuGrid->addLayout(row);
        emuEdits[name] = edit;
    }
    layout->addWidget(emuWidget);

    auto* bbx = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(bbx);

    connect(bbx, &QDialogButtonBox::accepted, dlg, [=, &settings]() {
        for (const QString& name : emulatorNames) {
            const QString p = emuEdits[name]->text().trimmed();
            if (p.isEmpty()) settings.clearEmulatorPath(name);
            else             settings.setEmulatorPath(name, p);
        }
        dlg->accept();
    });
    connect(bbx, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    dlg->exec();
}

void GameVaultWindow::setStatusText(const QString& text) {
    if (m_statusLabel) m_statusLabel->setText(text);
}

void GameVaultWindow::onThemeChanged(bool) {
    wintools::logger::Logger::log(
        kLog,
        wintools::logger::Severity::Pass,
        QStringLiteral("Theme change signal received."),
        QStringLiteral("dark=%1").arg(wintools::themes::ThemeHelper::isDarkTheme()));

    applyTheme(wintools::themes::ThemeHelper::currentPalette());

    if (m_stack && m_stack->currentIndex() == kDetailPage
        && !m_currentEntry.title.trimmed().isEmpty()) {
        goToDetail(m_currentEntry);
    }
}

void GameVaultWindow::applyTheme(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;
    wintools::logger::Logger::log(
        kLog,
        wintools::logger::Severity::Pass,
        QStringLiteral("Applying GameVault theme."),
        QStringLiteral("dark=%1").arg(wintools::themes::ThemeHelper::isDarkTheme()));

    wintools::themes::ThemeHelper::applyThemeTo(this, buildGameVaultQss(palette));
    updateSidebarItemStyles();
}

void GameVaultWindow::updateSidebarItemStyles() {
    if (!m_sidebar) return;

    const QColor sidebarSeparatorText = m_palette.mutedForeground;

    for (int i = 0; i < m_sidebar->count(); ++i) {
        QListWidgetItem* item = m_sidebar->item(i);
        if (!item) continue;

        if (item->data(kSidebarIsSeparator).toBool()) {
            item->setForeground(sidebarSeparatorText);
            item->setBackground(Qt::transparent);
            continue;
        }

        item->setForeground(QBrush());
        item->setBackground(QBrush());
    }

    m_sidebar->viewport()->update();
}

void GameVaultWindow::launchEntry(const GameEntry& e) {
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Launching game."),
        QStringLiteral("title=%1").arg(e.title));

    const QString platStr = platformName(e.platform);
    bool launched = false;
    QString emulatorPath = e.emulatorPath;

    if (emulatorPath.isEmpty() && e.platform == GamePlatform::DeSmuME) {
        const QString overridePath = GameVaultSettings::instance().emulatorPath("DeSmuME").trimmed();
        if (!overridePath.isEmpty() && QFileInfo::exists(overridePath)) {
            emulatorPath = overridePath;
        } else {
            const QStringList candidates = {
                qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
                qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"),
                qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"),
                qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Program Files/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Program Files (x86)/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/DeSmuME/DeSmuME.exe"),
                QStringLiteral("C:/Emulators/DeSmuME/DeSmuME.exe")
            };
            for (const QString& candidate : candidates) {
                if (!candidate.trimmed().isEmpty() && QFileInfo::exists(candidate)) {
                    emulatorPath = candidate;
                    break;
                }
            }
        }
    }

    if (emulatorPath.isEmpty() && e.platform == GamePlatform::Dolphin) {
        const QString overridePath = GameVaultSettings::instance().emulatorPath("Dolphin").trimmed();
        if (!overridePath.isEmpty() && QFileInfo::exists(overridePath)) {
            emulatorPath = overridePath;
        } else {
            const QStringList candidates = {
                QStringLiteral("C:/Dolphin/Dolphin.exe"),
                QStringLiteral("C:/Emulators/Dolphin/Dolphin.exe"),
                QStringLiteral("C:/Program Files/Dolphin/Dolphin.exe"),
                QStringLiteral("C:/Program Files (x86)/Dolphin/Dolphin.exe"),
                qEnvironmentVariable("LOCALAPPDATA") + "/Dolphin/Dolphin.exe",
                qEnvironmentVariable("USERPROFILE") + "/AppData/Local/Dolphin/Dolphin.exe"
            };
            for (const QString& candidate : candidates) {
                if (!candidate.trimmed().isEmpty() && QFileInfo::exists(candidate)) {
                    emulatorPath = candidate;
                    break;
                }
            }
        }

        if (e.emulatorArgs.isEmpty()) {
            const_cast<GameEntry&>(e).emulatorArgs = {"--exec"};
        }
    }

    if (!e.launchUri.isEmpty()) {
        launched = QDesktopServices::openUrl(QUrl(e.launchUri));
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to open launch URI for \"%1\".\n\nURI: %2")
                    .arg(e.title, e.launchUri));
        }
    } else if (!emulatorPath.isEmpty() && !e.executablePath.isEmpty()) {
        if (!QFileInfo::exists(emulatorPath)) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Emulator executable not found for \"%1\".\n\nEmulator: %2\nROM: %3")
                    .arg(e.title, emulatorPath, e.executablePath));
            return;
        }

        QStringList args = e.emulatorArgs;
        args << e.executablePath;
        const QString workingDir = QFileInfo(emulatorPath).absolutePath();
        launched = QProcess::startDetached(emulatorPath, args, workingDir);
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to start emulator for \"%1\".\n\nEmulator: %2\nROM: %3")
                    .arg(e.title, emulatorPath, e.executablePath));
        }
    } else if (!e.executablePath.isEmpty()) {
        launched = QProcess::startDetached(e.executablePath, {});
        if (!launched) {
            QMessageBox::warning(this, "Launch Failed",
                QString("Failed to start \"%1\".\n\nPath: %2")
                    .arg(e.title, e.executablePath));
        }
    } else {
        QMessageBox::warning(this, "Launch Failed",
            QString("No launch method available for \"%1\".").arg(e.title));
    }

    if (launched) {
        PlaytimeTracker::instance().startSession(platStr, trackingIdForEntry(e));
    }
}

void GameVaultWindow::addCustomGame() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add Custom Game / App");
    dlg->setMinimumWidth(440);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(styleSheet());

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto mkRow = [&](const QString& label) -> QLineEdit* {
        auto* lbl = new QLabel(label, dlg);
        lbl->setStyleSheet(QString("color: %1; font-size: 12px;").arg(m_palette.mutedForeground.name()));
        layout->addWidget(lbl);
        auto* le = new QLineEdit(dlg);
        layout->addWidget(le);
        return le;
    };

    auto* titleEdit = mkRow("Title *");
    auto* exeEdit   = mkRow("Executable path");
    auto* artEdit   = mkRow("Art URL (banner / capsule)");

    auto* browseBtn = new QPushButton("Browse…", dlg);
    browseBtn->setFixedWidth(90);
    connect(browseBtn, &QPushButton::clicked, dlg, [exeEdit, dlg]() {
        const QString path = QFileDialog::getOpenFileName(dlg,
            "Select executable", {}, "Executables (*.exe *.bat *.cmd *.lnk);;All files (*)");
        if (!path.isEmpty())
            exeEdit->setText(path);
    });

    auto* browseRow = new QHBoxLayout;
    browseRow->addStretch();
    browseRow->addWidget(browseBtn);
    layout->addLayout(browseRow);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, titleEdit, exeEdit, artEdit]() {
        const QString title = titleEdit->text().trimmed();
        if (title.isEmpty()) {
            QMessageBox::warning(dlg, "Missing title",
                "Please enter a game title.");
            return;
        }

        GameEntry entry;
        entry.title = title;
        entry.platform = GamePlatform::Unknown;
        entry.platformId = "custom_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        entry.executablePath = exeEdit->text().trimmed();
        entry.installed = !entry.executablePath.isEmpty();

        const QString artUrl = artEdit->text().trimmed();
        if (!artUrl.isEmpty())
            entry.artBannerUrl = artUrl;

        if (!entry.executablePath.isEmpty()) {
            QFileInfo fi(entry.executablePath);
            entry.installPath = fi.absolutePath();
        }

        m_model->addGame(entry);
        GameVaultSettings::instance().addOrUpdateManualGame(entry);
        updateSidebarCounts();
        updateFavouritesCount();
        dlg->accept();

        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("Custom game added."),
            QStringLiteral("title=%1").arg(title));
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
}

void GameVaultWindow::startSteamGridDbLookups() {
    if (!m_sgdbClient || !m_sgdbClient->hasApiKey()) return;

    m_sgdbQueue.clear();
    m_sgdbIndex = 0;
    m_sgdbGameIds.clear();

    for (int r = 0; r < m_model->rowCount(); ++r) {
        const GameEntry* e = m_model->entryAt(r);
        if (!e) continue;

        const bool hasBanner  = !e->artBannerUrl.isEmpty()
                              && !e->artBannerUrl.startsWith("local-art://");
        const bool hasCapsule = !e->artCapsuleUrl.isEmpty()
                              && !e->artCapsuleUrl.startsWith("local-art://");

        if (!hasBanner && !hasCapsule && !e->title.isEmpty())
            m_sgdbQueue.append({r, e->title});
    }

    if (m_sgdbQueue.isEmpty()) return;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("SteamGridDB lookup started."),
        QStringLiteral("games=%1").arg(m_sgdbQueue.size()));

    disconnect(m_sgdbClient, nullptr, this, nullptr);

    connect(m_sgdbClient, &SteamGridDBClient::searchFinished,
            this, [this](const QString& queryTitle, QVector<SteamGridDBResult> results) {
        if (!results.isEmpty()) {

            m_sgdbGameIds.insert(queryTitle, results.first().gameId);
            m_sgdbClient->fetchGrids(results.first().gameId);
        } else {

            ++m_sgdbIndex;
            QTimer::singleShot(300, this, [this]() {
                if (m_sgdbIndex < m_sgdbQueue.size()) {
                    const auto& [row, title] = m_sgdbQueue[m_sgdbIndex];
                    m_sgdbClient->searchGame(title);
                }
            });
        }
    });

    connect(m_sgdbClient, &SteamGridDBClient::gridsLoaded,
            this, [this](int gameId, QVector<SteamGridDBImage> images) {

        if (m_sgdbIndex >= m_sgdbQueue.size()) return;
        const auto& [row, title] = m_sgdbQueue[m_sgdbIndex];

        if (!images.isEmpty() && row < m_model->rowCount()) {
            GameEntry* e = const_cast<GameEntry*>(m_model->entryAt(row));
            if (e && e->title == title) {
                e->artCapsuleUrl = images.first().url;

                if (!m_artCache.contains(e->artCapsuleUrl))
                    m_artQueue.append(e->artCapsuleUrl);
            }
        }

        m_sgdbClient->fetchHeroes(gameId);
    });

    connect(m_sgdbClient, &SteamGridDBClient::heroesLoaded,
            this, [this](int , QVector<SteamGridDBImage> images) {
        if (m_sgdbIndex >= m_sgdbQueue.size()) return;
        const auto& [row, title] = m_sgdbQueue[m_sgdbIndex];

        if (!images.isEmpty() && row < m_model->rowCount()) {
            GameEntry* e = const_cast<GameEntry*>(m_model->entryAt(row));
            if (e && e->title == title) {
                e->artBannerUrl = images.first().url;
                if (!m_artCache.contains(e->artBannerUrl))
                    m_artQueue.append(e->artBannerUrl);
            }
        }

        ++m_sgdbIndex;

        if (!m_cardReply)
            fetchNextCardArt();

        QTimer::singleShot(350, this, [this]() {
            if (m_sgdbIndex < m_sgdbQueue.size()) {
                const auto& [row, title] = m_sgdbQueue[m_sgdbIndex];
                m_sgdbClient->searchGame(title);
            } else {
                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                    QStringLiteral("SteamGridDB lookup complete."),
                    QStringLiteral("resolved=%1").arg(m_sgdbGameIds.size()));
            }
        });
    });

    const auto& [row, title] = m_sgdbQueue[0];
    m_sgdbClient->searchGame(title);
}

}
