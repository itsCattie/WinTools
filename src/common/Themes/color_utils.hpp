#pragma once

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QString>

#include <algorithm>
#include <cmath>

#include "common/themes/window_colour.hpp"

namespace wintools::themes {

inline QColor blendColor(const QColor& a, const QColor& b, float alpha) {
    const float t = std::clamp(alpha, 0.0f, 1.0f);
    return QColor(
        static_cast<int>(a.red()   * (1.0f - t) + b.red()   * t),
        static_cast<int>(a.green() * (1.0f - t) + b.green() * t),
        static_cast<int>(a.blue()  * (1.0f - t) + b.blue()  * t));
}

inline QColor compositeOver(const QColor& base, const QColor& overlay) {
    if (overlay.alpha() >= 255) return overlay;
    const float alpha = static_cast<float>(overlay.alpha()) / 255.0f;
    return blendColor(base, overlay, alpha);
}

inline double linearizeChannel(double c) {
    c /= 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

inline double relativeLuminance(const QColor& c) {
    return 0.2126 * linearizeChannel(c.red())
         + 0.7152 * linearizeChannel(c.green())
         + 0.0722 * linearizeChannel(c.blue());
}

inline double contrastRatio(const QColor& a, const QColor& b) {
    const double l1 = relativeLuminance(a);
    const double l2 = relativeLuminance(b);
    const double lighter = std::max(l1, l2);
    const double darker  = std::min(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

inline QColor bestTextColorFor(const QColor& background,
                               const QColor& preferred,
                               const ThemePalette& p) {
    const QColor candidates[] = {
        preferred,
        p.foreground,
        p.mutedForeground,
        QColor(Qt::white),
        QColor(Qt::black)
    };
    QColor best = preferred;
    double bestScore = 0.0;
    for (const auto& c : candidates) {
        const double score = contrastRatio(background, c);
        if (score > bestScore) {
            bestScore = score;
            best = c;
        }
    }
    return best;
}

inline QColor readableTextOn(const QColor& bg) {
    return bg.lightness() > 150 ? QColor(Qt::black) : QColor(Qt::white);
}

inline QString cssRgba(const QColor& c, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

inline QString cssColor(const QColor& c) {
    if (c.alpha() < 255) {
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    }
    return c.name();
}

inline QIcon tintedIcon(const QIcon& base, const QSize& size, const QColor& tint) {
    if (base.isNull()) return {};

    QPixmap pix = base.pixmap(size, QIcon::Normal, QIcon::Off);
    if (pix.isNull()) {
        pix = QPixmap(size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        base.paint(&painter, QRect(QPoint(0, 0), size));
        painter.end();
        if (pix.isNull() || pix.size().isEmpty()) return {};
    }

    QImage image = pix.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), tint);
    painter.end();
    return QIcon(QPixmap::fromImage(image));
}

}
