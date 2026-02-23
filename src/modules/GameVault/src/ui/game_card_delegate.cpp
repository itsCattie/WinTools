// GameVault: game card delegate manages UI behavior and presentation.

#include "game_card_delegate.hpp"
#include "modules/GameVault/src/model/game_model.hpp"
#include "modules/GameVault/src/core/game_entry.hpp"

#include <QApplication>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOptionViewItem>

namespace wintools::gamevault {

namespace clr {
    constexpr QRgb card        = 0xFF141C26;
    constexpr QRgb cardHover   = 0xFF1B2634;
    constexpr QRgb cardBorder  = 0xFF243447;
    constexpr QRgb cardSel     = 0xFF4C90C0;
    constexpr QRgb textPrimary = 0xFFC6D4DF;
    constexpr QRgb textMuted   = 0xFF8F98A0;
    constexpr QRgb achFill     = 0xFF4D8BBD;
    constexpr QRgb achEmpty    = 0xFF2A3F52;
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

    QPainterPath cardPath;
    cardPath.addRoundedRect(card, kRadius, kRadius);

    painter->fillPath(cardPath, QColor::fromRgba(
        selected ? clr::cardSel : hovered ? clr::cardHover : clr::card));
    painter->setPen(QPen(QColor::fromRgba(selected ? clr::cardSel : clr::cardBorder),
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

    if (entryVar.isValid()) {
        const GameEntry e = entryVar.value<GameEntry>();
        platLabel = platformName(e.platform);

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
        grad.setColorAt(0.0, QColor(0x16, 0x20, 0x2D));
        grad.setColorAt(1.0, QColor(0x1E, 0x2D, 0x3D));
        painter->fillRect(artRect, grad);

        painter->setPen(QPen(QColor(255, 255, 255, 10), 1));
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
            painter->setPen(QColor(0x4A, 0x61, 0x74));
            painter->drawText(artRect, Qt::AlignCenter, platLabel.toUpper());
        }
    }

    painter->restore();

    {
        const QRect ov(artRect.left(), artRect.bottom() - 32, artRect.width(), 32);
        QLinearGradient fade(ov.topLeft(), ov.bottomLeft());
        fade.setColorAt(0.0, Qt::transparent);
        fade.setColorAt(1.0, QColor::fromRgba(0xCC1E2D3D));
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

    painter->setPen(QColor::fromRgba(clr::textPrimary));
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

            painter->fillRect(barX, barY, barW,  barH, QColor::fromRgba(clr::achEmpty));
            if (fill > 0)
                painter->fillRect(barX, barY, fill, barH, QColor::fromRgba(clr::achFill));

            painter->setPen(QColor::fromRgba(clr::textMuted));
            painter->drawText(
                QRect(barX + barW + 2, line2Y, textW - barW - 2, metaFm.height()),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                QString(" %1/%2").arg(e.achievementsUnlocked).arg(e.achievementsTotal));

            painter->drawText(
                QRect(textX, line2Y, textW, metaFm.height()),
                Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
                meta);
        } else {
            painter->setPen(QColor::fromRgba(clr::textMuted));
            painter->drawText(QRect(textX, line2Y, textW, metaFm.height()),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              meta);
        }
    }

    painter->restore();
}

}
