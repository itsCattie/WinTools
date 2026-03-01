#include "modules/AdvancedTaskManager/src/ui/perf_graph_widget.hpp"

#include <algorithm>
#include <numeric>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QtMath>

namespace wintools::taskmanager {

PerfGraphWidget::PerfGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
    setMouseTracking(true);

    m_history.resize(static_cast<std::size_t>(m_historyLen), 0.0);
}

void PerfGraphWidget::setTitle(const QString& title)     { m_title = title; update(); }
void PerfGraphWidget::setMaxValue(double v)              { m_maxValue = v;  update(); }
void PerfGraphWidget::setGridColor(const QColor& c)      { m_gridColor = c; update(); }
void PerfGraphWidget::setBackground(const QColor& c)     { m_background = c; update(); }
void PerfGraphWidget::setTextColor(const QColor& c)      { m_textColor = c; update(); }
void PerfGraphWidget::setSubtleTextColor(const QColor& c){ m_subtleTextColor = c; update(); }
void PerfGraphWidget::setCrosshairColor(const QColor& c) { m_crosshairColor = c; update(); }

void PerfGraphWidget::setLineColor(const QColor& c) {
    m_lineColor = c;
    m_fillColor = c;
    m_fillColor.setAlpha(60);
    update();
}

void PerfGraphWidget::setValueFormat(ValueFormat fmt) {
    m_valueFormat = fmt;
    update();
}

void PerfGraphWidget::setAutoScale(bool enabled) {
    m_autoScale = enabled;
    update();
}

void PerfGraphWidget::setHistoryLength(int samples) {
    m_historyLen = qMax(10, samples);
    while (m_history.size() > static_cast<std::size_t>(m_historyLen))
        m_history.pop_front();
    while (m_history.size() < static_cast<std::size_t>(m_historyLen))
        m_history.push_front(0.0);
    update();
}

void PerfGraphWidget::addValue(double value) {
    m_history.push_back(qBound(0.0, value, m_autoScale ? value : m_maxValue));
    while (m_history.size() > static_cast<std::size_t>(m_historyLen))
        m_history.pop_front();

    if (m_autoScale && !m_history.empty()) {
        double peak = *std::max_element(m_history.begin(), m_history.end());
        double needed = peak * 1.25;
        if (needed > m_maxValue || m_maxValue > needed * 4.0)
            m_maxValue = qMax(1024.0, needed);
    }

    update();
}

double PerfGraphWidget::currentValue() const {
    return m_history.empty() ? 0.0 : m_history.back();
}

QSize PerfGraphWidget::sizeHint()        const { return { 280, 130 }; }
QSize PerfGraphWidget::minimumSizeHint() const { return { 120,  80 }; }

void PerfGraphWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect rc = rect();

    p.fillRect(rc, m_background);

    const int marginTop    = 18;
    const int marginBottom = 18;
    const int marginLeft   = 8;
    const int marginRight  = 8;
    const QRect gr(marginLeft, marginTop,
                   rc.width()  - marginLeft - marginRight,
                   rc.height() - marginTop  - marginBottom);

    if (gr.width() <= 0 || gr.height() <= 0) return;

    drawGrid(p, gr);
    drawCurve(p, gr);
    drawLabels(p, gr);
    drawCrosshair(p, gr);
}

void PerfGraphWidget::drawGrid(QPainter& p, const QRect& r) const {
    p.setPen(QPen(m_gridColor, 1, Qt::DotLine));

    for (int i = 1; i <= 4; ++i) {
        int y = r.bottom() - static_cast<int>(r.height() * i / 4);
        p.drawLine(r.left(), y, r.right(), y);
    }
}

