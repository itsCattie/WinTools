#include "game_card_delegate.hpp"
#include "modules/GameVault/src/model/game_model.hpp"
#include "modules/GameVault/src/core/game_entry.hpp"
#include "modules/GameVault/src/core/gamevault_settings.hpp"
#include "common/themes/theme_helper.hpp"

#include <QApplication>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOptionViewItem>

namespace wintools::gamevault {

static QStringList platformExecutableIconCandidates(GamePlatform p) {
    switch (p) {
        case GamePlatform::Steam:
#ifdef Q_OS_WIN
            return {
                QStringLiteral("C:/Program Files (x86)/Steam/steam.exe"),
                QStringLiteral("C:/Program Files/Steam/steam.exe")
            };
#elif defined(Q_OS_LINUX)
            return { QStringLiteral("/usr/bin/steam") };
#elif defined(Q_OS_MACOS)
            return { QStringLiteral("/Applications/Steam.app") };
#else
            return {};
#endif
        case GamePlatform::EpicGames:
#ifdef Q_OS_WIN
            return {
                QStringLiteral("C:/Program Files (x86)/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe"),
                QStringLiteral("C:/Program Files/Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe")
            };
#elif defined(Q_OS_LINUX)
            return { QStringLiteral("/usr/bin/heroic") };
#elif defined(Q_OS_MACOS)
            return { QStringLiteral("/Applications/Epic Games Launcher.app") };
#else
            return {};
#endif
        case GamePlatform::GOG:
#ifdef Q_OS_WIN
            return {
                QStringLiteral("C:/Program Files (x86)/GOG Galaxy/GalaxyClient.exe"),
                QStringLiteral("C:/Program Files/GOG Galaxy/GalaxyClient.exe")
            };
#elif defined(Q_OS_LINUX)
            return {
                QStringLiteral("/usr/bin/minigalaxy"),
                QStringLiteral("/usr/bin/lutris")
            };
#elif defined(Q_OS_MACOS)
            return { QStringLiteral("/Applications/GOG Galaxy.app") };
#else
            return {};
#endif
        case GamePlatform::Xbox:
#ifdef Q_OS_WIN
            return {
                QStringLiteral("C:/Program Files/Xbox/XboxApp.exe")
            };
#else
            return {};
#endif
        case GamePlatform::RetroArch: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("RetroArch")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/RetroArch/retroarch.exe")
                  << QStringLiteral("C:/Program Files/RetroArch/retroarch.exe");
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/retroarch");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/RetroArch.app");
#endif
            return paths;
        }
        case GamePlatform::RPCS3: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("RPCS3")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/RPCS3/rpcs3.exe")
                  << QStringLiteral("C:/Program Files/RPCS3/rpcs3.exe");
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/rpcs3");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/rpcs3.app");
#endif
            return paths;
        }
        case GamePlatform::Yuzu: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Yuzu")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/Yuzu/yuzu.exe")
                  << QStringLiteral("C:/Program Files/yuzu/yuzu.exe");
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/yuzu");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/yuzu.app");
#endif
            return paths;
        }
        case GamePlatform::Ryujinx: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Ryujinx")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/Ryujinx/Ryujinx.exe")
                  << QStringLiteral("C:/Program Files/Ryujinx/Ryujinx.exe");
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/Ryujinx");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/Ryujinx.app");
#endif
            return paths;
        }
        case GamePlatform::Dolphin: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("Dolphin")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/Dolphin/Dolphin.exe")
                  << QStringLiteral("C:/Emulators/Dolphin/Dolphin.exe")
                  << QStringLiteral("C:/Program Files/Dolphin/Dolphin.exe")
                  << QStringLiteral("C:/Program Files (x86)/Dolphin/Dolphin.exe")
                  << (qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Dolphin/Dolphin.exe"))
                  << (qEnvironmentVariable("USERPROFILE") + QStringLiteral("/AppData/Local/Dolphin/Dolphin.exe"));
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/dolphin-emu");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/Dolphin.app");
#endif
            return paths;
        }
        case GamePlatform::PCSX2: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("PCSX2")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/PCSX2/pcsx2-qt.exe")
                  << QStringLiteral("C:/Program Files/PCSX2/pcsx2-qt.exe");
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/pcsx2");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/PCSX2.app");
#endif
            return paths;
        }
        case GamePlatform::DeSmuME: {
            QStringList paths = {
                GameVaultSettings::instance().emulatorPath(QStringLiteral("DeSmuME")).trimmed(),
            };
#ifdef Q_OS_WIN
            paths << QStringLiteral("C:/DeSmuME/DeSmuME.exe")
                  << QStringLiteral("C:/Emulators/DeSmuME/DeSmuME.exe")
                  << QStringLiteral("C:/Program Files/DeSmuME/DeSmuME.exe")
                  << QStringLiteral("C:/Program Files (x86)/DeSmuME/DeSmuME.exe")
                  << (qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"))
                  << (qEnvironmentVariable("USERPROFILE") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"))
                  << (qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/Desmume/DeSmuME.exe"))
                  << (qEnvironmentVariable("OneDrive") + QStringLiteral("/Documents/My Games/DeSmuME/DeSmuME.exe"));
#elif defined(Q_OS_LINUX)
            paths << QStringLiteral("/usr/bin/desmume");
#elif defined(Q_OS_MACOS)
            paths << QStringLiteral("/Applications/DeSmuME.app");
#endif
            return paths;
        }
        case GamePlatform::Unknown:
            return {};
    }
    return {};
}

static QIcon platformBadgeIcon(GamePlatform p) {
    static QHash<int, QIcon> iconCache;
    const int key = static_cast<int>(p);
    if (iconCache.contains(key)) {
        return iconCache.value(key);
    }

    static QFileIconProvider iconProvider;
    for (const QString& candidate : platformExecutableIconCandidates(p)) {
        if (candidate.trimmed().isEmpty()) continue;
        if (!QFileInfo::exists(candidate)) continue;
        const QIcon icon = iconProvider.icon(QFileInfo(candidate));
        if (!icon.isNull()) {
            iconCache.insert(key, icon);
            return icon;
        }
    }

    const QIcon none;
    iconCache.insert(key, none);
    return none;
}

static QPixmap platformBadgePixmap(GamePlatform p, const QSize& size) {
    const QString cacheKey = QStringLiteral("%1|%2x%3")
        .arg(static_cast<int>(p))
        .arg(size.width())
        .arg(size.height());

    static QHash<QString, QPixmap> cache;
    const auto it = cache.constFind(cacheKey);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QPixmap px = platformBadgeIcon(p).pixmap(size);
    cache.insert(cacheKey, px);
    return px;
}

static QColor blendColor(const QColor& a, const QColor& b, float alpha) {
    return QColor(
        static_cast<int>(a.red() * (1.0f - alpha) + b.red() * alpha),
        static_cast<int>(a.green() * (1.0f - alpha) + b.green() * alpha),
        static_cast<int>(a.blue() * (1.0f - alpha) + b.blue() * alpha));
}

static QColor compositeOver(const QColor& base, const QColor& overlay) {
    if (overlay.alpha() >= 255) return overlay;
    const float alpha = static_cast<float>(overlay.alpha()) / 255.0f;
    return blendColor(base, overlay, alpha);
}

GameCardDelegate::GameCardDelegate(QObject* parent)
    : QAbstractItemDelegate(parent) {}

QSize GameCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return {kCardW + 8, kCardH + 8};
}

