#include "modules/AdvancedTaskManager/src/ui/profiler_overlay_controller.hpp"

#include "common/themes/theme_helper.hpp"
#include "common/themes/theme_listener.hpp"
#include "logger/logger.hpp"
#include "modules/AdvancedTaskManager/src/core/process_monitor.hpp"
#include "modules/AdvancedTaskManager/src/ui/profiler_overlay_widget.hpp"

namespace wintools::taskmanager {

namespace {
constexpr const char* kLog = "TaskManager/OverlayController";
}

ProfilerOverlayController* ProfilerOverlayController::instance() {
    static auto* ctrl = new ProfilerOverlayController();
    return ctrl;
}

ProfilerOverlayController::ProfilerOverlayController(QObject* parent)
    : QObject(parent)
    , m_palette(wintools::themes::ThemeHelper::currentPalette())
{
    m_monitor = new ProcessMonitor(this);
    connect(m_monitor, &ProcessMonitor::processesUpdated,
            this, &ProfilerOverlayController::onProcessesUpdated);
    m_monitor->start();

    m_themeListener = new wintools::themes::ThemeListener(this);
    connect(m_themeListener, &wintools::themes::ThemeListener::themeChanged,
            this, [this](bool) {
                applyTheme(wintools::themes::ThemeHelper::currentPalette());
            });

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        "Profiler overlay controller started.");
}

void ProfilerOverlayController::ensureOverlay() {
    if (m_overlay) {
        return;
    }

    m_overlay = new ProfilerOverlayWidget(nullptr);
    m_overlay->applyTheme(m_palette);
    m_overlay->setCorner(static_cast<ProfilerOverlayWidget::Corner>(m_corner));
    m_overlay->setScaleFactor(m_scale);
    m_overlay->setOverlayOpacity(m_opacity);
    m_overlay->setShowNetwork(m_showNetwork);
    m_overlay->setShowDisk(m_showDisk);
    m_overlay->setSystemPerf(m_lastSysPerf);

    connect(m_overlay, &QObject::destroyed, this, [this]() {
        m_overlay = nullptr;
    });
}

void ProfilerOverlayController::toggleOverlay() {
    if (isOverlayVisible()) {
        hideOverlay();
    } else {
        showOverlay();
    }
}

void ProfilerOverlayController::showOverlay() {
    ensureOverlay();
    if (!m_overlay) {
        return;
    }

    m_overlay->setSystemPerf(m_lastSysPerf);
    m_overlay->show();
    emit overlayVisibilityChanged(true);
}

void ProfilerOverlayController::hideOverlay() {
    if (!m_overlay) {
        emit overlayVisibilityChanged(false);
        return;
    }

    m_overlay->hide();
    emit overlayVisibilityChanged(false);
}

bool ProfilerOverlayController::isOverlayVisible() const {
    return m_overlay && m_overlay->isVisible();
}

void ProfilerOverlayController::applyTheme(const wintools::themes::ThemePalette& palette) {
    m_palette = palette;
    if (m_overlay) {
        m_overlay->applyTheme(palette);
    }
}

void ProfilerOverlayController::setCorner(int corner) {
    m_corner = corner;
    ensureOverlay();
    if (m_overlay) {
        m_overlay->setCorner(static_cast<ProfilerOverlayWidget::Corner>(m_corner));
    }
}

void ProfilerOverlayController::setScaleFactor(double scale) {
    m_scale = scale;
    ensureOverlay();
    if (m_overlay) {
        m_overlay->setScaleFactor(m_scale);
    }
}

void ProfilerOverlayController::setOverlayOpacity(double opacity) {
    m_opacity = opacity;
    ensureOverlay();
    if (m_overlay) {
        m_overlay->setOverlayOpacity(m_opacity);
    }
}

void ProfilerOverlayController::setShowNetwork(bool show) {
    m_showNetwork = show;
    ensureOverlay();
    if (m_overlay) {
        m_overlay->setShowNetwork(show);
    }
}

void ProfilerOverlayController::setShowDisk(bool show) {
    m_showDisk = show;
    ensureOverlay();
    if (m_overlay) {
        m_overlay->setShowDisk(show);
    }
}

void ProfilerOverlayController::onProcessesUpdated(
    QVector<wintools::taskmanager::ProcessInfo>,
    wintools::taskmanager::SystemPerf sysPerf) {
    m_lastSysPerf = sysPerf;
    if (m_overlay && m_overlay->isVisible()) {
        m_overlay->setSystemPerf(sysPerf);
    }
}

}
