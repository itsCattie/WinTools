#include "pie_chart_widget.hpp"

#include "common/themes/theme_helper.hpp"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace wintools::disksentinel {

PieChartWidget::PieChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    setMinimumSize(200, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PieChartWidget::setRoot(DiskNode* root) {
    m_root = root;
    rebuildSlices();
    update();
}

void PieChartWidget::setThemePalette(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;
    rebuildSlices();
    update();
}

void PieChartWidget::rebuildSlices() {
    m_slices.clear();
    if (!m_root) return;

    QHash<QString, qint64> buckets;
    std::function<void(DiskNode*)> walk = [&](DiskNode* node) {
        if (!node) return;
        if (!node->isDir) {
            const QString cat = DiskNode::category(node->name);
            buckets[cat] += node->size;
            return;
        }
        for (auto& child : node->children)
            walk(child.get());
    };
    walk(m_root);

    if (buckets.isEmpty()) return;

    qint64 total = 0;
    for (auto it = buckets.cbegin(); it != buckets.cend(); ++it)
        total += it.value();
    if (total <= 0) return;

    const bool isDark = wintools::themes::ThemeHelper::isDarkTheme();

    static const QStringList order = {
        "image", "video", "audio", "document", "archive", "code", "executable", "other"
    };

    for (const QString& cat : order) {
        const qint64 bytes = buckets.value(cat, 0);
        if (bytes <= 0) continue;
        Slice s;
        s.category = cat;
        s.bytes    = bytes;
        s.fraction = static_cast<double>(bytes) / static_cast<double>(total);
        s.color    = colorForCategory(cat, isDark);
        m_slices.append(s);
    }
}

QColor PieChartWidget::colorForCategory(const QString& cat, bool isDark) {

    if (cat == "image")      return isDark ? QColor(0x5B, 0x8D, 0xEF) : QColor(0x33, 0x66, 0xCC);
    if (cat == "video")      return isDark ? QColor(0xE0, 0x6C, 0x75) : QColor(0xCC, 0x33, 0x33);
    if (cat == "audio")      return isDark ? QColor(0x56, 0xB6, 0xC2) : QColor(0x00, 0x99, 0xCC);
    if (cat == "document")   return isDark ? QColor(0xE5, 0xC0, 0x7B) : QColor(0xCC, 0x99, 0x00);
    if (cat == "archive")    return isDark ? QColor(0xC6, 0x78, 0xDD) : QColor(0x99, 0x33, 0xCC);
    if (cat == "code")       return isDark ? QColor(0x98, 0xC3, 0x79) : QColor(0x33, 0x99, 0x33);
    if (cat == "executable") return isDark ? QColor(0xD1, 0x9A, 0x66) : QColor(0xCC, 0x66, 0x00);
    return isDark ? QColor(0x7A, 0x7E, 0x85) : QColor(0x88, 0x88, 0x88);
}

void PieChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), m_palette.windowBackground);

    if (m_slices.isEmpty()) {
        p.setPen(m_palette.mutedForeground);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No data"));
        return;
    }

    const int margin  = 12;
    const int spacing = 16;

    const int available = qMin(height() - 2 * margin,
                               (width() - 2 * margin - spacing) / 2);
    const int diameter = qMax(80, available);
    const int radius   = diameter / 2;
    const QPointF center(margin + radius, height() / 2.0);

    const QRectF pieRect(center.x() - radius, center.y() - radius,
                         diameter, diameter);
    double startAngle = 90.0;

    for (const Slice& s : m_slices) {
        const double spanAngle = s.fraction * 360.0;
        QPainterPath path;
        path.moveTo(center);
        path.arcTo(pieRect, startAngle, -spanAngle);
        path.closeSubpath();
        p.fillPath(path, s.color);

        p.setPen(QPen(m_palette.windowBackground, 1));
        p.drawPath(path);

        startAngle -= spanAngle;
    }

    const int legendX = static_cast<int>(center.x()) + radius + spacing;
    const int swatchSize = 10;
    const int lineHeight = 18;
    int y = qMax(margin, static_cast<int>(center.y()) - static_cast<int>(m_slices.size()) * lineHeight / 2);

    QFont labelFont = font();
    labelFont.setPixelSize(11);
    p.setFont(labelFont);

    for (const Slice& s : m_slices) {

        p.fillRect(legendX, y + 3, swatchSize, swatchSize, s.color);

        QString label = s.category;
        if (!label.isEmpty())
            label[0] = label[0].toUpper();

        const double pct = s.fraction * 100.0;
        const QString text = QStringLiteral("%1  %2%  (%3)")
            .arg(label)
            .arg(pct, 0, 'f', 1)
            .arg(DiskNode::prettySize(s.bytes));

        p.setPen(m_palette.foreground);
        p.drawText(legendX + swatchSize + 6, y, width() - legendX - swatchSize - 6 - margin,
                   lineHeight, Qt::AlignVCenter | Qt::TextSingleLine, text);

        y += lineHeight;
    }
}

}
