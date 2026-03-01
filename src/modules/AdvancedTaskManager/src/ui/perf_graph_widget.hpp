#pragma once

#include <QWidget>
#include <QPoint>
#include <deque>
#include <QColor>
#include <QString>

namespace wintools::taskmanager {

class PerfGraphWidget : public QWidget {
    Q_OBJECT

public:
    enum class ValueFormat {
        Percent,
        BytesPerSec
    };

    explicit PerfGraphWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setLineColor(const QColor& color);
    void setGridColor(const QColor& color);
    void setBackground(const QColor& color);
    void setTextColor(const QColor& color);
    void setSubtleTextColor(const QColor& color);
    void setCrosshairColor(const QColor& color);
    void setMaxValue(double maxValue);

    void setValueFormat(ValueFormat fmt);

    void setAutoScale(bool enabled);

    void setHistoryLength(int samples);

    void addValue(double value);

    double currentValue() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString       m_title;
    QColor        m_lineColor   { 0x17, 0xB3, 0x78 };
    QColor        m_gridColor   { 0x30, 0x30, 0x30 };
    QColor        m_fillColor;
    QColor        m_background  { 0x10, 0x10, 0x10 };
    QColor        m_textColor   { 0xCC, 0xCC, 0xCC };
    QColor        m_subtleTextColor { 0x88, 0x88, 0x88 };
    QColor        m_crosshairColor { 0xFF, 0xFF, 0xFF, 0x80 };
    double        m_maxValue    { 100.0 };
    int           m_historyLen  { 60 };
    ValueFormat   m_valueFormat { ValueFormat::Percent };
    bool          m_autoScale   { false };
    std::deque<double> m_history;

    void drawGrid(class QPainter& p, const QRect& r) const;
    void drawCurve(class QPainter& p, const QRect& r) const;
    void drawLabels(class QPainter& p, const QRect& r) const;
    void drawCrosshair(class QPainter& p, const QRect& r) const;
    QString buildTooltip(int sampleIndex) const;

    QPoint m_mousePos;
    bool   m_mouseInside { false };
    int    m_hoveredSample { -1 };

    static QString formatByteRate(double bps);
    static QString formatByteRateShort(double bps);
};

}
