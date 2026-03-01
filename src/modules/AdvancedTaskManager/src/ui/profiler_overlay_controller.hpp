#pragma once

#include "common/themes/window_colour.hpp"
#include "modules/AdvancedTaskManager/src/core/process_info.hpp"

#include <QObject>

namespace wintools::themes { class ThemeListener; }

namespace wintools::taskmanager {

class ProcessMonitor;
class ProfilerOverlayWidget;

class ProfilerOverlayController : public QObject {
    Q_OBJECT

public:
    static ProfilerOverlayController* instance();

    void toggleOverlay();
    void showOverlay();
    void hideOverlay();
    bool isOverlayVisible() const;

    void applyTheme(const wintools::themes::ThemePalette& palette);
    void setCorner(int corner);
    void setScaleFactor(double scale);
    void setOverlayOpacity(double opacity);
    void setShowNetwork(bool show);
    void setShowDisk(bool show);

    bool showNetwork() const { return m_showNetwork; }
    bool showDisk() const { return m_showDisk; }

signals:
    void overlayVisibilityChanged(bool visible);

private slots:
    void onProcessesUpdated(QVector<wintools::taskmanager::ProcessInfo> processes,
                            wintools::taskmanager::SystemPerf sysPerf);

private:
    explicit ProfilerOverlayController(QObject* parent = nullptr);
    void ensureOverlay();

    ProcessMonitor* m_monitor = nullptr;
    ProfilerOverlayWidget* m_overlay = nullptr;
    wintools::themes::ThemeListener* m_themeListener = nullptr;
    wintools::themes::ThemePalette m_palette;
    SystemPerf m_lastSysPerf;

    int m_corner = 1;
    double m_scale = 1.0;
    double m_opacity = 0.85;
    bool m_showNetwork = true;
    bool m_showDisk = true;
};

}
