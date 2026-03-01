#pragma once

#include <QWidget>
#include <QRect>

class QTimer;

#include "modules/AdvancedTaskManager/src/core/process_info.hpp"
#include "common/Themes/window_colour.hpp"

namespace wintools::taskmanager {

class ProfilerOverlayWidget : public QWidget {
    Q_OBJECT

public:
    enum class Corner {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    explicit ProfilerOverlayWidget(QWidget* parent = nullptr);

    void setSystemPerf(const SystemPerf& perf);
    void applyTheme(const wintools::themes::ThemePalette& palette);

    void setOverlayOpacity(double opacity);
    void setScaleFactor(double scale);
    void setCorner(Corner corner);
    void setShowNetwork(bool show);
    void setShowDisk(bool show);

    bool showNetwork() const { return m_showNetwork; }
    bool showDisk() const { return m_showDisk; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateAnchorPosition();
    void updateHoverInteraction();
    int lineCount() const;
    QRect nextLineRect(int index) const;

    SystemPerf m_perf;

    wintools::themes::ThemePalette  m_palette;

    double m_opacity = 0.85;
    double m_scale = 1.0;
    Corner m_corner = Corner::TopRight;
    bool   m_showNetwork = true;
    bool   m_showDisk = true;

    bool m_autoHiddenByHover = false;
    QRect m_lastVisibleFrame;
    QTimer* m_hoverInteractionTimer = nullptr;
};

}
