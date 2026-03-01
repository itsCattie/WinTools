#include "modules/disksentinel/src/ui/treemap_widget.hpp"
#include "common/themes/theme_helper.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <algorithm>

namespace wintools::disksentinel {

static const QHash<QString, QColor>& categoryColours() {
    static const QHash<QString, QColor> map{
        { QStringLiteral("image"),      QColor(0x5b, 0x9b, 0xd5) },
        { QStringLiteral("video"),      QColor(0xc0, 0x39, 0x2b) },
        { QStringLiteral("audio"),      QColor(0x27, 0xae, 0x60) },
        { QStringLiteral("document"),   QColor(0xe6, 0xac, 0x00) },
        { QStringLiteral("archive"),    QColor(0x8e, 0x44, 0xad) },
        { QStringLiteral("code"),       QColor(0x16, 0xa0, 0x85) },
        { QStringLiteral("executable"), QColor(0xe6, 0x7e, 0x22) },
        { QStringLiteral("other"),      QColor(0x7f, 0x8c, 0x8d) },
    };
    return map;
}

QColor categoryColor(const QString& category) {
    return categoryColours().value(category, QColor(0x7f, 0x8c, 0x8d));
}

QColor blend(const QColor& a, const QColor& b, float alpha) {
    return QColor(
        static_cast<int>(a.red() * (1.0f - alpha) + b.red() * alpha),
        static_cast<int>(a.green() * (1.0f - alpha) + b.green() * alpha),
        static_cast<int>(a.blue() * (1.0f - alpha) + b.blue() * alpha));
}

TreemapWidget::TreemapWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(200, 150);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setToolTip(QString());
}

void TreemapWidget::setRoot(DiskNode* root) {
    m_root    = root;
    m_hovered = nullptr;
    m_cells.clear();
    update();
}

void TreemapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_cells.clear();
    update();
}

void TreemapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), palette().window());

    m_cells.clear();
    m_cells.reserve(4096);

    if (!m_root || m_root->size == 0) {
        p.setPen(palette().placeholderText().color());
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No data — select a folder to scan"));
        return;
    }

    const auto theme = wintools::themes::ThemeHelper::currentPalette();
    const bool darkTheme = wintools::themes::ThemeHelper::isDarkTheme();
    renderRoot(p, theme, darkTheme);
}

void TreemapWidget::mouseMoveEvent(QMouseEvent* event) {
    DiskNode* hit = hitTest(event->position());
    if (hit != m_hovered) {
        const QRectF oldRect = nodeRect(m_hovered);
        const QRectF newRect = nodeRect(hit);
        m_hovered = hit;
        if (oldRect.isValid()) {
            update(oldRect.toAlignedRect().adjusted(-2, -2, 2, 2));
        }
        if (newRect.isValid()) {
            update(newRect.toAlignedRect().adjusted(-2, -2, 2, 2));
        }
        emit nodeHovered(hit);
    }
    if (hit) {
        QToolTip::showText(event->globalPosition().toPoint(),
            QStringLiteral("%1\n%2")
                .arg(hit->path, DiskNode::prettySize(hit->size)),
            this);
    } else {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void TreemapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        DiskNode* hit = hitTest(event->position());
        if (hit) emit nodeClicked(hit);
    }
    QWidget::mousePressEvent(event);
}

