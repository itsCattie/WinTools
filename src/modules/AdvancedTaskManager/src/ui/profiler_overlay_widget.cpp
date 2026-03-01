#include "modules/AdvancedTaskManager/src/ui/profiler_overlay_widget.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/Themes/window_colour.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QCursor>
#include <QScreen>
#include <QTimer>
#include <QtMath>

namespace wintools::taskmanager {

namespace {
QString formatBytes(quint64 bytes) {
    constexpr double k1024 = 1024.0;
    const double value = static_cast<double>(bytes);
    if (value >= k1024 * k1024 * k1024) {
        return QStringLiteral("%1 GB").arg(value / (k1024 * k1024 * k1024), 0, 'f', 1);
    }
    if (value >= k1024 * k1024) {
        return QStringLiteral("%1 MB").arg(value / (k1024 * k1024), 0, 'f', 1);
    }
    if (value >= k1024) {
        return QStringLiteral("%1 KB").arg(value / k1024, 0, 'f', 0);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

QString formatRate(quint64 bytesPerSec) {
    return QStringLiteral("%1/s").arg(formatBytes(bytesPerSec));
}

double distanceToRect(const QRect& rect, const QPoint& point) {
    const int dx = (point.x() < rect.left())
        ? (rect.left() - point.x())
        : ((point.x() > rect.right()) ? (point.x() - rect.right()) : 0);
    const int dy = (point.y() < rect.top())
        ? (rect.top() - point.y())
        : ((point.y() > rect.bottom()) ? (point.y() - rect.bottom()) : 0);
    return qSqrt(static_cast<double>((dx * dx) + (dy * dy)));
}
}

ProfilerOverlayWidget::ProfilerOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);

    m_palette = wintools::themes::ThemeHelper::currentPalette();
    setWindowOpacity(m_opacity);

        m_hoverInteractionTimer = new QTimer(this);
        m_hoverInteractionTimer->setInterval(33);
        connect(m_hoverInteractionTimer, &QTimer::timeout,
            this, &ProfilerOverlayWidget::updateHoverInteraction);
}

void ProfilerOverlayWidget::setSystemPerf(const SystemPerf& perf) {
    m_perf = perf;
    update();
}

void ProfilerOverlayWidget::applyTheme(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;
    update();
}

void ProfilerOverlayWidget::setOverlayOpacity(double opacity) {
    m_opacity = qBound(0.35, opacity, 1.0);
    if (!m_autoHiddenByHover && isVisible()) {
        setWindowOpacity(m_opacity);
    }
}

void ProfilerOverlayWidget::setScaleFactor(double scale) {
    const double oldScale = m_scale;
    m_scale = qBound(0.75, scale, 1.75);
    if (!qFuzzyCompare(oldScale, m_scale)) {
        resize(sizeHint());
        updateAnchorPosition();
        update();
    }
}

void ProfilerOverlayWidget::setCorner(Corner corner) {
    m_corner = corner;
    updateAnchorPosition();
}

void ProfilerOverlayWidget::setShowNetwork(bool show) {
    m_showNetwork = show;
    resize(sizeHint());
    updateAnchorPosition();
    update();
}

void ProfilerOverlayWidget::setShowDisk(bool show) {
    m_showDisk = show;
    resize(sizeHint());
    updateAnchorPosition();
    update();
}

QSize ProfilerOverlayWidget::sizeHint() const {
    const int width = static_cast<int>(260.0 * m_scale);
    const int lineHeight = static_cast<int>(18.0 * m_scale);
    const int pad = static_cast<int>(10.0 * m_scale);
    const int header = static_cast<int>(22.0 * m_scale);
    const int barBlockHeight = static_cast<int>(26.0 * m_scale);
    const int bars = barBlockHeight * 2;
    const int bodyHeight = (lineCount() * lineHeight) + bars;
    return { width, pad + header + bodyHeight + pad };
}

void ProfilerOverlayWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF outer = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(m_palette.cardBorder, 1.0));
    p.setBrush(m_palette.cardBackground);
    p.drawRoundedRect(outer, 10.0, 10.0);

    QFont titleFont = p.font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * m_scale);
    p.setFont(titleFont);
    p.setPen(m_palette.foreground);
    const int pad = static_cast<int>(10.0 * m_scale);
    p.drawText(QRect(pad, pad, width() - (pad * 2), static_cast<int>(20.0 * m_scale)),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Profiler Overlay"));

    const int barTop = static_cast<int>(34.0 * m_scale);
    const int barHeight = static_cast<int>(8.0 * m_scale);
    const int barBlockHeight = static_cast<int>(26.0 * m_scale);
    const int barWidth = width() - (pad * 2);

    auto drawBar = [&](int y, double pct, const QColor& color, const QString& text) {
        QRect bg(pad, y, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        QColor bgCol = m_palette.windowBackground;
        bgCol.setAlpha(180);
        p.setBrush(bgCol);
        p.drawRoundedRect(bg, 4, 4);

        const int filled = static_cast<int>(qBound(0.0, pct, 100.0) / 100.0 * barWidth);
        QRect fill(bg.left(), bg.top(), filled, barHeight);
        p.setBrush(color);
        p.drawRoundedRect(fill, 4, 4);

        p.setPen(m_palette.mutedForeground);
        QFont infoFont = p.font();
        infoFont.setBold(false);
        infoFont.setPointSizeF(infoFont.pointSizeF() * 0.9);
        p.setFont(infoFont);
        p.drawText(QRect(pad, y + barHeight + 2, barWidth, static_cast<int>(14.0 * m_scale)),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   text);
    };

    drawBar(barTop,
            m_perf.cpuUsagePercent,
            m_palette.accent,
            QStringLiteral("CPU %1%").arg(m_perf.cpuUsagePercent, 0, 'f', 1));

        QColor gpuColor = QColor(0x00, 0xB4, 0xD8);
    drawBar(barTop + barBlockHeight,
            m_perf.gpuUsagePercent,
            gpuColor,
            QStringLiteral("GPU %1%").arg(m_perf.gpuUsagePercent, 0, 'f', 1));

    QFont bodyFont = p.font();
    bodyFont.setPointSizeF(bodyFont.pointSizeF() * 0.95);
    p.setFont(bodyFont);
    p.setPen(m_palette.foreground);

    int line = 0;
    p.drawText(nextLineRect(line++), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("CPU: %1%   GPU: %2%")
                   .arg(m_perf.cpuUsagePercent, 0, 'f', 1)
                   .arg(m_perf.gpuUsagePercent, 0, 'f', 1));

    p.drawText(nextLineRect(line++), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Processes: %1   Threads: %2")
                   .arg(m_perf.processCount)
                   .arg(m_perf.threadCount));

    if (m_showNetwork) {
        p.drawText(nextLineRect(line++), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Network: ↓ %1  ↑ %2")
                       .arg(formatRate(m_perf.network.recvBytesPerSec))
                       .arg(formatRate(m_perf.network.sentBytesPerSec)));
    }

    if (m_showDisk) {
        quint64 totalRead = 0;
        quint64 totalWrite = 0;
        for (const auto& disk : m_perf.disks) {
            totalRead += disk.readBytesPerSec;
            totalWrite += disk.writeBytesPerSec;
        }
        p.drawText(nextLineRect(line++), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Disk: R %1  W %2")
                       .arg(formatRate(totalRead))
                       .arg(formatRate(totalWrite)));
    }

    p.setPen(m_palette.mutedForeground);
    p.drawText(nextLineRect(line), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("RAM: %1 / %2")
                   .arg(formatBytes(m_perf.usedMemoryBytes))
                   .arg(formatBytes(m_perf.totalMemoryBytes)));
}

void ProfilerOverlayWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    resize(sizeHint());
    updateAnchorPosition();
    if (m_hoverInteractionTimer && !m_hoverInteractionTimer->isActive()) {
        m_hoverInteractionTimer->start();
    }
    updateHoverInteraction();
}

void ProfilerOverlayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateAnchorPosition();
}

void ProfilerOverlayWidget::updateAnchorPosition() {
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const QRect area = screen->availableGeometry();
    const int margin = static_cast<int>(10.0 * m_scale);

    int x = area.right() - width() - margin;
    int y = area.top() + margin;

    switch (m_corner) {
    case Corner::TopLeft:
        x = area.left() + margin;
        y = area.top() + margin;
        break;
    case Corner::TopRight:
        x = area.right() - width() - margin;
        y = area.top() + margin;
        break;
    case Corner::BottomLeft:
        x = area.left() + margin;
        y = area.bottom() - height() - margin;
        break;
    case Corner::BottomRight:
        x = area.right() - width() - margin;
        y = area.bottom() - height() - margin;
        break;
    }

    move(x, y);
}

void ProfilerOverlayWidget::updateHoverInteraction() {
    if (!m_hoverInteractionTimer) {
        return;
    }

    if (!isVisible() && !m_autoHiddenByHover) {
        m_hoverInteractionTimer->stop();
        return;
    }

    const QPoint cursorPos = QCursor::pos();

    if (m_autoHiddenByHover) {
        if (m_lastVisibleFrame.contains(cursorPos)) {
            return;
        }

        m_autoHiddenByHover = false;
        show();
        m_lastVisibleFrame = frameGeometry();
    }

    if (!isVisible()) {
        return;
    }

    m_lastVisibleFrame = frameGeometry();

    if (m_lastVisibleFrame.contains(cursorPos)) {
        m_autoHiddenByHover = true;
        hide();
        return;
    }

    constexpr double kMinVisibleFactor = 0.02;
    const double proximityRange = 140.0 * m_scale;
    const double distance = distanceToRect(m_lastVisibleFrame, cursorPos);
    const double t = qBound(0.0, distance / proximityRange, 1.0);
    const double eased = t * t;
    const double visibilityFactor = kMinVisibleFactor + ((1.0 - kMinVisibleFactor) * eased);
    setWindowOpacity(m_opacity * visibilityFactor);
}

int ProfilerOverlayWidget::lineCount() const {
    int count = 3;
    if (m_showNetwork) {
        ++count;
    }
    if (m_showDisk) {
        ++count;
    }
    return count;
}

QRect ProfilerOverlayWidget::nextLineRect(int index) const {
    const int pad = static_cast<int>(10.0 * m_scale);
    const int lineHeight = static_cast<int>(18.0 * m_scale);
    const int top = static_cast<int>(88.0 * m_scale) + (index * lineHeight);
    return QRect(pad, top, width() - (pad * 2), lineHeight);
}

}