void PerfGraphWidget::drawCurve(QPainter& p, const QRect& r) const {
    if (m_history.size() < 2) return;

    const int n = static_cast<int>(m_history.size());
    const double xStep = static_cast<double>(r.width()) / (n - 1);

    auto toPoint = [&](int i, double val) -> QPointF {
        double x = r.left() + i * xStep;
        double y = r.bottom() - (val / m_maxValue) * r.height();
        return { x, y };
    };

    auto buildSmoothPath = [&]() -> QPainterPath {
        QPainterPath path;
        QPointF p0 = toPoint(0, m_history[0]);
        path.moveTo(p0);

        if (n == 2) {
            path.lineTo(toPoint(1, m_history[1]));
            return path;
        }

        for (int i = 0; i < n - 1; ++i) {
            QPointF pPrev = (i > 0) ? toPoint(i - 1, m_history[i - 1]) : toPoint(i, m_history[i]);
            QPointF pCur  = toPoint(i, m_history[i]);
            QPointF pNext = toPoint(i + 1, m_history[i + 1]);
            QPointF pNext2 = (i + 2 < n) ? toPoint(i + 2, m_history[i + 2]) : pNext;

            QPointF cp1(
                pCur.x() + (pNext.x() - pPrev.x()) / 6.0,
                pCur.y() + (pNext.y() - pPrev.y()) / 6.0);
            QPointF cp2(
                pNext.x() - (pNext2.x() - pCur.x()) / 6.0,
                pNext.y() - (pNext2.y() - pCur.y()) / 6.0);

            path.cubicTo(cp1, cp2, pNext);
        }
        return path;
    };

    QPainterPath curve = buildSmoothPath();

    QPainterPath fill;
    fill.moveTo(QPointF(r.left(), r.bottom()));
    fill.lineTo(toPoint(0, m_history[0]));

    for (int i = 1; i < curve.elementCount(); ++i) {
        QPainterPath::Element el = curve.elementAt(i);
        if (el.type == QPainterPath::CurveToElement && i + 2 < curve.elementCount()) {
            QPainterPath::Element cp2 = curve.elementAt(i + 1);
            QPainterPath::Element end = curve.elementAt(i + 2);
            fill.cubicTo(el.x, el.y, cp2.x, cp2.y, end.x, end.y);
            i += 2;
        } else if (el.type == QPainterPath::LineToElement) {
            fill.lineTo(el.x, el.y);
        }
    }
    fill.lineTo(QPointF(r.right(), r.bottom()));
    fill.closeSubpath();

    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    QColor topColor = m_lineColor;
    topColor.setAlpha(80);
    QColor botColor = m_lineColor;
    botColor.setAlpha(15);
    grad.setColorAt(0.0, topColor);
    grad.setColorAt(1.0, botColor);
    p.fillPath(fill, grad);

    p.setPen(QPen(m_lineColor, 1.5));
    p.drawPath(curve);
}

void PerfGraphWidget::drawLabels(QPainter& p, const QRect& r) const {
    p.setPen(QPen(m_textColor));
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);

    if (!m_title.isEmpty()) {
        p.drawText(r.left(), r.top() - 4, m_title);
    }

    double cur = currentValue();
    QString valStr;
    if (m_valueFormat == ValueFormat::BytesPerSec) {
        valStr = formatByteRate(cur);
    } else {
        valStr = QStringLiteral("%1%").arg(cur, 0, 'f', 1);
    }
    p.drawText(r.right() - 80, r.top() - 4, 80, 16, Qt::AlignRight, valStr);

    f.setPixelSize(9);
    p.setFont(f);
    p.setPen(QPen(m_subtleTextColor));
    p.drawText(r.left(), r.bottom() + 14, QStringLiteral("0"));
    if (m_valueFormat == ValueFormat::BytesPerSec) {
        p.drawText(r.left(), r.top() + 9, formatByteRateShort(m_maxValue));
    } else {
        p.drawText(r.left(), r.top() + 9, QStringLiteral("100%"));
    }
}

