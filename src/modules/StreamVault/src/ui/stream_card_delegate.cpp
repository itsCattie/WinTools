#include "stream_card_delegate.hpp"
#include "modules/StreamVault/src/core/stream_entry.hpp"
#include "modules/StreamVault/src/model/stream_model.hpp"

#include <QLinearGradient>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionViewItem>

// StreamVault: stream card delegate manages UI behavior and presentation.

namespace wintools::streamvault {

StreamCardDelegate::StreamCardDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
{}

QSize StreamCardDelegate::sizeHint(const QStyleOptionViewItem& ,
                                    const QModelIndex& ) const {
    return QSize(kCardW, kCardH);
}

void StreamCardDelegate::paint(QPainter*                   painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex&          index) const {
    painter->save();

    const QRect card   = option.rect;
    const bool  sel    = option.state & QStyle::State_Selected;
    const bool  hover  = option.state & QStyle::State_MouseOver;

    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    painter->setClipPath(path);

    QColor bg = sel   ? QColor("#2a475e")
              : hover ? QColor("#2c3e50")
                      : QColor("#1f2c38");
    painter->fillPath(path, bg);

    const QRect artRect(card.left(), card.top(), kCardW, kArtH);
    int tmdbId = index.data(TmdbIdRole).toInt();

    if (m_posterCache && m_posterCache->contains(tmdbId)) {
        const QPixmap& pix = (*m_posterCache)[tmdbId];

        QPixmap scaled = pix.scaled(artRect.size(),
                                    Qt::KeepAspectRatioByExpanding,
                                    Qt::SmoothTransformation);

        int sx = (scaled.width()  - artRect.width())  / 2;
        int sy = (scaled.height() - artRect.height()) / 2;
        painter->drawPixmap(artRect, scaled,
                            QRect(sx, sy, artRect.width(), artRect.height()));
    } else {

        QString title = index.data(Qt::DisplayRole).toString();
        drawPlaceholder(painter, artRect, title);
    }

    const QRect infoRect(card.left(),
                         card.top() + kArtH,
                         kCardW,
                         kCardH - kArtH);

    painter->fillRect(infoRect, sel ? QColor("#2a475e") : QColor("#1b2838"));

    const auto entry = qvariant_cast<StreamEntry>(index.data(StreamEntryRole));
    QFont titleFont = option.font;
    titleFont.setPixelSize(12);
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(QColor("#c6d4df"));

    QRect titleRect(card.left() + kPadding,
                    card.top() + kArtH + 5,
                    kCardW - 2 * kPadding,
                    36);
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      entry.title);

    QFont metaFont = option.font;
    metaFont.setPixelSize(11);
    painter->setFont(metaFont);
    painter->setPen(QColor("#6a8499"));

    QString metaTypeStr = entry.mediaType == MediaType::Movie ? "Movie"
                        : entry.mediaType == MediaType::TvShow ? "TV Series"
                        : "";
    QString meta;
    if (!entry.releaseYear.isEmpty())
        meta += entry.releaseYear;
    if (entry.voteAverage > 0.f) {
        if (!meta.isEmpty()) meta += "  ";
        meta += QString("★ %1").arg(entry.voteAverage, 0, 'f', 1);
    }
    if (!metaTypeStr.isEmpty()) {
        if (!meta.isEmpty()) meta += "  ";
        meta += metaTypeStr;
    }

    QRect metaRect(card.left() + kPadding,
                   card.top() + kArtH + 44,
                   kCardW - 2 * kPadding,
                   18);
    painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter, meta);

    if (sel || hover) {
        painter->setClipping(false);
        QPen borderPen(sel ? QColor("#66c0f4") : QColor("#4a6174"), 1.5);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(card.adjusted(1, 1, -1, -1), kRadius, kRadius);
    }

    painter->restore();
}

void StreamCardDelegate::drawPlaceholder(QPainter*      p,
                                          const QRect&   artRect,
                                          const QString& title) {

    QLinearGradient grad(artRect.topLeft(), artRect.bottomLeft());
    grad.setColorAt(0.0, QColor("#2c3e50"));
    grad.setColorAt(1.0, QColor("#1a252f"));
    p->fillRect(artRect, grad);

    QFont f;
    f.setPixelSize(28);
    f.setWeight(QFont::Bold);
    p->setFont(f);
    p->setPen(QColor("#3d5166"));

    QString initials;
    for (const QString& word : title.split(' ', Qt::SkipEmptyParts)) {
        initials += word[0].toUpper();
        if (initials.length() >= 2) break;
    }
    p->drawText(artRect, Qt::AlignCenter, initials);
}

}