void GameCardDelegate::paint(QPainter*                   painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex&          index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing,         true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect outer = option.rect;
    const QRect card  = outer.adjusted(4, 4, -4, -4);

    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered  = (option.state & QStyle::State_MouseOver) != 0;

    const auto palette = wintools::themes::ThemeHelper::currentPalette();
    const QColor cardBase = palette.cardBackground;
    const QColor cardHover = compositeOver(palette.cardBackground, palette.hoverBackground);
    const QColor cardBorder = palette.cardBorder;
    const QColor cardSelected = compositeOver(palette.cardBackground, palette.accent);
    const QColor cardSelectedBorder = palette.accent;
    const QColor textPrimary = palette.foreground;
    const QColor textMuted = palette.mutedForeground;
    const QColor achFill = palette.accent;
    const QColor achEmpty = palette.cardBorder;

    QPainterPath cardPath;
    cardPath.addRoundedRect(card, kRadius, kRadius);

    painter->fillPath(cardPath, selected ? cardSelected : hovered ? cardHover : cardBase);
    painter->setPen(QPen(selected ? cardSelectedBorder : cardBorder,
                         selected ? 2 : 1));
    painter->drawPath(cardPath);

    const QRect artRect(card.left(), card.top(), card.width(), kArtH);
    if (m_scaledArtSize != artRect.size()) {
        m_scaledArtSize = artRect.size();
        m_scaledArtCache.clear();
        m_scaledArtSourceKeys.clear();
    }

    QPainterPath artClip;
    artClip.setFillRule(Qt::WindingFill);
    const qreal r = kRadius;
    artClip.moveTo(artRect.left(),          artRect.bottom());
    artClip.lineTo(artRect.left(),          artRect.top() + r);
    artClip.arcTo (artRect.left(),          artRect.top(), r * 2, r * 2, 180, -90);
    artClip.lineTo(artRect.right() - r,     artRect.top());
    artClip.arcTo (artRect.right() - r * 2, artRect.top(), r * 2, r * 2,  90, -90);
    artClip.lineTo(artRect.right(),         artRect.bottom());
    artClip.closeSubpath();

    painter->save();
    painter->setClipPath(artClip);

    bool artDrawn = false;
    const QVariant entryVar  = index.data(GameEntryRole);
    QString        platLabel;
    GamePlatform   platform = GamePlatform::Unknown;

    if (entryVar.isValid()) {
        const GameEntry e = entryVar.value<GameEntry>();
        platLabel = platformName(e.platform);
        platform = e.platform;

        const QString artKey = e.artBannerUrl.isEmpty() ? e.artCapsuleUrl
                                                        : e.artBannerUrl;
        if (!artKey.isEmpty() && m_artCache && m_artCache->contains(artKey)) {
            const QPixmap& px = (*m_artCache)[artKey];
            if (!px.isNull()) {
                const qint64 srcKey = px.cacheKey();
                bool needsRescale = !m_scaledArtCache.contains(artKey)
                    || !m_scaledArtSourceKeys.contains(artKey)
                    || m_scaledArtSourceKeys.value(artKey) != srcKey;

                if (needsRescale) {
                    m_scaledArtCache.insert(
                        artKey,
                        px.scaled(artRect.size(),
                                  Qt::KeepAspectRatioByExpanding,
                                  Qt::SmoothTransformation));
                    m_scaledArtSourceKeys.insert(artKey, srcKey);
                }

                const QPixmap& scaled = m_scaledArtCache[artKey];
                const QPoint offset(
                    artRect.left() + (artRect.width()  - scaled.width())  / 2,
                    artRect.top()  + (artRect.height() - scaled.height()) / 2);
                painter->drawPixmap(offset, scaled);
                artDrawn = true;
            }
        }
    }

    if (!artDrawn) {

        QLinearGradient grad(artRect.topLeft(), artRect.bottomLeft());
        QColor gradTop = palette.windowBackground;
        QColor gradBottom = compositeOver(palette.cardBackground, palette.surfaceOverlay);
        grad.setColorAt(0.0, gradTop);
        grad.setColorAt(1.0, gradBottom);
        painter->fillRect(artRect, grad);

        painter->setPen(QPen(QColor(textMuted.red(), textMuted.green(), textMuted.blue(), 20), 1));
        for (int x = artRect.left() - artRect.height(); x < artRect.right(); x += 12)
            painter->drawLine(x, artRect.bottom(), x + artRect.height(), artRect.top());

        bool iconDrawn = false;
        if (entryVar.isValid()) {
            const GameEntry e = entryVar.value<GameEntry>();
            QString iconSource = e.iconPath;
            if (iconSource.isEmpty() && !e.executablePath.isEmpty()) {
                iconSource = e.executablePath;
            }

            if (!iconSource.isEmpty()) {
                static QFileIconProvider iconProvider;
                const QIcon icon = iconProvider.icon(QFileInfo(iconSource));
                const QPixmap iconPx = icon.pixmap(72, 72);
                if (!iconPx.isNull()) {
                    const QPoint p(artRect.center().x() - iconPx.width() / 2,
                                   artRect.center().y() - iconPx.height() / 2);
                    painter->drawPixmap(p, iconPx);
                    iconDrawn = true;
                }
            }
        }

        if (!iconDrawn && !platLabel.isEmpty()) {
            QFont pf = option.font;
            pf.setPixelSize(11);
            pf.setBold(true);
            painter->setFont(pf);
            painter->setPen(textMuted);
            painter->drawText(artRect, Qt::AlignCenter, platLabel.toUpper());
        }
    }

    {
        const QPixmap badgePx = platformBadgePixmap(platform, QSize(16, 16));
        if (!badgePx.isNull()) {
            const QPoint iconPos(artRect.left() + 8, artRect.top() + 8);
            painter->drawPixmap(iconPos, badgePx);
        }
    }

    painter->restore();

    {
        const QRect ov(artRect.left(), artRect.bottom() - 32, artRect.width(), 32);
        QLinearGradient fade(ov.topLeft(), ov.bottomLeft());
        fade.setColorAt(0.0, Qt::transparent);
        QColor overlay = compositeOver(
            palette.windowBackground,
            QColor(palette.surfaceOverlay.red(),
                   palette.surfaceOverlay.green(),
                   palette.surfaceOverlay.blue(),
                   204));
        fade.setColorAt(1.0, overlay);
        painter->fillRect(ov, fade);
    }

    const QRect info(card.left(), card.top() + kArtH, card.width(), card.height() - kArtH);

    QFont titleFont = option.font;
    titleFont.setPixelSize(12);
    titleFont.setBold(true);

    QFont metaFont = option.font;
    metaFont.setPixelSize(10);

    const QFontMetrics titleFm(titleFont);
    const QFontMetrics metaFm(metaFont);

    const int textX  = info.left() + kPadding;
    const int textW  = info.width() - kPadding * 2;
    const int line1Y = info.top() + 8;
    const int line2Y = line1Y + titleFm.height() + 2;

    painter->setPen(textPrimary);
    painter->setFont(titleFont);

    const QString titleText = index.data(Qt::DisplayRole).toString();
    painter->drawText(QRect(textX, line1Y, textW, titleFm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      titleFm.elidedText(titleText, Qt::ElideRight, textW));

    if (entryVar.isValid()) {
        const GameEntry e = entryVar.value<GameEntry>();

        painter->setFont(metaFont);

        const QString meta = (e.playtimeSeconds > 0)
                             ? e.playtimeDisplay()
                     : (e.platform == GamePlatform::EpicGames
                         ? QStringLiteral("Epic playtime unavailable")
                         : QStringLiteral("Never played"));

        if (e.achievementsTotal > 0) {
            const int barW = 36;
            const int barH = 4;
            const int barX = textX;
            const int barY = line2Y + metaFm.ascent() / 2 - barH / 2 + 2;
            const int fill = static_cast<int>(barW * e.achievementsUnlocked
                                              / e.achievementsTotal);

            painter->fillRect(barX, barY, barW,  barH, achEmpty);
            if (fill > 0)
                painter->fillRect(barX, barY, fill, barH, achFill);

            painter->setPen(textMuted);
            painter->drawText(
                QRect(barX + barW + 2, line2Y, textW - barW - 2, metaFm.height()),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                QString(" %1/%2").arg(e.achievementsUnlocked).arg(e.achievementsTotal));

            painter->drawText(
                QRect(textX, line2Y, textW, metaFm.height()),
                Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
                meta);
        } else {
            painter->setPen(textMuted);
            painter->drawText(QRect(textX, line2Y, textW, metaFm.height()),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              meta);
        }
    }

    painter->restore();
}

}
