#include "modules/AudioMaster/src/ui/vu_meter_widget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

namespace wintools::audiomaster {

VuMeterWidget::VuMeterWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMinimumHeight(60);
}

void VuMeterWidget::setLevel(float level) {
    m_level = qBound(0.0f, level, 1.0f);
    update();
}

QSize VuMeterWidget::sizeHint() const {
    return {12, 200};
}

QSize VuMeterWidget::minimumSizeHint() const {
    return {8, 60};
}

void VuMeterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int r = qMin(w, 6) / 2;

    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(0, 0, w, h), r, r);
    p.fillPath(bgPath, QColor(60, 60, 60, 160));

    const int fillH = qRound(m_level * h);
    if (fillH > 0) {
        QRectF fillRect(0, h - fillH, w, fillH);

        QLinearGradient grad(0, h, 0, 0);
        grad.setColorAt(0.0, QColor(0x17, 0xB3, 0x78));
        grad.setColorAt(0.6, QColor(0xF5, 0xC5, 0x42));
        grad.setColorAt(1.0, QColor(0xE8, 0x3B, 0x3B));

        QPainterPath fillPath;
        fillPath.addRoundedRect(fillRect, r, r);

        fillPath = fillPath.intersected(bgPath);
        p.fillPath(fillPath, grad);
    }
}

}