QString PerfGraphWidget::formatByteRate(double bps) {
    if (bps >= 1024.0 * 1024.0 * 1024.0)
        return QStringLiteral("%1 GB/s").arg(bps / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    if (bps >= 1024.0 * 1024.0)
        return QStringLiteral("%1 MB/s").arg(bps / (1024.0 * 1024.0), 0, 'f', 1);
    if (bps >= 1024.0)
        return QStringLiteral("%1 KB/s").arg(bps / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 B/s").arg(bps, 0, 'f', 0);
}

QString PerfGraphWidget::formatByteRateShort(double bps) {
    if (bps >= 1024.0 * 1024.0 * 1024.0)
        return QStringLiteral("%1G").arg(bps / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    if (bps >= 1024.0 * 1024.0)
        return QStringLiteral("%1M").arg(bps / (1024.0 * 1024.0), 0, 'f', 0);
    if (bps >= 1024.0)
        return QStringLiteral("%1K").arg(bps / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1").arg(bps, 0, 'f', 0);
}

void PerfGraphWidget::mouseMoveEvent(QMouseEvent* event) {
    m_mousePos    = event->pos();
    m_mouseInside = true;

    const int marginTop    = 18;
    const int marginBottom = 18;
    const int marginLeft   = 8;
    const int marginRight  = 8;
    const QRect gr(marginLeft, marginTop,
                   width()  - marginLeft - marginRight,
                   height() - marginTop  - marginBottom);

    const int n = static_cast<int>(m_history.size());
    if (n > 1 && gr.width() > 0) {
        double xStep = static_cast<double>(gr.width()) / (n - 1);
        int idx = qBound(0,
            static_cast<int>(std::round((m_mousePos.x() - gr.left()) / xStep)),
            n - 1);
        if (idx != m_hoveredSample) {
            m_hoveredSample = idx;
            QToolTip::showText(event->globalPosition().toPoint(),
                               buildTooltip(idx), this, rect(), 5000);
        }
    }
    update();
    QWidget::mouseMoveEvent(event);
}

void PerfGraphWidget::leaveEvent(QEvent* event) {
    m_mouseInside   = false;
    m_hoveredSample = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}

void PerfGraphWidget::drawCrosshair(QPainter& p, const QRect& r) const {
    if (!m_mouseInside || m_hoveredSample < 0) return;
    const int n = static_cast<int>(m_history.size());
    if (n < 2) return;

    double xStep = static_cast<double>(r.width()) / (n - 1);
    int cx = r.left() + static_cast<int>(m_hoveredSample * xStep);

    p.setPen(QPen(m_crosshairColor, 1, Qt::DashLine));
    p.drawLine(cx, r.top(), cx, r.bottom());

    double val  = m_history[static_cast<std::size_t>(m_hoveredSample)];
    int    cy   = r.bottom() - static_cast<int>((val / m_maxValue) * r.height());
    p.setBrush(m_lineColor);
    p.setPen(QPen(Qt::white, 1));
    p.drawEllipse(QPoint(cx, cy), 4, 4);
}

QString PerfGraphWidget::buildTooltip(int idx) const {
    if (m_history.empty() || idx < 0 || idx >= static_cast<int>(m_history.size()))
        return {};

    double val     = m_history[static_cast<std::size_t>(idx)];
    double cur     = m_history.back();
    double minVal  = *std::min_element(m_history.begin(), m_history.end());
    double maxVal  = *std::max_element(m_history.begin(), m_history.end());
    double sum     = std::accumulate(m_history.begin(), m_history.end(), 0.0);
    double avg     = sum / static_cast<double>(m_history.size());

    int secsAgo = static_cast<int>(m_history.size()) - 1 - idx;

    auto fmt = [this](double v) -> QString {
        if (m_valueFormat == ValueFormat::BytesPerSec)
            return formatByteRate(v);
        return QStringLiteral("%1%").arg(v, 0, 'f', 1);
    };

    const QString accent = m_lineColor.name();
    const QString subtle = m_subtleTextColor.name();
    const QString strong = m_textColor.name();

    QString html = QStringLiteral(
        "<table style='font-size:11px; white-space:nowrap;'>"
        "<tr><td colspan='2' style='color:%9; font-weight:bold; padding-bottom:3px;'>%1</td></tr>"
        "<tr><td style='color:%10; padding-right:8px;'>At cursor</td>"
            "<td style='color:%11;'>%2</td></tr>"
        "<tr><td style='color:%10;'>%3</td>"
            "<td style='color:%11;'>%4</td></tr>"
        "<tr><td style='color:%10;'>Current</td>"
            "<td style='color:%11;'>%5</td></tr>"
        "<tr><td style='color:%10;'>Min</td>"
            "<td style='color:%11'>%6</td></tr>"
        "<tr><td style='color:%10;'>Max</td>"
            "<td style='color:%11'>%7</td></tr>"
        "<tr><td style='color:%10;'>Avg</td>"
            "<td style='color:%11;'>%8</td></tr>"
        "</table>")
        .arg(m_title)
        .arg(fmt(val))
        .arg(secsAgo == 0 ? QStringLiteral("(now)") :
             QStringLiteral("%1s ago").arg(secsAgo))
        .arg(secsAgo == 0 ? fmt(val) : fmt(val))
        .arg(fmt(cur))
        .arg(fmt(minVal))
        .arg(fmt(maxVal))
        .arg(fmt(avg))
        .arg(accent)
        .arg(subtle)
        .arg(strong);
    return html;
}
}