void TreemapWidget::leaveEvent(QEvent* event) {
    if (m_hovered) {
        m_hovered = nullptr;
        update();
        emit nodeHovered(nullptr);
    }
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

void TreemapWidget::renderRoot(QPainter& p,
                               const wintools::themes::ThemePalette& theme,
                               bool darkTheme) {

    QVector<DiskNode*> children;
    children.reserve(static_cast<int>(m_root->children.size()));
    for (const auto& c : m_root->children)
        children.push_back(c.get());

    qint64 total = 0;
    for (auto* c : children) total += c->size;
    if (total == 0) return;

    layoutItems(p, QRectF(rect()), children, total, 0, theme, darkTheme);
}

void TreemapWidget::layoutItems(QPainter& p, QRectF rect,
                                 const QVector<DiskNode*>& items,
                                 qint64 total, int depth,
                                 const wintools::themes::ThemePalette& theme,
                                 bool darkTheme) {
    if (items.isEmpty() || total == 0 || rect.width() < 2 || rect.height() < 2)
        return;

    if (items.size() == 1) {
        drawNode(p, rect, items[0], depth, theme, darkTheme);
        return;
    }

    qint64 half  = total / 2;
    qint64 accum = 0;
    int    split = 0;
    for (int i = 0; i < items.size() - 1; ++i) {
        accum += items[i]->size;
        split  = i + 1;
        if (accum >= half) break;
    }

    const double frac = (total > 0) ? (static_cast<double>(accum) / total) : 0.5;
    const qint64 leftTotal  = accum;
    const qint64 rightTotal = total - accum;

    const QVector<DiskNode*> left (items.begin(), items.begin() + split);
    const QVector<DiskNode*> right(items.begin() + split, items.end());

    QRectF leftRect, rightRect;
    if (rect.width() >= rect.height()) {
        const double w = rect.width() * frac;
        leftRect  = {rect.x(),         rect.y(), w,              rect.height()};
        rightRect = {rect.x() + w,     rect.y(), rect.width()-w, rect.height()};
    } else {
        const double h = rect.height() * frac;
        leftRect  = {rect.x(), rect.y(),         rect.width(), h             };
        rightRect = {rect.x(), rect.y() + h,     rect.width(), rect.height()-h};
    }

    layoutItems(p, leftRect,  left,  leftTotal,  depth, theme, darkTheme);
    layoutItems(p, rightRect, right, rightTotal, depth, theme, darkTheme);
}

void TreemapWidget::drawNode(QPainter& p, QRectF rect,
                              DiskNode* node, int depth,
                              const wintools::themes::ThemePalette& theme,
                              bool darkTheme) {
    const QColor bg = colorForNode(node, depth, theme, darkTheme);

    const bool canRecurse = node->isDir
                         && !node->children.empty()
                         && rect.width()  > 40
                         && rect.height() > 40;

    const bool hovered = (node == m_hovered);
    const QColor fill = hovered
        ? (darkTheme ? bg.lighter(130) : bg.darker(108))
        : bg;

    if (canRecurse) {

        constexpr int headerH = 16;
        constexpr int pad     = 2;

        p.fillRect(rect.toRect(), fill);

        QColor borderColor = darkTheme ? fill.darker(152) : fill.darker(125);
        if (hovered) {
            p.setPen(QPen(theme.accent, 2));
        } else {
            p.setPen(QPen(borderColor, 1.5));
        }
        p.drawRect(rect.adjusted(0, 0, -1, -1));

        if (rect.width() > 30) {
            QFont f = p.font();
            f.setPixelSize(10);
            f.setBold(true);
            p.setFont(f);
            QColor titleColor = fill.lightness() > 150 ? QColor(0x1f, 0x1f, 0x1f) : QColor(Qt::white);
            p.setPen(titleColor);
            p.drawText(QRectF(rect.x() + pad + 2, rect.y() + 1,
                              rect.width() - pad * 2, headerH - 1),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        node->name);
        }

        const QRectF inner = rect.adjusted(pad, headerH, -pad, -pad);

        QVector<DiskNode*> kids;
        kids.reserve(static_cast<int>(node->children.size()));
        for (const auto& c : node->children)
            kids.push_back(c.get());

        qint64 kidsTotal = 0;
        for (auto* k : kids) kidsTotal += k->size;

        if (kidsTotal > 0)
            layoutItems(p, inner, kids, kidsTotal, depth + 1, theme, darkTheme);

    } else {

        p.fillRect(rect.toRect(), fill);

        QColor borderColor = darkTheme ? fill.darker(145) : fill.darker(120);
        if (hovered) {
            p.setPen(QPen(theme.accent, 2));
        } else {
            p.setPen(QPen(borderColor, 1));
        }
        p.drawRect(rect.adjusted(0, 0, -1, -1));

        if (rect.width() > 45 && rect.height() > 18) {
            QFont f = p.font();
            f.setPixelSize(qMin(11, qMax(8, static_cast<int>(rect.height() / 3.5))));
            f.setBold(false);
            p.setFont(f);
            p.setPen(fill.lightness() > 100 ? Qt::black : Qt::white);

            const QString label = (rect.height() > 34)
                ? node->name + QStringLiteral("\n") + DiskNode::prettySize(node->size)
                : node->name;

            p.drawText(rect.adjusted(3, 3, -3, -3),
                       Qt::AlignCenter | Qt::TextWordWrap, label);
        }
    }

    m_cells.append({rect, node, depth});
}

QColor TreemapWidget::colorForNode(DiskNode* node, int depth,
                                   const wintools::themes::ThemePalette& theme,
                                   bool darkTheme) {

    if (node->isDir) {
        const QColor from = darkTheme
            ? blend(theme.cardBackground, theme.windowBackground, 0.55f)
            : blend(theme.cardBackground, theme.windowBackground, 0.18f);
        const QColor to = darkTheme
            ? blend(theme.accent, theme.windowBackground, 0.55f)
            : blend(theme.accent, theme.cardBackground, 0.22f);
        const float t = qBound(0.0f, static_cast<float>(depth) / 5.0f, 1.0f);
        return blend(from, to, t);
    }
    const QString cat = DiskNode::category(node->name);
    QColor c = categoryColor(cat);
    if (darkTheme) {
        return blend(c, theme.windowBackground, 0.08f);
    }
    return blend(c, theme.windowBackground, 0.30f);
}

DiskNode* TreemapWidget::hitTest(const QPointF& pos) const {

    for (int i = m_cells.size() - 1; i >= 0; --i) {
        if (m_cells[i].rect.contains(pos))
            return m_cells[i].node;
    }
    return nullptr;
}

QRectF TreemapWidget::nodeRect(DiskNode* node) const {
    if (!node) return {};
    for (int i = m_cells.size() - 1; i >= 0; --i) {
        if (m_cells[i].node == node) {
            return m_cells[i].rect;
        }
    }
    return {};
}

}
